#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraNetworkDevice.generated.h"
class UStaticMeshComponent;
class UPrimitiveComponent;
class UBoxComponent;
class USplineComponent;
class UPointLightComponent;

struct FClaraDevicePort
{
    FString Id, Label, Kind;
    FVector Position = FVector::ZeroVector;
    FVector Normal = FVector(0,-1,0);
};

UCLASS()
class CLARADAYONE_API AClaraNetworkDevice : public AActor
{
    GENERATED_BODY()
public:
    AClaraNetworkDevice();
    void Initialize(const FString& ModelId, bool bPreview = false);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetPreviewSource(AClaraNetworkDevice* Source);
    AClaraNetworkDevice* GetOriginalDevice() const;
    bool IsPreview() const { return bIsPreview; }
    FString GetModelId() const { return ModelId; }
    FString GetDisplayName() const { return DisplayName; }
    FVector GetDeviceSize() const { return DeviceSize; }
    const TArray<FClaraDevicePort>& GetPorts() const { return Ports; }
    FString PortIdForComponent(const UPrimitiveComponent* Component) const;
    FVector GetPortWorldLocation(const FString& PortId) const;
    FVector GetPortWorldNormal(const FString& PortId) const;
    bool IsPortOccupied(const FString& PortId) const;
    FString GetPortStatus(const FString& PortId) const;
    AClaraNetworkDevice* GetPeerForPort(const FString& PortId, FString& PeerPort) const;
    FString GetEffectivePortKind(const FString& PortId) const;
    bool InstallOpticalModule(const FString& PortId);
    bool HasOpticalModule(const FString& PortId) const;
    // LED della porta: 0 spento, 1 ambra (lo switch sta aspettando), 2 verde, 3 verde con traffico.
    int32 GetLedState(const FString& PortId) const;
    void MarkTraffic(const FString& PortId, float Seconds=2.5f);
    void SetLaptopCarried(bool bCarried);
    bool IsLaptopCarried() const { return GetOriginalDevice()->bLaptopCarried; }
    bool DisconnectPort(const FString& PortId, FString& Error);
    void SetPendingPort(const FString& PortId, const FString& Kind=TEXT("ethernet")) { PendingPort = PortId; PendingKind=Kind; }
    static bool ConnectPorts(AClaraNetworkDevice* A, const FString& PortA, AClaraNetworkDevice* B, const FString& PortB, const FString& CableKind, FString& Error);
    static bool IsCableCompatible(const FString& CableKind, const FString& KindA, const FString& KindB);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TArray<TObjectPtr<UBoxComponent>> PortHits;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Plugs;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Leds;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> LedLights;
    TMap<FString,double> TrafficUntil;
    TWeakObjectPtr<AClaraNetworkDevice> PreviewSource;
    FString ModelId, DisplayName, PendingPort, PendingKind=TEXT("ethernet");
    FVector DeviceSize = FVector(44.5,27.94,4.45);
    TArray<FClaraDevicePort> Ports;
    struct FConnection { TWeakObjectPtr<AClaraNetworkDevice> Peer; FString PeerPort; FString CableKind; double Since=0; };
    TMap<FString,FConnection> Connections;
    struct FCable { FString Port, OtherPort, Kind; TWeakObjectPtr<AClaraNetworkDevice> Peer; TObjectPtr<USplineComponent> Spline; TArray<TObjectPtr<class USplineMeshComponent>> Segments; };
    TArray<FCable> Cables;
    bool bIsPreview = false;
    bool bLaptopCarried=false;
    FVector LaptopOpenSize=FVector::ZeroVector;
    TSet<FString> OpticalModules;
    const FClaraDevicePort* FindPort(const FString& PortId) const;
    void AddCable(const FString& Port, AClaraNetworkDevice* Peer, const FString& PeerPort, const FString& CableKind);
    void RefreshCables();
};

bool RunClaraNetworkDeviceSelfTest(UWorld* World);
