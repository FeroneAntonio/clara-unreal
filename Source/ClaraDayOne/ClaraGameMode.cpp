#include "ClaraGameMode.h"
#include "ClaraGatewayCheck.h"
#include "ClaraStoryDirector.h"
#include "ClaraStoryCast.h"
#include "ClaraStoryHighlights.h"
#include "ClaraOfficeLayout.h"
#include "ClaraNetworkDevice.h"
#include "ClaraRoomVisitor.h"
#include "ClaraLayoutNavigation.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

AClaraGameMode::AClaraGameMode()
{
    DefaultPawnClass=AClaraRoomVisitor::StaticClass();
    PlayerControllerClass=AClaraRoomController::StaticClass();
    HUDClass=nullptr;
}
void AClaraGameMode::BeginPlay()
{
    Super::BeginPlay();
    if(!FApp::IsUnattended())
    {
        FTimerHandle IdleDelay;
        GetWorldTimerManager().SetTimer(IdleDelay,[]()
        {if(auto* Idle=IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground")))Idle->Set(1,ECVF_SetByCode);},3.f,false);
    }
    AClaraOfficeLayout* Layout=GetWorld()->SpawnActor<AClaraOfficeLayout>();
    APawn* Visitor=GetWorld()->GetFirstPlayerController()->GetPawn();
    if(Visitor)
    {
        Visitor->SetActorLocation({175,80,90});
        GetWorld()->GetFirstPlayerController()->SetControlRotation({-3,65,0});
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraGatewayCheck"))){RunClaraGatewayCheck(GetWorld());return;}
    AClaraStoryDirector* Story=nullptr;
    if(!FParse::Param(FCommandLine::Get(),TEXT("ClaraLayoutCheck")))
    {
        GetWorld()->SpawnActor<AClaraStoryCast>();
        Story=GetWorld()->SpawnActor<AClaraStoryDirector>();
        GetWorld()->SpawnActor<AClaraStoryHighlights>();
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraStoryCheck")))
    {
        FTimerHandle StoryCheck;
        GetWorldTimerManager().SetTimer(StoryCheck,[Story]()
        {
            const bool Good=Story&&Story->RunSelfTest();
            FPlatformMisc::RequestExitWithStatus(false,Good?0:1);
        },1.f,false);
        return;
    }
    if(Story&&FParse::Param(FCommandLine::Get(),TEXT("ClaraStoryDay2")))Story->StartDay2();
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraLayoutCheck")))
    {
        FTimerHandle Check;
        GetWorldTimerManager().SetTimer(Check,[this,Layout,Visitor]()
        {
            bool Good=Layout && Layout->GetZoneCount()==5 && Visitor && Visitor->IsA<AClaraRoomVisitor>();
            const FVector DoorCenters[]={{175,540,90},{690,540,90},{470,760,90},{680,760,90},{1510,760,90}};
            TArray<UStaticMeshComponent*> Parts; Layout->GetComponents(Parts);
            for(FVector Door : DoorCenters)
            {
                FHitResult Hit; FCollisionQueryParams Params; Params.AddIgnoredActor(Visitor);
                const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Door-FVector(0,65,0),Door+FVector(0,65,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(28,88),Params);
                if(Blocked) UE_LOG(LogTemp,Error,TEXT("CLARA_LAYOUT_CHECK: blocked doorway at %s: %s"),*Door.ToString(),*GetNameSafe(Hit.GetComponent()));
                Good &= !Blocked;
                const FBox Opening(Door+FVector(-54,-28,-75),Door+FVector(54,28,140));
                for(auto* Part:Parts)
                    if(Part->ComponentHasTag(TEXT("SlidingDoor")) && Part->Bounds.GetBox().Intersect(Opening))
                    {
                        UE_LOG(LogTemp,Error,TEXT("CLARA_DOOR_VISUAL_FAIL: leaf intrudes into opening %s"),*Door.ToString());
                        Good=false;
                    }
            }
            int32 ReceptionDesks=0;
            for(auto* Part:Parts)
                if(Part->ComponentHasTag(TEXT("OfficeDesk")) && Part->Bounds.Origin.X<560 && Part->Bounds.Origin.Y<540)++ReceptionDesks;
            UE_LOG(LogTemp,Display,TEXT("CLARA_RECEPTION_DESK_%s: %d desk"),ReceptionDesks==1?TEXT("PASS"):TEXT("FAIL"),ReceptionDesks);
            Good &= ReceptionDesks==1;
            Good &= RunClaraNetworkDeviceSelfTest(GetWorld());
            Good &= CheckClaraWalkableRoutes(GetWorld(),Visitor);
            if(auto* PC=Cast<AClaraRoomController>(GetWorld()->GetFirstPlayerController()))
            {
                FCollisionQueryParams Params;Params.AddIgnoredActor(Visitor);
                for(int32 ViewIndex=1;ViewIndex<=4;++ViewIndex)
                {
                    PC->View(ViewIndex);PC->ResumeWalk();
                    const bool Blocked=GetWorld()->OverlapBlockingTestByChannel(Visitor->GetActorLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(28,88),Params);
                    const bool Free=PC->GetViewTarget()==Visitor && !Blocked;
                    UE_LOG(LogTemp,Display,TEXT("CLARA_VIEW_WALK_%s: view %d"),Free?TEXT("PASS"):TEXT("FAIL"),ViewIndex);
                    Good &= Free;
                }
                PC->OpenFirstDevice(); Good &= PC->IsDeviceOpen();
                PC->ZoomDevice(-16); PC->CloseDevice(); Good &= !PC->IsDeviceOpen();
                PC->ReturnToEntrance();
                Good &= Visitor->GetActorLocation().Equals(FVector(175,80,90),5.f);
            }
            if(Layout){Layout->ShowPlan(true);Layout->ShowPlan(false);}
            UE_LOG(LogTemp,Display,TEXT("CLARA_LAYOUT_CHECK_%s: five zones and five traversable internal doorways; no story pawn."),Good?TEXT("PASS"):TEXT("FAIL"));
            FPlatformMisc::RequestExitWithStatus(false,Good?0:1);
        },.8f,false);
    }
    else if(FParse::Param(FCommandLine::Get(),TEXT("ClaraDeviceView")))
    {
        FTimerHandle View;
        GetWorldTimerManager().SetTimer(View,[this](){if(auto* PC=Cast<AClaraRoomController>(GetWorld()->GetFirstPlayerController()))PC->OpenFirstDevice();},.8f,false);
    }
    else if(FParse::Param(FCommandLine::Get(),TEXT("ClaraPlanView")) || FParse::Param(FCommandLine::Get(),TEXT("ClaraChairView")))
    {
        FTimerHandle View;
        GetWorldTimerManager().SetTimer(View,[this](){if(auto* PC=Cast<AClaraRoomController>(GetWorld()->GetFirstPlayerController()))PC->View(FParse::Param(FCommandLine::Get(),TEXT("ClaraChairView"))?6:0);},.8f,false);
    }
}
