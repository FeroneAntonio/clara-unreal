#include "ClaraDevTour.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "ClaraCyberSubsystem.h"
#include "Engine/GameInstance.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "ClaraNarrativeCamera.h"
#include "ClaraAudio.h"
#include "ClaraNetworkDevice.h"
#include "ClaraRoomVisitor.h"
#include "ClaraStoryCast.h"
#include "ClaraStoryDirector.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
#if !UE_BUILD_SHIPPING
bool HasLabel(const TSharedRef<SWidget>& Widget,const FString& Label,bool Partial=false)
{
    if(!Widget->GetVisibility().IsVisible())return false;
    if(Widget->GetType()==FName(TEXT("STextBlock"))&&(Partial?StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString().Contains(Label):StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString()==Label))return true;
    FChildren* Children=Widget->GetChildren();
    for(int32 I=0;I<Children->Num();++I)if(HasLabel(Children->GetChildAt(I),Label,Partial))return true;
    return false;
}
void FindTagged(const TSharedRef<SWidget>& Widget,const FName Tag,TArray<TSharedRef<SWidget>>& Matches)
{
    if(!Widget->GetVisibility().IsVisible())return;
    if(Widget->GetTag()==Tag){Matches.Add(Widget);return;}
    FChildren* Children=Widget->GetChildren();
    for(int32 I=0;I<Children->Num();++I)FindTagged(Children->GetChildAt(I),Tag,Matches);
}
void FindControls(const TSharedRef<SWidget>& Widget,const FString& Label,bool Input,TArray<TSharedRef<SWidget>>& Matches)
{
    if(!Widget->GetVisibility().IsVisible()||!Widget->IsEnabled())return;
    if(Input&&Widget->GetType()==FName(TEXT("SEditableTextBox")))
    {
        if((Label.StartsWith(TEXT("@"))&&Widget->GetTag()==FName(*Label.Mid(1)))||StaticCastSharedRef<SEditableTextBox>(Widget)->GetHintText().ToString()==Label)Matches.Add(Widget);
    }
    else if(!Input&&Widget->GetType()==FName(TEXT("SButton"))&&HasLabel(Widget,Label))
    {Matches.Add(Widget);return;}
    FChildren* Children=Widget->GetChildren();
    for(int32 I=0;I<Children->Num();++I)FindControls(Children->GetChildAt(I),Label,Input,Matches);
}
#endif
struct FGaitSample
{
    double WorldTime=0;float AnimTime=0,AnimLength=0;
    FVector ActorLocation,RootFeet[2],BodyFeet[2],RootWorldFeet[2],BodyWorldFeet[2];
    FString Animation,RootMesh,BodyMesh,BodyLeader;bool TickEnabled=false,Playing=false,Visible=false;
};
struct FGaitProbe
{
    FString Character,Mode,Error;
    TArray<FGaitSample> Samples;
};
struct FTourStep {float Wait=0;FString Verb,Rest;TArray<FString> Args;};
struct FTour
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AClaraStoryDirector> Story;
    FGaitProbe Gait;
    TArray<FTourStep> Steps;int32 Index=0, Checks=0;float Clock=0;FString ShotDir;bool Failed=false;
    FTransform WalkPose;FRotator WalkLook;FVector ProbeStart;float WalkHalfHeight=88,WalkFOV=85,ProbeTime=0;
    uint8 WalkMode=0;bool Marked=false,WalkCollision=true,WalkMoveIgnored=false,WalkLookIgnored=false,OriginalReducedMotion=false,ChangedMotion=false,ProbeReleasedLock=false;double ShotRequestedAt=0;double AwaitStartedAt=0,CyberWaitStartedAt=0;
};
void RestoreTourSettings(FTour& Tour)
{
    if(auto* World=Tour.World.Get())if(auto* PC=World->GetFirstPlayerController())
    {
        if(Tour.ProbeReleasedLock){PC->SetIgnoreMoveInput(true);Tour.ProbeReleasedLock=false;}
        if(Tour.ChangedMotion){auto* Camera=UClaraNarrativeCamera::Get(PC);if(Camera->HasReducedMotion()!=Tour.OriginalReducedMotion)Camera->ToggleReducedMotion();Tour.ChangedMotion=false;}
    }
}
bool SampleGait(UWorld* World,FGaitProbe& Probe)
{
    AClaraStoryNPC* NPC=nullptr;
    for(TActorIterator<AClaraStoryNPC> It(World);It;++It)if(It->GetCharacterId()==Probe.Character){NPC=*It;break;}
    if(!NPC){Probe.Error=TEXT("NPC not found");return false;}
    TArray<AActor*> Attached;NPC->GetAttachedActors(Attached,true,true);
    USkeletalMeshComponent* Root=nullptr;USkeletalMeshComponent* Body=nullptr;
    for(AActor* Child:Attached)
    {
        TInlineComponentArray<USkeletalMeshComponent*> Parts(Child);
        for(auto* Part:Parts)
        {
            const auto* Mesh=Part->GetSkeletalMeshAsset();
            if(Mesh&&Mesh->GetName()==TEXT("m_tal_nrw_base"))Root=Part;
            if(Part->GetFName()==TEXT("Body"))Body=Part;
        }
    }
    if(!Root||!Body||!Body->GetSkeletalMeshAsset())
    {Probe.Error=TEXT("Native m_tal_nrw_base root or visible Body component missing");return false;}
    auto* Anim=Root->GetSingleNodeInstance();
    if(!Anim||!Anim->GetCurrentAsset()){Probe.Error=TEXT("Native root has no SingleNode animation asset");return false;}
    FGaitSample S;S.WorldTime=World->GetTimeSeconds();S.AnimTime=Anim->GetCurrentTime();S.AnimLength=Anim->GetLength();
    S.ActorLocation=NPC->GetActorLocation();S.Animation=Anim->GetCurrentAsset()->GetName();
    S.RootMesh=Root->GetSkeletalMeshAsset()->GetPathName();S.BodyMesh=Body->GetSkeletalMeshAsset()->GetPathName();
    S.BodyLeader=GetNameSafe(Body->LeaderPoseComponent.Get());S.TickEnabled=Root->IsComponentTickEnabled();
    S.Playing=Anim->IsPlaying();S.Visible=Body->IsVisible()&&!Body->bHiddenInGame&&!Body->GetOwner()->IsHidden()&&!NPC->IsHidden();
    for(int32 I=0;I<2;++I)
    {
        const FName Bone(I==0?TEXT("foot_l"):TEXT("foot_r"));
        if(Root->GetBoneIndex(Bone)==INDEX_NONE||Body->GetBoneIndex(Bone)==INDEX_NONE)
        {Probe.Error=TEXT("foot_l/foot_r missing on native root or Body");return false;}
        S.RootWorldFeet[I]=Root->GetSocketTransform(Bone,RTS_World).GetLocation();
        S.BodyWorldFeet[I]=Body->GetSocketTransform(Bone,RTS_World).GetLocation();
        // Remove each component's translation, rotation and scale: sliding cannot pass as a moving leg.
        S.RootFeet[I]=Root->GetComponentTransform().InverseTransformPosition(S.RootWorldFeet[I]);
        S.BodyFeet[I]=Body->GetComponentTransform().InverseTransformPosition(S.BodyWorldFeet[I]);
    }
    Probe.Samples.Add(S);return true;
}
bool WriteGaitResult(const FGaitProbe& Probe,const FString& Directory)
{
    const bool Walk=Probe.Mode==TEXT("walk");
    bool Good=Probe.Error.IsEmpty()&&Probe.Samples.Num()>=3;
    double Elapsed=0,Advance=0,Travel=0;double RootMotion[2]={0,0},BodyMotion[2]={0,0};
    const auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Samples;
    const auto Vector=[](const FVector& P){return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};};
    for(int32 I=0;I<Probe.Samples.Num();++I)
    {
        const auto& S=Probe.Samples[I];const auto& First=Probe.Samples[0];
        Good&=S.Animation==(Walk?TEXT("MTN_N_Walk_InPlace"):TEXT("MTN_N_Idle"))&&S.TickEnabled&&S.Playing&&S.Visible;
        Elapsed=S.WorldTime-First.WorldTime;
        if(I>0)
        {
            const auto& Previous=Probe.Samples[I-1];double Delta=S.AnimTime-Previous.AnimTime;
            if(Delta<0&&S.Animation==Previous.Animation)Delta+=S.AnimLength;
            Advance+=FMath::Max(0.,Delta);Travel+=FVector::Dist(S.ActorLocation,Previous.ActorLocation);
        }
        auto Item=MakeShared<FJsonObject>();Item->SetNumberField(TEXT("world_seconds"),S.WorldTime);Item->SetNumberField(TEXT("animation_seconds"),S.AnimTime);
        Item->SetNumberField(TEXT("animation_length"),S.AnimLength);Item->SetStringField(TEXT("animation"),S.Animation);
        Item->SetStringField(TEXT("root_mesh"),S.RootMesh);Item->SetStringField(TEXT("body_mesh"),S.BodyMesh);Item->SetStringField(TEXT("body_leader"),S.BodyLeader);
        Item->SetBoolField(TEXT("root_tick_enabled"),S.TickEnabled);Item->SetBoolField(TEXT("playing"),S.Playing);Item->SetBoolField(TEXT("body_visible"),S.Visible);
        Item->SetArrayField(TEXT("actor_world_cm"),Vector(S.ActorLocation));
        for(int32 Foot=0;Foot<2;++Foot)
        {
            RootMotion[Foot]=FMath::Max(RootMotion[Foot],FVector::Dist(S.RootFeet[Foot],First.RootFeet[Foot]));
            BodyMotion[Foot]=FMath::Max(BodyMotion[Foot],FVector::Dist(S.BodyFeet[Foot],First.BodyFeet[Foot]));
            const FString Side=Foot==0?TEXT("left"):TEXT("right");
            Item->SetArrayField(TEXT("root_")+Side+TEXT("_local_cm"),Vector(S.RootFeet[Foot]));
            Item->SetArrayField(TEXT("body_")+Side+TEXT("_local_cm"),Vector(S.BodyFeet[Foot]));
            Item->SetArrayField(TEXT("root_")+Side+TEXT("_world_cm"),Vector(S.RootWorldFeet[Foot]));
            Item->SetArrayField(TEXT("body_")+Side+TEXT("_world_cm"),Vector(S.BodyWorldFeet[Foot]));
        }
        Samples.Add(MakeShared<FJsonValueObject>(Item));
    }
    Good&=Elapsed>=.3&&Advance>=.1;
    Good&=Walk?(Travel>2&&RootMotion[0]>1&&RootMotion[1]>1&&BodyMotion[0]>1&&BodyMotion[1]>1):Travel<2;
    Report->SetStringField(TEXT("character"),Probe.Character);Report->SetStringField(TEXT("mode"),Probe.Mode);Report->SetBoolField(TEXT("passed"),Good);
    Report->SetStringField(TEXT("error"),Probe.Error);Report->SetNumberField(TEXT("elapsed_seconds"),Elapsed);Report->SetNumberField(TEXT("animation_advance_seconds"),Advance);
    Report->SetNumberField(TEXT("actor_travel_cm"),Travel);
    Report->SetArrayField(TEXT("root_local_foot_motion_cm"),{MakeShared<FJsonValueNumber>(RootMotion[0]),MakeShared<FJsonValueNumber>(RootMotion[1])});
    Report->SetArrayField(TEXT("body_local_foot_motion_cm"),{MakeShared<FJsonValueNumber>(BodyMotion[0]),MakeShared<FJsonValueNumber>(BodyMotion[1])});
    Report->SetArrayField(TEXT("samples"),Samples);
    FString Data;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Data));
    const FString File=Directory/(TEXT("npc-gait-")+FPaths::MakeValidFileName(Probe.Character)+TEXT("-")+Probe.Mode+TEXT(".json"));
    return FFileHelper::SaveStringToFile(Data,*File)&&Good;
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraNativeGaitEvidenceTest,"Clara.Cast.NativeGaitEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraNativeGaitEvidenceTest::RunTest(const FString& Parameters)
{
    const FString Directory=FPaths::ProjectIntermediateDir()/TEXT("NativeGaitEvidenceTest")/FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*Directory,true);
    FGaitProbe Probe;Probe.Character=TEXT("SyntheticRegression");Probe.Mode=TEXT("walk");
    for(int32 I=0;I<3;++I)
    {
        FGaitSample S;S.WorldTime=I*.4;S.AnimTime=I*.4;S.AnimLength=2;S.Animation=TEXT("MTN_N_Walk_InPlace");
        S.TickEnabled=S.Playing=S.Visible=true;S.ActorLocation=FVector(I*30,0,0);
        for(int32 Foot=0;Foot<2;++Foot){S.RootFeet[Foot]=S.BodyFeet[Foot]=FVector(0,Foot*10,0);S.RootWorldFeet[Foot]=S.BodyWorldFeet[Foot]=S.ActorLocation+S.RootFeet[Foot];}
        Probe.Samples.Add(S);
    }
    TestFalse(TEXT("Root translation and an advancing clock cannot prove a moving leg"),WriteGaitResult(Probe,Directory));
    for(int32 I=0;I<3;++I)for(int32 Foot=0;Foot<2;++Foot){auto& S=Probe.Samples[I];S.RootFeet[Foot].X=I*4;S.RootWorldFeet[Foot]=S.ActorLocation+S.RootFeet[Foot];}
    TestFalse(TEXT("Animating only the invisible native root cannot prove visible walking"),WriteGaitResult(Probe,Directory));
    for(int32 I=0;I<3;++I)for(int32 Foot=0;Foot<2;++Foot){auto& S=Probe.Samples[I];S.BodyFeet[Foot].X=I*4;S.BodyWorldFeet[Foot]=S.ActorLocation+S.BodyFeet[Foot];}
    TestTrue(TEXT("Native clock, movement and both visible feet prove walking"),WriteGaitResult(Probe,Directory));
    for(auto& S:Probe.Samples)S.AnimTime=0;
    TestFalse(TEXT("A frozen animation clock fails even with moving world feet"),WriteGaitResult(Probe,Directory));
    IFileManager::Get().DeleteDirectory(*Directory,false,true);return true;
}
#endif
void RunStep(FTour& Tour,const FTourStep& Step)
{
    UWorld* World=Tour.World.Get();AClaraStoryDirector* Story=Tour.Story.Get();if(!World||!Story)return;
    AClaraRoomController* PC=Cast<AClaraRoomController>(World->GetFirstPlayerController());APawn* Pawn=PC?PC->GetPawn():nullptr;
    const auto Arg=[&Step](int32 I){return Step.Args.IsValidIndex(I)?Step.Args[I]:FString();};
    const auto Num=[&Arg](int32 I){return FCString::Atof(*Arg(I));};
    const FString& V=Step.Verb;
    bool SecretInput=Story->IsTerminalPasswordInput()&&(V==TEXT("cmd")||V==TEXT("type"));
    if(V==TEXT("expect"))
    {
        bool Good=false;
        if(Arg(0)==TEXT("phase"))Good=Story->GetPhase()==FCString::Atoi(*Arg(1));
        else if(Arg(0)==TEXT("page"))Good=Story->GetPage()==(Arg(1)==TEXT("none")?FString():Arg(1));
        else if(Arg(0)==TEXT("screen"))Good=Story->IsScreenOpen()==(Arg(1)==TEXT("open"));
        else if(Arg(0)==TEXT("portpeer"))
        {
            if(const auto* Device=Story->GetDevice(Arg(1)))
            {
                FString PeerPort;const auto* Peer=Device->GetPeerForPort(Arg(2),PeerPort);
                Good=!Device->GetEffectivePortKind(Arg(2)).IsEmpty()&&(Arg(3)==TEXT("none")?!Peer:(Peer&&Peer==Story->GetDevice(Arg(3))&&PeerPort==Arg(4)));
            }
        }
#if !UE_BUILD_SHIPPING
        else if(Arg(0)==TEXT("uitext"))
        {
            const auto Window=World->GetGameViewport()?World->GetGameViewport()->GetWindow():nullptr;
            Good=Window.IsValid()&&HasLabel(Window.ToSharedRef(),Step.Rest.Mid(7),true);
        }
        else if(Arg(0)==TEXT("field"))
        {
            FString Label,Value;TArray<TSharedRef<SWidget>> Matches;
            const auto Window=World->GetGameViewport()?World->GetGameViewport()->GetWindow():nullptr;
            if(Window.IsValid()&&Step.Rest.Mid(6).Split(TEXT(" | "),&Label,&Value))
            {
                FindControls(Window.ToSharedRef(),Label,true,Matches);
                Good=Matches.Num()==1&&StaticCastSharedRef<SEditableTextBox>(Matches[0])->GetText().ToString()==(Value==TEXT("<empty>")?FString():Value);
            }
        }
#endif
        else if(Arg(0)==TEXT("cyber"))
        {
            const auto* Lab=World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UClaraCyberSubsystem>():nullptr;
            if(Lab){const auto& State=Lab->GetState();const FString Check=Arg(1);const int32 Index=FCString::Atoi(*Arg(2));
                if(Check==TEXT("day"))Good=State.day==Index;
                else if(Check==TEXT("busy"))Good=Lab->IsTerminalBusy()==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("scene"))Good=State.dialogueScene==Index;
                else if(Check==TEXT("offset"))Good=State.dialogueOffset==Index;
                else if(Check==TEXT("red"))Good=State.red==Index;
                else if(Check==TEXT("blue"))Good=State.blue==Index;
                else if(Check==TEXT("path"))Good=State.day<=3&&FString::Chr(TCHAR(State.path[State.day-1]))==Arg(2);
                else if(Check==TEXT("finished"))Good=State.day<=3&&State.finished[State.day-1]==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("observed"))Good=Index>=0&&Index<3&&State.observed[Index]==FCString::Atoi(*Arg(3));
                else if(Check==TEXT("verified"))Good=Index>=0&&Index<3&&State.verified[Index]==(Arg(3)==TEXT("yes"));
                else if(Check==TEXT("transcript"))Good=Lab->TerminalTranscript().Contains(Step.Rest.Mid(17));
                else if(Check==TEXT("feedback"))Good=Lab->LastFeedback().Contains(Step.Rest.Mid(15));
                else if(Check==TEXT("isolated"))Good=Lab->IsIsolatedSession();
                else if(Check==TEXT("plain"))Good=State.plainSeen==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("tlsseen"))Good=State.tlsSeen==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("traffic"))Good=State.trafficChecked==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("password"))Good=State.passwordChanged==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("admin"))Good=State.adminEnabled==(Arg(2)==TEXT("yes"));
                else if(Check==TEXT("tls"))Good=State.tls==(Arg(2)==TEXT("yes"));
            }
        }
        else if(Arg(0)==TEXT("transition"))Good=Story->IsDayTransitionActive()==(Arg(1)==TEXT("active"));
        else if(Arg(0)==TEXT("clock"))Good=Story->GetOfficeClockTime()==Arg(1);
        else if(Arg(0)==TEXT("ready"))Good=Story->CanRebootChallenge()==(Arg(1)==TEXT("yes"));
        else if(Arg(0)==TEXT("terminal"))Good=Story->GetTerminalTarget()==Arg(1);
        else if(Arg(0)==TEXT("busy"))Good=Story->IsTerminalBusy()==(Arg(1)==TEXT("yes"));
        else if(Arg(0)==TEXT("transcript"))Good=Story->GetTranscript().Contains(Step.Rest.Mid(Arg(0).Len()+1));
        else if(Arg(0)==TEXT("simmessage"))Good=Story->GetSimMessage().Contains(Step.Rest.Mid(Arg(0).Len()+1));
        else if(Arg(0)==TEXT("camera"))
        {
            const auto* Camera=PC?PC->FindComponentByClass<UClaraNarrativeCamera>():nullptr;
            Good=Camera&&(Arg(1)==TEXT("active")?Camera->IsActive():Arg(1)==TEXT("holding")?Camera->IsHolding():Arg(1)==TEXT("clear")?Camera->IsApproachClear():!Camera->IsActive());
        }
        else if(Arg(0)==TEXT("seated"))
        {
            auto* Character=Cast<ACharacter>(Pawn);const auto* Camera=PC?PC->FindComponentByClass<UClaraNarrativeCamera>():nullptr;
            Good=Camera&&Camera->IsHolding()&&Character&&PC->GetViewTarget()&&PC->GetViewTarget()->GetActorLocation().Equals(FVector(180,1110,121),1.f)&&
                FMath::IsNearlyEqual(Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),55.f,.1f)&&!Character->GetActorEnableCollision();
        }
        else if(Arg(0)==TEXT("walkrestored"))
        {
            auto* Character=Cast<ACharacter>(Pawn);
            Good=Tour.Marked&&Character&&Character->GetActorLocation().Equals(Tour.WalkPose.GetLocation(),2.f)&&PC->GetControlRotation().Equals(Tour.WalkLook,.1f)&&
                FMath::IsNearlyEqual(Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),Tour.WalkHalfHeight,.1f)&&Character->GetActorEnableCollision()==Tour.WalkCollision&&
                Character->GetCharacterMovement()->MovementMode==Tour.WalkMode&&PC->IsMoveInputIgnored()==Tour.WalkMoveIgnored&&PC->IsLookInputIgnored()==Tour.WalkLookIgnored&&
                PC->GetViewTarget()==Pawn&&(!PC->PlayerCameraManager||FMath::IsNearlyEqual(PC->PlayerCameraManager->GetFOVAngle(),Tour.WalkFOV,.1f));
        }
        else if(Arg(0)==TEXT("walked"))Good=Pawn&&FVector::Dist2D(Pawn->GetActorLocation(),Tour.ProbeStart)>2;
        else if(Arg(0)==TEXT("motion"))
        {
            const bool Wanted=Arg(1)==TEXT("reduced");const auto* Camera=PC?PC->FindComponentByClass<UClaraNarrativeCamera>():nullptr;FConfigFile Config;bool Stored=false;Config.Read(GGameUserSettingsIni);
            Good=Camera&&Camera->HasReducedMotion()==Wanted&&Config.GetBool(TEXT("Clara.Camera"),TEXT("ReducedMotion"),Stored)&&Stored==Wanted;
        }
        else if(Arg(0)==TEXT("towerpreview"))Good=Story->GetLayerTower().IsPreview()==(Arg(1)==TEXT("yes"));
        else if(Arg(0)==TEXT("towererrors"))Good=Story->GetLayerTower().Errors==FCString::Atoi(*Arg(1));
        else if(Arg(0)==TEXT("towerhint"))Good=Story->GetLayerTower().OffersHint()==(Arg(1)==TEXT("yes"));
        else if(Arg(0)==TEXT("towercorrect"))Good=Story->GetLayerTower().IsCorrect()==(Arg(1)==TEXT("yes"));
        else if(Arg(0)==TEXT("towerpeg"))
        {
            const int32 Peg=FCString::Atoi(*Arg(1));TArray<FString> Ids;
            if(Peg>=0&&Peg<3){for(int32 Id:Story->GetLayerTower().Pegs[Peg])Ids.Add(FString::FromInt(Id));Good=FString::Join(Ids,TEXT(","))==Arg(2);}
        }
        else if(Arg(0)==TEXT("customLuca"))
        {
            for(TActorIterator<AClaraStoryNPC> It(World);It;++It)
                if(It->GetCharacterId()==TEXT("Luca"))Good=It->HasCustomLucaHead()==(Arg(1)==TEXT("1"));
        }
        else if(Arg(0)==TEXT("cast"))
        {
            int32 Count=0;
            for(TActorIterator<AClaraStoryNPC> It(World);It;++It)if(It->HasMetaHuman())++Count;
            Good=Count==FCString::Atoi(*Arg(1));
        }
        ++Tour.Checks;
        if(!Good)
        {
            Tour.Failed=true;
            UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_ASSERT_FAIL: %s; actual phase=%d page=%s screen=%d"),*Step.Rest,Story->GetPhase(),*Story->GetPage(),Story->IsScreenOpen());
            RestoreTourSettings(Tour);FPlatformMisc::RequestExitWithStatus(false,1);
            return;
        }
        UE_LOG(LogTemp,Display,TEXT("CLARA_TOUR_ASSERT_PASS: %s"),*Step.Rest);
    }
    else if(V==TEXT("gait"))
    {
        auto& Probe=Tour.Gait;
        if(Probe.Samples.IsEmpty()){Probe.Character=Arg(0);Probe.Mode=Arg(1);Probe.Error.Empty();}
        if(Probe.Mode!=TEXT("walk")&&Probe.Mode!=TEXT("idle"))Probe.Error=TEXT("Expected gait <NPC> walk|idle");
        if(Probe.Error.IsEmpty())SampleGait(World,Probe);
        if(Probe.Error.IsEmpty()&&World->GetTimeSeconds()-Probe.Samples[0].WorldTime<.7){--Tour.Index;return;}
        const bool Passed=WriteGaitResult(Probe,Tour.ShotDir);
        Probe=FGaitProbe();++Tour.Checks;
        if(!Passed){Tour.Failed=true;RestoreTourSettings(Tour);UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: native gait; see npc-gait JSON in ShotDir"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
    }
    else if(V==TEXT("awaitcyberidle"))
    {
        const auto* Lab=World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UClaraCyberSubsystem>():nullptr;
        if(!Lab){Tour.Failed=true;FPlatformMisc::RequestExitWithStatus(false,1);return;}
        if(Lab->IsTerminalBusy()){
            if(Tour.CyberWaitStartedAt==0)Tour.CyberWaitStartedAt=FPlatformTime::Seconds();
            if(FPlatformTime::Seconds()-Tour.CyberWaitStartedAt>30){Tour.Failed=true;UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: Cyber terminal remained busy for 30 seconds"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
            --Tour.Index;return;
        }
        Tour.CyberWaitStartedAt=0;++Tour.Checks;
    }
    else if(V==TEXT("reveal"))
    {
#if !UE_BUILD_SHIPPING
        const bool Input=Arg(0)==TEXT("field");const FString Label=Step.Rest.Mid(Arg(0).Len()+1);
        TArray<TSharedRef<SWidget>> Matches;const auto Window=World->GetGameViewport()?World->GetGameViewport()->GetWindow():nullptr;
        if(Window.IsValid()){if(Arg(0)==TEXT("widget"))FindTagged(Window.ToSharedRef(),FName(*Label.Mid(1)),Matches);else FindControls(Window.ToSharedRef(),Label,Input,Matches);}
        if(Matches.Num()!=1){Tour.Failed=true;UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: reveal %s found %d controls"),*Label,Matches.Num());FPlatformMisc::RequestExitWithStatus(false,1);return;}
        for(TSharedPtr<SWidget> Parent=Matches[0]->GetParentWidget();Parent.IsValid();Parent=Parent->GetParentWidget())
            if(Parent->GetType()==FName(TEXT("SScrollBox")))StaticCastSharedPtr<SScrollBox>(Parent)->ScrollDescendantIntoView(Matches[0],false,EDescendantScrollDestination::Center);
#else
        Tour.Failed=true;FPlatformMisc::RequestExitWithStatus(false,1);return;
#endif
    }
    else if(V==TEXT("ui")||V==TEXT("type"))
    {
#if !UE_BUILD_SHIPPING
        FString Label=Step.Rest,Value;
        const bool Input=V==TEXT("type");
        const bool Valid=!Input||Step.Rest.Split(TEXT(" | "),&Label,&Value);
        TArray<TSharedRef<SWidget>> Matches;
        const auto Window=World->GetGameViewport()?World->GetGameViewport()->GetWindow():nullptr;
        if(Valid&&Window.IsValid())FindControls(Window.ToSharedRef(),Label,Input,Matches);
        if(Matches.Num()!=1)
        {
            Tour.Failed=true;
            UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: UI control '%s' has %d matches (expected one enabled control)"),*Label,Matches.Num());
            FPlatformMisc::RequestExitWithStatus(false,1);return;
        }
        // Hold the widget alive while its callback may rebuild the panel.
        if(Input)
        {
            SecretInput=SecretInput||StaticCastSharedRef<SEditableTextBox>(Matches[0])->IsPassword();
            StaticCastSharedRef<SEditableTextBox>(Matches[0])->SetText(FText::FromString(Value));
            FSlateApplication::Get().SetKeyboardFocus(Matches[0]);
        }
        else StaticCastSharedRef<SButton>(Matches[0])->SimulateClick();
#else
        Tour.Failed=true;FPlatformMisc::RequestExitWithStatus(false,1);return;
#endif
    }
    else if(V==TEXT("pos")&&PC&&Pawn){PC->ResumeWalk();Pawn->TeleportTo(FVector(Num(0),Num(1),Num(2)),FRotator(0,Num(4),0),false,true);PC->SetControlRotation(FRotator(Num(3),Num(4),0));}
    else if(V==TEXT("shot")){FScreenshotRequest::RequestScreenshot(Tour.ShotDir/Arg(0)+TEXT(".png"),true,false);Tour.ShotRequestedAt=FPlatformTime::Seconds();}
    else if(V==TEXT("talk")){for(TActorIterator<AClaraStoryNPC> It(World);It;++It)if(It->GetCharacterId()==Arg(0)){Story->HandleInteraction(*It);break;}}
    else if(V==TEXT("prop")){for(TActorIterator<AClaraStoryProp> It(World);It;++It)if(It->GetPropId()==Arg(0)&&!It->IsHidden()){Story->HandleInteraction(*It);break;}}
    else if(V==TEXT("key"))
    {
        const bool Control=Arg(0).StartsWith(TEXT("Ctrl+"));
        const FKeyEvent Event(FKey(*(Control?Arg(0).Mid(5):Arg(0))),FModifierKeysState(false,false,Control,false,false,false,false,false,false),0,false,0,0);
        FSlateApplication::Get().ProcessKeyDownEvent(Event);FSlateApplication::Get().ProcessKeyUpEvent(Event);
    }
    else if(V==TEXT("camera")&&Arg(0)==TEXT("mark")&&PC)
    {
        if(auto* Character=Cast<ACharacter>(Pawn))
        {Tour.WalkPose=Character->GetActorTransform();Tour.WalkLook=PC->GetControlRotation();Tour.WalkHalfHeight=Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
            Tour.WalkMode=Character->GetCharacterMovement()->MovementMode;Tour.WalkCollision=Character->GetActorEnableCollision();Tour.WalkMoveIgnored=PC->IsMoveInputIgnored();Tour.WalkLookIgnored=PC->IsLookInputIgnored();
            Tour.WalkFOV=PC->PlayerCameraManager?PC->PlayerCameraManager->GetFOVAngle():85;Tour.Marked=true;}
    }
    else if(V==TEXT("camera")&&Arg(0)==TEXT("walkprobe")&&PC&&Pawn)
    {Tour.ProbeStart=Pawn->GetActorLocation();Tour.ProbeTime=.3f;PC->SetIgnoreMoveInput(false);Tour.ProbeReleasedLock=true;}
    else if(V==TEXT("motion")&&PC)
    {auto* Camera=UClaraNarrativeCamera::Get(PC);if(!Tour.ChangedMotion){Tour.OriginalReducedMotion=Camera->HasReducedMotion();Tour.ChangedMotion=true;}if(Camera->HasReducedMotion()!=(Arg(0)==TEXT("reduced")))Camera->ToggleReducedMotion();}
    else if(V==TEXT("checkpoint"))
    {if(!Story->DevRoundTripCheckpoint(Arg(0))){Tour.Failed=true;RestoreTourSettings(Tour);UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_CHECKPOINT_FAIL"));FPlatformMisc::RequestExitWithStatus(false,1);return;}++Tour.Checks;}
    else if(V==TEXT("choose"))Story->Choose(FCString::Atoi(*Arg(0)));
    else if(V==TEXT("day3test"))Story->RunDayThreeTest();
    else if(V==TEXT("day3reboot"))Story->RebootDayThree();
    else if(V==TEXT("next"))Story->ContinueDialogue();
    else if(V==TEXT("skip"))Story->FinishDialogue();
    else if(V==TEXT("close"))Story->ClosePage();
    else if(V==TEXT("notebook"))Story->OpenNotebook();
    else if(V==TEXT("inventory"))Story->OpenInventory();
    else if(V==TEXT("hint"))Story->RequestHint();
    else if(V==TEXT("pickup"))Story->PickupLaptop();
    else if(V==TEXT("laptop"))Story->OpenLaptopScreen();
    else if(V==TEXT("closelaptop"))Story->CloseLaptopScreen();
    else if(V==TEXT("screen"))Story->SetScreenPage(Arg(0));
    else if(V==TEXT("target"))Story->SelectTerminalTarget(Arg(0));
    else if(V==TEXT("cmd"))Story->RunCommand(Step.Rest);
    else if(V==TEXT("await")&&Arg(0)==TEXT("idle")){}
    else if(V==TEXT("interrupt"))Story->CancelTerminalCommand();
    else if(V==TEXT("answer"))Story->SubmitAnswer(Step.Rest);
    else if(V==TEXT("towerpeg"))Story->SelectTowerPeg(FCString::Atoi(*Arg(0)));
    else if(V==TEXT("towercheck"))Story->CheckPuzzle();
    else if(V==TEXT("towerpreview"))Story->PreviewLayerTower();
    else if(V==TEXT("towermnemonic"))Story->ShowTowerMnemonic();
    else if(V==TEXT("solve")){const bool Tower=Story->IsTowerPuzzle();Story->DevSolvePuzzle();Story->CheckPuzzle();if(Tower)Story->FinishDialogue();}
    else if(V==TEXT("nextday"))Story->BeginNextDayTransition();
    else if(V==TEXT("skipday"))Story->FinishNextDayTransition();
    else if(V==TEXT("sim")&&Arg(0)==TEXT("cable"))Story->SetSimCableKind(Arg(1)==TEXT("none")?FString():Arg(1));
    else if(V==TEXT("sim")&&Arg(0)==TEXT("select"))Story->SelectSimNode(Arg(1));
    else if(V==TEXT("sim")&&Arg(0)==TEXT("ip"))Story->SetSimHostIP(Arg(1),Arg(2));
    else if(V==TEXT("sim")&&Arg(0)==TEXT("test"))Story->RunChallengeTest();
    else if(V==TEXT("sim")&&Arg(0)==TEXT("reboot"))Story->RebootChallenge();
    else if(V==TEXT("take"))Story->SelectSortingCable(Arg(0));
    else if(V==TEXT("box"))Story->SortCable(Arg(0));
    else if(V==TEXT("use"))Story->UseItem(Arg(0));
    else if(V==TEXT("device")&&PC)PC->OpenDevice(Story->GetDevice(Arg(0)));
    else if(V==TEXT("cable")&&PC)PC->SetCableKind(Arg(0));
    else if(V==TEXT("port")&&PC)PC->SelectDevicePort(Arg(0));
    else if((V==TEXT("probeport")||V==TEXT("pickport")||V==TEXT("dragdevice"))&&PC)
    {
        const bool Passed=V==TEXT("dragdevice")?PC->DevCheckDeviceDrag():PC->DevProbeDevicePort(Arg(0),V==TEXT("pickport"));
        if(!Passed){Tour.Failed=true;RestoreTourSettings(Tour);UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: device input %s %s"),*V,*Step.Rest);FPlatformMisc::RequestExitWithStatus(false,1);return;}
        ++Tour.Checks;
    }
    else if(V==TEXT("view")&&PC)PC->SetDeviceView(Num(0),Num(1),Num(2));
    else if(V==TEXT("closedevice")&&PC)PC->CloseDevice();
    else if(V==TEXT("mute")){if(AClaraAudio* Audio=AClaraAudio::Get(World))Audio->SetMuted(Arg(0)!=TEXT("off"));}
    else if(V==TEXT("quit"))
    {
        UE_LOG(LogTemp,Display,TEXT("CLARA_TOUR_PASS: %d assertions; final phase=%d"),Tour.Checks,Story->GetPhase());
        RestoreTourSettings(Tour);FPlatformMisc::RequestExitWithStatus(false,0);
    }
    else
    {
        Tour.Failed=true;
        UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: unsupported or unavailable action %s"),*Step.Verb);
        RestoreTourSettings(Tour);FPlatformMisc::RequestExitWithStatus(false,1);
    }
    UE_LOG(LogTemp,Display,TEXT("CLARA_TOUR: %s %s -> fase %d, pagina '%s', schermo %s, obiettivo: %s"),*V,SecretInput?TEXT("<redacted>"):*Step.Rest,Story->GetPhase(),*Story->GetPage(),Story->IsScreenOpen()?TEXT("aperto"):TEXT("chiuso"),*Story->GetObjective());
}
}

void StartClaraDevTour(UWorld* World,AClaraStoryDirector* Story,const FString& File)
{
    TArray<FString> Lines;
    if(!FFileHelper::LoadFileToStringArray(Lines,*File)){UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR: file non leggibile %s"),*File);FPlatformMisc::RequestExitWithStatus(false,1);return;}
    const TSharedRef<FTour> Tour=MakeShared<FTour>();Tour->World=World;Tour->Story=Story;
    if(!FParse::Value(FCommandLine::Get(),TEXT("ClaraShotDir="),Tour->ShotDir))Tour->ShotDir=FPaths::ProjectSavedDir()/TEXT("Tour");
    for(FString Line:Lines)
    {
        Line.TrimStartAndEndInline();if(Line.IsEmpty()||Line.StartsWith(TEXT("#")))continue;
        TArray<FString> Parts;Line.ParseIntoArrayWS(Parts);if(Parts.Num()<2)continue;
        FTourStep Step;Step.Wait=FCString::Atof(*Parts[0]);Step.Verb=Parts[1].ToLower();
        for(int32 I=2;I<Parts.Num();++I)Step.Args.Add(Parts[I]);
        Step.Rest=FString::Join(Step.Args,TEXT(" "));Tour->Steps.Add(MoveTemp(Step));
    }
    if(Tour->Steps.IsEmpty()||Tour->Steps.Last().Verb!=TEXT("quit"))
    {
        UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: tour must contain steps and end with quit"));
        FPlatformMisc::RequestExitWithStatus(false,1);return;
    }
    IFileManager::Get().MakeDirectory(*Tour->ShotDir,true);
    // Real keyboard/mouse input must not move the camera during scripted captures.
    if (APlayerController* Controller = World->GetFirstPlayerController())
    {
        Controller->SetIgnoreMoveInput(true);
        Controller->SetIgnoreLookInput(true);
        if (APawn* Pawn = Controller->GetPawn()) Pawn->DisableInput(Controller);
        Controller->DisableInput(Controller);
    }
    UE_LOG(LogTemp,Display,TEXT("CLARA_TOUR: %d passi, schermate in %s"),Tour->Steps.Num(),*Tour->ShotDir);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Tour](float Delta)
    {
        if(Tour->Failed||!Tour->World.IsValid()||!Tour->Story.IsValid()||!Tour->Steps.IsValidIndex(Tour->Index))return false;
        // Keep the same UI state until the frame requested for the PNG has been captured.
        if(Tour->ShotRequestedAt>0&&FScreenshotRequest::IsScreenshotRequested())
        {
            if(FPlatformTime::Seconds()-Tour->ShotRequestedAt>30)
            {
                Tour->Failed=true;UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: screenshot was not processed within 30 seconds"));
                FPlatformMisc::RequestExitWithStatus(false,1);return false;
            }
            return true;
        }
        Tour->ShotRequestedAt=0;
        if(Tour->ProbeTime>0)if(auto* PC=Tour->World->GetFirstPlayerController())
        {if(APawn* Pawn=PC->GetPawn())Pawn->AddMovementInput(FVector(-1,0,0),1);Tour->ProbeTime-=Delta;if(Tour->ProbeTime<=0&&Tour->ProbeReleasedLock){PC->SetIgnoreMoveInput(true);Tour->ProbeReleasedLock=false;}}
        Tour->Clock+=FMath::Min(Delta,.1f);
        const bool SamplingGait=Tour->Steps[Tour->Index].Verb==TEXT("gait")&&!Tour->Gait.Samples.IsEmpty();
        if(!SamplingGait&&Tour->Clock<Tour->Steps[Tour->Index].Wait)return true;
        const auto& PendingStep=Tour->Steps[Tour->Index];
        if(PendingStep.Verb==TEXT("await")&&PendingStep.Rest==TEXT("idle")&&Tour->Story->IsTerminalBusy())
        {
            const double Now=FPlatformTime::Seconds();if(Tour->AwaitStartedAt==0)Tour->AwaitStartedAt=Now;
            if(Now-Tour->AwaitStartedAt>45)
            {Tour->Failed=true;RestoreTourSettings(*Tour);UE_LOG(LogTemp,Error,TEXT("CLARA_TOUR_FAIL: terminal still busy after 45 seconds"));FPlatformMisc::RequestExitWithStatus(false,1);return false;}
            return true;
        }
        Tour->AwaitStartedAt=0;
        Tour->Clock=0;RunStep(*Tour,Tour->Steps[Tour->Index++]);
        return true;
    }));
}
