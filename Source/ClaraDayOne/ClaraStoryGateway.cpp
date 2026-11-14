#include "ClaraStoryDirector.h"
#include "ClaraGateway.h"
#include "ClaraNetworkDevice.h"
#include "ClaraRoomVisitor.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
struct FClaraPortBridge {const TCHAR* Asset;const TCHAR* Gateway;const TCHAR* IOSInterface;};
// Explicit adapter, not matching interface names: the gateway selects the first switch of the active room.
const FClaraPortBridge PortBridge[]={{TEXT("Gi1/0/1"),TEXT("sw-g1"),TEXT("Ethernet0/0")},{TEXT("Gi1/0/2"),TEXT("sw-1"),TEXT("Ethernet0/1")},{TEXT("Gi1/0/3"),TEXT("sw-2"),TEXT("Ethernet0/2")},{TEXT("Gi1/0/4"),TEXT("sw-3"),TEXT("Ethernet0/3")}};
FString JsonText(const TSharedPtr<FJsonObject>& Object)
{
    FString Text;if(Object.IsValid())FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text));return Text;
}
FString JsonString(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key)
{FString Value;if(Object.IsValid())Object->TryGetStringField(Key,Value);return Value;}
FString DefaultNode(const FString& Room)
{return Room==TEXT("corridor")?TEXT("SW-ACC"):Room==TEXT("datacenter_vault")?TEXT("SW-DC1"):TEXT("SW-PIANO1");}
FString SessionKey(const FString& Room,const FString& Node){return Room+TEXT("|")+Node;}
FString LastIOSPrompt(const FString& Output)
{
    TArray<FString> Lines;Output.ParseIntoArrayLines(Lines);
    for(int32 I=Lines.Num()-1;I>=0;--I)
    {const FString Line=Lines[I].TrimStartAndEnd();if(Line.EndsWith(TEXT("#"))||Line.EndsWith(TEXT(">")))return Line;}
    return FString();
}
bool IOSPortReadback(const FString& Output,const FString& Interface,bool Connected)
{
    if(Output.Contains(TEXT("consoles>"),ESearchCase::IgnoreCase)||Output.Contains(TEXT("[SIM]"))||Output.Contains(TEXT("(OK)"))||Output.Contains(TEXT("% Invalid"),ESearchCase::IgnoreCase))return false;
    const FString Prefix=Interface+TEXT(" is ");TArray<FString> Lines;Output.ParseIntoArrayLines(Lines);
    for(const auto& Raw:Lines)
    {
        const FString Line=Raw.TrimStartAndEnd();if(!Line.StartsWith(Prefix,ESearchCase::IgnoreCase))continue;
        return Connected?Line.StartsWith(Interface+TEXT(" is up, line protocol is up"),ESearchCase::IgnoreCase):
            Line.StartsWith(Interface+TEXT(" is administratively down, line protocol is down"),ESearchCase::IgnoreCase);
    }
    return false;
}
TSharedPtr<FJsonObject> RepairEvidence(const FString& Command,const FClaraGatewayResponse& R)
{
    auto Evidence=MakeShared<FJsonObject>();Evidence->SetStringField(TEXT("command"),Command);Evidence->SetStringField(TEXT("source"),R.Source);Evidence->SetStringField(TEXT("mode"),R.Mode);Evidence->SetBoolField(TEXT("success"),R.Success);Evidence->SetBoolField(TEXT("live_cml"),R.LiveCML);Evidence->SetStringField(TEXT("error"),R.Error);if(R.Data.IsValid())Evidence->SetObjectField(TEXT("response"),R.Data);return Evidence;
}
bool IOSOutput(const FString& Output)
{
    if(Output.Contains(TEXT("consoles>"))||Output.Contains(TEXT("consoles#"))||Output.Contains(TEXT("[SIM]"))||Output.Contains(TEXT("unknown command")))return false;
    FString Last=Output.TrimStartAndEnd();int32 Break=INDEX_NONE;if(Last.FindLastChar(TEXT('\n'),Break))Last=Last.Mid(Break+1).TrimStartAndEnd();
    return Last.EndsWith(TEXT("#"))||Last.EndsWith(TEXT(">"))||Output.Contains(TEXT("Cisco IOS"))||(Output.Contains(TEXT("Interface"))&&Output.Contains(TEXT("Protocol")));
}
}

void AClaraStoryDirector::GatewayBegin()
{
    if(Gateway)return;
    Gateway=NewObject<UClaraGateway>(this);GatewaySessionId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    const bool Allow=FParse::Param(FCommandLine::Get(),TEXT("ClaraGatewayPortCheck"))||(!FApp::IsUnattended()&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraStoryCheck"))&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraLayoutCheck")));
    Gateway->Initialize(Allow);
    if(auto* Switch=Devices.FindRef(TEXT("SW1")).Get())for(const auto& P:PortBridge)GatewayObservedPorts.Add(P.Asset,Switch->IsPortOccupied(P.Asset));
    GatewayMessage=TEXT("Il banco dei Giorni 1 e 2 resta un laboratorio locale. Il terminale CML e' una sessione remota separata.");
    if(Gateway->Enabled())GatewayRefresh();
}
void AClaraStoryDirector::GatewayShutdown(){GatewayPortRepairShuttingDown=true;++GatewayPortRepairGeneration;GatewayPendingPortRepairs.Empty();GatewayPortRepairBusy=false;if(Gateway)Gateway->Shutdown();}
bool AClaraStoryDirector::IsGatewayLive()const{return Gateway&&Gateway->IsLive()&&GatewayRoomReady;}
FString AClaraStoryDirector::GetGatewayStatus()const
{
    if(!Gateway)return TEXT("Gateway non inizializzato · esercizi locali");
    return Gateway->GetStatusLabel()+TEXT("\nStanza: ")+(GatewayRoom.IsEmpty()?TEXT("da sincronizzare"):GatewayRoom)+TEXT(" · nodo: ")+GatewayNode+
        (GatewayRoomPending?TEXT(" · cambio laboratorio in corso"):TEXT(""))+(GatewayPortRepairBusy?TEXT(" · sincronizzazione IOS della porta in corso"):TEXT(""))+
        (GatewayIOSConfirmed?TEXT("\nConsole IOS: risposta riconosciuta"):TEXT("\nConsole IOS: ancora da verificare"))+TEXT("\n")+GatewayMessage;
}
void AClaraStoryDirector::RecordGatewayResult(const FString& Action,const FClaraGatewayResponse& R)
{
    if(R.Superseded)return;
    const FString Origin=!R.Source.IsEmpty()?R.Source:!R.Mode.IsEmpty()?R.Mode:TEXT("risposta API senza prova di esecuzione CML");
    GatewayMessage=Action+TEXT(" · ")+Origin+(R.Success?TEXT(""):TEXT(" · ")+R.Error);
    GatewayResult=GatewayMessage+TEXT("\nHTTP ")+FString::FromInt(R.StatusCode)+TEXT("\n")+JsonText(R.Data);
    Notify(GatewayMessage.Left(240));
}
void AClaraStoryDirector::GatewayRefresh()
{
    if(!Gateway){GatewayBegin();return;}
    GatewayGeneratedLab=false;
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Health([Weak](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Stato servizi"),R);
        D->GatewayHealthy=R.Success&&R.LiveCML;
        if(!D->GatewayHealthy){D->GatewayRoomReady=false;return;}
        if(!D->GatewayBootstrapped)D->GatewayBootstrap();else if(!D->GatewayRoomReady)D->RequestGatewayRoom(D->CurrentGatewayRoom());
    });
}
void AClaraStoryDirector::GatewayBootstrap()
{
    if(!Gateway||!Gateway->Enabled())return;
    if(GatewayPortRepairBusy){Notify(TEXT("Attendi la conferma della porta IOS prima di riavviare il laboratorio."));return;}
    GatewayRoomReady=false;TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Bootstrap([Weak](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Avvio laboratorio remoto"),R);
        D->GatewayBootstrapped=R.Success&&R.LiveCML;
        if(D->GatewayBootstrapped){D->GatewayRoom=JsonString(R.Data,TEXT("room_id"));D->RequestGatewayRoom(D->CurrentGatewayRoom());}
    });
}
FString AClaraStoryDirector::CurrentGatewayRoom()const
{
    if(!Controller.IsValid()||!Controller->GetPawn())return TEXT("office");
    const FVector P=Controller->GetPawn()->GetActorLocation();
    if(P.Y>=540&&P.Y<760)return TEXT("corridor");
    if(P.X>=1350&&P.Y>=760)return TEXT("datacenter_vault");
    return TEXT("office");
}
void AClaraStoryDirector::RequestGatewayRoom(const FString& Room)
{
    if(!Gateway||!GatewayHealthy||GatewayRoomPending||GatewayPortRepairBusy)return;
    GatewayRoomPending=true;GatewayRoomReady=false;GatewayIOSConfirmed=false;GatewayNextRoomRetry=GetWorld()->GetTimeSeconds()+15.;
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->SyncRoom(Room,[Weak,Room](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid())return;auto* D=Weak.Get();D->GatewayRoomPending=false;if(R.Superseded)return;
        D->RecordGatewayResult(TEXT("Laboratorio stanza ")+Room,R);
        const FString Actual=JsonString(R.Data,TEXT("room_id"));
        D->GatewayRoomReady=R.Success&&R.LiveCML&&(Actual.IsEmpty()||Actual==Room);
        if(D->GatewayRoomReady)
        {
            const FString NewRoom=Actual.IsEmpty()?Room:Actual;const bool Changed=D->GatewayRoom!=NewRoom;
            D->GatewayNextRoomRetry=0;D->GatewayRoom=NewRoom;
            if(Changed||!D->GatewayCustomNode)D->GatewayNode=DefaultNode(NewRoom);
            if(Changed)D->GatewayCustomNode=false;
            D->GatewayIOSConfirmed=false;
        }
    });
}
void AClaraStoryDirector::TickGateway()
{
    if(!Gateway||!Gateway->Enabled()||!GetWorld())return;
    const double Now=GetWorld()->GetTimeSeconds();if(Now<GatewayNextPoll)return;GatewayNextPoll=Now+.25;
    const FString Room=CurrentGatewayRoom();
    if(GatewayGeneratedLab){if(Room==GatewayRoomCandidate)return;GatewayGeneratedLab=false;}
    if(Room!=GatewayRoomCandidate){GatewayRoomCandidate=Room;GatewayRoomCandidateSince=Now;}
    if(GatewayHealthy&&GatewayBootstrapped&&!GatewayRoomPending&&Now>=GatewayNextRoomRetry&&Now-GatewayRoomCandidateSince>=.8&&(GatewayRoom!=Room||!GatewayRoomReady))RequestGatewayRoom(Room);
    PollGatewayPatches();
}
void AClaraStoryDirector::GatewaySetNode(const FString& Value)
{
    FString Node=Value.TrimStartAndEnd();
    if(Node.IsEmpty()||Node.Len()>128||Node.Contains(TEXT("\n"))||Node.Contains(TEXT("\r"))){Notify(TEXT("Inserisci il nome esatto di un nodo CML, per esempio SW-PIANO1."));return;}
    GatewayNode=Node;GatewayCustomNode=true;GatewayIOSConfirmed=false;
    GatewayMessage=TEXT("Nodo selezionato: ")+Node+TEXT(". Il prossimo comando verra' inviato a questo nodo, senza riscrivere i comandi IOS.");
}
void AClaraStoryDirector::PollGatewayPatches()
{
    if(!Gateway||!GatewayHealthy||!GatewayRoomReady||GatewayRoomPending||GatewayRoom!=TEXT("office"))return;
    auto* Switch=Devices.FindRef(TEXT("SW1")).Get();if(!Switch)return;
    for(const auto& P:PortBridge)
    {
        const bool Now=Switch->IsPortOccupied(P.Asset);const bool* Previous=GatewayObservedPorts.Find(P.Asset);
        // The first snapshot is observation only: never send four implicit unplug operations at startup.
        if(!Previous){GatewayObservedPorts.Add(P.Asset,Now);continue;}
        if(*Previous==Now)continue;
        GatewayObservedPorts.Add(P.Asset,Now);GatewayPatchPort(P.Gateway,Now);
    }
}
void AClaraStoryDirector::GatewayPatchPort(const FString& PortId,bool Connected)
{
    if(!Gateway||!GatewayHealthy||!GatewayRoomReady||GatewayRoomPending||GatewayRoom!=TEXT("office")||GatewayPortRepairShuttingDown)
    {Notify(TEXT("Prima sincronizza il laboratorio office. Nessuna modifica remota inviata."));return;}
    bool Mapped=false;for(const auto& P:PortBridge)if(PortId==P.Gateway){Mapped=true;break;}
    if(!Mapped){Notify(TEXT("Questa porta non ha una mappatura CML: il collegamento rimane locale."));return;}
    GatewayAcknowledgedPorts.Remove(PortId);GatewayPortProofMethods.Remove(PortId);
    // Keep the last desired state; a running IOS transaction must finish before a reversing request.
    const int32 Existing=GatewayPendingPortRepairs.IndexOfByPredicate([&](const auto& P){return P.Key==PortId;});
    if(GatewayPortRepairBusy&&GatewayPortRepairPort==PortId&&GatewayPortRepairConnected==Connected)
    {if(Existing!=INDEX_NONE)GatewayPendingPortRepairs.RemoveAt(Existing);return;}
    if(Existing!=INDEX_NONE)GatewayPendingPortRepairs[Existing].Value=Connected;else GatewayPendingPortRepairs.Add({PortId,Connected});
    ProcessGatewayPortRepair();
}
void AClaraStoryDirector::ProcessGatewayPortRepair()
{
    if(GatewayPortRepairBusy||GatewayPortRepairShuttingDown||GatewayPendingPortRepairs.IsEmpty())return;
    if(!Gateway||!GatewayHealthy||!GatewayRoomReady||GatewayRoomPending||GatewayRoom!=TEXT("office"))return;
    const auto Job=GatewayPendingPortRepairs[0];GatewayPendingPortRepairs.RemoveAt(0);
    GatewayPortRepairPort=Job.Key;GatewayPortRepairConnected=Job.Value;GatewayPortRepairInterface.Empty();
    FString AssetPort;for(const auto& P:PortBridge)if(Job.Key==P.Gateway){AssetPort=P.Asset;GatewayPortRepairInterface=P.IOSInterface;break;}
    if(GatewayPortRepairInterface.IsEmpty())return;
    GatewayPortRepairBusy=true;GatewayPortRepairReconnectTried=false;GatewayPortRepairEvidence.Empty();const uint64 Token=++GatewayPortRepairGeneration;
    if(GatewayLegacyPatchBroken){StepGatewayPortRepair(0,Token);return;}
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Patch(Job.Key,TEXT("ue-SW1-")+AssetPort,Job.Value,[Weak,Token](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid())return;auto* D=Weak.Get();if(Token!=D->GatewayPortRepairGeneration||!D->GatewayPortRepairBusy||D->GatewayPortRepairShuttingDown)return;
        D->GatewayPortRepairEvidence.Add(RepairEvidence(TEXT("legacy /api/rack/patch"),R));
        bool Up=false;const bool Echo=R.Data.IsValid()&&R.Data->TryGetBoolField(TEXT("up"),Up)&&Up==D->GatewayPortRepairConnected;
        const FString Link=JsonString(R.Data,TEXT("link_id")),ActualNode=JsonString(R.Data,TEXT("node"));
        if(R.Success&&R.LiveCML&&Echo&&!Link.IsEmpty()&&ActualNode==DefaultNode(TEXT("office")))
        {D->FinishGatewayPortRepair(true,TEXT("Il gateway ha confermato il link CML sul nodo corretto."),TEXT("CML_LINK"));return;}
        if(R.Data.IsValid()&&(Link.IsEmpty()||ActualNode!=DefaultNode(TEXT("office"))))
        {
            D->GatewayLegacyPatchBroken=true;
            D->GatewayMessage=TEXT("Patch legacy non verificata: applico lo stato amministrativo sulla porta IOS reale e ne rileggo lo stato. Nessun nuovo link CML dichiarato.");
            D->StepGatewayPortRepair(0,Token);return;
        }
        D->FinishGatewayPortRepair(false,TEXT("Patch remota non confermata: ")+R.Error);
    });
}
void AClaraStoryDirector::StepGatewayPortRepair(int32 Step,uint64 Token)
{
    if(!Gateway||Token!=GatewayPortRepairGeneration||!GatewayPortRepairBusy||GatewayPortRepairShuttingDown)return;
    if(Step<0||Step>1){FinishGatewayPortRepair(false,TEXT("Fase IOS non valida."));return;}
    const FString Query=TEXT("show interfaces ")+GatewayPortRepairInterface+TEXT(" | include ^")+GatewayPortRepairInterface;
    FString Batch;Batch.AppendChar(static_cast<TCHAR>(0x1a));
    Batch+=TEXT("enable\rconfigure terminal\rinterface ")+GatewayPortRepairInterface+TEXT("\r")+(GatewayPortRepairConnected?TEXT("no shutdown"):TEXT("shutdown"))+TEXT("\rend\r")+Query;
    const FString Command=Step==0?Batch:Query;TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    FClaraGatewayCallback Complete=[Weak,Token,Step,Command](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid())return;auto* D=Weak.Get();if(Token!=D->GatewayPortRepairGeneration||!D->GatewayPortRepairBusy||D->GatewayPortRepairShuttingDown)return;
        D->GatewayPortRepairEvidence.Add(RepairEvidence(Command,R));
        const FString Output=JsonString(R.Data,TEXT("output")),Node=JsonString(R.Data,TEXT("node")),Prompt=LastIOSPrompt(Output);
        if(Step==0&&!D->GatewayPortRepairReconnectTried&&Output.Contains(TEXT("consoles>"),ESearchCase::IgnoreCase))
        {
            D->GatewayPortRepairReconnectTried=true;
            // This is the one known office console path, never a path or shell command supplied by the player.
            const FString Open=TEXT("open /FuoriRete_Room_Office/SW-PIANO1/0");
            D->Gateway->Terminal(DefaultNode(TEXT("office")),Open,[Weak,Token](const FClaraGatewayResponse& Attach)
            {
                if(!Weak.IsValid())return;auto* Owner=Weak.Get();
                if(Token!=Owner->GatewayPortRepairGeneration||!Owner->GatewayPortRepairBusy||Owner->GatewayPortRepairShuttingDown)return;
                Owner->GatewayPortRepairEvidence.Add(RepairEvidence(TEXT("TermServ attach: open /FuoriRete_Room_Office/SW-PIANO1/0"),Attach));
                const FString Text=JsonString(Attach.Data,TEXT("output")),AttachedNode=JsonString(Attach.Data,TEXT("node")),IOSPrompt=LastIOSPrompt(Text);
                const bool NodePrompt=IOSPrompt.StartsWith(TEXT("SW-PIANO1"))&&(IOSPrompt.EndsWith(TEXT(">"))||IOSPrompt.EndsWith(TEXT("#")));
                const bool TransportAck=Text.Contains(TEXT("Connecting to console for SW-PIANO1"))&&Text.Contains(TEXT("Connected to CML terminalserver"));
                const bool Attached=Attach.Success&&Attach.LiveCML&&!Attach.Fallback&&AttachedNode==DefaultNode(TEXT("office"))&&!Text.Contains(TEXT("consoles>"),ESearchCase::IgnoreCase)&&!Text.Contains(TEXT("consoles#"),ESearchCase::IgnoreCase)&&!Text.Contains(TEXT("(OK)"))&&(NodePrompt||TransportAck);
                if(!Attached){Owner->FinishGatewayPortRepair(false,TEXT("Unico tentativo di collegamento alla console IOS non confermato. Nessuna porta dichiarata aggiornata."));return;}
                // A transport acknowledgement is not IOS evidence. Retry the batch and require IOS prompts plus independent interface readback.
                Owner->StepGatewayPortRepair(0,Token);
            });
            return;
        }
        const bool Invalid=Output.Contains(TEXT("% Invalid"),ESearchCase::IgnoreCase)||Output.Contains(TEXT("% Incomplete"),ESearchCase::IgnoreCase)||Output.Contains(TEXT("% Ambiguous"),ESearchCase::IgnoreCase);
        const bool Live=R.Success&&R.LiveCML&&!R.Fallback&&Node==DefaultNode(TEXT("office"))&&!Output.IsEmpty()&&!Output.Contains(TEXT("(OK)"))&&!Output.Contains(TEXT("[SIM]"))&&!Output.Contains(TEXT("consoles>"),ESearchCase::IgnoreCase)&&!Output.Contains(TEXT("consoles#"),ESearchCase::IgnoreCase);
        bool Expected=Live&&!Invalid&&Prompt.EndsWith(TEXT("#"))&&!Prompt.Contains(TEXT("("));
        if(Step==0)
        {
            const FString InterfaceCommand=TEXT("interface ")+D->GatewayPortRepairInterface;
            bool ExactInterfaceEcho=false;TArray<FString> Lines;Output.ParseIntoArrayLines(Lines);
            for(const auto& Line:Lines)
                if(Line.TrimStartAndEnd().EndsWith(InterfaceCommand,ESearchCase::IgnoreCase))ExactInterfaceEcho=true;
            Expected=Expected&&Output.Contains(TEXT("(config)#"))&&Output.Contains(TEXT("(config-if)#"))&&ExactInterfaceEcho;
        }
        else Expected=Expected&&IOSPortReadback(Output,D->GatewayPortRepairInterface,D->GatewayPortRepairConnected);
        if(Output.Contains(TEXT("Password:"),ESearchCase::IgnoreCase)||Output.Contains(TEXT("Authorization failed"),ESearchCase::IgnoreCase))Expected=false;
        if(!Expected)
        {D->FinishGatewayPortRepair(false,TEXT("IOS non conferma '")+Command+TEXT("'. Nessuna porta remota dichiarata aggiornata; riprova con un'azione esplicita. ")+R.Error);return;}
        if(Step==1)
        {
            D->GatewayIOSConfirmed=true;
            D->FinishGatewayPortRepair(true,TEXT("Stato amministrativo IOS e lettura indipendente confermati su SW-PIANO1 / ")+D->GatewayPortRepairInterface+TEXT(". Il link CML non e' stato ricablato."),TEXT("IOS_ADMIN"));return;
        }
        D->StepGatewayPortRepair(1,Token);
    };
    if(Step==0)
    {
        // One byte burst reduces cross-request console-mode races; this is not a server-side transaction lock.
        auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("node"),DefaultNode(TEXT("office")));Body->SetStringField(TEXT("command"),Command);
        Gateway->Request(TEXT("POST"),TEXT("/api/terminal/command"),Body,MoveTemp(Complete),30.f);
    }
    else Gateway->Terminal(DefaultNode(TEXT("office")),Command,MoveTemp(Complete));
}
void AClaraStoryDirector::FinishGatewayPortRepair(bool Success,const FString& Reason,const FString& Method)
{
    const FString Port=GatewayPortRepairPort;
    if(Success){GatewayAcknowledgedPorts.Add(Port,GatewayPortRepairConnected);GatewayPortProofMethods.Add(Port,Method);}
    else {GatewayAcknowledgedPorts.Remove(Port);GatewayPortProofMethods.Remove(Port);}
    auto Proof=MakeShared<FJsonObject>();Proof->SetBoolField(TEXT("confirmed"),Success);Proof->SetStringField(TEXT("proof_method"),Method);Proof->SetStringField(TEXT("port_id"),Port);Proof->SetStringField(TEXT("node"),DefaultNode(TEXT("office")));Proof->SetStringField(TEXT("interface"),GatewayPortRepairInterface);Proof->SetBoolField(TEXT("requested_connected"),GatewayPortRepairConnected);Proof->SetBoolField(TEXT("cml_link_rewired"),Success&&Method==TEXT("CML_LINK"));Proof->SetStringField(TEXT("detail"),Reason);Proof->SetBoolField(TEXT("termserv_attach_attempted"),GatewayPortRepairReconnectTried);Proof->SetStringField(TEXT("console_transport"),TEXT("one bounded IOS byte burst, then a separate interface read; no server-side atomic lock claimed"));
    TArray<TSharedPtr<FJsonValue>> Evidence;for(const auto& E:GatewayPortRepairEvidence)Evidence.Add(MakeShared<FJsonValueObject>(E));Proof->SetArrayField(TEXT("responses"),Evidence);
    GatewayMessage=Success?TEXT("Porta remota confermata · ")+Method:TEXT("Porta remota non confermata");GatewayResult=GatewayMessage+TEXT("\n")+Reason+TEXT("\n")+JsonText(Proof);
    GatewayTranscript+=TEXT("[PORT SYNC / ")+Port+TEXT(" / ")+Method+TEXT("] ")+Reason+TEXT("\n");Notify((GatewayMessage+TEXT(" · ")+Port).Left(240));
    UE_LOG(LogTemp,Display,TEXT("CLARA_GATEWAY_PORT_%s: %s %s method=%s"),Success?TEXT("PASS"):TEXT("FAIL"),*Port,*GatewayPortRepairInterface,*Method);
    GatewayPortRepairBusy=false;GatewayPortRepairEvidence.Empty();GatewayPortRepairPort.Empty();GatewayPortRepairInterface.Empty();
    ProcessGatewayPortRepair();
}
bool AClaraStoryDirector::RunGatewayPortRepairSelfTest()
{
    bool OK=IOSPortReadback(TEXT("Ethernet0/1 is up, line protocol is up (connected)\nSW-PIANO1#"),TEXT("Ethernet0/1"),true);
    OK&=IOSPortReadback(TEXT("Ethernet0/1 is administratively down, line protocol is down\nSW-PIANO1#"),TEXT("Ethernet0/1"),false);
    OK&=!IOSPortReadback(TEXT("SW-PIANO1# (OK)"),TEXT("Ethernet0/1"),true);
    OK&=!IOSPortReadback(TEXT("Ethernet0/10 is up, line protocol is up"),TEXT("Ethernet0/1"),true);
    OK&=!IOSPortReadback(TEXT("Ethernet0/1 is up, line protocol is down"),TEXT("Ethernet0/1"),true);
    OK&=!IOSPortReadback(TEXT("Ethernet0/1 is down, line protocol is down"),TEXT("Ethernet0/1"),false);
    OK&=!IOSPortReadback(TEXT("consoles>\nEthernet0/1 is up, line protocol is up"),TEXT("Ethernet0/1"),true);
    UE_LOG(LogTemp,Display,TEXT("CLARA_GATEWAY_PORT_PROOF_%s: exact-interface readback; admin-down and placeholders rejected appropriately"),OK?TEXT("PASS"):TEXT("FAIL"));return OK;
}
FString AClaraStoryDirector::RunGatewayCommand(const FString& Command)
{
    if(GatewayPortRepairBusy)return TEXT("Sincronizzazione della porta IOS in corso. Attendi la lettura di conferma prima di usare la console.");
    if(!Gateway||!GatewayHealthy||!GatewayRoomReady||GatewayRoomPending)return TEXT("CML non pronto. Aggiorna lo stato e sincronizza la stanza; nessun comando locale viene usato come ripiego.");
    if(Command.Len()>4096)return TEXT("Comando troppo lungo.");
    const FString Node=GatewayNode,Room=GatewayRoom,Key=SessionKey(Room,Node);
    GatewayTranscript+=TEXT("CML / ")+Room+TEXT(" / ")+Node+TEXT(" > ")+Command+TEXT("\n");
    Transcript+=TEXT("CML / ")+Node+TEXT(" > ")+Command+TEXT("\n");
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Terminal(Node,Command,[Weak,Node,Key,Command](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Console ")+Node,R);
        FString Output=JsonString(R.Data,TEXT("output"));
        if(!R.Success||!R.LiveCML)Output=TEXT("[ESECUZIONE IOS NON CONFERMATA] ")+R.Error+TEXT("\n")+Output;
        else
        {
            const bool Recognized=IOSOutput(Output);D->GatewayIOSConfirmed=Recognized;
            if(Recognized&&!Output.Contains(TEXT("% Invalid"))&&!Output.Contains(TEXT("% Incomplete"))&&!Output.Contains(TEXT("% Ambiguous"))&&!Command.TrimStartAndEnd().IsEmpty())D->GatewayCommands.FindOrAdd(Key).Add(Command);
            if(!Recognized)Output=TEXT("[RISPOSTA DEL GATEWAY · IOS NON ANCORA VERIFICATO]\n")+Output;
        }
        const FString Entry=TEXT("[")+(!R.Source.IsEmpty()?R.Source:R.Mode)+TEXT(" / ")+Node+TEXT("]\n")+Output+TEXT("\n\n");
        D->GatewayTranscript+=Entry;D->Transcript+=Entry;
        if(D->GatewayTranscript.Len()>40000)D->GatewayTranscript.RightChopInline(D->GatewayTranscript.Len()-30000);
        if(D->Transcript.Len()>40000)D->Transcript.RightChopInline(D->Transcript.Len()-30000);
    });
    return TEXT("Richiesta inviata al gateway. L'output remoto comparira' qui; nessuna esecuzione locale sostitutiva.");
}
TSharedPtr<FJsonObject> AClaraStoryDirector::GatewayPlayerState()const
{
    auto State=MakeShared<FJsonObject>();auto Ports=MakeShared<FJsonObject>();auto AdminPorts=MakeShared<FJsonObject>();auto Methods=MakeShared<FJsonObject>();
    if(GatewayRoom==TEXT("office"))for(const auto& P:GatewayAcknowledgedPorts)
    {
        const FString Method=GatewayPortProofMethods.FindRef(P.Key);Methods->SetStringField(P.Key,Method);
        if(Method==TEXT("CML_LINK"))Ports->SetBoolField(P.Key,P.Value);
        else if(Method==TEXT("IOS_ADMIN"))AdminPorts->SetBoolField(P.Key,P.Value);
    }
    State->SetObjectField(TEXT("patched_ports"),Ports);State->SetObjectField(TEXT("ios_admin_ports"),AdminPorts);State->SetObjectField(TEXT("port_proof_methods"),Methods);
    State->SetStringField(TEXT("ios_admin_limit"),TEXT("IOS_ADMIN proves shutdown/no shutdown and an independent interface-status read on fixed CML topology. It is not a CML cable/link rewire and does not satisfy physical rack patched_ports."));
    TArray<TSharedPtr<FJsonValue>> Domains;for(const auto& Domain:GatewayBlockedDomains)Domains.Add(MakeShared<FJsonValueString>(Domain));State->SetArrayField(TEXT("blocked_domains"),Domains);
    State->SetBoolField(TEXT("endpoint_isolated"),false);State->SetStringField(TEXT("session_id"),GatewaySessionId);State->SetStringField(TEXT("room_id"),GatewayRoom);State->SetStringField(TEXT("node"),GatewayNode);
    State->SetStringField(TEXT("evidence"),TEXT("Only this game session's acknowledged patch actions, sinkhole requests and recognized IOS command replies; not shared server telemetry."));
    return State;
}
void AClaraStoryDirector::GatewayVerifyMission(const FString& MissionId)
{
    if(!Gateway||!Gateway->Enabled())return;
    const FString Id=MissionId.TrimStartAndEnd();if(Id.IsEmpty()){Notify(TEXT("Scegli una missione da verificare."));return;}
    TArray<FString> Commands=GatewayCommands.FindRef(SessionKey(GatewayRoom,GatewayNode));
    // The deployed verifier substitutes shared telemetry for []; an explicit inert line preserves an empty local submission.
    if(Commands.IsEmpty())Commands.Add(TEXT("! unreal-session-no-command-evidence"));
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Verify(Id,Commands,GatewayPlayerState(),[Weak,Id](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Verifica consegna ")+Id,R);
        D->GatewayResult=TEXT("VERIFICA DELLA CONSEGNA DI QUESTA SESSIONE\nIl verificatore confronta comandi e azioni inviati con una specifica. Questo esito non e' una lettura indipendente della running-config IOS e non sblocca le missioni locali.\n\n")+D->GatewayResult;
    });
}
void AClaraStoryDirector::GatewayHint(const FString& Question,const FString& Character)
{
    if(!Gateway||!Gateway->Enabled())return;
    const FString Id=Character.Equals(TEXT("Jonny"),ESearchCase::IgnoreCase)||Character.Equals(TEXT("Jonathan"),ESearchCase::IgnoreCase)?TEXT("ciro"):Character.Equals(TEXT("Alessia"),ESearchCase::IgnoreCase)?TEXT("rosaria"):TEXT("marco");
    TArray<FString> Lines;GatewayTranscript.Right(3500).ParseIntoArrayLines(Lines);
    for(auto& Line:Lines)if(Line.Contains(TEXT("password"),ESearchCase::IgnoreCase)||Line.Contains(TEXT("secret"),ESearchCase::IgnoreCase))Line=TEXT("[comando con credenziali omesso dal contesto AI]");
    const FString Context=GetObjective()+TEXT("\nStanza ")+GatewayRoom+TEXT("; nodo ")+GatewayNode+TEXT("\n")+FString::Join(Lines,TEXT("\n"));
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->AI(TEXT("hint"),Id,Question,TEXT("terminal"),Context,[Weak,Character](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Indizio AI · ")+Character,R);
        D->GatewayResult=TEXT("SUGGERIMENTO AI · ")+Character+TEXT("\nNon sostituisce le prove richieste dalla missione.\n\n")+D->GatewayResult;
    });
}
void AClaraStoryDirector::GatewayBlockDomain(const FString& Value,bool Block)
{
    if(!Gateway||!Gateway->Enabled())return;const FString Domain=Value.TrimStartAndEnd().ToLower();
    if(Domain.IsEmpty()||Domain.Len()>253||Domain.Contains(TEXT("/"))||Domain.Contains(TEXT(" "))||Domain.Contains(TEXT("\n"))){Notify(TEXT("Inserisci solo il dominio, senza indirizzo web o spazi."));return;}
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Sinkhole(Domain,Block,[Weak,Domain,Block](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Sinkhole DNS · ")+Domain,R);
        bool OK=false;if(R.Success&&R.Data.IsValid()&&R.Data->TryGetBoolField(TEXT("ok"),OK)&&OK)
        {if(Block)D->GatewayBlockedDomains.Add(Domain);else D->GatewayBlockedDomains.Remove(Domain);}
        D->GatewayResult=TEXT("AZIONE SUL SINKHOLE DEL GATEWAY\nIl servizio aggiorna la propria blocklist; questa risposta non dimostra un'integrazione con il prodotto Cisco Umbrella ne' l'isolamento di un endpoint.\n\n")+D->GatewayResult;
    });
}
void AClaraStoryDirector::GatewayGenerate(const FString& Value,bool DeployToCML)
{
    if(!Gateway||!Gateway->Enabled())return;
    if(GatewayPortRepairBusy){Notify(TEXT("Attendi la conferma della porta IOS prima di generare un altro laboratorio."));return;}
    const FString Prompt=Value.TrimStartAndEnd();
    if(Prompt.IsEmpty()||Prompt.Len()>6000){Notify(TEXT("Descrivi la missione da generare in un testo breve."));return;}
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Generate(Prompt,DeployToCML,[Weak,DeployToCML](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(DeployToCML?TEXT("Generazione e richiesta di deploy"):TEXT("Generazione senza deploy"),R);
        if(DeployToCML&&R.Success){D->GatewayGeneratedLab=true;D->GatewayRoomCandidate=D->CurrentGatewayRoom();D->GatewayRoomReady=false;D->GatewayIOSConfirmed=false;D->GatewayMessage+=TEXT(" · autosincronizzazione sospesa per non sostituire il lab generato; Aggiorna ripristina il lab della stanza");}
        D->GatewayResult=TEXT("MISSIONE GENERATA DAL SERVIZIO\nLa topologia generata non modifica automaticamente stanze, apparati 3D o obiettivi del Giorno 1/2. Leggi l'esito effettivo del deploy nella risposta.\n\n")+D->GatewayResult;
    });
}
