#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraAudio.generated.h"
class UAudioComponent;

// Audio della scena: ambienti in anello, effetti e voci dei personaggi.
// I file sono WAV PCM a 16 bit letti a runtime da Content/Audio, senza asset Unreal:
//   Ambience/*.wav e Sfx/*.wav       prodotti da Scripts/audio/make_sfx.py
//   Voices/<Personaggio>/<chiave>.wav prodotti da Scripts/voices/generate_voices.py
// La chiave di una battuta e' VoiceKey(testo). Se il file manca la battuta resta soltanto scritta.
UCLASS()
class CLARADAYONE_API AClaraAudio : public AActor
{
    GENERATED_BODY()
public:
    AClaraAudio();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    static AClaraAudio* Get(const UObject* Context);
    static void Cue(const UObject* Context,const TCHAR* Name,float Volume=1.f,float Pitch=1.f);
    static FString VoiceKey(const FString& Line);
    bool Speak(const FString& Speaker,const FString& Line);
    void StopVoice();
    bool IsSpeaking() const {return Voice!=nullptr;}
    void PlayCue(const FString& Name,float Volume=1.f,float Pitch=1.f);
    void SetMuted(bool bMute);
    bool IsMuted() const {return bMuted;}
    bool IsEnabled() const {return bEnabled;}
    float GetVoiceVolume() const {return VoiceVolume;}
    float GetEffectsVolume() const {return EffectsVolume;}
    float GetAmbienceVolume() const {return AmbienceVolume;}
    void SetVolumes(float Voices,float Effects,float Ambience);
    void SavePreferences() const;
private:
    float VoiceVolume=1.f,EffectsVolume=1.f,AmbienceVolume=1.f;
    void LoadPreferences();
public:
private:
    struct FClip {TArray<uint8> Pcm;int32 Rate=0,Channels=0;float Seconds=0;};
    struct FOneShot {TWeakObjectPtr<UAudioComponent> Component;float EndTime=0;float Volume=1.f;};
    TMap<FString,TSharedPtr<FClip>> Clips;
    TArray<FOneShot> OneShots;
    TArray<float> BedVolume;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Beds;
    UPROPERTY() TObjectPtr<UAudioComponent> Voice;
    float VoiceEnd=0,StepTravel=0;
    int32 StepIndex=0;
    bool bEnabled=false,bMuted=false;
    TSharedPtr<FClip> Load(const FString& Relative,bool bCache=true);
    UAudioComponent* Start(const TSharedPtr<FClip>& Clip,float Volume,float Pitch,bool bLoop);
};
