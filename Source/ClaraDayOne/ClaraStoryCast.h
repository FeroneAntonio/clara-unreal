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
class USkinnedMeshComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Story colleague with a local MetaHuman visual and a hidden animation driver. */
UCLASS()
class CLARADAYONE_API AClaraStoryNPC : public AActor
{
    GENERATED_BODY()
public:
    AClaraStoryNPC();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Initialize(const FString& Id, const FString& Name, const FString& RoleName, bool bUseQuinn, FLinearColor Accent);
    UFUNCTION(BlueprintPure) FString GetCharacterId() const { return CharacterId; }
    UFUNCTION(BlueprintPure) FString GetDisplayName() const { return DisplayName; }
    UFUNCTION(BlueprintPure) FString GetRole() const { return RoleDescription; }
    UFUNCTION(BlueprintPure) UPrimitiveComponent* GetInteractionComponent() const;
    UFUNCTION(BlueprintPure) USkeletalMeshComponent* GetVisualComponent() const { return Mannequin.Get(); }
    UFUNCTION(BlueprintPure) bool HasMetaHuman() const { return MetaHuman != nullptr; }
    UFUNCTION(BlueprintPure) bool HasCustomLucaHead() const { return bCustomLucaHead; }
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
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> OfficeAnimationRoot;
    /** Local MetaHuman (Scripts/setup_metahuman_cast.py) driven by the hidden mannequin pose. */
    UPROPERTY() TObjectPtr<AActor> MetaHuman;
    TWeakObjectPtr<AClaraStoryDirector> Director;
    FVector FollowGoal;
    float NextRouteTime=0;
    bool bWalking=false;
    bool bCustomLucaHead=false;
    void ConfigureSeatedPose(USkeletalMesh* Mesh,float Scale);
    void AttachMetaHuman(const FString& AssetName, USkinnedMeshComponent* Leader);
    bool AttachOfficeCharacter(const FString& AssetName);
    void AttachCustomLucaHead(USkinnedMeshComponent* Leader, USkeletalMeshComponent* Face);
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
    bool IsBuildReady() const {return bBuildReady;}

private:
    bool bBuildReady=false;
    int32 NextColleague=0;
    void BuildNextColleague();
    UPROPERTY() TArray<TObjectPtr<AClaraStoryNPC>> Colleagues;
    UPROPERTY() TObjectPtr<AClaraStoryProp> Badge;
};
