#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraAircraftIntro.generated.h"
class ACameraActor;
class UStaticMeshComponent;
class UTexture2D;
UCLASS()
class CLARADAYONE_API AClaraAircraftIntro final:public AActor
{
    GENERATED_BODY()
public:
    AClaraAircraftIntro();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool IsUsable() const {return bUsable;}
    bool IsComplete() const {return Time>=Duration;}
    FString Caption() const;
    static bool RunSelfTest();
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Cabin;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> View;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TObjectPtr<UTexture2D> AerialPhoto;
    UPROPERTY() TArray<TObjectPtr<AActor>> Lights;
    FVector Eye,Paper,Window;
    float Time=0,Duration=27;
    bool bUsable=false;
    struct FCaption {float Start,End;FString Text;};
    TArray<FCaption> Captions;
    FString ShotDirectory;
    int32 NextShot=0;
};
