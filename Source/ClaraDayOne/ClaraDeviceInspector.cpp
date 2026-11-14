#include "ClaraRoomVisitor.h"
#include "ClaraNetworkDevice.h"
#include "ClaraOfficeLayout.h"
#include "ClaraStoryDirector.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Brushes/SlateColorBrush.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void AClaraRoomController::OpenFirstDevice()
{
    AClaraNetworkDevice* Fallback=nullptr;
    AClaraNetworkDevice* SwitchFallback=nullptr;
    for(TActorIterator<AClaraNetworkDevice> It(GetWorld());It;++It)
    {
        if(It->IsPreview())continue;
        if(!Fallback)Fallback=*It;
        if(It->GetModelId()==TEXT("Cisco2960X"))
        {
            if(It->ActorHasTag(TEXT("BenchDevice"))){OpenDevice(*It);return;}
            if(!SwitchFallback)SwitchFallback=*It;
        }
    }
    if(SwitchFallback)OpenDevice(SwitchFallback);
    else if(Fallback)OpenDevice(Fallback);
}
void AClaraRoomController::OpenDevice(AClaraNetworkDevice* Device)
{
    if(!Device)return;Device=Device->GetOriginalDevice();
    if(DevicePreview)DevicePreview->Destroy();
    DeviceSource=Device;
    DevicePreview=GetWorld()->SpawnActor<AClaraNetworkDevice>(FVector(0,0,5000),FRotator::ZeroRotator);
    if(!DevicePreview)return;
    DevicePreview->Initialize(Device->GetModelId(),true);DevicePreview->SetPreviewSource(Device);
    DevicePreview->SetPendingPort(PendingDevice.Get()==Device?PendingDevicePort:FString(),GetCableKind());
    PreviewBackdrop=NewObject<UStaticMeshComponent>(DevicePreview);DevicePreview->AddInstanceComponent(PreviewBackdrop);
    PreviewBackdrop->SetMobility(EComponentMobility::Movable);PreviewBackdrop->SetupAttachment(DevicePreview->GetRootComponent());
    PreviewBackdrop->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    PreviewBackdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);PreviewBackdrop->SetCastShadow(false);
    auto* BackMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Office.M_Office")),DevicePreview);
    if(BackMaterial){BackMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(.008,.012,.018));PreviewBackdrop->SetMaterial(0,BackMaterial);}
    PreviewBackdrop->SetRelativeScale3D({.02,8,6});PreviewBackdrop->RegisterComponent();
    if(!DeviceCamera)DeviceCamera=GetWorld()->SpawnActor<ACameraActor>();
    DeviceYaw=Device->GetModelId()==TEXT("Laptop")?(GetCableKind()==TEXT("console")?0.f:180.f):Device->GetModelId()==TEXT("Cisco1941")?90.f:-90.f;
    DevicePitch=18;DeviceDistance=FMath::Max(75.f,static_cast<float>(Device->GetDeviceSize().GetMax())*2.1f);
    DeviceCamera->GetCameraComponent()->SetFieldOfView(45);
    bDeviceDragging=false;HoveredDevicePort.Empty();ActiveView=-1;
    for(TActorIterator<AClaraOfficeLayout> It(GetWorld());It;++It)It->ShowPlan(false);
    if(auto* Character=Cast<ACharacter>(GetPawn()))Character->GetCharacterMovement()->StopMovementImmediately();
    if(Toolbar.IsValid())Toolbar->SetVisibility(EVisibility::Collapsed);
    UpdateDeviceCamera();SetViewTarget(DeviceCamera);Pointer();BuildDevicePanel();
}
void AClaraRoomController::CloseDevice()
{
    if(DevicePanel.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DevicePanel.ToSharedRef());
    DevicePanel.Reset();
    if(DevicePreview)DevicePreview->Destroy();DevicePreview=nullptr;DeviceSource.Reset();
    if(DeviceCamera)DeviceCamera->Destroy();DeviceCamera=nullptr;
    bDeviceDragging=false;HoveredDevicePort.Empty();ActiveView=-1;
    if(Toolbar.IsValid())Toolbar->SetVisibility(EVisibility::SelfHitTestInvisible);
    if(GetPawn())SetViewTarget(GetPawn());Pointer();
}
void AClaraRoomController::UpdateDeviceCamera()
{
    if(!DevicePreview||!DeviceCamera)return;
    const FVector Center=DevicePreview->GetActorLocation()+FVector(0,0,DevicePreview->GetDeviceSize().Z*.5);
    const float Yaw=FMath::DegreesToRadians(DeviceYaw),Pitch=FMath::DegreesToRadians(DevicePitch);
    const FVector Offset(DeviceDistance*FMath::Cos(Pitch)*FMath::Cos(Yaw),DeviceDistance*FMath::Cos(Pitch)*FMath::Sin(Yaw),DeviceDistance*FMath::Sin(Pitch));
    DeviceCamera->SetActorLocation(Center+Offset);DeviceCamera->SetActorRotation((-Offset).Rotation());
    if(PreviewBackdrop){PreviewBackdrop->SetWorldLocation(Center-Offset.GetSafeNormal()*110);PreviewBackdrop->SetWorldRotation(Offset.Rotation());}
}
void AClaraRoomController::OrbitDevice(float X,float Y)
{
    if(!bDeviceDragging)return;
    if(FMath::Abs(X)+FMath::Abs(Y)>.05f)bDeviceMoved=true;
    DeviceYaw-=X*1.4f;DevicePitch=FMath::Clamp(DevicePitch-Y*1.4f,-65.f,75.f);UpdateDeviceCamera();
}
void AClaraRoomController::ZoomDevice(float Amount)
{DeviceDistance=FMath::Clamp(DeviceDistance+Amount,32.f,260.f);UpdateDeviceCamera();}
void AClaraRoomController::Tick(float Delta)
{
    Super::Tick(Delta);
    if(!DevicePreview)return;
    FHitResult Hit;HoveredDevicePort.Empty();
    if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)&&Hit.GetActor()==DevicePreview)
        HoveredDevicePort=DevicePreview->PortIdForComponent(Hit.GetComponent());
}
void AClaraRoomController::ClickDevicePort()
{
    if(!DevicePreview||!DeviceSource.IsValid())return;
    FHitResult Hit;
    if(!GetHitResultUnderCursor(ECC_Visibility,false,Hit)||Hit.GetActor()!=DevicePreview)return;
    SelectDevicePort(DevicePreview->PortIdForComponent(Hit.GetComponent()));
}
void AClaraRoomController::SelectDevicePort(const FString& Id)
{
    if(!DevicePreview||!DeviceSource.IsValid()||Id.IsEmpty()||DeviceSource->GetEffectivePortKind(Id).IsEmpty())return;
    AClaraNetworkDevice* Source=DeviceSource.Get();FString Error;
    if(GetCableKind()==TEXT("sfp_module"))
    {
        DeviceMessage=Source->InstallOpticalModule(Id)?TEXT("Modulo SFP inserito. Ora scegli il cavo in fibra e collega una porta ottica."):TEXT("Il modulo si inserisce soltanto in uno slot SFP libero.");
        return;
    }
    if(Source->IsPortOccupied(Id))
    {
        Source->DisconnectPort(Id,Error);
        DeviceMessage=Error.IsEmpty()?TEXT("Connettore sganciato. Entrambe le porte sono libere."):Error;
        return;
    }
    const FString Kind=Source->GetEffectivePortKind(Id);
    const bool CanStart=(GetCableKind()==TEXT("ethernet")&&Kind==TEXT("ethernet"))
        ||(GetCableKind()==TEXT("console")&&(Kind==TEXT("console")||Kind==TEXT("console_host")))
        ||(GetCableKind()==TEXT("fiber")&&Kind==TEXT("fiber"));
    if(!CanStart)
    {
        DeviceMessage=GetCableKind()==TEXT("fiber")?TEXT("Serve una porta ottica. Inserisci prima il modulo negli slot SFP."):
            (GetCableKind()==TEXT("console")?TEXT("La console va fra USB console dedicata del portatile e CONSOLE dello switch; non una USB di memoria."):TEXT("Ethernet richiede una porta dati Ethernet. Leggi etichetta e funzione della presa."));
        return;
    }
    if(!PendingDevice.IsValid())
    {
        PendingDevice=Source;PendingDevicePort=Id;DevicePreview->SetPendingPort(Id,GetCableKind());
        DeviceMessage=TEXT("Prima estremita' scelta. Apri l'altro apparato e clicca una porta Ethernet libera.");return;
    }
    if(PendingDevice.Get()==Source&&PendingDevicePort==Id)
    {
        PendingDevice.Reset();PendingDevicePort.Empty();DevicePreview->SetPendingPort(FString());
        DeviceMessage=TEXT("Estremita' annullata. Puoi scegliere un'altra porta.");return;
    }
    if(AClaraNetworkDevice::ConnectPorts(PendingDevice.Get(),PendingDevicePort,Source,Id,GetCableKind(),Error))
    {
        PendingDevice.Reset();PendingDevicePort.Empty();DevicePreview->SetPendingPort(FString());
        DeviceMessage=TEXT("Collegamento fisico completato. La verifica della rete sara' il passo successivo.");
    }
    else DeviceMessage=Error;
}
void AClaraRoomController::BuildDevicePanel()
{
    if(DevicePanel.IsValid()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DevicePanel.ToSharedRef());
    TWeakObjectPtr<AClaraRoomController> Weak(this);
    const FLinearColor TextColor(.88,.91,.92),Muted(.47,.55,.59),Surface(.025,.036,.045,.96);
    static const FButtonStyle Flat=FButtonStyle().SetNormal(FSlateColorBrush(FLinearColor::White)).SetHovered(FSlateColorBrush(FLinearColor(.7,.85,.83))).SetPressed(FSlateColorBrush(FLinearColor(.5,.7,.68)));
    auto Label=[&](const TCHAR* Name){return SNew(STextBlock).Text(FText::FromString(Name)).Font(FCoreStyle::GetDefaultFontStyle("Bold",11)).ColorAndOpacity(Muted);};
    auto Button=[&](const TCHAR* Name,TFunction<void(AClaraRoomController*)> Action)
    {return SNew(SButton).ButtonStyle(&Flat).ButtonColorAndOpacity(FLinearColor(.09,.13,.15)).IsFocusable(false).ContentPadding(FMargin(12,9))
        .OnClicked_Lambda([Weak,Action]{if(Weak.IsValid())Action(Weak.Get());return FReply::Handled();})[
            SNew(STextBlock).Text(FText::FromString(Name)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).ColorAndOpacity(FLinearColor(.88,.91,.92))];};
    auto Select=[](AClaraRoomController* PC,const FString& Model)
    {AClaraNetworkDevice* Found=nullptr;for(TActorIterator<AClaraNetworkDevice> It(PC->GetWorld());It;++It)if(!It->IsPreview()&&It->GetModelId()==Model){Found=*It;if(It->ActorHasTag(TEXT("BenchDevice")))break;}if(Found)PC->OpenDevice(Found);};
    auto Side=SNew(SVerticalBox);
    Side->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("APPARATO"))];
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("Switch"),TEXT("Cisco2960X")},{TEXT("Router"),TEXT("Cisco1941")},{TEXT("Portatile"),TEXT("Laptop")},{TEXT("PC 2"),TEXT("PC2")},{TEXT("Distribuzione ottica"),TEXT("FiberUplink")}})
        Side->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(*Pair.Key,[Select,Model=Pair.Value](auto* PC){Select(PC,Model);})];
    Side->AddSlot().AutoHeight().Padding(0,23,0,12)[Label(TEXT("CAVO / MODULO"))];
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("Ethernet"),TEXT("ethernet")},{TEXT("Console USB"),TEXT("console")},{TEXT("Fibra LC"),TEXT("fiber")},{TEXT("Modulo SFP"),TEXT("sfp_module")}})
        Side->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(*Pair.Key,[Kind=Pair.Value](auto* PC){PC->SetCableKind(Kind);})];
    Side->AddSlot().AutoHeight().Padding(0,20,0,10)[Label(TEXT("VISTA"))];
    Side->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("Fronte"),[](auto* PC){PC->DeviceYaw=-90;PC->DevicePitch=12;PC->UpdateDeviceCamera();})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Retro"),[](auto* PC){PC->DeviceYaw=90;PC->DevicePitch=12;PC->UpdateDeviceCamera();})]];
    Side->AddSlot().AutoHeight().Padding(0,12,0,0)[Button(TEXT("Annulla selezione"),[](auto* PC){PC->PendingDevice.Reset();PC->PendingDevicePort.Empty();if(PC->DevicePreview)PC->DevicePreview->SetPendingPort(FString());PC->DeviceMessage.Empty();})];
    DevicePanel=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        +SOverlay::Slot().VAlign(VAlign_Top).Padding(0)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(FMargin(24,15))[
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",19)).ColorAndOpacity(TextColor)
                .Text_Lambda([Weak]{return FText::FromString(Weak.IsValid()&&Weak->DeviceSource.IsValid()?Weak->DeviceSource->GetDisplayName():FString());})]
            +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Chiudi · Esc"),[](auto* PC){PC->CloseDevice();})]]]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(20,80,0,75)[SNew(SBox).WidthOverride(195)[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(15)[SNew(SScrollBox)+SScrollBox::Slot()[Side]]]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(235,0,30,22)[SNew(SBox).MaxDesiredWidth(790)[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(FMargin(18,12))[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).ColorAndOpacity(TextColor).AutoWrapText(true)
                    .Text_Lambda([Weak]{if(!Weak.IsValid()||!Weak->DevicePreview)return FText::GetEmpty();return FText::FromString(Weak->DevicePreview->GetPortStatus(Weak->HoveredDevicePort));})]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Muted).AutoWrapText(true)
                    .Text_Lambda([Weak]{if(!Weak.IsValid())return FText::GetEmpty();FString T=Weak->PendingDevice.IsValid()?TEXT("Primo capo: ")+Weak->PendingDevicePort+TEXT(" · scegli il secondo apparato"):TEXT("Trascina per ruotare · rotella per zoom · clic sulla presa");if(!Weak->DeviceMessage.IsEmpty())T+=TEXT("\n")+Weak->DeviceMessage;return FText::FromString(T);})]
            ]]];
    if(GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->AddViewportWidgetContent(DevicePanel.ToSharedRef(),30);
}

void AClaraRoomController::SetCableKind(const FString& Kind)
{
    if(Kind!=TEXT("ethernet")&&Kind!=TEXT("console")&&Kind!=TEXT("fiber")&&Kind!=TEXT("sfp_module"))return;
    if(DeviceSource.IsValid()&&DeviceSource->GetModelId()==TEXT("Laptop"))
    {DeviceYaw=Kind==TEXT("console")?0.f:180.f;DevicePitch=12;UpdateDeviceCamera();}
    if(CableKind==Kind)
    {DeviceMessage=PendingDevice.IsValid()?TEXT("Primo capo mantenuto. Scegli la presa sul secondo apparato."):TEXT("Cavo pronto. Clicca la prima presa.");return;}
    CableKind=Kind;PendingDevice.Reset();PendingDevicePort.Empty();
    if(DevicePreview)DevicePreview->SetPendingPort(FString(),Kind);
    DeviceMessage=Kind==TEXT("sfp_module")?TEXT("Modulo SFP selezionato: clicca uno slot SFP libero."):TEXT("Cavo selezionato: ")+Kind+TEXT(". Scegli le due estremita'.");
}
