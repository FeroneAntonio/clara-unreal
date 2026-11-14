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
struct FClaraPortBridge {const TCHAR* Asset;const TCHAR* Gateway;};
// Explicit adapter, not matching interface names: the gateway selects the first switch of the active room.
const FClaraPortBridge PortBridge[]={{TEXT("Gi1/0/1"),TEXT("sw-g1")},{TEXT("Gi1/0/2"),TEXT("sw-1")},{TEXT("Gi1/0/3"),TEXT("sw-2")},{TEXT("Gi1/0/4"),TEXT("sw-3")}};
FString JsonText(const TSharedPtr<FJsonObject>& Object)
{
    FString Text;if(Object.IsValid())FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text));return Text;
}
FString JsonString(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key)
{FString Value;if(Object.IsValid())Object->TryGetStringField(Key,Value);return Value;}
FString DefaultNode(const FString& Room)
{return Room==TEXT("corridor")?TEXT("SW-ACC"):Room==TEXT("datacenter_vault")?TEXT("SW-DC1"):TEXT("SW-PIANO1");}
FString SessionKey(const FString& Room,const FString& Node){return Room+TEXT("|")+Node;}
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
    const bool Allow=!FApp::IsUnattended()&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraStoryCheck"))&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraLayoutCheck"));
    Gateway->Initialize(Allow);
    if(auto* Switch=Devices.FindRef(TEXT("SW1")).Get())for(const auto& P:PortBridge)GatewayObservedPorts.Add(P.Asset,Switch->IsPortOccupied(P.Asset));
    GatewayMessage=TEXT("Il banco dei Giorni 1 e 2 resta un laboratorio locale. Il terminale CML e' una sessione remota separata.");
    if(Gateway->Enabled())GatewayRefresh();
}
void AClaraStoryDirector::GatewayShutdown(){if(Gateway)Gateway->Shutdown();}
bool AClaraStoryDirector::IsGatewayLive()const{return Gateway&&Gateway->IsLive()&&GatewayRoomReady;}
FString AClaraStoryDirector::GetGatewayStatus()const
{
    if(!Gateway)return TEXT("Gateway non inizializzato · esercizi locali");
    return Gateway->GetStatusLabel()+TEXT("\nStanza: ")+(GatewayRoom.IsEmpty()?TEXT("da sincronizzare"):GatewayRoom)+TEXT(" · nodo: ")+GatewayNode+
        (GatewayRoomPending?TEXT(" · cambio laboratorio in corso"):TEXT(""))+
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
    if(!Gateway||!GatewayHealthy||GatewayRoomPending)return;
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
    if(!Gateway||!Gateway->IsLive()||!GatewayRoomReady||GatewayRoomPending||GatewayRoom!=TEXT("office"))return;
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
    if(!Gateway||!GatewayHealthy||!GatewayRoomReady||GatewayRoomPending||GatewayRoom!=TEXT("office"))
    {Notify(TEXT("Prima sincronizza il laboratorio office. Nessuna modifica remota inviata."));return;}
    const FClaraPortBridge* Mapping=nullptr;for(const auto& P:PortBridge)if(PortId==P.Gateway){Mapping=&P;break;}
    if(!Mapping){Notify(TEXT("Questa porta non ha una mappatura CML: il collegamento rimane locale."));return;}
    const FString AssetPort=Mapping->Asset,CableId=TEXT("ue-SW1-")+AssetPort;const FString Room=GatewayRoom;
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Patch(PortId,CableId,Connected,[Weak,PortId,Connected,Room,AssetPort](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(TEXT("Patch ")+AssetPort+TEXT(" → ")+PortId,R);
        bool Up=false;const bool Echo=R.Data.IsValid()&&R.Data->TryGetBoolField(TEXT("up"),Up)&&Up==Connected;
        const FString Link=JsonString(R.Data,TEXT("link_id")),ActualNode=JsonString(R.Data,TEXT("node"));
        if(R.Success&&R.LiveCML&&Echo&&!Link.IsEmpty()&&ActualNode==DefaultNode(Room)&&D->GatewayRoom==Room)D->GatewayAcknowledgedPorts.Add(PortId,Connected);
        else D->GatewayMessage+=TEXT(" · applicazione remota non confermata; il cavo 3D resta locale");
    });
}
FString AClaraStoryDirector::RunGatewayCommand(const FString& Command)
{
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
    auto State=MakeShared<FJsonObject>();auto Ports=MakeShared<FJsonObject>();
    if(GatewayRoom==TEXT("office"))for(const auto& P:GatewayAcknowledgedPorts)Ports->SetBoolField(P.Key,P.Value);
    State->SetObjectField(TEXT("patched_ports"),Ports);
    TArray<TSharedPtr<FJsonValue>> Domains;for(const auto& Domain:GatewayBlockedDomains)Domains.Add(MakeShared<FJsonValueString>(Domain));State->SetArrayField(TEXT("blocked_domains"),Domains);
    State->SetBoolField(TEXT("endpoint_isolated"),false);State->SetStringField(TEXT("session_id"),GatewaySessionId);State->SetStringField(TEXT("room_id"),GatewayRoom);State->SetStringField(TEXT("node"),GatewayNode);
    State->SetStringField(TEXT("evidence"),TEXT("Only this game session's acknowledged patch actions, sinkhole requests and recognized IOS command replies; not shared server telemetry."));
    return State;
}
void AClaraStoryDirector::GatewayVerifyMission(const FString& MissionId)
{
    if(!Gateway||!Gateway->Enabled())return;
    const FString Id=MissionId.TrimStartAndEnd();if(Id.IsEmpty()){Notify(TEXT("Scegli una missione da verificare."));return;}
    const TArray<FString> Commands=GatewayCommands.FindRef(SessionKey(GatewayRoom,GatewayNode));
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
    if(!Gateway||!Gateway->Enabled())return;const FString Prompt=Value.TrimStartAndEnd();
    if(Prompt.IsEmpty()||Prompt.Len()>6000){Notify(TEXT("Descrivi la missione da generare in un testo breve."));return;}
    TWeakObjectPtr<AClaraStoryDirector> Weak(this);
    Gateway->Generate(Prompt,DeployToCML,[Weak,DeployToCML](const FClaraGatewayResponse& R)
    {
        if(!Weak.IsValid()||R.Superseded)return;auto* D=Weak.Get();D->RecordGatewayResult(DeployToCML?TEXT("Generazione e richiesta di deploy"):TEXT("Generazione senza deploy"),R);
        if(DeployToCML&&R.Success){D->GatewayGeneratedLab=true;D->GatewayRoomCandidate=D->CurrentGatewayRoom();D->GatewayRoomReady=false;D->GatewayIOSConfirmed=false;D->GatewayMessage+=TEXT(" · autosincronizzazione sospesa per non sostituire il lab generato; Aggiorna ripristina il lab della stanza");}
        D->GatewayResult=TEXT("MISSIONE GENERATA DAL SERVIZIO\nLa topologia generata non modifica automaticamente stanze, apparati 3D o obiettivi del Giorno 1/2. Leggi l'esito effettivo del deploy nella risposta.\n\n")+D->GatewayResult;
    });
}
