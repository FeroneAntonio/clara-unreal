#include "ClaraAudio.h"
#include <limits>
#include "Audio.h"
#include "ClaraAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace
{
const TCHAR* BedFiles[]={TEXT("Ambience/office.wav"),TEXT("Ambience/server.wav"),TEXT("Ambience/keys.wav")};
bool ReadPcmWave(const TArray<uint8>& Bytes,FWaveModInfo& Info)
{
    if(Bytes.Num()<44||!Info.ReadWaveInfo(Bytes.GetData(),Bytes.Num())||*Info.pFormatTag!=FWaveModInfo::WAVE_INFO_FORMAT_PCM
        ||*Info.pBitsPerSample!=16||*Info.pChannels<1||*Info.pChannels>2
        ||*Info.pSamplesPerSec<8000||*Info.pSamplesPerSec>192000||Info.SampleDataSize==0)return false;
    const uint32 FrameBytes=2u*uint32(*Info.pChannels);
    return *Info.pBlockAlign==FrameBytes&&Info.SampleDataSize%FrameBytes==0
        &&Info.SampleDataStart>=Bytes.GetData()&&Info.SampleDataStart<=Bytes.GetData()+Bytes.Num()
        &&Info.SampleDataSize<=uint64(Bytes.GetData()+Bytes.Num()-Info.SampleDataStart);
}
// Distanza in pianta da una stanza rettangolare: zero quando il giocatore e' dentro.
float OutsideRoom(const FVector& P,float X0,float Y0,float X1,float Y1)
{
    const float DX=FMath::Max(FMath::Max(X0-float(P.X),float(P.X)-X1),0.f),DY=FMath::Max(FMath::Max(Y0-float(P.Y),float(P.Y)-Y1),0.f);
    return FMath::Sqrt(DX*DX+DY*DY);
}
}

AClaraAudio::AClaraAudio(){PrimaryActorTick.bCanEverTick=true;}
AClaraAudio* AClaraAudio::Get(const UObject* Context)
{
    UWorld* World=Context?Context->GetWorld():nullptr;if(!World)return nullptr;
    TActorIterator<AClaraAudio> It(World);if(It)return *It;
    return nullptr;
}
void AClaraAudio::Cue(const UObject* Context,const TCHAR* Name,float Volume,float Pitch)
{if(AClaraAudio* Audio=Get(Context))Audio->PlayCue(Name,Volume,Pitch);}
FString AClaraAudio::VoiceKey(const FString& Line)
{
    // Stessa regola di Scripts/voices: MD5 del testo UTF-8 senza spazi ai bordi, prime 16 cifre.
    FString Text=Line;Text.TrimStartAndEndInline();const FTCHARToUTF8 Utf8(*Text);
    return FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()),Utf8.Length()).ToLower().Left(16);
}
void AClaraAudio::BeginPlay()
{
    Super::BeginPlay();
    LoadPreferences();
    bEnabled=GEngine&&GEngine->UseSound()&&!FApp::IsUnattended()&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraMute"))&&GetWorld()->GetAudioDeviceRaw()!=nullptr;
    if(!bEnabled)return;
    for(const TCHAR* File:BedFiles){Beds.Add(Start(Load(File),.001f,1.f,true));BedVolume.Add(0.f);}
    UE_LOG(LogTemp,Display,TEXT("CLARA_AUDIO_READY: %d ambient loops from Content/Audio; voices resolved per line."),Beds.FilterByPredicate([](const TObjectPtr<UAudioComponent>& Bed){return Bed!=nullptr;}).Num());
}
void AClaraAudio::EndPlay(const EEndPlayReason::Type Reason)
{
    StopVoice();
    for(UAudioComponent* Bed:Beds)if(Bed){Bed->Stop();Bed->DestroyComponent();}
    for(const FOneShot& Shot:OneShots)if(UAudioComponent* Component=Shot.Component.Get()){Component->Stop();Component->DestroyComponent();}
    Beds.Empty();OneShots.Empty();Clips.Empty();
    Super::EndPlay(Reason);
}
TSharedPtr<AClaraAudio::FClip> AClaraAudio::Load(const FString& Relative,bool bCache)
{
    if(const TSharedPtr<FClip>* Found=Clips.Find(Relative))return *Found;
    TSharedPtr<FClip> Clip;TArray<uint8> Bytes;FWaveModInfo Info;
    if(FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectContentDir()/TEXT("Audio")/Relative),FILEREAD_Silent)
        &&ReadPcmWave(Bytes,Info))
    {
        Clip=MakeShared<FClip>();Clip->Rate=int32(*Info.pSamplesPerSec);Clip->Channels=int32(*Info.pChannels);
        Clip->Pcm.Append(Info.SampleDataStart,int32(Info.SampleDataSize));
        Clip->Seconds=float(Info.SampleDataSize)/float(2*Clip->Channels*Clip->Rate);
    }
    if(bCache)Clips.Add(Relative,Clip); // anche un file assente resta annotato: nessuna lettura ripetuta dal disco
    return Clip;
}
UAudioComponent* AClaraAudio::Start(const TSharedPtr<FClip>& Clip,float Volume,float Pitch,bool bLoop)
{
    if(!bEnabled||!Clip.IsValid()||Clip->Pcm.IsEmpty())return nullptr;
    USoundWaveProcedural* Wave=NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(uint32(Clip->Rate));Wave->NumChannels=Clip->Channels;
    Wave->Duration=bLoop?INDEFINITELY_LOOPING_DURATION:Clip->Seconds+.1f;
    Wave->SoundGroup=SOUNDGROUP_Default;Wave->bLooping=false;Wave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;
    Wave->QueueAudio(Clip->Pcm.GetData(),Clip->Pcm.Num());
    // Anello: quando il buffer si svuota lo stesso audio viene rimesso in coda, senza giunta udibile.
    if(bLoop)Wave->OnSoundWaveProceduralUnderflow.BindLambda([Clip](USoundWaveProcedural* Source,int32){Source->QueueAudio(Clip->Pcm.GetData(),Clip->Pcm.Num());});
    UAudioComponent* Component=NewObject<UAudioComponent>(this);
    Component->bAutoDestroy=false;Component->bAllowSpatialization=false;Component->bIsUISound=true;
    Component->SetSound(Wave);Component->SetVolumeMultiplier(Volume);Component->SetPitchMultiplier(Pitch);
    Component->RegisterComponent();Component->Play();
    return Component;
}
void AClaraAudio::PlayCue(const FString& Name,float Volume,float Pitch)
{
    if(!bEnabled||bMuted)return;
    Pitch=FMath::Clamp(Pitch,.25f,4.f);
    const TSharedPtr<FClip> Clip=Load(TEXT("Sfx/")+Name+TEXT(".wav"));
    if(UAudioComponent* Component=Start(Clip,Volume*EffectsVolume,Pitch,false))
        OneShots.Add({Component,float(GetWorld()->GetTimeSeconds())+Clip->Seconds/FMath::Max(Pitch,.25f)+.35f,Volume});
}
bool AClaraAudio::Speak(const FString& Speaker,const FString& Line)
{
    StopVoice();
    if(!bEnabled||bMuted||Speaker.IsEmpty()||Line.IsEmpty())return false;
    FString Folder;for(const TCHAR C:Speaker)if(FChar::IsAlnum(C))Folder.AppendChar(C);
    const TSharedPtr<FClip> Clip=Load(TEXT("Voices/")+Folder+TEXT("/")+VoiceKey(Line)+TEXT(".wav"),false);
    if(!Clip.IsValid())return false;
    Voice=Start(Clip,VoiceVolume,1.f,false);VoiceEnd=float(GetWorld()->GetTimeSeconds())+Clip->Seconds+.2f;
    return Voice!=nullptr;
}
void AClaraAudio::StopVoice()
{
    if(Voice){Voice->Stop();Voice->DestroyComponent();}
    Voice=nullptr;
}
void AClaraAudio::SetMuted(bool bMute)
{
    bMuted=bMute;
    if(!bMuted)return;
    StopVoice();
    for(const FOneShot& Shot:OneShots)if(UAudioComponent* Component=Shot.Component.Get())
    {Component->Stop();Component->DestroyComponent();}
    OneShots.Empty();
    for(int32 I=0;I<Beds.Num();++I)
    {BedVolume[I]=0.f;if(Beds[I])Beds[I]->SetVolumeMultiplier(0.f);}
}
namespace
{
float SafeVolume(float Value){return FMath::IsFinite(Value)?FMath::Clamp(Value,0.f,1.f):1.f;}
bool CanPersistAudio()
{
    return !GIsAutomationTesting&&!FApp::IsUnattended()&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraMute"))
        &&!FParse::Param(FCommandLine::Get(),TEXT("ClaraDevTour"))
        &&!FParse::Param(FCommandLine::Get(),TEXT("ClaraSelfTest"));
}
const TCHAR* AudioSection=TEXT("Clara.AudioPreferences");
}
void AClaraAudio::SetVolumes(float Voices,float Effects,float Ambience)
{
    VoiceVolume=SafeVolume(Voices);EffectsVolume=SafeVolume(Effects);AmbienceVolume=SafeVolume(Ambience);
    if(Voice)Voice->SetVolumeMultiplier(bMuted?0.f:VoiceVolume);
    for(const FOneShot& Shot:OneShots)if(UAudioComponent* Component=Shot.Component.Get())
        Component->SetVolumeMultiplier(bMuted?0.f:Shot.Volume*EffectsVolume);
    if(AmbienceVolume==0.f)for(UAudioComponent* Bed:Beds)if(Bed)Bed->SetVolumeMultiplier(0.f);
}
void AClaraAudio::LoadPreferences()
{
    if(!GConfig||!CanPersistAudio())return;
    float Voices=1.f,Effects=1.f,Ambience=1.f;
    GConfig->GetFloat(AudioSection,TEXT("Voices"),Voices,GGameUserSettingsIni);
    GConfig->GetFloat(AudioSection,TEXT("Effects"),Effects,GGameUserSettingsIni);
    GConfig->GetFloat(AudioSection,TEXT("Ambience"),Ambience,GGameUserSettingsIni);
    SetVolumes(Voices,Effects,Ambience);
}
void AClaraAudio::SavePreferences() const
{
    if(!GConfig||!CanPersistAudio())return;
    GConfig->SetFloat(AudioSection,TEXT("Voices"),VoiceVolume,GGameUserSettingsIni);
    GConfig->SetFloat(AudioSection,TEXT("Effects"),EffectsVolume,GGameUserSettingsIni);
    GConfig->SetFloat(AudioSection,TEXT("Ambience"),AmbienceVolume,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}
void AClaraAudio::Tick(float Delta)
{
    Super::Tick(Delta);if(!bEnabled)return;
    const float Now=float(GetWorld()->GetTimeSeconds());
    for(int32 I=OneShots.Num()-1;I>=0;--I)if(Now>OneShots[I].EndTime||!OneShots[I].Component.IsValid())
    {
        if(UAudioComponent* Component=OneShots[I].Component.Get()){Component->Stop();Component->DestroyComponent();}
        OneShots.RemoveAtSwap(I);
    }
    if(Voice&&Now>VoiceEnd)StopVoice();
    const APlayerController* PC=GetWorld()->GetFirstPlayerController();const APawn* Pawn=PC?PC->GetPawn():nullptr;if(!Pawn)return;
    // Ambienti legati alla piantina: ventole in sala apparati, tastiere nella stanza cyber, aria ovunque.
    const FVector P=Pawn->GetActorLocation();
    const float Server=OutsideRoom(P,1350,760,1900,1440),Cyber=OutsideRoom(P,560,0,1900,540);
    const float Duck=bMuted?0.f:(Voice?.55f:1.f);
    const float Targets[]={Server<=0?.20f:.30f,Server<=0?.50f:.02f+.16f*FMath::Exp(-Server/420.f),Cyber<=0?.30f:.10f*FMath::Exp(-Cyber/350.f)};
    for(int32 I=0;I<Beds.Num();++I)if(Beds[I])
    {BedVolume[I]=FMath::FInterpTo(BedVolume[I],Targets[I]*Duck*AmbienceVolume,Delta,1.6f);Beds[I]->SetVolumeMultiplier(BedVolume[I]);}
    // Passi: un colpo morbido a ogni falcata, piu' rado camminando e piu' fitto a passo rapido.
    const ACharacter* Walker=Cast<ACharacter>(Pawn);const float Speed=Walker?float(Walker->GetVelocity().Size2D()):0.f;
    if(Walker&&Speed>70.f&&Walker->GetCharacterMovement()->IsMovingOnGround())
    {
        StepTravel+=Speed*Delta;
        if(StepTravel>=(Speed>400.f?92.f:74.f))
        {StepTravel=0;StepIndex=(StepIndex+1+FMath::RandRange(0,1))%4;PlayCue(FString::Printf(TEXT("step%d"),StepIndex+1),.30f,FMath::FRandRange(.93f,1.07f));}
    }
    else StepTravel=FMath::Min(StepTravel,40.f);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraAudioVolumeTest,"Clara.Audio.VolumeValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraAudioVolumeTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Negative volume is silent"),SafeVolume(-1.f),0.f);
    TestEqual(TEXT("Excess volume is limited"),SafeVolume(2.f),1.f);
    TestEqual(TEXT("Normal volume is preserved"),SafeVolume(.35f),.35f);
    TestEqual(TEXT("Invalid volume uses the default"),SafeVolume(std::numeric_limits<float>::quiet_NaN()),1.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraAudioPcmTest,"Clara.Audio.PcmValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraAudioPcmTest::RunTest(const FString& Parameters)
{
    const uint8 Header[]={
        'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,0x40,0x1f,0,0,0x80,0x3e,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,0,0,1,0};
    TArray<uint8> Bytes;Bytes.Append(Header,UE_ARRAY_COUNT(Header));FWaveModInfo Info;
    TestTrue(TEXT("PCM16 mono is accepted"),ReadPcmWave(Bytes,Info));
    Bytes[32]=1;
    TestFalse(TEXT("Misaligned PCM frames are rejected"),ReadPcmWave(Bytes,Info));
    Bytes[32]=2;Bytes[22]=2;
    TestFalse(TEXT("Stereo must contain complete interleaved frames"),ReadPcmWave(Bytes,Info));
    Bytes.Empty();
    TestFalse(TEXT("An incomplete file is rejected without dereferencing its header"),ReadPcmWave(Bytes,Info));
    return true;
}
#endif
