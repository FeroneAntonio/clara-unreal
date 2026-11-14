#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraStoryCast.generated.h"

class UBoxComponent;
class UPoseableMeshComponent;
class UAnimSequence;
class AClaraStoryDirector;
class UCapsuleComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Temporary Epic mannequin used to validate the story, not a likeness of a real colleague. */
UCLASS()
class CLARADAYONE_API AClaraStoryNPC : public AActor
{
    GENERATED_BODY()
public:
    AClaraStoryNPC();
    virtual void Tick(float DeltaSeconds) override;
    void Initialize(const FString& Id, const FString& Name, const FString& RoleName, bool bUseQuinn, FLinearColor Accent);
    UFUNCTION(BlueprintPure) FString GetCharacterId() const { return CharacterId; }
    UFUNCTION(BlueprintPure) FString GetDisplayName() const { return DisplayName; }
    UFUNCTION(BlueprintPure) FString GetRole() const { return RoleDescription; }
    UFUNCTION(BlueprintPure) UPrimitiveComponent* GetInteractionComponent() const;
    UFUNCTION(BlueprintPure) USkeletalMeshComponent* GetVisualComponent() const { return Mannequin.Get(); }
    UFUNCTION(BlueprintCallable) void SetStoryVisible(bool bVisible);

private:
    UPROPERTY(VisibleAnywhere, Category="Story") FString CharacterId;
    UPROPERTY(VisibleAnywhere, Category="Story") FString DisplayName;
    UPROPERTY(VisibleAnywhere, Category="Story") FString RoleDescription;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Interaction;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Mannequin;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Nameplate;
    UPROPERTY() TObjectPtr<UPoseableMeshComponent> SeatedPose;
    UPROPERTY() TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> WalkAnimation;
    TWeakObjectPtr<AClaraStoryDirector> Director;
    FVector FollowGoal;
    float NextRouteTime=0;
    bool bWalking=false;
    void ConfigureSeatedPose(USkeletalMesh* Mesh,float Scale);
};

UCLASS()
class CLARADAYONE_API AClaraStoryProp : public AActor
{
    GENERATED_BODY()
public:
    AClaraStoryProp();
    void Initialize(const FString& Id, const FString& Label);
    UFUNCTION(BlueprintPure) FString GetPropId() const { return PropId; }
    UFUNCTION(BlueprintPure) FString GetDisplayName() const { return DisplayName; }
    UFUNCTION(BlueprintPure) UPrimitiveComponent* GetInteractionComponent() const;

private:
    UPROPERTY(VisibleAnywhere, Category="Story") FString PropId;
    UPROPERTY(VisibleAnywhere, Category="Story") FString DisplayName;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Interaction;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Card;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stripe;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Clip;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Lettering;
};

/** Adds only story actors; the existing floor plan and furniture are left intact. */
UCLASS()
class CLARADAYONE_API AClaraStoryCast : public AActor
{
    GENERATED_BODY()
public:
    AClaraStoryCast();
    virtual void BeginPlay() override;

private:
    UPROPERTY() TArray<TObjectPtr<AClaraStoryNPC>> Colleagues;
    UPROPERTY() TObjectPtr<AClaraStoryProp> Badge;
};
