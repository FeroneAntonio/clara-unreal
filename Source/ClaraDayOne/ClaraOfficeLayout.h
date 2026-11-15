#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraOfficeLayout.generated.h"
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
UCLASS()
class CLARADAYONE_API AClaraOfficeLayout : public AActor
{
    GENERATED_BODY()
public:
    AClaraOfficeLayout();
    virtual void BeginPlay() override;
    void ShowPlan(bool bPlan);
    bool IsBuildReady() const {return bBuildReady;}
    int32 GetZoneCount() const { return 5; }
private:
    int32 BuildStage=0;
    bool bBuildReady=false;
    void BuildNextStage();
    UStaticMeshComponent* Shape(const TCHAR* Name, FVector P, FVector Size, FLinearColor C, bool bSolid = true, FRotator R = FRotator::ZeroRotator, UStaticMesh* Mesh = nullptr);
    void Text(const FString& Value, FVector P, float Size = 6, FRotator R = FRotator(0,-90,0), FColor C = FColor(35,65,65));
    UMaterialInstanceDynamic* Mat(FLinearColor C);
    bool Asset(const TCHAR* Id, FVector P, FVector TargetSize, float Yaw=0);
    void Architecture();
    void Reception();
    void Lounge();
    void Network();
    void Cyber();
    void Apparatus();
    void Details();
    void GlassWallX(float X, float Y, float Length);
    void GlassWallY(float Y, float X, float Length);
    void Portal(float X, float Y, const FString& Name);
    void Table(FVector P, FVector2D Size, float Height = 75);
    void Chair(FVector P, float Yaw = 0);
    void Computer(FVector P, bool bDual = false, bool bRed = false, bool bTower = true);
    void Monitor(FVector P, bool bRed = false);
    void Tower(FVector P);
    void Switch(FVector P, int32 Ports = 24);
    void Router(FVector P);
    void Laptop(FVector P);
    void Cable(FVector P, float Radius, FLinearColor C);
    void Plant(FVector P, float Height = 110);
    void Rack(FVector P);
    void WallScreen(FVector P, bool bLogs = false);
    float RoomOffsetY = 0;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UMaterialInterface> Base;
    UPROPERTY() TObjectPtr<UMaterialInterface> Wood;
    UPROPERTY() TObjectPtr<UMaterialInterface> Glass;
    UPROPERTY() TMap<FString,TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ceiling;
};
