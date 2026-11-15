#include "ClaraNarrativeCamera.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
const FVector BriefingSeatEye(180,1110,121);
const FVector BriefingStandingEye(180,1110,156);
float Ease(float T){return T*T*(3.f-2.f*T);}
float TravelTime(const FVector& A,const FVector& B){return FMath::Clamp(FVector::Distance(A,B)/260.f,.25f,1.15f);}
}

UClaraNarrativeCamera::UClaraNarrativeCamera()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;}

UClaraNarrativeCamera* UClaraNarrativeCamera::Get(APlayerController* PC)
{
    if(!PC)return nullptr;
    if(auto* Existing=PC->FindComponentByClass<UClaraNarrativeCamera>())return Existing;
    auto* Result=NewObject<UClaraNarrativeCamera>(PC,TEXT("NarrativeCamera"));
    PC->AddInstanceComponent(Result);Result->Controller=PC;Result->RegisterComponent();
    if(GConfig)GConfig->GetBool(TEXT("Clara.Camera"),TEXT("ReducedMotion"),Result->bReducedMotion,GGameUserSettingsIni);
    return Result;
}

FVector UClaraNarrativeCamera::SpeakerEye()const
{return Speaker.IsValid()?Speaker->GetActorLocation()+FVector(0,0,Speaker->ActorHasTag(TEXT("NPC_Alessia"))?116:154):SegmentEnd+FVector(0,-180,0);}

void UClaraNarrativeCamera::BeginConversation(AActor* Person,bool bSit)
{
    if(!Controller.IsValid()||!IsValid(Person)||Person->IsHidden())return;
    auto* Character=Cast<ACharacter>(Controller->GetPawn());
    if(!Character||FVector::Dist2D(Character->GetActorLocation(),Person->GetActorLocation())>360)return;
    // A chained Say can cancel the requested return before the next camera tick.
    if(IsActive()&&Speaker.Get()==Person&&bSitting==bSit){bEndRequested=false;return;}
    if(IsActive())EndConversation(true);
    // Device, laptop and clock cameras retain ownership of their own sequences.
    if(Controller->GetViewTarget()!=Character)return;
    Walker=Character;Speaker=Person;bSitting=bSit;bEndRequested=false;
    ResumeTransform=Character->GetActorTransform();ResumeLook=Controller->GetControlRotation();OriginalView=Controller->GetViewTarget();
    Controller->GetPlayerViewPoint(ResumeEye,ResumeViewRotation);
    OriginalFOV=Controller->PlayerCameraManager?Controller->PlayerCameraManager->GetFOVAngle():85;
    OriginalHalfHeight=Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();EyeOffset=ResumeEye.Z-ResumeTransform.GetLocation().Z;
    auto* Movement=Character->GetCharacterMovement();OriginalMovementMode=Movement->MovementMode;OriginalCustomMode=Movement->CustomMovementMode;
    bOriginalCollision=Character->GetActorEnableCollision();bOriginalYawControl=Character->bUseControllerRotationYaw;
    if(!IsValid(Shot))Shot=GetWorld()->SpawnActor<ACameraActor>();
    if(!IsValid(Shot)){Walker.Reset();Speaker.Reset();return;}
    // IgnoreInput is a counter: own exactly one increment, even if another owner already ignores input.
    Controller->SetIgnoreMoveInput(true);Controller->SetIgnoreLookInput(true);bOwnedInputLocks=true;
    Movement->StopMovementImmediately();Movement->DisableMovement();Character->bUseControllerRotationYaw=false;
    if(bSit)Character->SetActorEnableCollision(false);
    Shot->SetActorLocationAndRotation(ResumeEye,ResumeViewRotation);Shot->GetCameraComponent()->SetFieldOfView(OriginalFOV);
    Controller->SetViewTarget(Shot);SetComponentTickEnabled(true);
    ApproachPoints.Empty();ReturnPoints.Empty();ApproachIndex=0;ReturnIndex=0;
    if(bReducedMotion)
    {
        if(bSit)
        {
            Shot->SetActorLocation(BriefingSeatEye);Character->GetCapsuleComponent()->SetCapsuleHalfHeight(55);
            Character->SetActorLocationAndRotation(BriefingSeatEye-FVector(0,0,EyeOffset),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        }
        Shot->SetActorRotation((SpeakerEye()-Shot->GetActorLocation()).Rotation());State=EState::Holding;return;
    }
    if(bSit)
    {
        // Left aisle avoids the table and coffee cabinet; 1040 clears the front chairs plus capsule radius.
        if(ResumeEye.X>110)
        {
            const float SafeY=ResumeEye.Y>1255?1365.f:1040.f;
            ApproachPoints.Add(FVector(ResumeEye.X,SafeY,156));ApproachPoints.Add(FVector(75,SafeY,156));
        }
        else ApproachPoints.Add(FVector(75,ResumeEye.Y,156));
        ApproachPoints.Add(FVector(75,1110,156));ApproachPoints.Add(BriefingStandingEye);
        if(!IsApproachClear())
        {
            // Never pass a body through furniture if the player starts on the wrong side of an obstacle.
            bSitting=false;Character->SetActorEnableCollision(bOriginalCollision);ApproachPoints.Empty();
            UE_LOG(LogTemp,Display,TEXT("CLARA_CAMERA: seating route obstructed; retaining standing conversation."));
            StartSegment(ResumeEye,.4f,EState::Sitting);return;
        }
        StartSegment(ApproachPoints[0],TravelTime(ResumeEye,ApproachPoints[0]),EState::Approach);
    }
    else StartSegment(ResumeEye,.4f,EState::Sitting);
}

bool UClaraNarrativeCamera::IsApproachClear()const
{
    if(!Walker.IsValid()||!GetWorld())return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClaraConversationPath),false,Walker.Get());
    if(Speaker.IsValid())Query.AddIgnoredActor(Speaker.Get());if(Shot)Query.AddIgnoredActor(Shot);
    FVector From=ResumeEye;const float Radius=Walker->GetCapsuleComponent()->GetScaledCapsuleRadius();
    for(int32 I=0;I<ApproachPoints.Num();++I)
    {
        const FVector To=ApproachPoints[I];FHitResult Hit;
        if(GetWorld()->SweepSingleByChannel(Hit,From,To,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(5),Query))return false;
        // The last lateral leg deliberately enters the selected chair; all approach legs stay clear.
        if(I<ApproachPoints.Num()-1&&GetWorld()->SweepSingleByChannel(Hit,From-FVector(0,0,EyeOffset),To-FVector(0,0,EyeOffset),FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(Radius,OriginalHalfHeight*Walker->GetActorScale3D().Z),Query))return false;
        From=To;
    }
    return true;
}

void UClaraNarrativeCamera::StartSegment(const FVector& Target,float Duration,EState NextState)
{
    SegmentStart=Shot->GetActorLocation();SegmentEnd=Target;SegmentRotation=Shot->GetActorRotation();SegmentFOV=Shot->GetCameraComponent()->FieldOfView;
    SegmentTime=0;SegmentDuration=Duration;State=NextState;
}

void UClaraNarrativeCamera::EndConversation(bool bImmediately)
{
    if(!IsActive())return;
    if(bImmediately||bReducedMotion||!Walker.IsValid()||!Controller.IsValid()){RestorePlayer();return;}
    if(State!=EState::Rising&&State!=EState::Returning)bEndRequested=true;
}

void UClaraNarrativeCamera::StartReturn()
{
    bEndRequested=false;ReturnPoints.Empty();ReturnIndex=0;
    // Include only completed approach legs. An early Escape retraces its partial leg first.
    for(int32 I=FMath::Min(ApproachIndex,ApproachPoints.Num())-1;I>=0;--I)ReturnPoints.Add(ApproachPoints[I]);
    ReturnPoints.Add(ResumeEye);
    const FVector Eye=Shot->GetActorLocation();
    if(bSitting&&Eye.Z<BriefingStandingEye.Z-1)StartSegment(FVector(Eye.X,Eye.Y,BriefingStandingEye.Z),.4f,EState::Rising);
    else NextReturnSegment();
}

void UClaraNarrativeCamera::NextReturnSegment()
{
    while(ReturnPoints.IsValidIndex(ReturnIndex)&&FVector::DistSquared(Shot->GetActorLocation(),ReturnPoints[ReturnIndex])<1)++ReturnIndex;
    if(ReturnPoints.IsValidIndex(ReturnIndex)){const FVector Target=ReturnPoints[ReturnIndex++];StartSegment(Target,TravelTime(Shot->GetActorLocation(),Target),EState::Returning);}
    else StartSegment(ResumeEye,.3f,EState::Returning);
}

void UClaraNarrativeCamera::RestorePlayer()
{
    if(Walker.IsValid())
    {
        Walker->GetCapsuleComponent()->SetCapsuleHalfHeight(OriginalHalfHeight);
        Walker->SetActorTransform(ResumeTransform,false,nullptr,ETeleportType::TeleportPhysics);
        Walker->SetActorEnableCollision(bOriginalCollision);Walker->bUseControllerRotationYaw=bOriginalYawControl;
        Walker->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(OriginalMovementMode),OriginalCustomMode);
    }
    if(Controller.IsValid())
    {
        if(Controller->GetViewTarget()==Shot)
        {
            Controller->SetControlRotation(ResumeLook);
            Controller->SetViewTarget(OriginalView.IsValid()?OriginalView.Get():Controller->GetPawn());
        }
        if(bOwnedInputLocks){Controller->SetIgnoreMoveInput(false);Controller->SetIgnoreLookInput(false);}
    }
    State=EState::Idle;bOwnedInputLocks=false;bEndRequested=false;Speaker.Reset();Walker.Reset();SetComponentTickEnabled(false);
}

void UClaraNarrativeCamera::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    if(!IsActive())return;
    if(!Walker.IsValid()||!Controller.IsValid()||!Speaker.IsValid()||!IsValid(Shot)||Controller->GetPawn()!=Walker.Get()||Controller->GetViewTarget()!=Shot){RestorePlayer();return;}
    if(bEndRequested)StartReturn();
    if(State==EState::Holding)return;
    SegmentTime+=FMath::Max(Delta,0.f);const float T=FMath::Clamp(SegmentTime/FMath::Max(SegmentDuration,.01f),0.f,1.f),Blend=Ease(T);
    const FVector Eye=FMath::Lerp(SegmentStart,SegmentEnd,Blend);
    const bool bReturning=State==EState::Returning||State==EState::Rising;
    const FRotator Look=bReturning?ResumeViewRotation:(SpeakerEye()-Eye).Rotation();
    Shot->SetActorLocationAndRotation(Eye,FQuat::Slerp(SegmentRotation.Quaternion(),Look.Quaternion(),Blend));
    // Keep the original lens throughout: no zoom pulse at the end of each waypoint.
    Shot->GetCameraComponent()->SetFieldOfView(FMath::Lerp(SegmentFOV,OriginalFOV,Blend));
    if(bSitting)
    {
        Walker->SetActorLocationAndRotation(Eye-FVector(0,0,EyeOffset),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        Walker->GetCapsuleComponent()->SetCapsuleHalfHeight(FMath::Clamp(Eye.Z-EyeOffset,55.f,OriginalHalfHeight));
    }
    if(T<1)return;
    if(State==EState::Approach)
    {
        if(++ApproachIndex<ApproachPoints.Num())StartSegment(ApproachPoints[ApproachIndex],TravelTime(Eye,ApproachPoints[ApproachIndex]),EState::Approach);
        else StartSegment(BriefingSeatEye,.55f,EState::Sitting);
    }
    else if(State==EState::Sitting)State=EState::Holding;
    else if(State==EState::Rising)NextReturnSegment();
    else if(State==EState::Returning){if(ReturnIndex<ReturnPoints.Num())NextReturnSegment();else RestorePlayer();}
}

void UClaraNarrativeCamera::ToggleReducedMotion()
{
    bReducedMotion=!bReducedMotion;
    if(GConfig){GConfig->SetBool(TEXT("Clara.Camera"),TEXT("ReducedMotion"),bReducedMotion,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
    if(IsActive()&&bReducedMotion)EndConversation(true);
}

void UClaraNarrativeCamera::EndPlay(const EEndPlayReason::Type Reason)
{
    if(IsActive())RestorePlayer();
    if(IsValid(Shot))Shot->Destroy();
    Super::EndPlay(Reason);
}
