#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClaraNarrativeCamera.generated.h"

class ACameraActor;
class ACharacter;
class APlayerController;

UCLASS()
class CLARADAYONE_API UClaraNarrativeCamera : public UActorComponent
{
    GENERATED_BODY()
public:
    UClaraNarrativeCamera();
    static UClaraNarrativeCamera* Get(APlayerController* Controller);
    void BeginConversation(AActor* Speaker,bool bSitAtBriefingTable);
    void EndConversation(bool bImmediately=false);
    void ToggleReducedMotion();
    bool IsActive() const {return State!=EState::Idle;}
    bool IsHolding() const {return State==EState::Holding&&!bEndRequested;}
    bool HasReducedMotion() const {return bReducedMotion;}
    bool IsApproachClear() const;
    FTransform GetResumeTransform() const {return ResumeTransform;}
    FRotator GetResumeLook() const {return ResumeLook;}
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    enum class EState:uint8 {Idle,Approach,Sitting,Holding,Rising,Returning};
    EState State=EState::Idle;
    UPROPERTY() TObjectPtr<ACameraActor> Shot;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<ACharacter> Walker;
    TWeakObjectPtr<AActor> Speaker,OriginalView;
    FTransform ResumeTransform;
    FRotator ResumeLook,ResumeViewRotation,SegmentRotation;
    FVector ResumeEye,SegmentStart,SegmentEnd;
    TArray<FVector> ApproachPoints,ReturnPoints;
    int32 ApproachIndex=0,ReturnIndex=0;
    float SegmentTime=0,SegmentDuration=0,SegmentFOV=85,OriginalFOV=85,OriginalHalfHeight=88,EyeOffset=66;
    uint8 OriginalMovementMode=0,OriginalCustomMode=0;
    bool bReducedMotion=false,bSitting=false,bOriginalCollision=true,bOriginalYawControl=true,bOwnedInputLocks=false,bEndRequested=false;
    void StartSegment(const FVector& Target,float Duration,EState NextState);
    void StartReturn();
    void NextReturnSegment();
    void RestorePlayer();
    FVector SpeakerEye() const;
};
