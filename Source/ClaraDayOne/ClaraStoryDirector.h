#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraSwitchCLI.h"
#include "ClaraStoryDirector.generated.h"
class AClaraRoomController;
class AClaraNetworkDevice;
class AClaraStoryProp;
class ACameraActor;
class UClaraDisplay;
class UClaraGateway;
struct FClaraGatewayResponse;
class FJsonObject;
class SWidget;

struct FClaraStoryItem { FString Id,Label,Detail; bool Owned=false; };
struct FClaraSimNode
{
    FString Id,Label,Kind,IP,Mask,Gateway;
    FVector2D Position;
};

UCLASS()
class CLARADAYONE_API AClaraStoryDirector : public AActor
{
    GENERATED_BODY()
public:
    AClaraStoryDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool HandleInteraction(AActor* Target);
    void InteractFocused();
    AActor* FindInteractionTarget() const;
    FString GetInteractionLabel() const;
    TArray<AActor*> GetImportantActors() const;
    bool IsMissionTarget(AActor* Actor) const;
    FString GetObjective() const;
    FString GetTitle() const;
    FString GetSpeaker() const {return Speaker;}
    FString GetDialogue() const {return Dialogue;}
    FString GetToast() const {return Toast;}
    FString GetPage() const {return Page;}
    FString GetScreenPage() const {return ScreenPage;}
    TArray<FString> GetChoices() const {return DialogueIndex==DialogueQueue.Num()-1?Choices:TArray<FString>();}
    int32 GetDay() const {return Phase>=100?2:1;}
    int32 GetPhase() const {return Phase;}
    bool IsComplete() const {return Phase==110;}
    void ContinueDialogue();
    void Choose(int32 Index);
    void ClosePage();
    void OpenNotebook();
    void OpenInventory();
    FString GetNotebookText() const;
    TArray<FClaraStoryItem> GetItems() const;
    void UseItem(const FString& Id);
    void RequestHint();
    void StartDay2();
    void PickupLaptop();
    void PlaceLaptop();
    bool IsCarryingLaptop() const {return CarriedLaptop;}
    void OpenLaptopScreen();
    void CloseLaptopScreen();
    bool IsDeviceInspectionOpen() const;
    bool IsScreenOpen() const {return ScreenOpen;}
    void SetScreenPage(const FString& Name);
    TArray<FString> GetPuzzleItems() const {return Puzzle;}
    void MovePuzzleItem(int32 Index,int32 Direction);
    bool CheckPuzzle();
    FString GetQuestion() const;
    FString GetAnswerHint() const;
    bool SubmitAnswer(const FString& Text);
    FString GetSortingCable() const {return SortingCable;}
    void SelectSortingCable(const FString& Kind);
    bool SortCable(const FString& Bin);
    bool IsCableSorted(const FString& Kind) const {return Sorted.Contains(Kind);}
    FString GetSelectedCable() const {return EquippedCable;}
    void EquipCable(const FString& Kind);
    FString GetTerminalTitle() const;
    FString GetTerminalPrompt() const;
    FString GetTranscript() const {return Transcript;}
    FString RunCommand(const FString& Command);
    FString CompleteCommand(const FString& Prefix) const;
    FString CommandHelp(const FString& Prefix) const;
    void SelectTerminalTarget(const FString& Id);
    FString GetTerminalTarget() const {return TerminalTarget;}
    void ResetTrialSwitch();
    TArray<FClaraSimNode> GetSimNodes() const;
    TArray<TPair<FString,FString>> GetSimLinks() const;
    FString GetSimSelected() const {return SimSelected;}
    FString GetSimMessage() const {return SimMessage;}
    void SelectSimNode(const FString& Id);
    void SetSimCableKind(const FString& Kind);
    FString GetSimCableKind() const {return SimCableKind;}
    bool SetSimHostIP(const FString& Id,const FString& Address);
    FString RunChallengeTest();
    void RebootChallenge();
    bool CanRebootChallenge() const {return ChallengeReady;}
    bool RunSelfTest();
    void GatewayBegin();
    void TickGateway();
    void GatewayShutdown();
    FString GetGatewayStatus() const;
    FString GetGatewayNode() const {return GatewayNode;}
    FString GetGatewayRoom() const {return GatewayRoom;}
    FString GetGatewayResult() const {return GatewayResult;}
    FString GetGatewayTranscript() const {return GatewayTranscript;}
    bool IsGatewayLive() const;
    void GatewayRefresh();
    void GatewayBootstrap();
    void GatewaySetNode(const FString& Node);
    void GatewayVerifyMission(const FString& MissionId);
    void GatewayHint(const FString& Question=FString(),const FString& Character=TEXT("Luca"));
    void GatewayBlockDomain(const FString& Domain,bool Block);
    void GatewayGenerate(const FString& Prompt,bool DeployToCML);
    void GatewayPatchPort(const FString& PortId,bool Connected);
    static bool RunGatewayPortRepairSelfTest();
    bool IsGatewayPortRepairPending() const {return GatewayPortRepairBusy||!GatewayPendingPortRepairs.IsEmpty();}
    FString GetGatewayPortProofMethod(const FString& PortId) const {return GatewayPortProofMethods.FindRef(PortId);}
    bool IsGatewayPortConfirmed(const FString& PortId,bool Connected) const {const bool* State=GatewayAcknowledgedPorts.Find(PortId);return State&&*State==Connected;}
    FString RunGatewayCommand(const FString& Command);

private:
    int32 Phase=0, DialogueIndex=0, AfterDialogue=-1, SubnetStep=0, HintLevel=0;
    FString Page,ScreenPage=TEXT("desktop"),Speaker,Dialogue,Toast,Transcript,TerminalTarget=TEXT("LAPTOP");
    FString EquippedCable=TEXT("ethernet"),SortingCable,SimSelected,SimPending,SimCableKind,SimMessage,ConsoleTarget;
    TArray<TPair<FString,FString>> DialogueQueue;
    TArray<FString> Choices,Puzzle,PuzzleExpected;
    TSet<FString> Owned,Sorted,Completed;
    bool CarriedLaptop=false,ScreenOpen=false,ChallengeReady=false,ChallengeRebooted=false,PendingDay1Final=false,SelfTesting=false;
    float ToastUntil=0,PollAt=0;
    TWeakObjectPtr<AClaraRoomController> Controller;
    TWeakObjectPtr<AClaraStoryProp> HeldCable;
    FTransform HeldCableHome;
    void ReturnHeldCable();
    UPROPERTY() TMap<FString,TObjectPtr<AClaraNetworkDevice>> Devices;
    UPROPERTY() TObjectPtr<UClaraDisplay> Display;
    UPROPERTY() TObjectPtr<ACameraActor> ScreenCamera;
    TSharedPtr<SWidget> HUD,Panel,ScreenOverlay;
    bool LaptopMenu=false;
    FClaraSwitchCLI Day1CLI,TrialCLI,SwitchA,SwitchB;
    TArray<FClaraSimNode> SimNodes;
    TSet<FString> SimLinks;
    TMap<FString,FString> ArpCache;
    TMap<FString,FString> LearnedMACs;
    FString LaptopIP=TEXT("192.168.1.10"),PC2IP=TEXT("192.168.1.11");
    FString LaptopMAC=TEXT("02-AA-00-00-00-10"),PC2MAC=TEXT("02-AA-00-00-00-11");
    void SetPhase(int32 Next);
    void Say(const FString& Who,const TArray<FString>& Lines,int32 Next=-1,const TArray<FString>& Options={});
    void ShowPanel(const FString& Name);
    void Notify(const FString& Text);
    void CreateProps();
    void CreateLabHosts();
    void PollLab();
    bool PhysicalLinked(const FString& A,const FString& B,const FString& Kind=TEXT("ethernet")) const;
    bool HostsReachable() const;
    void LearnFromPing();
    FString ExecutePC(const FString& Command);
    FClaraSwitchCLI* ActiveCLI();
    const FClaraSwitchCLI* ActiveCLI() const;
    void SetupPuzzle(const FString& Kind);
    void SetupChallenge();
    bool SimReachable(const FString& A,const FString& B) const;
    bool SimHasLink(const FString& A,const FString& B) const;
    void RefreshScreen();
    void SaveProgress();
    UPROPERTY() TObjectPtr<UClaraGateway> Gateway;
    FString GatewayNode=TEXT("SW-PIANO1"),GatewayRoom,GatewayRoomCandidate,GatewayResult,GatewayTranscript,GatewayMessage;
    FString GatewaySessionId;
    bool GatewayHealthy=false,GatewayBootstrapped=false,GatewayRoomPending=false,GatewayRoomReady=false,GatewayCustomNode=false,GatewayIOSConfirmed=false,GatewayGeneratedLab=false;
    double GatewayRoomCandidateSince=0,GatewayNextPoll=0,GatewayNextRoomRetry=0;
    TMap<FString,bool> GatewayObservedPorts,GatewayAcknowledgedPorts;
    TSet<FString> GatewayBlockedDomains;
    TMap<FString,TArray<FString>> GatewayCommands;
    FString CurrentGatewayRoom() const;
    void RequestGatewayRoom(const FString& Room);
    void RecordGatewayResult(const FString& Action,const FClaraGatewayResponse& Response);
    void PollGatewayPatches();
    TSharedPtr<FJsonObject> GatewayPlayerState() const;
    TArray<TPair<FString,bool>> GatewayPendingPortRepairs;
    bool GatewayPortRepairBusy=false,GatewayPortRepairConnected=false,GatewayPortRepairShuttingDown=false,GatewayLegacyPatchBroken=false,GatewayPortRepairReconnectTried=false;
    FString GatewayPortRepairPort,GatewayPortRepairInterface;
    uint64 GatewayPortRepairGeneration=0;
    TArray<TSharedPtr<FJsonObject>> GatewayPortRepairEvidence;
    TMap<FString,FString> GatewayPortProofMethods;
    void ProcessGatewayPortRepair();
    void StepGatewayPortRepair(int32 Step,uint64 Token);
    void FinishGatewayPortRepair(bool Success,const FString& Reason,const FString& Method=FString());

};
TSharedRef<SWidget> MakeClaraStoryHUD(AClaraStoryDirector* Director);
TSharedRef<SWidget> MakeClaraStoryPanel(AClaraStoryDirector* Director);
TSharedRef<SWidget> MakeClaraStoryScreen(AClaraStoryDirector* Director);
