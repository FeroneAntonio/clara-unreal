#include "ClaraPlayer.h"
#include "ClaraPanels.h"
#include "ClaraDesktop.h"
#include "ClaraDisplay.h"
#include "ClaraOffice.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

AClaraPlayer* AClaraPlayerController::GetClaraPlayer() const
{
    return Cast<AClaraPlayer>(GetPawn());
}

bool AClaraPlayerController::IsLookEnabled() const
{
    return bLookEnabled && GetWorld() && GetWorld()->GetTimeSeconds() >= LookReadyTime;
}

void AClaraPlayerController::RestorePointer()
{
    bLookEnabled = false;
    bShowMouseCursor = true;
    if (PanelRoot.IsValid() && !bPanelOnDisplay)
    {
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(PanelRoot);
        SetInputMode(Mode);
    }
    else
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
}

void AClaraPlayerController::CreateToolbar()
{
    if (ToolbarRoot.IsValid() || !GetWorld()->GetGameViewport()) return;
    TWeakObjectPtr<AClaraPlayerController> Weak(this);
    auto Button = [Weak](const FString& Label, TFunction<void(AClaraPlayerController*)> Action)
    {
        return SNew(SButton).ContentPadding(FMargin(15, 10))
            .ButtonColorAndOpacity(FLinearColor(.07f, .065f, .05f, .8f))
            .OnClicked_Lambda([Weak, Action]()
            {
                if (Weak.IsValid()) Action(Weak.Get());
                return FReply::Handled();
            })
            [SNew(STextBlock).Text(FText::FromString(Label)).ColorAndOpacity(FLinearColor(.94f, .91f, .82f))];
    };
    ToolbarRoot = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
    + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0, 0, 28, 22)
    [SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(3)
        [Button(TEXT("Taccuino"), [](AClaraPlayerController* PC) { PC->OpenNotebookPanel(); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(3)
        [Button(TEXT("Borsa"), [](AClaraPlayerController* PC) { PC->OpenInventory(); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(3)
        [Button(TEXT("Un indizio"), [](AClaraPlayerController* PC) { if (auto* P = PC->GetClaraPlayer()) P->ShowHint(); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(3)
        [Button(TEXT("Desktop"), [](AClaraPlayerController* PC) { PC->OpenDesktop(); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(3)
        [SNew(SButton).ContentPadding(FMargin(15, 10))
            .Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->GetClaraPlayer() && Weak->GetClaraPlayer()->IsInspectingPC() ? EVisibility::Visible : EVisibility::Collapsed; })
            .Text(FText::FromString(TEXT("Torna alla postazione")))
            .OnClicked_Lambda([Weak]() { if (Weak.IsValid() && Weak->GetClaraPlayer()) Weak->GetClaraPlayer()->EndPCInspection(); return FReply::Handled(); })]
    ];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(ToolbarRoot.ToSharedRef(), 20);
}

void AClaraPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshInterface();
    if (ToolbarRoot.IsValid()) ToolbarRoot->SetVisibility(
        IsTerminalOpen() || IsPanelOpen() || (GetClaraPlayer() && GetClaraPlayer()->HasDialogue())
        ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible);
}

void AClaraPlayerController::RefreshInterface()
{
    AClaraPlayer* Player = GetClaraPlayer();
    if (!Player || !GetWorld()->GetGameViewport()) return;
    FString Fingerprint = Player->GetDialogue();
    for (const FString& Choice : Player->GetChoices()) Fingerprint += TEXT("|") + Choice;
    if (Fingerprint == LastDialogue) return;
    LastDialogue = Fingerprint;
    if (DialogueRoot.IsValid()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DialogueRoot.ToSharedRef());
    DialogueRoot.Reset();
    if (Player->HasDialogue())
    {
        CloseUtilityPanel();
        DialogueRoot = MakeClaraDialogue(Player);
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(DialogueRoot.ToSharedRef(), 80);
    }
    RestorePointer();
}

void AClaraPlayerController::OpenNotebookPanel()
{
    if (!GetClaraPlayer() || GetClaraPlayer()->HasDialogue() || IsTerminalOpen()) return;
    CloseUtilityPanel();
    PanelRoot = MakeClaraNotebook(GetClaraPlayer(), FSimpleDelegate::CreateUObject(this, &AClaraPlayerController::CloseUtilityPanel));
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(PanelRoot.ToSharedRef(), 90);
    RestorePointer();
}

void AClaraPlayerController::OpenInventory()
{
    if (!GetClaraPlayer() || GetClaraPlayer()->HasDialogue() || IsTerminalOpen()) return;
    CloseUtilityPanel();
    PanelRoot = MakeClaraInventory(GetClaraPlayer(), FSimpleDelegate::CreateUObject(this, &AClaraPlayerController::CloseUtilityPanel));
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(PanelRoot.ToSharedRef(), 90);
    RestorePointer();
}

void AClaraPlayerController::OpenDesktop()
{
    AClaraPlayer* Player = GetClaraPlayer();
    if (!Player || Player->HasDialogue() || IsTerminalOpen()) return;
    if (!Player->IsNearPC()) { Player->ShowHint(); return; }
    CloseUtilityPanel();
    PanelRoot = MakeClaraDesktop(Player, FSimpleDelegate::CreateUObject(this, &AClaraPlayerController::CloseUtilityPanel));
    if (!ShowOnDisplay(PanelRoot, false)) { PanelRoot.Reset(); return; }
    bPanelOnDisplay = true;
    RestorePointer();
}

void AClaraPlayerController::CloseUtilityPanel()
{
    if (bPanelOnDisplay) ClearDisplay();
    else if (PanelRoot.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PanelRoot.ToSharedRef());
    PanelRoot.Reset();
    bPanelOnDisplay = false;
    RestorePointer();
}

bool AClaraPlayerController::ShowOnDisplay(TSharedPtr<SWidget> Widget, bool bLaptop)
{
    if (!Widget.IsValid() || !GetWorld() || !GetClaraPlayer()) return false;
    for (TActorIterator<AClaraOffice> It(GetWorld()); It; ++It)
    {
        UClaraDisplay* Display = bLaptop ? It->GetSwitchDisplay() : It->GetPCDisplay();
        if (!Display) continue;
        ActiveDisplay = Display;
        Display->ActivateDisplay(Widget);
        GetClaraPlayer()->BeginMonitorInspection(Display->GetComponentLocation(), bLaptop);
        return true;
    }
    UE_LOG(LogTemp, Error, TEXT("CLARA: schermo fisico non disponibile"));
    return false;
}

void AClaraPlayerController::ClearDisplay()
{
    if (ActiveDisplay.IsValid()) ActiveDisplay->DeactivateDisplay();
    ActiveDisplay.Reset();
    if (GetClaraPlayer()) GetClaraPlayer()->EndMonitorInspection();
}
