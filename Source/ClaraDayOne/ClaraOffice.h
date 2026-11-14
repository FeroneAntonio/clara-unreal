#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraOffice.generated.h"

class AStaticMeshActor;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UClaraDisplay;

UCLASS()
class CLARADAYONE_API AClaraOffice : public AActor
{
    GENERATED_BODY()
public:
    AClaraOffice();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable) void SetCableConnected(bool bConnected);
    UFUNCTION(BlueprintCallable) void SetCableHeld(bool bHeld);
    UFUNCTION(BlueprintCallable) void SetVision(bool bEnabled);
    UFUNCTION(BlueprintPure) FVector GetPCSocketLocation() const { return PCSocket; }
    UFUNCTION(BlueprintPure) UClaraDisplay* GetPCDisplay() const { return PCDisplay.Get(); }
    UFUNCTION(BlueprintPure) UClaraDisplay* GetSwitchDisplay() const { return SwitchDisplay.Get(); }

private:
    AStaticMeshActor* Box(const FString& Name, FVector Position, FVector Size, FLinearColor Color,
        FName Interaction = NAME_None, bool bCollision = true, FRotator Rotation = FRotator::ZeroRotator);
    AStaticMeshActor* Cylinder(const FString& Name, FVector Position, FVector Size, FLinearColor Color,
        bool bCollision = false, FRotator Rotation = FRotator::ZeroRotator);
    void Label(const FString& Text, FVector Position, float Size = 5.f,
        FColor Color = FColor::White, FRotator Rotation = FRotator(0, 180, 0));
    void Desk(FVector Position, bool bMainDesk);
    void Rack();
    void Room();
    void Details();
    void People();
    void UpdateCable();
    void UpdateConsoleCable(bool bConnected, bool bHeld, bool bCarried);
    UMaterialInstanceDynamic* Material(FLinearColor Color, float Roughness = .65f);

    UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TObjectPtr<UClaraDisplay> PCDisplay;
    UPROPERTY() TObjectPtr<UClaraDisplay> SwitchDisplay;
    UPROPERTY() TObjectPtr<UMaterialInterface> WoodMaterial;
    UPROPERTY() TObjectPtr<UStaticMesh> LucaMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> AlessiaMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> JonnyMesh;
    UPROPERTY() TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TObjectPtr<USplineComponent> CableSpline;
    UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> CableSegments;
    UPROPERTY() TObjectPtr<USplineComponent> ConsoleSpline;
    UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> ConsoleSegments;
    UPROPERTY() TObjectPtr<AStaticMeshActor> ConsoleCoil;
    UPROPERTY() TObjectPtr<AStaticMeshActor> ConsoleConnector;
    UPROPERTY() TObjectPtr<AStaticMeshActor> ConsoleConnectorClip;
    UPROPERTY() TObjectPtr<AStaticMeshActor> ConsoleSwitchPlug;
    UPROPERTY() TObjectPtr<AStaticMeshActor> ConsoleLaptopPlug;
    UPROPERTY() TObjectPtr<AStaticMeshActor> FreeConnector;
    UPROPERTY() TObjectPtr<AStaticMeshActor> LinkLED;
    UPROPERTY() TObjectPtr<AStaticMeshActor> PCLinkLED;
    UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> CableRestCoilSegments;
    UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> VisionMarkers;
    FVector FixedCableEnd = FVector(632.7, 264.4, 123);
    FVector PCSocket = FVector(351.8, 200, 103);
    FVector FreeCableRest = FVector(340, 175, 64);
    bool bCableConnected = false;
    bool bCableHeld = false;
    bool bVision = false;
};
