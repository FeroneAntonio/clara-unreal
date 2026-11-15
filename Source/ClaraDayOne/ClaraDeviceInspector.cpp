#include "ClaraRoomVisitor.h"
#include "ClaraNetworkDevice.h"
#include "ClaraOfficeLayout.h"
#include "ClaraStoryDirector.h"
#include "ClaraNarrativeCamera.h"
#include "ClaraAudio.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
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
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
bool CanAttachCable(const FString& Cable, const FString& PortKind)
{
    return (Cable==TEXT("ethernet")&&PortKind==TEXT("ethernet"))
        ||(Cable==TEXT("console")&&(PortKind==TEXT("console")||PortKind==TEXT("console_host")))
        ||(Cable==TEXT("fiber")&&PortKind==TEXT("fiber"));
}

FString DeviceCableName(const FString& Kind)
{
    return Kind==TEXT("console")?TEXT("cavo console"):Kind==TEXT("fiber")?TEXT("fibra")
        :Kind==TEXT("sfp_module")?TEXT("modulo SFP"):TEXT("Cat 6");
}

float DevicePortYaw(const AClaraNetworkDevice* Device, const FString& Cable)
{
    if(!Device)return -90.f;
    const auto& Ports=Device->GetPorts();
    const FClaraDevicePort* Target=Ports.IsEmpty()?nullptr:&Ports[0];
    for(const auto& Port:Ports)
        if(CanAttachCable(Cable,Device->GetEffectivePortKind(Port.Id))
            ||(Cable==TEXT("sfp_module")&&Port.Kind==TEXT("sfp")))
        {Target=&Port;break;}
    return Target?FMath::RadiansToDegrees(FMath::Atan2(Target->Normal.Y,Target->Normal.X)):-90.f;
}

struct FDeviceFrame
{
    float TanX, TanY;
    FVector2D Center, HalfSize;
};

FDeviceFrame DeviceFrame(int32 Width,int32 Height,float FOV)
{
    if(Width<=0||Height<=0){Width=1280;Height=800;}
    // Leave the sidebar, title and multiline port feedback outside the fitted model.
    const float Left=FMath::Min(280.f,Width*.32f),Right=Width-32.f;
    const float Top=FMath::Min(90.f,Height*.15f),Bottom=Height-FMath::Min(180.f,Height*.30f);
    const float TanX=FMath::Tan(FMath::DegreesToRadians(FOV*.5f));
    return {TanX,TanX*Height/Width,
        FVector2D((Left+Right)/Width-1.f,1.f-(Top+Bottom)/Height),
        FVector2D((Right-Left)/Width,(Bottom-Top)/Height)};
}

FDeviceFrame DeviceFrame(const APlayerController* Controller,float FOV)
{
    int32 Width=0,Height=0;Controller->GetViewportSize(Width,Height);return DeviceFrame(Width,Height,FOV);
}

float FitDeviceBounds(const FVector& Size,const FDeviceFrame& Frame,float Yaw,float Pitch)
{
    const FVector Away=FRotator(Pitch,Yaw,0).Vector();
    const FRotationMatrix Look((-Away).Rotation());
    const FVector Right=Look.GetUnitAxis(EAxis::Y),Up=Look.GetUnitAxis(EAxis::Z);
    const FVector Extent=Size*.5+FVector(2);
    float Distance=32.f;
    // Fit all corners in perspective, including depth and the off-centre camera target.
    for(int32 X:{-1,1})for(int32 Y:{-1,1})for(int32 Z:{-1,1})
    {
        const FVector Corner=Extent*FVector(X,Y,Z);
        const float Depth=FVector::DotProduct(Corner,Away);
        Distance=FMath::Max(Distance,Depth+static_cast<float>(FMath::Abs(FVector::DotProduct(Corner,Right)/Frame.TanX+Depth*Frame.Center.X)/Frame.HalfSize.X));
        Distance=FMath::Max(Distance,Depth+static_cast<float>(FMath::Abs(FVector::DotProduct(Corner,Up)/Frame.TanY+Depth*Frame.Center.Y)/Frame.HalfSize.Y));
    }
    return Distance+5.f;
}

float FittedDeviceDistance(const APlayerController* Controller,const AClaraNetworkDevice* Device,float Yaw,float Pitch)
{
    return Device?FitDeviceBounds(Device->GetDeviceSize(),DeviceFrame(Controller,45.f),Yaw,Pitch):90.f;
}

FString DevicePortDescription(const AClaraNetworkDevice* Device, const FString& Id)
{
    if(!Device)return FString();
    const auto* Port=Device->GetPorts().FindByPredicate([&](const auto& P){return P.Id==Id;});
    if(!Port)return TEXT("Punta una presa per vedere nome e compatibilità");
    const FString Kind=Device->GetEffectivePortKind(Id);
    const FString Type=Kind==TEXT("ethernet")?TEXT("Ethernet RJ-45"):Kind==TEXT("console_host")?TEXT("USB console")
        :Kind==TEXT("console")?TEXT("console"):Kind==TEXT("fiber")?TEXT("porta ottica"):Kind==TEXT("sfp")?TEXT("slot SFP senza modulo")
        :Kind==TEXT("power")?TEXT("alimentazione"):Kind==TEXT("usb")?TEXT("USB"):Kind==TEXT("aux")?TEXT("AUX"):Kind;
    FString PeerPort;const auto* Peer=Device->GetPeerForPort(Id,PeerPort);
    FString Text=Port->Label+(Port->Label==Id?FString():TEXT(" (")+Id+TEXT(")"))+TEXT(" · ")+Type;
    Text+=Peer?TEXT(" · collegata a ")+Peer->GetDisplayName()+TEXT(" / ")+PeerPort:TEXT(" · libera");
    const int32 Led=Device->GetLedState(Id);
    if(Led==1)Text+=TEXT(" · LED ambra: attesa");else if(Led>=2)Text+=TEXT(" · LED verde");
    return Text;
}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraInspectorFramingTest,"CLARA.Inspector.Framing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraInspectorFramingTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> Sizes={{44.5,27.94,4.45},{34.3,29.8,8.9},{33,24,23.1},{33,24,2},{20.5,41.65,47.4}};
    for(const FIntPoint Viewport:{FIntPoint(1280,800),FIntPoint(1440,900),FIntPoint(1920,1080),FIntPoint(2560,1080)})
    for(const FVector& Size:Sizes)for(const float Yaw:{-90.f,90.f,0.f,180.f})
    {
        const FDeviceFrame Frame=DeviceFrame(Viewport.X,Viewport.Y,45.f);
        const float Distance=FitDeviceBounds(Size,Frame,Yaw,12.f);
        const FVector Away=FRotator(12,Yaw,0).Vector();const FRotationMatrix Look((-Away).Rotation());
        const FVector Right=Look.GetUnitAxis(EAxis::Y),Up=Look.GetUnitAxis(EAxis::Z);
        const FVector Camera=Away*Distance-Right*(Distance*Frame.TanX*Frame.Center.X)-Up*(Distance*Frame.TanY*Frame.Center.Y);
        for(int32 X:{-1,1})for(int32 Y:{-1,1})for(int32 Z:{-1,1})
        {
            const FVector ToCorner=(Size*.5+FVector(2))*FVector(X,Y,Z)-Camera;
            const double Depth=FVector::DotProduct(ToCorner,-Away);
            const double PixelX=Viewport.X*.5*(1+FVector::DotProduct(ToCorner,Right)/(Depth*Frame.TanX));
            const double PixelY=Viewport.Y*.5*(1-FVector::DotProduct(ToCorner,Up)/(Depth*Frame.TanY));
            if(Depth<=0||PixelX<280||PixelX>Viewport.X-32||PixelY<90||PixelY>Viewport.Y-180)
            {AddError(FString::Printf(TEXT("Apparatus clipped: %dx%d, size %s, yaw %.0f"),Viewport.X,Viewport.Y,*Size.ToString(),Yaw));return false;}
        }
    }
    return true;
}
#endif

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
    // The desktop and the physical inspector cannot own input at the same time.
    if(auto* Story=GetStory())Story->CloseLaptopScreen();
    if(auto* Camera=FindComponentByClass<UClaraNarrativeCamera>())Camera->EndConversation(true);
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
    if(BackMaterial){BackMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor(.008,.012,.018));BackMaterial->SetVectorParameterValue(TEXT("BaseColor"),FLinearColor(.008,.012,.018));PreviewBackdrop->SetMaterial(0,BackMaterial);}
    PreviewBackdrop->SetRelativeScale3D({.02,18,13});PreviewBackdrop->RegisterComponent();
    if(!DeviceCamera)DeviceCamera=GetWorld()->SpawnActor<ACameraActor>();
    DeviceYaw=DevicePortYaw(DevicePreview,GetCableKind());DevicePitch=12;
    DeviceDistance=FittedDeviceDistance(this,DevicePreview,DeviceYaw,DevicePitch);
    DeviceCamera->GetCameraComponent()->SetFieldOfView(45);
    DeviceCamera->GetCameraComponent()->bConstrainAspectRatio=false;
    DeviceCamera->GetCameraComponent()->bOverrideAspectRatioAxisConstraint=true;
    DeviceCamera->GetCameraComponent()->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
    bDeviceDragging=false;HoveredDevicePort.Empty();DeviceMessage.Empty();ActiveView=-1;
    for(TActorIterator<AClaraOfficeLayout> It(GetWorld());It;++It)It->ShowPlan(false);
    if(auto* Character=Cast<ACharacter>(GetPawn()))Character->GetCharacterMovement()->StopMovementImmediately();
    if(Toolbar.IsValid())Toolbar->SetVisibility(EVisibility::Collapsed);
    UpdateDeviceCamera();SetViewTarget(DeviceCamera);Pointer();BuildDevicePanel();AClaraAudio::Cue(this,TEXT("open"),.55f);
}
void AClaraRoomController::CloseDevice()
{
    if(DevicePanel.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DevicePanel.ToSharedRef());
    DevicePanel.Reset();
    if(DevicePreview)AClaraAudio::Cue(this,TEXT("close"),.5f);
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
    const FRotator Look = (-Offset).Rotation();
    const FVector Right = FRotationMatrix(Look).GetUnitAxis(EAxis::Y);
    const FVector Up = FRotationMatrix(Look).GetUnitAxis(EAxis::Z);
    const FDeviceFrame Frame=DeviceFrame(this,DeviceCamera->GetCameraComponent()->FieldOfView);
    DeviceCamera->SetActorLocation(Center+Offset-Right*(DeviceDistance*Frame.TanX*Frame.Center.X)-Up*(DeviceDistance*Frame.TanY*Frame.Center.Y));
    DeviceCamera->SetActorRotation(Look);
    if(PreviewBackdrop){PreviewBackdrop->SetWorldLocation(Center-Offset.GetSafeNormal()*110);PreviewBackdrop->SetWorldRotation(Offset.Rotation());}
}
void AClaraRoomController::OrbitDevice(float X,float Y)
{
    if(!bDeviceDragging)return;
    // Un clic con la mano non perfettamente ferma resta un clic: si ruota solo dopo un trascinamento vero.
    DeviceDragTravel+=FMath::Abs(X)+FMath::Abs(Y);
    if(DeviceDragTravel<.45f)return;
    bDeviceMoved=true;
    DeviceYaw-=X*1.4f;DevicePitch=FMath::Clamp(DevicePitch-Y*1.4f,-65.f,75.f);UpdateDeviceCamera();
}
void AClaraRoomController::ZoomDevice(float Amount)
{DeviceDistance=FMath::Clamp(DeviceDistance+Amount,32.f,600.f);UpdateDeviceCamera();}
void AClaraRoomController::Tick(float Delta)
{
    Super::Tick(Delta);
    // Passo rapido letto dallo stato del tasto: un rilascio avvenuto dentro un pannello non lascia la corsa attiva.
    if(auto* Walker=Cast<ACharacter>(GetPawn()))Walker->GetCharacterMovement()->MaxWalkSpeed=(IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift))?480.f:330.f;
    if(!DevicePreview)return;
    FHitResult Hit;HoveredDevicePort.Empty();
    if(GetHitResultUnderCursor(ECC_Visibility,false,Hit)&&Hit.GetActor()==DevicePreview)
        HoveredDevicePort=DevicePreview->PortIdForComponent(Hit.GetComponent());
}
void AClaraRoomController::ClickDevicePort()
{
    float X=0,Y=0;if(GetMousePosition(X,Y))SelectDevicePortAt(FVector2D(X,Y));
}
bool AClaraRoomController::SelectDevicePortAt(const FVector2D& Position)
{
    if(!DevicePreview||!DeviceSource.IsValid())return false;
    FHitResult Hit;
    if(!GetHitResultAtScreenPosition(Position,ECC_Visibility,false,Hit)||Hit.GetActor()!=DevicePreview)return false;
    const FString Id=DevicePreview->PortIdForComponent(Hit.GetComponent());if(Id.IsEmpty())return false;
    SelectDevicePort(Id);return true;
}
bool AClaraRoomController::DevProbeDevicePort(const FString& Id,bool bSelect)
{
    if(!DevicePreview||!DeviceCamera||DevicePreview->GetEffectivePortKind(Id).IsEmpty())return false;
    const FVector Point=DevicePreview->GetPortWorldLocation(Id)+DevicePreview->GetPortWorldNormal(Id)*.6f;
    if(FVector::DotProduct(DevicePreview->GetPortWorldNormal(Id),DeviceCamera->GetActorLocation()-Point)<=0)return false;
    FVector2D Pixel;
    if(!ProjectWorldLocationToScreen(Point,Pixel,false))return false;
    int32 Width=0,Height=0;GetViewportSize(Width,Height);
    if(Pixel.X<280||Pixel.X>Width-32||Pixel.Y<90||Pixel.Y>Height-180)
    {UE_LOG(LogTemp,Error,TEXT("CLARA_PORT_PICK_FAIL %s: outside clear viewport at %.1f,%.1f"),*Id,Pixel.X,Pixel.Y);return false;}
    FHitResult Hit;
    const bool HitPort=GetHitResultAtScreenPosition(Pixel,ECC_Visibility,false,Hit)&&Hit.GetActor()==DevicePreview
        &&DevicePreview->PortIdForComponent(Hit.GetComponent())==Id;
    UE_LOG(LogTemp,Display,TEXT("CLARA_PORT_PICK_%s %s at %.1f,%.1f hit %s"),HitPort?TEXT("PASS"):TEXT("FAIL"),*Id,Pixel.X,Pixel.Y,*GetNameSafe(Hit.GetComponent()));
    return HitPort&&(!bSelect||SelectDevicePortAt(Pixel));
}
bool AClaraRoomController::DevCheckDeviceDrag()
{
    if(!DevicePreview||!DeviceSource.IsValid()||!GetPawn()||!GetPawn()->InputComponent)return false;
    const float StartYaw=DeviceYaw,StartPitch=DevicePitch;
    const auto FirstDevice=PendingDevice;const FString FirstPort=PendingDevicePort;
    TMap<FString,TPair<AClaraNetworkDevice*,FString>> Links;
    for(const auto& Port:DeviceSource->GetPorts())
    {FString PeerPort;auto* Peer=DeviceSource->GetPeerForPort(Port.Id,PeerPort);Links.Add(Port.Id,{Peer,PeerPort});}
    InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,IE_Pressed,1.f));
    for(auto& Axis:GetPawn()->InputComponent->AxisBindings)
    {
        if(Axis.AxisName==TEXT("Turn"))Axis.AxisDelegate.Execute(20.f);
        if(Axis.AxisName==TEXT("LookUp"))Axis.AxisDelegate.Execute(-5.f);
    }
    InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,IE_Released,0.f));
    bool Passed=!FMath::IsNearlyEqual(DeviceYaw,StartYaw)&&!FMath::IsNearlyEqual(DevicePitch,StartPitch)
        &&!bDeviceDragging&&GetViewTarget()==DeviceCamera&&PendingDevice==FirstDevice&&PendingDevicePort==FirstPort;
    for(const auto& Link:Links)
    {FString PeerPort;auto* Peer=DeviceSource->GetPeerForPort(Link.Key,PeerPort);Passed&=Peer==Link.Value.Key&&PeerPort==Link.Value.Value;}
    DeviceYaw=StartYaw;DevicePitch=StartPitch;UpdateDeviceCamera();
    UE_LOG(LogTemp,Display,TEXT("CLARA_DEVICE_DRAG_%s"),Passed?TEXT("PASS"):TEXT("FAIL"));return Passed;
}
bool AClaraRoomController::StoryGuardsWiring() const
{return GetStory()&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraDeviceView"));}
void AClaraRoomController::ChooseCable(const FString& Kind)
{
    if(StoryGuardsWiring())
    {
        AClaraStoryDirector* Story=GetStory();
        if(Story->IsCableTest()){DeviceMessage=TEXT("Hai in mano un cavo da provare: clicca una presa.");return;}
        if(!Story->HasItem(Kind)){DeviceMessage=TEXT("Questo materiale non è ancora nella tua borsa: prima va riconosciuto e riposto nella sua scatola.");AClaraAudio::Cue(this,TEXT("error"),.5f);return;}
    }
    SetCableKind(Kind);AClaraAudio::Cue(this,TEXT("click"),.5f);
}
void AClaraRoomController::SetDeviceView(float Yaw,float Pitch,float Distance)
{DeviceYaw=Yaw;DevicePitch=FMath::Clamp(Pitch,-65.f,75.f);DeviceDistance=FMath::Clamp(Distance,32.f,600.f);UpdateDeviceCamera();}
void AClaraRoomController::SelectDevicePort(const FString& Id)
{
    if(!DevicePreview||!DeviceSource.IsValid()||Id.IsEmpty()||DeviceSource->GetEffectivePortKind(Id).IsEmpty())return;
    AClaraNetworkDevice* Source=DeviceSource.Get();FString Error;
    if(StoryGuardsWiring())
    {
        // Scena dei cavi: il cavo che si ha in mano si prova sulla presa, senza lasciarlo collegato.
        AClaraStoryDirector* Story=GetStory();
        if(Story->IsCableTest()){DeviceMessage=Story->TestHeldCable(Source,Id);return;}
        if(Story->GetPhase()<5){DeviceMessage=TEXT("Prima metti in ordine il tavolo dei cavi: prendi un cavo e provalo su una presa.");AClaraAudio::Cue(this,TEXT("error"),.5f);return;}
    }
    if(GetCableKind()==TEXT("sfp_module"))
    {
        if(Source->HasOpticalModule(Id)){DeviceMessage=TEXT("Il modulo SFP è già presente. Scegli la fibra per collegare la porta ottica.");return;}
        const bool Installed=Source->InstallOpticalModule(Id);AClaraAudio::Cue(this,Installed?TEXT("plug"):TEXT("error"),Installed?.8f:.5f);
        DeviceMessage=Installed?TEXT("Modulo SFP inserito. Ora scegli la fibra e collega questa porta al rack della fibra."):TEXT("Il modulo si inserisce soltanto in uno slot SFP libero.");
        return;
    }
    if(Source->IsPortOccupied(Id)&&PendingDevice.IsValid())
    {
        // Con un primo capo gia' scelto un clic su una presa occupata non stacca il cavo di qualcun altro.
        AClaraAudio::Cue(this,TEXT("error"),.5f);
        DeviceMessage=TEXT("Questa presa è già occupata. Scegli una presa libera; per scollegare un cavo premi prima «Annulla selezione».");
        return;
    }
    if(Source->IsPortOccupied(Id))
    {
        const bool Disconnected=Source->DisconnectPort(Id,Error);
        AClaraAudio::Cue(this,Disconnected?TEXT("unplug"):TEXT("error"),Disconnected?.8f:.5f);
        DeviceMessage=Disconnected?TEXT("Connettore sganciato. Entrambe le porte sono libere."):Error;
        return;
    }
    const FString Kind=Source->GetEffectivePortKind(Id);
    const bool CanStart=CanAttachCable(GetCableKind(),Kind);
    if(!CanStart)
    {
        AClaraAudio::Cue(this,TEXT("error"),.5f);
        DeviceMessage=GetCableKind()==TEXT("fiber")?(Kind==TEXT("sfp")?TEXT("Non entra ancora: nello slot SFP serve prima il modulo ottico."):TEXT("Non entra: la fibra vuole una porta ottica."))
            :GetCableKind()==TEXT("console")?(Kind==TEXT("ethernet")?TEXT("Nessun segnale: una porta dati non è una console. Il cavo console va tra la presa USB console del portatile e la porta CONSOLE."):TEXT("Non entra. Il cavo console va tra la presa USB console del portatile e la porta CONSOLE."))
            :(Kind==TEXT("console")?TEXT("Nessun segnale: la porta CONSOLE non porta traffico di rete. Il Cat 6 va in una porta dati."):TEXT("Non entra: il Cat 6 va in una porta dati RJ-45."));
        return;
    }
    if(!PendingDevice.IsValid())
    {
        PendingDevice=Source;PendingDevicePort=Id;DevicePreview->SetPendingPort(Id,GetCableKind());AClaraAudio::Cue(this,TEXT("click"),.6f);
        DeviceMessage=GetCableKind()==TEXT("console")?TEXT("Primo capo scelto. Ora apri l'altro apparato dal pannello a sinistra e clicca la presa console corrispondente.")
            :GetCableKind()==TEXT("fiber")?TEXT("Primo capo scelto. Ora apri l'altro apparato dal pannello a sinistra e clicca la sua porta ottica.")
            :TEXT("Primo capo scelto. Ora apri l'altro apparato dal pannello a sinistra e clicca una porta Ethernet libera.");return;
    }
    if(PendingDevice.Get()==Source&&PendingDevicePort==Id)
    {
        PendingDevice.Reset();PendingDevicePort.Empty();DevicePreview->SetPendingPort(FString());
        DeviceMessage=TEXT("Estremità annullata. Puoi scegliere un'altra porta.");return;
    }
    if(AClaraNetworkDevice::ConnectPorts(PendingDevice.Get(),PendingDevicePort,Source,Id,GetCableKind(),Error))
    {
        PendingDevice.Reset();PendingDevicePort.Empty();DevicePreview->SetPendingPort(FString());AClaraAudio::Cue(this,TEXT("plug"),.9f);
        DeviceMessage=GetCableKind()==TEXT("console")?TEXT("Cavo console collegato ai due capi. Chiudi con Esc e apri il portatile.")
            :TEXT("Cavo collegato ai due capi. Il LED verde indica il collegamento; sullo switch può precederlo una breve attesa in ambra.");
    }
    else {DeviceMessage=Error;AClaraAudio::Cue(this,TEXT("error"),.5f);}
}
void AClaraRoomController::BuildDevicePanel()
{
    if(DevicePanel.IsValid()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DevicePanel.ToSharedRef());
    TWeakObjectPtr<AClaraRoomController> Weak(this);
    const FLinearColor TextColor(.88,.91,.92),Muted(.47,.55,.59),Surface(.025,.036,.045,.96);
    static const FButtonStyle Flat=FButtonStyle().SetNormal(FSlateColorBrush(FLinearColor::White)).SetHovered(FSlateColorBrush(FLinearColor(.7,.85,.83))).SetPressed(FSlateColorBrush(FLinearColor(.5,.7,.68)));
    auto Label=[&](const TCHAR* Name){return SNew(STextBlock).Text(FText::FromString(Name)).Font(FCoreStyle::GetDefaultFontStyle("Medium",11)).ColorAndOpacity(Muted);};
    auto Button=[&](const TCHAR* Name,TFunction<void(AClaraRoomController*)> Action)
    {return SNew(SButton).ButtonStyle(&Flat).ButtonColorAndOpacity(FLinearColor(.09,.13,.15)).IsFocusable(false).ContentPadding(FMargin(12,9))
        .OnClicked_Lambda([Weak,Action]{if(Weak.IsValid())Action(Weak.Get());return FReply::Handled();})[
            SNew(STextBlock).Text(FText::FromString(Name)).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).ColorAndOpacity(FLinearColor(.88,.91,.92)).AutoWrapText(false)];};
    auto Select=[](AClaraRoomController* PC,const FString& Model)
    {AClaraNetworkDevice* Found=nullptr;for(TActorIterator<AClaraNetworkDevice> It(PC->GetWorld());It;++It)if(!It->IsPreview()&&It->GetModelId()==Model){Found=*It;if(It->ActorHasTag(TEXT("BenchDevice")))break;}if(Found)PC->OpenDevice(Found);};
    auto FrameDevice=[](AClaraRoomController* PC,bool Opposite)
    {
        PC->DeviceYaw=DevicePortYaw(PC->DevicePreview,PC->GetCableKind())+(Opposite?180.f:0.f);PC->DevicePitch=12;
        PC->DeviceDistance=FittedDeviceDistance(PC,PC->DevicePreview,PC->DeviceYaw,PC->DevicePitch);PC->UpdateDeviceCamera();
    };
    auto Side=SNew(SVerticalBox);
    Side->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("APPARATO"))];
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("Switch"),TEXT("Cisco2960X")},{TEXT("Router"),TEXT("Cisco1941")},{TEXT("Portatile"),TEXT("Laptop")},{TEXT("PC2"),TEXT("PC2")},{TEXT("Rack della fibra"),TEXT("FiberUplink")}})
        Side->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(*Pair.Key,[Select,Model=Pair.Value](auto* PC){Select(PC,Model);})];
    if(DeviceSource.IsValid()&&DeviceSource->GetModelId()==TEXT("Laptop"))
    {
        Side->AddSlot().AutoHeight().Padding(0,16,0,8)[Label(TEXT("LATO DEL PORTATILE"))];
        for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("Lato Ethernet"),TEXT("ethernet")},{TEXT("Lato console"),TEXT("console")}})
            Side->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(*Pair.Key,[Cable=Pair.Value](auto* PC)
            {
                PC->DeviceYaw=DevicePortYaw(PC->DevicePreview,Cable);PC->DevicePitch=12;
                PC->DeviceDistance=FittedDeviceDistance(PC,PC->DevicePreview,PC->DeviceYaw,PC->DevicePitch);PC->UpdateDeviceCamera();
            })];
    }
    Side->AddSlot().AutoHeight().Padding(0,23,0,12)[Label(TEXT("CAVO / MODULO"))];
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("Cat 6 · rete"),TEXT("ethernet")},{TEXT("Console"),TEXT("console")},{TEXT("Fibra"),TEXT("fiber")},{TEXT("Modulo SFP"),TEXT("sfp_module")}})
        Side->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(*Pair.Key,[Kind=Pair.Value](auto* PC){PC->ChooseCable(Kind);})];
    Side->AddSlot().AutoHeight().Padding(0,20,0,10)[Label(TEXT("VISTA"))];
    Side->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("Prese"),[FrameDevice](auto* PC){FrameDevice(PC,false);})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Opposto"),[FrameDevice](auto* PC){FrameDevice(PC,true);})]];
    Side->AddSlot().AutoHeight().Padding(0,5,0,0)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("Ruota SX"),[](auto* PC){PC->DeviceYaw-=30.f;PC->UpdateDeviceCamera();})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Ruota DX"),[](auto* PC){PC->DeviceYaw+=30.f;PC->UpdateDeviceCamera();})]];
    Side->AddSlot().AutoHeight().Padding(0,12,0,0)[Button(TEXT("Annulla selezione"),[](auto* PC){PC->PendingDevice.Reset();PC->PendingDevicePort.Empty();if(PC->DevicePreview)PC->DevicePreview->SetPendingPort(FString());PC->DeviceMessage.Empty();})];
    DevicePanel=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        +SOverlay::Slot().VAlign(VAlign_Top).Padding(0)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(FMargin(24,15))[
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Medium",19)).ColorAndOpacity(TextColor)
                .Text_Lambda([Weak]{return FText::FromString(Weak.IsValid()&&Weak->DeviceSource.IsValid()?Weak->DeviceSource->GetDisplayName():FString());})]
            +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Chiudi · Esc"),[](auto* PC){PC->CloseDevice();})]]]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(20,80,0,75)[SNew(SBox).WidthOverride(226)[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(15)[SNew(SScrollBox)+SScrollBox::Slot()[Side]]]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(266,0,30,22)[SNew(SBox).MaxDesiredWidth(790)[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(FMargin(18,12))[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).ColorAndOpacity(TextColor).AutoWrapText(true)
                    .Text_Lambda([Weak]{return Weak.IsValid()?FText::FromString(DevicePortDescription(Weak->DeviceSource.Get(),Weak->HoveredDevicePort)):FText::GetEmpty();})]
                +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(TextColor).AutoWrapText(true)
                    .Text_Lambda([Weak]
                    {
                        if(!Weak.IsValid()||!Weak->DeviceSource.IsValid())return FText::GetEmpty();
                        const auto* Source=Weak->DeviceSource.Get();const FString Id=Weak->HoveredDevicePort,Kind=Source->GetEffectivePortKind(Id);
                        FString Hint=TEXT("Trascina per ruotare · rotella per zoom · Prese ripristina l'inquadratura");
                        if(!Id.IsEmpty())
                        {
                            const auto* Story=Weak->GetStory();
                            if(Weak->StoryGuardsWiring()&&Story->IsCableTest())Hint=TEXT("Clic per provare il cavo in mano, senza lasciarlo collegato.");
                            else if(Weak->StoryGuardsWiring()&&Story->GetPhase()<5)Hint=TEXT("Prima completa il riconoscimento dei cavi sul tavolo.");
                            else if(Weak->CableKind==TEXT("sfp_module"))Hint=Source->HasOpticalModule(Id)?TEXT("Modulo già presente: scegli la fibra.")
                                :(Kind==TEXT("sfp")&&!Source->IsPortOccupied(Id)?TEXT("Clic per inserire il modulo SFP."):TEXT("Il modulo richiede uno slot SFP libero."));
                            else if(Source->IsPortOccupied(Id))Hint=Weak->PendingDevice.IsValid()?TEXT("Presa occupata: scegli una porta libera. Il collegamento esistente resta al suo posto.")
                                :TEXT("Clic per scollegare questo cavo da entrambi gli apparati.");
                            else if(Weak->PendingDevice.Get()==Source&&Weak->PendingDevicePort==Id)Hint=TEXT("Clic per annullare il primo capo.");
                            else if(Weak->PendingDevice.Get()==Source)Hint=TEXT("Il secondo capo va su un altro apparato: aprilo dal pannello a sinistra.");
                            else if(Weak->PendingDevice.IsValid())Hint=AClaraNetworkDevice::IsCableCompatible(Weak->CableKind,Weak->PendingDevice->GetEffectivePortKind(Weak->PendingDevicePort),Kind)
                                ?TEXT("Presa compatibile: clic per collegare il secondo capo."):TEXT("Presa incompatibile con il primo capo e il cavo scelto.");
                            else if(CanAttachCable(Weak->CableKind,Kind))Hint=TEXT("Presa compatibile: clic per scegliere il primo capo.");
                            else Hint=Weak->CableKind==TEXT("fiber")&&Kind==TEXT("sfp")?TEXT("Prima inserisci un modulo SFP, poi scegli la fibra."):TEXT("Presa incompatibile con il cavo in mano.");
                        }
                        return FText::FromString(Hint);
                    })]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Muted).AutoWrapText(true)
                    .Text_Lambda([Weak]{if(!Weak.IsValid())return FText::GetEmpty();
                        FString T=TEXT("In mano: ")+DeviceCableName(Weak->CableKind);
                        if(Weak->PendingDevice.IsValid())T+=TEXT(" · Capo 1: ")+Weak->PendingDevice->GetDisplayName()+TEXT(" / ")+Weak->PendingDevicePort+TEXT(". Scegli il secondo apparato.");
                        if(!Weak->DeviceMessage.IsEmpty())T+=TEXT("\n")+Weak->DeviceMessage;return FText::FromString(T);})]
            ]]];
    if(GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->AddViewportWidgetContent(DevicePanel.ToSharedRef(),30);
}

void AClaraRoomController::SetCableKind(const FString& Kind)
{
    if(Kind!=TEXT("ethernet")&&Kind!=TEXT("console")&&Kind!=TEXT("fiber")&&Kind!=TEXT("sfp_module"))return;
    if(DeviceSource.IsValid()&&DeviceSource->GetModelId()==TEXT("Laptop"))
    {DeviceYaw=DevicePortYaw(DevicePreview,Kind);DevicePitch=12;DeviceDistance=FittedDeviceDistance(this,DevicePreview,DeviceYaw,DevicePitch);UpdateDeviceCamera();}
    if(CableKind==Kind)
    {DeviceMessage=PendingDevice.IsValid()?TEXT("Primo capo mantenuto. Scegli la presa sul secondo apparato."):TEXT("Cavo pronto. Clicca la prima presa.");return;}
    CableKind=Kind;PendingDevice.Reset();PendingDevicePort.Empty();
    if(DevicePreview)DevicePreview->SetPendingPort(FString(),Kind);
    DeviceMessage=Kind==TEXT("sfp_module")?TEXT("Modulo SFP selezionato: clicca uno slot SFP libero."):TEXT("Cavo selezionato: ")+DeviceCableName(Kind)+TEXT(". Scegli il primo capo.");
}
