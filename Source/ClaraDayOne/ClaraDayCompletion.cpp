#include "ClaraNarrativeCamera.h"
#include "ClaraStoryDirector.h"
#include "ClaraAudio.h"
#include "ClaraRoomVisitor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
constexpr float TransitionSeconds=13.f;
const FVector ClockPosition(550,420,245);
}

void AClaraStoryDirector::CreateOfficeClock()
{
    if(OfficeClock||!GetWorld())return;
    OfficeClock=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(OfficeClock,TEXT("ClockRoot"));
    OfficeClock->AddInstanceComponent(Root);OfficeClock->SetRootComponent(Root);Root->RegisterComponent();
    // The reception side wall is clear here; no floor area or doorway changes.
    OfficeClock->SetActorLocationAndRotation(ClockPosition,FRotator(0,180,0));
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Office.M_Office"));
    const FLinearColor Dark(.018f,.035f,.037f),Ivory(.88f,.86f,.77f),Accent(.06f,.34f,.30f);
    auto Part=[&](const TCHAR* Name,UStaticMesh* Mesh,FVector At,FVector Size,FLinearColor Color,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* C=NewObject<UStaticMeshComponent>(OfficeClock,FName(Name));OfficeClock->AddInstanceComponent(C);
        C->SetMobility(EComponentMobility::Movable);C->SetupAttachment(Root);C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);
        if(Base){auto* M=UMaterialInstanceDynamic::Create(Base,OfficeClock);M->SetVectorParameterValue(TEXT("Color"),Color);M->SetVectorParameterValue(TEXT("BaseColor"),Color);C->SetMaterial(0,M);}
        C->RegisterComponent();C->SetRelativeLocation(At);C->SetRelativeRotation(Rotation);C->SetRelativeScale3D(Size/100.f);return C;
    };
    Part(TEXT("ClockCase"),Cylinder,{0,0,0},{61,61,4},Dark,{90,0,0});
    Part(TEXT("ClockFace"),Cylinder,{2.2,0,0},{56,56,.6},Ivory,{90,0,0});
    for(int32 I=0;I<60;++I)
    {
        const float Angle=I*6.f,Radians=FMath::DegreesToRadians(Angle),Length=I%5==0?2.8f:1.f;
        Part(*FString::Printf(TEXT("ClockMark%d"),I),Cube,{2.7,-FMath::Sin(Radians)*25,FMath::Cos(Radians)*25},
            {.2f,I%5==0?.7f:.3f,Length},Dark,{0,0,-Angle});
    }
    auto Label=[&](const FString& Value,FVector At,float Size,FColor Color)
    {
        auto* T=NewObject<UTextRenderComponent>(OfficeClock);OfficeClock->AddInstanceComponent(T);T->SetupAttachment(Root);
        T->SetText(FText::FromString(Value));T->SetTextRenderColor(Color);T->SetWorldSize(Size);
        T->SetHorizontalAlignment(EHTA_Center);T->SetVerticalAlignment(EVRTA_TextCenter);T->RegisterComponent();T->SetRelativeLocation(At);
    };
    for(int32 H:{12,3,6,9})
    {
        const float A=FMath::DegreesToRadians(H*30.f);
        Label(FString::FromInt(H),{2.9,-FMath::Sin(A)*19,FMath::Cos(A)*19},5.f,FColor(20,37,38));
    }
    Label(TEXT("TNES"),{2.9,0,-10},2.6f,FColor(35,75,70));
    OfficeHourHand=Part(TEXT("ClockHour"),Cube,{3.1,0,0},{.45f,1.4f,14},Dark);
    OfficeMinuteHand=Part(TEXT("ClockMinute"),Cube,{3.5,0,0},{.4f,.85f,21},Accent);
    Part(TEXT("ClockPin"),Cylinder,{4,0,0},{2.3f,2.3f,.8f},Dark,{90,0,0});
    SetOfficeClockMinutes(OfficeClockMinutes);
}

void AClaraStoryDirector::SetOfficeClockMinutes(float Minutes)
{
    OfficeClockMinutes=Minutes;
    auto Hand=[](UStaticMeshComponent* Component,float Degrees,float Length,float Depth)
    {
        if(!Component)return;
        const float A=FMath::DegreesToRadians(Degrees);
        Component->SetRelativeRotation({0,0,-Degrees});
        Component->SetRelativeLocation({Depth,-FMath::Sin(A)*Length*.42f,FMath::Cos(A)*Length*.42f});
    };
    Hand(OfficeHourHand,Minutes*.5f,14,3.1f);
    Hand(OfficeMinuteHand,Minutes*6.f,21,3.5f);
}

FString AClaraStoryDirector::GetOfficeClockTime()const
{
    const int32 Minutes=FMath::RoundToInt(OfficeClockMinutes)%1440;
    return FString::Printf(TEXT("%02d:%02d"),Minutes/60,Minutes%60);
}

float AClaraStoryDirector::GetDayTransitionProgress()const
{
    return IsDayTransitionActive()?FMath::Clamp(DayTransitionElapsed/TransitionSeconds,0.f,1.f):0.f;
}

void AClaraStoryDirector::BeginNextDayTransition()
{
    if((Phase!=16&&Phase!=110)||!Completed.Contains(Phase==110?TEXT("day2"):TEXT("day1"))||IsDayTransitionActive())return;
    if(Controller.IsValid())if(auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>())Camera->EndConversation(true);
    ClosePage();CloseLaptopScreen();ReturnHeldCable();Toast.Empty();
    if(Controller.IsValid())Controller->ResumeWalk();
    SaveProgress(); // An interrupted film reopens the completed day's report.
    CreateOfficeClock();SetOfficeClockMinutes(1080.f);
    if(!DayTransitionCamera)
        DayTransitionCamera=GetWorld()->SpawnActor<ACameraActor>();
    if(!DayTransitionCamera){if(Phase==110)StartDay3();else StartDay2();return;}
    DayTransitionElapsed=0.f;
    TickDayTransition(0.f);
    if(Controller.IsValid())Controller->SetViewTargetWithBlend(DayTransitionCamera,.65f);
    ShowPanel(TEXT("daytransition"));
    if(!SelfTesting)AClaraAudio::Cue(this,TEXT("page"),.6f);
}

void AClaraStoryDirector::TickDayTransition(float DeltaSeconds)
{
    if(!IsDayTransitionActive())return;
    DayTransitionElapsed+=FMath::Max(0.f,DeltaSeconds);
    const float Zoom=FMath::SmoothStep(0.f,1.f,FMath::Clamp(DayTransitionElapsed/2.f,0.f,1.f));
    const FVector At=FMath::Lerp(FVector(255,420,223),FVector(385,420,245),Zoom);
    if(DayTransitionCamera)
    {
        DayTransitionCamera->SetActorLocationAndRotation(At,(ClockPosition-At).Rotation());
        DayTransitionCamera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(58.f,48.f,Zoom));
    }
    const float Overnight=FMath::SmoothStep(0.f,1.f,FMath::Clamp((DayTransitionElapsed-2.f)/10.f,0.f,1.f));
    SetOfficeClockMinutes(1080.f+900.f*Overnight);
    if(DayTransitionElapsed>=TransitionSeconds)FinishNextDayTransition();
}

void AClaraStoryDirector::ResetDayTransition()
{
    DayTransitionElapsed=-1.f;
    if(Controller.IsValid()&&DayTransitionCamera&&Controller->GetViewTarget()==DayTransitionCamera&&Controller->GetPawn())
        Controller->SetViewTarget(Controller->GetPawn());
}

void AClaraStoryDirector::FinishNextDayTransition()
{
    if(!IsDayTransitionActive())return;
    ResetDayTransition();
    const bool ThirdDay=Phase==110;
    if(ThirdDay)StartDay3();else StartDay2();
    if(!SelfTesting){AClaraAudio::Cue(this,TEXT("success"),.6f);Notify(ThirdDay?TEXT("09:00 · Giorno 3. Alessia ti aspetta alla reception: oggi routing e guasti."):TEXT("09:00 · Giorno 2. Luca ti aspetta in laboratorio: oggi si configura."));}
}
