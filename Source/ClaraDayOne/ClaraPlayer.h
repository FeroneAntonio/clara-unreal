#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Types/SlateEnums.h"
#include "ClaraSwitchCLI.h"
#include "HttpFwd.h"
#include "TimerManager.h"
#include "ClaraPlayer.generated.h"

class UCameraComponent;
class SEditableTextBox;
class SWidget;
class SScrollBox;
class UClaraDisplay;
class FReply;
struct FKeyEvent;
struct FGeometry;
class FJsonObject;

enum class EClaraStage : uint8
{
    Reception, Welcome, Ticket, Cabling, Meaning, Guide, Terminal,
    Service, Transfer, Debrief, SwitchBrief, SwitchConfig, SwitchReport, Complete
};

struct FClaraProbeCommand
{
    bool bValid = false;
    FString Destination;
    FString Error;
};

/** A deliberately bounded local teaching exercise, never labelled as a Cisco measurement. */
FClaraProbeCommand ParseClaraProbe(const FString& Command);

UCLASS()
class CLARADAYONE_API AClaraPlayer : public ACharacter
{
    GENERATED_BODY()

public:
    AClaraPlayer();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

    AActor* GetFocusedActor() const;
    FString GetInteractionLabel() const;
    void InteractWithActor(AActor* Actor);
    void Interact();
    void ContinueDialogue();
    void Choose(int32 Choice);
    void ShowHint();
    void OpenNotebook();
    void RequestTerminal();
    void ToggleVision();
    void DropCable();
    void StowCable();
    void EquipCable();
    void MarkProbeGuideRead();
    void RequestSwitchTerminal();
    void RequestPCTerminal();
    void OpenCourseMaterial();
    void ReadCurrentTicket();
    void BeginPCInspection();
    void EndPCInspection();
    void BeginMonitorInspection(FVector ScreenPosition, bool bLaptop);
    void EndMonitorInspection();
    bool IsInspectingPC() const { return bInspectingPC; }
    bool IsNearPC() const { return IsNearTerminal(); }
    void OnSwitchConfigurationChecked(bool bCorrect);
    void RestartSwitchExercise();
    void ClosePanel();
    FString RunCommand(const FString& Command);

    bool IsCableConnected() const { return bCableConnected; }
    bool IsHoldingCable() const { return bHoldingCable; }
    bool IsCableStored() const { return bCableStored; }
    bool HasCableItem() const { return bHoldingCable || bCableStored || bCableConnected; }
    bool HasBadge() const { return !Career.IsEmpty(); }
    bool HasReadProbeGuide() const { return bGuideRead; }
    bool HasConsoleCable() const { return bHasConsoleCable; }
    bool IsHoldingConsole() const { return bHoldingConsole; }
    bool IsConsoleConnected() const { return bConsoleAtSwitch && bConsoleAtLaptop; }
    bool IsVisionEnabled() const { return bVisionEnabled; }
    bool IsInputBlocked() const;
    EClaraStage GetStage() const { return Stage; }
    FString GetObjective() const;
    FString GetStageTitle() const;
    const FString& GetSpeaker() const { return Speaker; }
    const FString& GetDialogue() const { return Dialogue; }
    const TArray<FString>& GetChoices() const { return Choices; }
    const FString& GetToast() const { return Toast; }
    bool HasDialogue() const { return !Dialogue.IsEmpty(); }
    FString GetCareer() const { return Career; }

private:
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    TWeakObjectPtr<AActor> CableActor;
    FTransform CableRestTransform;
    EClaraStage Stage = EClaraStage::Reception;
    EClaraStage StageAfterDialogue = EClaraStage::Reception;
    FString Speaker;
    FString Dialogue;
    TArray<FString> Choices;
    FString Career;
    FString Toast;
    float ToastUntil = 0;
    int32 HintLevel = 0;
    bool bHoldingCable = false;
    bool bCableStored = false;
    bool bCableConnected = false;
    bool bVisionEnabled = false;
    bool bFirstPing = false;
    bool bServiceOpened = false;
    bool bTransferPing = false;
    bool bGuideRead = false;
    bool bHasConsoleCable = false;
    bool bHoldingConsole = false;
    bool bConsoleAtSwitch = false;
    bool bConsoleAtLaptop = false;
    bool bInspectingPC = false;
    bool bInspectingMonitor = false;
    FVector SavedCameraLocation;
    FRotator SavedViewRotation;
    FVector SavedMonitorCameraLocation;
    FRotator SavedMonitorViewRotation;
    float SavedMonitorFieldOfView = 82;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void Say(const FString& Name, const FString& Text, EClaraStage Next);
    void Notify(const FString& Text);
    bool IsNearTerminal() const;
};

UCLASS()
class CLARADAYONE_API AClaraPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void OpenTerminal(bool bForSwitch = false);
    void CloseTerminal();
    bool IsTerminalOpen() const { return TerminalRoot.IsValid(); }
    bool IsLookEnabled() const;
    bool IsPanelOpen() const { return PanelRoot.IsValid(); }
    void OpenNotebookPanel();
    void OpenInventory();
    void OpenDesktop();
    void CloseUtilityPanel();
    void RestorePointer();
    bool ShowOnDisplay(TSharedPtr<SWidget> Widget, bool bLaptop);
    void ClearDisplay();
    AClaraPlayer* GetClaraPlayer() const;
    FString ExecuteSwitchCommand(const FString& Command);
    void AskSwitchMentor();
    void ResetSwitchWithMentor();
    bool IsSwitchMissionConfigured() const { return !bCMLMode && SwitchCLI.IsMissionConfigured(); }
    FString GetSwitchMentorFeedback() const { return bCMLMode
        ? TEXT("Luca: questa console usa il laboratorio CML reale. La validazione automatica di running-config e startup-config non e' ancora disponibile: leggi e confronta i risultati dell'apparato. Il ripristino del modello locale resta disattivato.")
        : SwitchCLI.GetMentorFeedback(); }

private:
    bool bLookEnabled = false;
    float LookReadyTime = 0;
    TSharedPtr<SWidget> ToolbarRoot;
    TSharedPtr<SWidget> PanelRoot;
    TSharedPtr<SWidget> DialogueRoot;
    FString LastDialogue;
    TWeakObjectPtr<UClaraDisplay> ActiveDisplay;
    bool bPanelOnDisplay = false;
    bool bTerminalOnDisplay = false;
    bool bEndingPlay = false;
    void RefreshInterface();
    void CreateToolbar();
    TSharedPtr<SWidget> TerminalRoot;
    TSharedPtr<SEditableTextBox> TerminalInput;
    TSharedPtr<SScrollBox> TerminalScroll;
    FString TerminalOutput;
    FClaraSwitchCLI SwitchCLI;
    bool bSwitchTerminal = false;
    bool bUpdatingTerminalText = false;
    bool bResetConfirmation = false;
    TArray<FString> TerminalHistory;
    int32 HistoryIndex = 0;
    void SubmitTerminal(const FText& Text, ETextCommit::Type Commit);
    void TerminalTextChanged(const FText& Text);
    FReply TerminalKeyDown(const FGeometry& Geometry, const FKeyEvent& Event);

    enum class ECMLAction : uint8 { None, Status, Open, Poll, Submit, Completion, Help };
    bool bCMLMode = false;
    bool bCMLReady = false;
    bool bCMLConnected = false;
    bool bCMLRequestInFlight = false;
    bool bCMLInputPending = false;
    bool bCMLAwaitingCompletion = false;
    uint32 CMLGeneration = 0;
    FString CMLBaseURL;
    FString CMLConsoleId;
    FString CMLQueuedText;
    ECMLAction CMLQueuedAction = ECMLAction::None;
    FHttpRequestPtr CMLRequest;
    FTimerHandle CMLPollTimer;
    void ConfigureCMLBridge();
    void BeginCMLConsole();
    void CloseCMLConsole();
    void PollCMLConsole();
    bool CanUseCMLInput() const;
    void SendCMLConsoleInput(const FString& Prefix, ECMLAction Action);
    void FlushCMLInput();
    void RequestCML(const FString& Verb, const FString& Route, const TSharedPtr<FJsonObject>& Body, ECMLAction Action);
    void ApplyCMLSnapshot(const TSharedPtr<FJsonObject>& Snapshot, ECMLAction Action);
    void CMLFailure(const FString& Message);
};
