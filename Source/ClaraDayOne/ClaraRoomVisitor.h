#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "ClaraRoomVisitor.generated.h"
class UCameraComponent;
class ACameraActor;
class AClaraNetworkDevice;
class UStaticMeshComponent;
class AClaraStoryDirector;
UCLASS()
class CLARADAYONE_API AClaraRoomVisitor : public ACharacter
{
    GENERATED_BODY()
public:
    AClaraRoomVisitor();
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Forward(float V);
    void Right(float V);
    void Turn(float V);
    void Look(float V);
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
};
UCLASS()
class CLARADAYONE_API AClaraRoomController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    void View(int32 Index);
    void ResumeWalk();
    void ReturnToEntrance();
    bool IsLooking() const { return bLook; }
    AClaraStoryDirector* GetStory() const;
    bool IsStoryModal() const;
    void SetCableKind(const FString& Kind);
    FString GetCableKind() const { return CableKind; }
    void OpenDevice(AClaraNetworkDevice* Device);
    void OpenFirstDevice();
    void SelectDevicePort(const FString& Id);
    void CloseDevice();
    bool IsDeviceOpen() const { return DevicePreview != nullptr; }
    void OrbitDevice(float X, float Y);
    void ZoomDevice(float Amount);
private:
    void Pointer();
    bool bLook = false;
    FString CableKind=TEXT("ethernet");
    int32 ActiveView = -1;
    TSharedPtr<class SWidget> Toolbar;
    UPROPERTY() TArray<TObjectPtr<ACameraActor>> Views;
    UPROPERTY() TObjectPtr<ACameraActor> InspectCamera;
    UPROPERTY() TObjectPtr<AClaraNetworkDevice> DevicePreview;
    UPROPERTY() TObjectPtr<ACameraActor> DeviceCamera;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> PreviewBackdrop;
    TWeakObjectPtr<AClaraNetworkDevice> DeviceSource;
    TWeakObjectPtr<AClaraNetworkDevice> PendingDevice;
    FString PendingDevicePort, HoveredDevicePort, DeviceMessage;
    TSharedPtr<class SWidget> DevicePanel;
    float DeviceYaw=-90, DevicePitch=18, DeviceDistance=90;
    bool bDeviceDragging=false, bDeviceLeftDrag=false, bDeviceMoved=false;
    void UpdateDeviceCamera();
    void ClickDevicePort();
    void BuildDevicePanel();
};
