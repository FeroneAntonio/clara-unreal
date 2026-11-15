#include "ClaraRoomVisitor.h"
#include "ClaraFrontEnd.h"
#include "ClaraNarrativeCamera.h"
#include "ClaraOfficeLayout.h"
#include "ClaraNetworkDevice.h"
#include "ClaraStoryDirector.h"
#include "ClaraAudio.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AClaraRoomVisitor::AClaraRoomVisitor()
{
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(28,88);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("VisitorCamera"));
    Camera->SetupAttachment(GetCapsuleComponent()); Camera->SetRelativeLocation({0,0,66});
    Camera->bUsePawnControlRotation=true; Camera->SetFieldOfView(85);
    GetCharacterMovement()->MaxWalkSpeed=330;
    GetCharacterMovement()->MaxAcceleration=3200;
    GetCharacterMovement()->BrakingDecelerationWalking=2600;
    GetCharacterMovement()->GroundFriction=10;
    bUseControllerRotationYaw=true;
}
void AClaraRoomVisitor::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"),this,&AClaraRoomVisitor::Forward);
    Input->BindAxis(TEXT("MoveRight"),this,&AClaraRoomVisitor::Right);
    Input->BindAxis(TEXT("Turn"),this,&AClaraRoomVisitor::Turn);
    Input->BindAxis(TEXT("LookUp"),this,&AClaraRoomVisitor::Look);
}
void AClaraRoomVisitor::Forward(float V)
{
    if(FMath::IsNearlyZero(V) || !Controller)return;
    if(auto* PC=Cast<AClaraRoomController>(Controller)){if(PC->IsStoryModal())return;PC->ResumeWalk();}
    AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V);
}
void AClaraRoomVisitor::Right(float V)
{
    if(FMath::IsNearlyZero(V) || !Controller)return;
    if(auto* PC=Cast<AClaraRoomController>(Controller)){if(PC->IsStoryModal())return;PC->ResumeWalk();}
    AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V);
}
void AClaraRoomVisitor::Turn(float V){auto* PC=Cast<AClaraRoomController>(Controller);if(PC&&PC->IsStoryModal())return;if(PC&&PC->IsDeviceOpen()){PC->OrbitDevice(V,0);return;}if(PC && PC->IsLooking())AddControllerYawInput(V);}
void AClaraRoomVisitor::Look(float V){auto* PC=Cast<AClaraRoomController>(Controller);if(PC&&PC->IsStoryModal())return;if(PC&&PC->IsDeviceOpen()){PC->OrbitDevice(0,V);return;}if(PC && PC->IsLooking())AddControllerPitchInput(-V);}
void AClaraRoomVisitor::Tick(float Delta){Super::Tick(Delta);if(GetActorLocation().Z < -300)SetActorLocation({175,85,90});}
void AClaraRoomController::Pointer()
{
    bLook=false; bShowMouseCursor=true;
    FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
}
void AClaraRoomController::BeginPlay()
{
    Super::BeginPlay(); Pointer();
    // Plan north is +Y. The camera's roll aligns the windows with the top of the plan.
    const FVector Positions[]={{950,720,2600},{175,90,158},{945,935,145},{1150,150,170},{1600,880,165},{2250,-700,2000},{580,240,112}};
    const FVector Targets[]={{950,720,0},{320,360,90},{970,1100,80},{1280,310,110},{1610,1220,130},{950,720,0},{735,200,55}};
    for(int32 I=0;I<7;++I)
    {
        auto* C=GetWorld()->SpawnActor<ACameraActor>(Positions[I],(Targets[I]-Positions[I]).Rotation());
        C->GetCameraComponent()->SetFieldOfView(I==6?72:(I==2?58:76));
        if(I==0){C->SetActorRotation({-90,-90,0}); C->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic); C->GetCameraComponent()->SetOrthoWidth(2500);}
        Views.Add(C);
    }
    TWeakObjectPtr<AClaraRoomController> Weak(this);
    auto StoryRoomVisibility=[Weak](){if(!Weak.IsValid()||Weak->GetStory())return EVisibility::Collapsed;TActorIterator<AClaraFrontEnd> It(Weak->GetWorld());if(It)return EVisibility::Collapsed;return EVisibility::Visible;};
    auto Row=SNew(SHorizontalBox).Visibility_Lambda(StoryRoomVisibility);
    const TCHAR* Names[]={TEXT("Esplora"),TEXT("1 Piantina"),TEXT("2 Ingresso"),TEXT("3 Banco reti"),TEXT("4 Cyber"),TEXT("5 Apparati"),TEXT("6 Assonometria"),TEXT("7 Sedia")};
    for(int32 I=0;I<8;++I)
    {
        const int32 ViewIndex=I-1;
        Row->AddSlot().AutoWidth().Padding(3)
        [SNew(SButton).IsFocusable(false).ButtonColorAndOpacity(FLinearColor(.04,.07,.07,.95)).ContentPadding(FMargin(11,8))
            .OnClicked_Lambda([Weak,ViewIndex](){if(Weak.IsValid())Weak->View(ViewIndex);return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(Names[I])).ColorAndOpacity(FLinearColor(.86,.89,.85))]];
    }
    Row->AddSlot().AutoWidth().Padding(3)
        [SNew(SButton).IsFocusable(false).ButtonColorAndOpacity(FLinearColor(.06,.1,.1)).ContentPadding(FMargin(10,8))
            .OnClicked_Lambda([Weak](){if(Weak.IsValid())Weak->OpenFirstDevice();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("8 Apparati Cisco")))]];
    Row->AddSlot().AutoWidth().Padding(3)
        [SNew(SButton).IsFocusable(false).ButtonColorAndOpacity(FLinearColor(.06,.1,.1)).ContentPadding(FMargin(11,8))
            .OnClicked_Lambda([Weak](){if(Weak.IsValid())Weak->ReturnToEntrance();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(TEXT("R Ingresso libero")))]];
    Toolbar=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,20)[Row]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(22,18)
        [SNew(SVerticalBox).Visibility_Lambda(StoryRoomVisibility)
            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("TNES  /  STUDIO DEGLI SPAZI"))).ColorAndOpacity(FLinearColor(.85,.90,.86))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(STextBlock).Text(FText::FromString(TEXT("WASD cammina  ·  Shift passo rapido  ·  destro guarda  ·  clic sedia/apparato: primo piano  ·  R torna all’ingresso"))).ColorAndOpacity(FLinearColor(.75,.80,.77))]];
    if(GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->AddViewportWidgetContent(Toolbar.ToSharedRef(),10);
}
void AClaraRoomController::View(int32 Index)
{
    if(IsDeviceOpen())CloseDevice();
    Pointer();
    if(auto* C=Cast<ACharacter>(GetPawn()))C->GetCharacterMovement()->StopMovementImmediately();
    for(TActorIterator<AClaraOfficeLayout> It(GetWorld());It;++It)It->ShowPlan(Index==0 || Index==5);
    if(Index>=0 && Views.IsValidIndex(Index))
    {
        ActiveView=Index;
        SetViewTargetWithBlend(Views[Index],.18f);
    }
    else if(GetPawn())
    {
        if(ActiveView>=1 && ActiveView<=4)
        {
            const FVector Anchors[]={{175,80,90},{945,935,90},{1180,160,90},{1600,900,90}};
            const int32 Anchor=ActiveView-1;
            const FVector Previous=GetPawn()->GetActorLocation();
            if(!GetPawn()->TeleportTo(Anchors[Anchor],FRotator(0,Views[ActiveView]->GetActorRotation().Yaw,0),false,false))
                GetPawn()->SetActorLocation(Previous);
            SetControlRotation(Views[ActiveView]->GetActorRotation());
        }
        ActiveView=-1;
        SetViewTargetWithBlend(GetPawn(),.2f);
    }
}
void AClaraRoomController::ResumeWalk()
{
    if(const auto* Camera=FindComponentByClass<UClaraNarrativeCamera>();Camera&&Camera->IsActive())return;
    if(IsDeviceOpen())CloseDevice();
    if(ActiveView!=-1 || GetViewTarget()!=GetPawn())
    {
        // Finish any camera blend once, so walking never feels like a locked display.
        View(-1);
        SetViewTarget(GetPawn());
    }
}
void AClaraRoomController::ReturnToEntrance()
{
    ActiveView=-1;
    if(auto* P=Cast<ACharacter>(GetPawn()))
    {
        P->GetCharacterMovement()->StopMovementImmediately();
        P->TeleportTo({175,80,90},{0,65,0},false,false);
        SetControlRotation({-3,65,0});
    }
    View(-1); SetViewTarget(GetPawn());
}
AClaraStoryDirector* AClaraRoomController::GetStory() const
{
    if(!GetWorld())return nullptr;
    TActorIterator<AClaraStoryDirector> It(GetWorld());
    if(It)return *It;
    return nullptr;
}
bool AClaraRoomController::IsStoryModal() const
{
    const auto* Camera=FindComponentByClass<UClaraNarrativeCamera>();if(Camera&&Camera->IsActive()&&!IsDeviceOpen())return true;
    const auto* Story=GetStory();return Story&&(Story->IsScreenOpen()||!Story->GetPage().IsEmpty());
}
bool AClaraRoomController::InputKey(const FInputKeyEventArgs& P)
{
    AClaraStoryDirector* Story=GetStory();
    if(Story)
    {
        if(P.Key==EKeys::V&&P.Event==IE_Pressed&&!Story->IsScreenOpen())
        {UClaraNarrativeCamera::Get(this)->ToggleReducedMotion();Pointer();return true;}
        if(P.Key==EKeys::Escape&&P.Event==IE_Pressed)
        {
            // Esc su un dialogo lo porta a termine: la storia non resta mai a meta' di una battuta.
            if(Story->GetPage()==TEXT("dialogue")){Story->FinishDialogue();Pointer();return true;}
            if(!Story->GetPage().IsEmpty()){Story->ClosePage();Pointer();return true;}
            if(Story->IsScreenOpen()){Story->CloseLaptopScreen();Pointer();return true;}
        }
        // Physical screen widgets receive typing and mouse events before world controls.
        if(Story->IsScreenOpen())return Super::InputKey(P);
        if(P.Event==IE_Pressed)
        {
            if(P.Key==EKeys::N){if(Story->GetPage()==TEXT("notebook"))Story->ClosePage();else Story->OpenNotebook();Pointer();return true;}
            if(P.Key==EKeys::I){if(Story->GetPage()==TEXT("inventory"))Story->ClosePage();else Story->OpenInventory();Pointer();return true;}
            if(Story->GetPage()==TEXT("dialogue")&&(P.Key==EKeys::E||P.Key==EKeys::SpaceBar||P.Key==EKeys::Enter))
            {Story->ContinueDialogue();return true;}
            if(!Story->GetPage().IsEmpty())return Super::InputKey(P);
            if(const auto* Camera=FindComponentByClass<UClaraNarrativeCamera>();Camera&&Camera->IsActive()&&!IsDeviceOpen())return true;
            if(P.Key==EKeys::H){Story->RequestHint();return true;}
            if(P.Key==EKeys::M){if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->SetMuted(!Audio->IsMuted());return true;}
            if(P.Key==EKeys::Q){Story->PlaceLaptop();return true;}
            if(P.Key==EKeys::T){Story->OpenLaptopScreen();return true;}
            if(P.Key==EKeys::E&&!IsDeviceOpen())
            {
                AActor* Target=Story->FindInteractionTarget();
                if(!Story->HandleInteraction(Target))if(auto* Device=Cast<AClaraNetworkDevice>(Target))OpenDevice(Device);
                Pointer();return true;
            }
        }
        if(!Story->GetPage().IsEmpty())return Super::InputKey(P);
        if(const auto* Camera=FindComponentByClass<UClaraNarrativeCamera>();Camera&&Camera->IsActive()&&!IsDeviceOpen())return true;
    }
    if(IsDeviceOpen())
    {
        if((P.Key==EKeys::W||P.Key==EKeys::A||P.Key==EKeys::S||P.Key==EKeys::D)&&P.Event==IE_Pressed)
        {CloseDevice();SetViewTarget(GetPawn());return Super::InputKey(P);}
        if(P.Key==EKeys::Escape&&P.Event==IE_Pressed){CloseDevice();return true;}
        if((P.Key==EKeys::MouseScrollUp||P.Key==EKeys::MouseScrollDown)&&P.Event==IE_Pressed)
        {ZoomDevice(P.Key==EKeys::MouseScrollUp?-8.f:8.f);return true;}
        if(P.Key==EKeys::LeftMouseButton||P.Key==EKeys::RightMouseButton)
        {
            if(P.Event==IE_Pressed){bDeviceDragging=true;bDeviceLeftDrag=P.Key==EKeys::LeftMouseButton;bDeviceMoved=false;DeviceDragTravel=0;return true;}
            if(P.Event==IE_Released)
            {if(bDeviceDragging&&bDeviceLeftDrag&&!bDeviceMoved)ClickDevicePort();bDeviceDragging=false;Pointer();return true;}
        }
    }
    if(P.Key==EKeys::RightMouseButton)
    {
        if(P.Event==IE_Pressed)
        {
            if(GetViewTarget()!=GetPawn())ResumeWalk();
            bLook=true; bShowMouseCursor=false; SetInputMode(FInputModeGameOnly()); return true;
        }
        if(P.Event==IE_Released){Pointer();return true;}
    }
    if(P.Event!=IE_Pressed)return Super::InputKey(P);
    // Scorciatoie dello studio degli spazi: nella storia un tasto premuto per sbaglio non deve
    // teletrasportare il giocatore ne' cambiare camera. Restano con -ClaraDevKeys.
    const bool bStudioKeys=!Story||FParse::Param(FCommandLine::Get(),TEXT("ClaraDevKeys"));
    if(P.Key==EKeys::Escape || P.Key==EKeys::Zero){View(-1);return true;}
    if(bStudioKeys)
    {
        if(P.Key==EKeys::R){ReturnToEntrance();return true;}
        if(P.Key==EKeys::Eight){OpenFirstDevice();return true;}
        const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven};
        for(int32 I=0;I<7;++I)if(P.Key==Keys[I]){View(I);return true;}
    }
    if(P.Key==EKeys::LeftMouseButton)
    {
        FHitResult Hit;
        if(GetHitResultUnderCursor(ECC_Visibility,false,Hit))
        {
            if(Story&&Story->HandleInteraction(Story->FindInteractionTarget())){Pointer();return true;}
            if(auto* Device=Cast<AClaraNetworkDevice>(Hit.GetActor())){OpenDevice(Device);return true;}
            UPrimitiveComponent* Part=Hit.GetComponent();
            if(bStudioKeys && Part && Part->ComponentHasTag(TEXT("Inspectable")))
            {
                if(!InspectCamera)InspectCamera=GetWorld()->SpawnActor<ACameraActor>();
                const FVector Center=Part->Bounds.Origin;
                const FVector Offset=Part->ComponentHasTag(TEXT("Chair"))
                    ? Part->GetComponentRotation().RotateVector(FVector(-145,135,25))
                    : (Part->ComponentHasTag(TEXT("PC"))?FVector(0,85,18):FVector(0,-62,30));
                InspectCamera->SetActorLocation(Center+Offset);
                InspectCamera->SetActorRotation((-Offset).Rotation());
                InspectCamera->GetCameraComponent()->SetFieldOfView(55);
                for(TActorIterator<AClaraOfficeLayout> It(GetWorld());It;++It)It->ShowPlan(false);
                ActiveView=-1; SetViewTargetWithBlend(InspectCamera,.3f); Pointer(); return true;
            }
        }
    }
    return Super::InputKey(P);
}
void AClaraRoomController::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseDevice();
    if(Toolbar.IsValid() && GetWorld() && GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Toolbar.ToSharedRef());
    Toolbar.Reset(); Super::EndPlay(Reason);
}
