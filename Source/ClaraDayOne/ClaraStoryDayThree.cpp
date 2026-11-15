#include "ClaraStoryDirector.h"
#include "ClaraAudio.h"
#include "ClaraNetworkDevice.h"
#include "ClaraRoomVisitor.h"
#include "ClaraStoryCast.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ClaraStoryDayThreeDialogue.inl"

namespace
{
struct FDayThreeCable { const TCHAR* A; const TCHAR* PortA; const TCHAR* B; const TCHAR* PortB; };
const FDayThreeCable DayThreeCables[] = {
    {TEXT("PC2"),TEXT("Ethernet"),TEXT("SW1"),TEXT("Gi1/0/1")},
    {TEXT("R1"),TEXT("Gi0/0"),TEXT("SW1"),TEXT("Gi1/0/2")},
    {TEXT("R1"),TEXT("Gi0/1"),TEXT("R2"),TEXT("Gi0/0")},
    {TEXT("R2"),TEXT("Gi0/1"),TEXT("SW2"),TEXT("Gi1/0/1")}
};
FString StateText(const TSharedRef<FJsonObject>& State)
{
    FString Result;FJsonSerializer::Serialize(State,TJsonWriterFactory<>::Create(&Result));return Result;
}
}

void AClaraStoryDirector::SetupDayThreeLab(bool ConnectInitial)
{
    auto Spawn=[this](const TCHAR* Id,const TCHAR* Model,FVector At)
    {
        if(auto* Existing=Devices.FindRef(Id).Get()){Existing->SetActorHiddenInGame(false);Existing->SetActorEnableCollision(true);return Existing;}
        auto* Device=GetWorld()->SpawnActor<AClaraNetworkDevice>(At,FRotator::ZeroRotator);
        Device->Initialize(Model);Device->Tags.Add(TEXT("StoryNode"));Device->Tags.Add(FName(Id));Devices.Add(Id,Device);return Device;
    };
    // A second rack beside the original server equipment, with independently selectable devices.
    Spawn(TEXT("R2"),TEXT("Cisco1941"),FVector(1740,1000,120));
    Spawn(TEXT("SW2"),TEXT("Cisco2960X"),FVector(1740,1000,140));
    if(!DayThreeRack)
    {
        DayThreeRack=GetWorld()->SpawnActor<AActor>();
        auto* Root=NewObject<USceneComponent>(DayThreeRack);DayThreeRack->AddInstanceComponent(Root);DayThreeRack->SetRootComponent(Root);Root->RegisterComponent();
        auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Office.M_Office"));
        auto* Material=UMaterialInstanceDynamic::Create(Base,DayThreeRack);Material->SetVectorParameterValue(TEXT("Color"),FLinearColor(.055f,.065f,.07f));Material->SetVectorParameterValue(TEXT("BaseColor"),FLinearColor(.055f,.065f,.07f));
        auto Beam=[&](FVector At,FVector Size)
        {
            auto* Mesh=NewObject<UStaticMeshComponent>(DayThreeRack);DayThreeRack->AddInstanceComponent(Mesh);Mesh->SetupAttachment(Root);Mesh->SetStaticMesh(Cube);Mesh->SetMaterial(0,Material);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();Mesh->SetWorldLocation(At);Mesh->SetWorldScale3D(Size/100);
        };
        for(float X:{1710.f,1770.f})for(float Y:{980.f,1020.f})Beam({X,Y,105},{3,3,190});
        for(float Z:{15.f,118.f,138.f,200.f})Beam({1740,1000,Z},{63,45,2});
        Beam({1450,980,76},{110,65,3});
        for(float X:{1403.f,1497.f})for(float Y:{955.f,1005.f})Beam({X,Y,38},{4,4,76});
        Beam({1765,1320,76},{200,100,3});
        for(float X:{1675.f,1855.f})for(float Y:{1278.f,1362.f})Beam({X,Y,38},{4,4,76});
        auto* Label=NewObject<UTextRenderComponent>(DayThreeRack);DayThreeRack->AddInstanceComponent(Label);Label->SetupAttachment(Root);Label->SetText(FText::FromString(TEXT("RACK 02\nR2  /  SW2")));Label->SetWorldSize(5);Label->SetHorizontalAlignment(EHTA_Center);Label->RegisterComponent();Label->SetWorldLocationAndRotation({1740,975,185},{0,-90,0});
    }
    DayThreeRack->SetActorHiddenInGame(false);
    if(auto* R1=Devices.FindRef(TEXT("R1")).Get())R1->SetActorLocationAndRotation({1505,1176,160},FRotator::ZeroRotator);
    if(auto* SW1=Devices.FindRef(TEXT("SW1")).Get())SW1->SetActorLocationAndRotation({1505,1176,180},FRotator::ZeroRotator);
    if(auto* PC2=Devices.FindRef(TEXT("PC2")).Get())PC2->SetActorLocationAndRotation({1450,980,78},FRotator::ZeroRotator);
    for(TActorIterator<AClaraNetworkDevice> It(GetWorld());It;++It)
    {
        bool Tracked=false;for(const auto& Pair:Devices)if(Pair.Value.Get()==*It){Tracked=true;break;}
        if(!It->IsPreview()&&!Tracked){It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
    }
    for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)
    {
        const FString Id=It->GetPropId();
        for(int32 I=0;I<3;++I)
        {
            const FString Kind=I==0?TEXT("ethernet"):I==1?TEXT("console"):TEXT("fiber");
            if(Id==TEXT("Box_")+Kind)It->SetActorLocation({1710.f+I*55.f,1340,79});
            if(Id==TEXT("Cable_")+Kind)It->SetActorLocation({1710.f+I*55.f,1300,79});
        }
    }
    if(auto* Uplink=Devices.FindRef(TEXT("UPLINK")).Get()){Uplink->SetActorHiddenInGame(true);Uplink->SetActorEnableCollision(false);}
    // Existing actors remain the identity of R1, SW1 and PC2: no sixth network node is created.
    if(ConnectInitial)
    {
        for(int32 I=0;I<2;++I)
        {
            const auto& C=DayThreeCables[I];FString Error;
            auto* A=Devices.FindRef(C.A).Get();auto* B=Devices.FindRef(C.B).Get();
            if(A&&B)ensureMsgf(AClaraNetworkDevice::ConnectPorts(A,C.PortA,B,C.PortB,TEXT("ethernet"),Error),TEXT("Day 3 initial cable: %s"),*Error);
        }
    }
}

void AClaraStoryDirector::StartDay3()
{
    CancelAllTerminalCommands();ResetDayTransition();ClosePage();CloseLaptopScreen();ReturnHeldCable();
    for(const auto& Entry:Devices)if(Entry.Value)for(const auto& Port:Entry.Value->GetPorts()){FString Error;Entry.Value->DisconnectPort(Port.Id,Error);}
    if(auto* Laptop=Devices.FindRef(TEXT("LAPTOP")).Get())
    {
        Laptop->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);Laptop->SetLaptopCarried(false);Laptop->SetActorLocationAndRotation({1135,1120,77.2},FRotator::ZeroRotator);
    }
    CarriedLaptop=false;SortingCable.Empty();EquippedCable.Empty();ConsoleTarget.Empty();TerminalTarget=TEXT("LAPTOP");Transcript.Empty();ScreenPage=TEXT("routing");
    TerminalTranscripts.Empty();TerminalHistories.Empty();PendingSSHUser.Empty();PendingSSHAddress.Empty();
    SimNodes.Empty();SimLinks.Empty();ArpCache.Empty();LearnedMACs.Empty();ChallengeReady=false;ChallengeRebooted=false;
    for(const FString& Key:Completed.Array())if(Key.StartsWith(TEXT("d3-"))||Key==TEXT("day3"))Completed.Remove(Key);
    for(const TCHAR* Item:{TEXT("notebook"),TEXT("badge"),TEXT("laptop")})Owned.Add(Item);
    for(const TCHAR* Item:{TEXT("ethernet"),TEXT("console"),TEXT("fiber"),TEXT("sfp_module")})Owned.Remove(Item);
    RoutingLab.Reset();SetupDayThreeLab(true);
    if(Controller.IsValid()&&Controller->GetPawn())
    {Controller->GetPawn()->TeleportTo({400,320,90},{0,90,0},false,false);Controller->SetControlRotation({-3,90,0});}
    SetPhase(200);SaveProgress();
}

FString AClaraStoryDirector::GetDayThreeObjective()const
{
    switch(Phase)
    {
    case 200:return TEXT("Giorno 3 · parla con Alessia alla reception");
    case 201:return TEXT("Raggiungi Luca in area scrivanie: il Network Layer");
    case 202:return TEXT("Rack nuovo: Cat 6 tra R1 Gi0/1 e R2 Gi0/0, poi R2 Gi0/1 e SW2");
    case 203:return TEXT("Portatile in mano e console su R1 · configura Gi0/1 10.0.0.1/30, attiva e salva");
    case 204:return TEXT("Console su R2 · nome, accessi, due interfacce attive e salvataggio");
    case 205:return TEXT("Console su SW2 · nome, accessi, VLAN 1 192.168.2.2/24, gateway e salvataggio");
    case 206:return TEXT("Vai a PC2 e prova ping 192.168.2.2: cosa manca al percorso?");
    case 207:return TEXT("R1: rotta statica verso la rete B, show ip route, salva; poi ping da PC2");
    case 208:return TEXT("R2: rotta di default verso R1, salva entrambi i router; poi ping da PC2");
    case 209:return TEXT("Da PC2: traceroute 192.168.2.2 · verifica R1, R2, SW2");
    case 210:return TEXT("Parla con Gaia per iniziare il collaudo");
    case 211:return ChallengeReady?TEXT("TEST superato · riavvia dal pannello Collaudo e verifica il salvataggio"):TEXT("Collaudo · trova e correggi quattro guasti, salva e premi TEST");
    case 212:return TEXT("Rete verificata dopo il riavvio · torna da Alessia alla reception");
    case 213:return TEXT("Giorno 3 completato · benvenuto in TNES. N riapre il riepilogo");
    default:return FString();
    }
}

void AClaraStoryDirector::EnterDayThreePhase(int32 Next)
{
    SetOfficeClockMinutes(Next==213?1080.f:540.f+(Next-200)*38.f);
    if(Next==203)SayDayThree(TEXT("console"));
    if(Next==204)SayDayThree(TEXT("r2"));
    if(Next==205)SayDayThree(TEXT("sw2"));
    if(Next==206)SayDayThree(TEXT("firstping"));
    if(Next==207)SayDayThree(TEXT("forward"));
    if(Next==208)SayDayThree(TEXT("return"));
    if(Next==209)SayDayThree(TEXT("trace"));
    if(Next==210)SayDayThree(TEXT("gaiaready"));
    if(Next==211&&!Completed.Contains(TEXT("d3-faults")))
    {RoutingLab.InjectFaults();Completed.Add(TEXT("d3-faults"));ChallengeReady=false;ChallengeRebooted=false;SayDayThree(TEXT("independent"));}
    if(Next==212)SayDayThree(TEXT("rebootpass"));
    if(Next==213)
    {
        if(!Completed.Contains(TEXT("d3-reboot-verified"))){Phase=211;Notify(TEXT("Completa prima TEST e verifica dopo il riavvio."));return;}
        Completed.Add(TEXT("day3"));CloseLaptopScreen();ShowPanel(TEXT("day3report"));SaveProgress();
    }
}

bool AClaraStoryDirector::HandleDayThreeInteraction(AActor* Target)
{
    if(const auto* Prop=Cast<AClaraStoryProp>(Target))
    {
        const FString Id=Prop->GetPropId();
        if(Id.StartsWith(TEXT("Box_")))
        {const FString Kind=Id.Mid(4);Owned.Add(Kind);AClaraAudio::Cue(this,TEXT("pickup"),.7f);EquipCable(Kind);return true;}
    }
    if(auto* NPC=Cast<AClaraStoryNPC>(Target))
    {
        const FString Id=NPC->GetCharacterId();
        if(Id==TEXT("Alessia")&&Phase==200)SayDayThree(TEXT("opening"),201);
        else if(Id==TEXT("Luca")&&Phase==201)SayDayThree(TEXT("network"),202);
        else if(Id==TEXT("Gaia")&&Phase==210)SayDayThree(TEXT("challenge"),211,{TEXT("Accetta il collaudo")});
        else if(Id==TEXT("Gaia")&&Phase==211){ScreenPage=TEXT("routing");OpenLaptopScreen();}
        else if(Id==TEXT("Alessia")&&Phase==212)
        {
            SayDayThree(TEXT("closingluca"));
            // Continue the same spoken queue across speakers; ContinueDialogue calls Audio::Speak.
            DialogueQueue.Add({TEXT("Alessia"),TEXT("Ti ho osservato per tre giorni. Cavi, indirizzi, router. Non sapevi tutto, ma hai provato, hai sbagliato e hai ricominciato: è quello che cerchiamo.")});
            DialogueQueue.Add({TEXT("Alessia"),TEXT("Benvenuto in TNES.")});AfterDialogue=213;
        }
        else if(Id==TEXT("Luca"))RequestDayThreeHint();
        else Notify(GetDayThreeObjective());
        return true;
    }
    if(Target==Devices.FindRef(TEXT("PC2")).Get())
    {SelectDayThreeTerminal(TEXT("PC2"));OpenLaptopScreen();return true;}
    return false;
}

bool AClaraStoryDirector::DayThreePhysicalCable(const FString& A,const FString& PA,const FString& B,const FString& PB,bool RequireGreen)const
{
    const auto* First=Devices.FindRef(A).Get();const auto* Second=Devices.FindRef(B).Get();if(!First||!Second)return false;
    FString ActualPort;const bool SwitchPort=Second->GetModelId()==TEXT("Cisco2960X");
    return First->GetPeerForPort(PA,ActualPort)==Second&&(ActualPort==PB||(SwitchPort&&ActualPort.StartsWith(TEXT("Gi1/0/"))&&Second->GetEffectivePortKind(ActualPort)==TEXT("ethernet")))&&(!RequireGreen||(First->GetLedState(PA)>=2&&Second->GetLedState(ActualPort)>=2));
}

void AClaraStoryDirector::SyncDayThreeCables()
{
    for(const auto& C:DayThreeCables)RoutingLab.SetCable(C.A,C.B,DayThreePhysicalCable(C.A,C.PortA,C.B,C.PortB,true));
}

void AClaraStoryDirector::PollDayThree()
{
    SyncDayThreeCables();
    if(Phase==202&&Page!=TEXT("dialogue")&&!IsDeviceInspectionOpen()&&RoutingLab.HasCable(TEXT("R1"),TEXT("R2"))&&RoutingLab.HasCable(TEXT("R2"),TEXT("SW2")))SetPhase(203);
    if(Phase==211&&ChallengeReady&&!RoutingLab.Validate(true).IsEmpty())ChallengeReady=false;
}

bool AClaraStoryDirector::IsNearDayThreePC()const
{
    const auto* PC=Devices.FindRef(TEXT("PC2")).Get();
    return SelfTesting||(PC&&Controller.IsValid()&&Controller->GetPawn()&&FVector::DistSquared(PC->GetActorLocation(),Controller->GetPawn()->GetActorLocation())<=FMath::Square(340.f));
}

void AClaraStoryDirector::SelectDayThreeTerminal(const FString& Id)
{
    if(Id==TEXT("PC2"))
    {if(!IsNearDayThreePC()){Notify(TEXT("Vai alla postazione PC2 per usare ping e traceroute. Il portatile non è nella rete."));return;}}
    else if(Id==TEXT("R1")||Id==TEXT("R2")||Id==TEXT("SW2"))
    {if(!CarriedLaptop||!PhysicalLinked(TEXT("LAPTOP"),Id,TEXT("console"))){Notify(TEXT("Prendi il portatile in mano e sposta il cavo azzurro sulla porta CONSOLE di ")+Id+TEXT("."));return;}}
    else if(Id!=TEXT("LAPTOP")){Notify(TEXT("Questo apparato non è configurabile nella prova del Giorno 3."));return;}
    SwitchTerminalSession(Id);ScreenPage=TEXT("terminal");
}

FString AClaraStoryDirector::RunDayThreeCommand(const FString& Input,bool RecordOutput)
{
    SyncDayThreeCables();const FString Prompt=RoutingLab.GetPrompt(TerminalTarget);
    FString Output,Command=Input.TrimStartAndEnd().ToLower();
    if(TerminalTarget==TEXT("PC2")&&!IsNearDayThreePC())Output=TEXT("Torna alla postazione PC2: il portatile serve soltanto alla console.");
    else if(TerminalTarget!=TEXT("PC2")&&TerminalTarget!=TEXT("LAPTOP")&&(!CarriedLaptop||!PhysicalLinked(TEXT("LAPTOP"),TerminalTarget,TEXT("console"))))
        Output=TEXT("Sessione console scollegata: tieni il portatile in mano e controlla entrambi i capi del cavo azzurro.");
    else
    {
        const FString Before=StateText(RoutingLab.ExportState());Output=RoutingLab.Execute(TerminalTarget,Input);
        if(Before!=StateText(RoutingLab.ExportState()))ChallengeReady=false;
        const bool ReadRoutes=(Command==TEXT("show ip route")||Command==TEXT("sh ip route")||Command==TEXT("do show ip route")||Command==TEXT("do sh ip route"))&&!Output.StartsWith(TEXT("%"));
        if(TerminalTarget==TEXT("R2")&&ReadRoutes)
        {
            if(RoutingLab.IsDeviceConfigured(TEXT("R2"),true))Completed.Add(TEXT("d3-r2-connected-read"));
            if(RoutingLab.HasRoute(TEXT("R2"),TEXT("0.0.0.0"),TEXT("0.0.0.0"),TEXT("10.0.0.1")))Completed.Add(TEXT("d3-default-read"));
        }
        if(Phase>=203&&Phase<=205)
        {
            const FString Expected=Phase==203?TEXT("R1"):Phase==204?TEXT("R2"):TEXT("SW2");
            if(RoutingLab.IsDeviceConfigured(Expected,true)&&(Phase!=204||Completed.Contains(TEXT("d3-r2-connected-read"))))SetPhase(Phase+1);
        }
        if(Phase==206&&TerminalTarget==TEXT("PC2")&&Command==TEXT("ping 192.168.2.2"))SetPhase(207);
        else if(Phase==207)
        {
            if(TerminalTarget==TEXT("R1")&&ReadRoutes&&RoutingLab.HasRoute(TEXT("R1"),TEXT("192.168.2.0"),TEXT("255.255.255.0"),TEXT("10.0.0.2")))Completed.Add(TEXT("d3-forward-read"));
            if(Completed.Contains(TEXT("d3-forward-read"))&&TerminalTarget==TEXT("PC2")&&Command==TEXT("ping 192.168.2.2"))SetPhase(208);
        }
        else if(Phase==208&&TerminalTarget==TEXT("PC2")&&Command==TEXT("ping 192.168.2.2")&&Output.Contains(TEXT("Reply from"))&&Completed.Contains(TEXT("d3-default-read"))&&RoutingLab.Validate(true).IsEmpty())SetPhase(209);
        else if(Phase==209&&TerminalTarget==TEXT("PC2")&&Command==TEXT("traceroute 192.168.2.2")&&RoutingLab.Validate(true).IsEmpty())
        {Completed.Add(TEXT("d3-traceroute"));SetPhase(210);}
        if(Output.StartsWith(TEXT("%"))&&Phase<211&&Page!=TEXT("dialogue"))SayDayThree(TEXT("retry"));
    }
    if(RecordOutput)
    {AClaraAudio::Cue(this,TEXT("enter"),.6f);Transcript+=Prompt+TEXT(" ")+Input+TEXT("\n")+Output+TEXT("\n\n");if(Transcript.Len()>30000)Transcript.RightChopInline(Transcript.Len()-24000);}
    return Output;
}

bool AClaraStoryDirector::PredictDayThreePing(const FString& Input,FString& Preview)const
{
    TArray<FString> Words;Input.ParseIntoArrayWS(Words);
    const bool Ping=Words.Num()==2&&Words[0].Equals(TEXT("ping"),ESearchCase::IgnoreCase);
    const bool DoPing=Words.Num()==3&&Words[0].Equals(TEXT("do"),ESearchCase::IgnoreCase)&&Words[1].Equals(TEXT("ping"),ESearchCase::IgnoreCase);
    if((!Ping&&!DoPing)||TerminalTarget==TEXT("LAPTOP"))return false;
    if(TerminalTarget==TEXT("PC2")) {if(!IsNearDayThreePC())return false;}
    else if(!CarriedLaptop||!PhysicalLinked(TEXT("LAPTOP"),TerminalTarget,TEXT("console")))return false;
    FClaraRoutingLab Probe=RoutingLab;
    for(const auto& C:DayThreeCables)Probe.SetCable(C.A,C.B,DayThreePhysicalCable(C.A,C.PortA,C.B,C.PortB,true));
    Preview=Probe.Execute(TerminalTarget,Input);
    return Preview.StartsWith(TEXT("Reply from"))||Preview.StartsWith(TEXT("Request timed out"))||Preview.StartsWith(TEXT("Destination host unreachable"));
}

FString AClaraStoryDirector::RunDayThreeTest()
{
    if(Phase!=211)return TEXT("Il collaudo si apre parlando con Gaia dopo il traceroute.");
    SyncDayThreeCables();const auto Problems=RoutingLab.Validate(true);ChallengeReady=Problems.IsEmpty();
    if(!ChallengeReady)
    {DayThreeTestResult=FString::Join(Problems,TEXT("\n"));Say(TEXT("Gaia"),Problems);return DayThreeTestResult;}
    DayThreeTestResult=TEXT("TEST superato: cavi e LED, accessi, interfacce, ping, traceroute e configurazioni salvate. Manca la verifica dopo il riavvio.");
    SayDayThree(TEXT("testpass"));return DayThreeTestResult;
}

void AClaraStoryDirector::RebootDayThree()
{
    if(Phase!=211||!ChallengeReady){Notify(TEXT("Premi TEST e correggi le verifiche mancanti prima del riavvio."));return;}
    SyncDayThreeCables();if(!RoutingLab.Validate(true).IsEmpty()){ChallengeReady=false;RunDayThreeTest();return;}
    RoutingLab.Reboot();SyncDayThreeCables();ChallengeReady=false;
    const auto Problems=RoutingLab.Validate(true);
    if(!Problems.IsEmpty()){ChallengeRebooted=false;DayThreeTestResult=FString::Join(Problems,TEXT("\n"));Say(TEXT("Gaia"),Problems);return;}
    ChallengeRebooted=true;Completed.Add(TEXT("d3-reboot-verified"));DayThreeTestResult=TEXT("Riavvio verificato: PC2 raggiunge R1, R2 e SW2; traceroute completo.");SetPhase(212);
}

void AClaraStoryDirector::RequestDayThreeHint()
{
    if(Phase==211)
    {
        const TCHAR* Hints[]={TEXT("hintphysical"),TEXT("hintgateway"),TEXT("hintreturn"),TEXT("hintinterface"),TEXT("hintforward"),TEXT("hintsave")};
        SayDayThree(Hints[HintLevel++%UE_ARRAY_COUNT(Hints)]);return;
    }
    SayDayThree(Phase<=202?TEXT("hintphysical"):Phase<=205?TEXT("hintconfig"):Phase<=207?TEXT("hintforward"):Phase==208?TEXT("hintreturn"):TEXT("hinttrace"));
}

FString AClaraStoryDirector::GetDayThreeNotebook()const
{
    FString Notes=TEXT("GIORNO 3 · ROUTING E GUASTI\n\nScheda di Gaia — credenziali del laboratorio: enable secret Cisco123; console Console123 con login.\n\nRETE A: PC2 192.168.1.11/24, gateway 192.168.1.1. R1 Gi0/0 192.168.1.1/24, tramite SW1.\nTRANSITO: R1 Gi0/1 10.0.0.1/30 ↔ R2 Gi0/0 10.0.0.2/30.\nRETE B: R2 Gi0/1 192.168.2.1/24 ↔ SW2 VLAN 1 192.168.2.2/24, gateway 192.168.2.1.\nIl portatile non è nella rete: porta soltanto la console. Ping e traceroute partono da PC2.\n");
    if(Phase>=202)Notes+=TEXT("\nNETWORK LAYER\nIl livello 3 di OSI usa gli IP e sceglie percorsi tra reti. Nella stessa subnet la destinazione si cerca direttamente; fuori subnet si usa il default gateway.\n");
    if(Phase>=203)Notes+=TEXT("\nINTERFACCE E /30\n255.255.255.252 lascia due host utilizzabili. Ogni interfaccia del router appartiene alla propria rete. no shutdown la attiva; copy running-config startup-config salva.\n");
    if(Phase>=207)Notes+=TEXT("\nROTTE\nshow ip route: C rete collegata, L indirizzo locale, S rotta statica, S* candidata di default.\nIn configure terminal: ip route <rete> <mask> <next-hop>. Il next-hop deve essere raggiungibile. Una rotta vale in un solo senso: controlla anche il ritorno.\n");
    if(Phase>=208)Notes+=TEXT("\nDEFAULT\nip route 0.0.0.0 0.0.0.0 <next-hop>: per le destinazioni senza una rotta più specifica. Utile su un router con un solo vicino. Salva su entrambi i router.\n");
    if(Phase>=209)Notes+=TEXT("\nTRACEROUTE\nSonde con TTL crescente; il router decrementa il TTL e a zero risponde ICMP time exceeded. PC2 → R1 → R2 → SW2: la sorgente non compare tra i salti.\n* * * significa risposta mancante, non prova da solo dove sia il guasto. In questo laboratorio confronta interfacce e rotte; nel mondo reale possono intervenire filtri e limiti ICMP. Nel gioco si scrive traceroute anche da PC2; Windows usa tracert.\n");
    if(Phase>=211)Notes+=TEXT("\nDIAGNOSI\nDal basso verso l’alto: cavo e LED, IP/mask, gateway, interfaccia, rotta, startup-config. Il riavvio carica la configurazione salvata. Non ci sono punti, penalità o limite di tempo.\n");
    if(Phase>=212)Notes+=TEXT("\nCOLLAUDO COMPLETATO\nQuattro guasti: gateway di SW2 fuori subnet, default mancante su R2, Gi0/1 di R2 spenta, next-hop errato su R1. Riparati e salvati: ping e traceroute funzionano anche dopo il riavvio.\n");
    return Notes;
}

FString AClaraStoryDirector::GetDayThreeCableStatus()const
{
    FString Result;
    for(const auto& C:DayThreeCables)Result+=FString(C.A)+TEXT(" ")+C.PortA+TEXT(" ↔ ")+C.B+TEXT(" ")+C.PortB+TEXT("  ·  ")+(DayThreePhysicalCable(C.A,C.PortA,C.B,C.PortB,true)?TEXT("verde"):DayThreePhysicalCable(C.A,C.PortA,C.B,C.PortB,false)?TEXT("ambra, attendi"):TEXT("scollegato o porta errata"))+TEXT("\n");
    return Result;
}

FString AClaraStoryDirector::CompleteDayThreeCommand(const FString& Prefix)const
{
    for(const FString Command:{TEXT("enable"),TEXT("configure terminal"),TEXT("hostname "),TEXT("enable secret "),TEXT("line console 0"),TEXT("password "),TEXT("login"),TEXT("interface gigabitethernet 0/"),TEXT("interface vlan 1"),TEXT("ip address "),TEXT("ip default-gateway "),TEXT("ip route "),TEXT("no ip route "),TEXT("no shutdown"),TEXT("shutdown"),TEXT("exit"),TEXT("end"),TEXT("show running-config"),TEXT("show startup-config"),TEXT("show ip interface brief"),TEXT("show ip route"),TEXT("copy running-config startup-config"),TEXT("write memory"),TEXT("ping "),TEXT("traceroute "),TEXT("ipconfig /all"),TEXT("help")})
        if(Command.StartsWith(Prefix,ESearchCase::IgnoreCase))return Command;
    return Prefix;
}

bool AClaraStoryDirector::IsDayThreeMissionTarget(AActor* Actor)const
{
    if(!Actor)return false;
    if(const auto* NPC=Cast<AClaraStoryNPC>(Actor))
    {
        const FString Id=NPC->GetCharacterId();
        return ((Phase==200||Phase==212)&&Id==TEXT("Alessia"))||(Phase==201&&Id==TEXT("Luca"))||(Phase==210&&Id==TEXT("Gaia"));
    }
    if(const auto* Prop=Cast<AClaraStoryProp>(Actor))
    {
        const FString Id=Prop->GetPropId();
        return Phase==202&&Id==TEXT("Box_ethernet")||Phase==203&&!Owned.Contains(TEXT("console"))&&Id==TEXT("Box_console");
    }
    if(Phase==202)return Actor==Devices.FindRef(TEXT("R1")).Get()||Actor==Devices.FindRef(TEXT("R2")).Get()||Actor==Devices.FindRef(TEXT("SW2")).Get();
    if(Phase>=203&&Phase<=205)return Actor==Devices.FindRef(!CarriedLaptop?TEXT("LAPTOP"):Phase==203?TEXT("R1"):Phase==204?TEXT("R2"):TEXT("SW2")).Get();
    if(Phase>=206&&Phase<=209)return Actor==Devices.FindRef(TEXT("PC2")).Get();
    if(Phase==211)return Actor==Devices.FindRef(TEXT("LAPTOP")).Get();
    return false;
}

bool AClaraStoryDirector::RunDayThreeSelfTest()
{
    TGuardValue<bool> Testing(SelfTesting,true),Held(CarriedLaptop,false),Ready(ChallengeReady,false),Rebooted(ChallengeRebooted,false);
    TGuardValue<int32> CurrentPhase(Phase,203);
    TGuardValue<FString> Target(TerminalTarget,TEXT("R1")),Log(Transcript,FString()),OldToast(Toast,FString());
    TGuardValue<float> Clock(OfficeClockMinutes,OfficeClockMinutes),ToastTime(ToastUntil,ToastUntil);
    TGuardValue<FClaraRoutingLab> Lab(RoutingLab,FClaraRoutingLab());
    TGuardValue<TSet<FString>> Achievements(Completed,TSet<FString>());
    bool Passed=UE_ARRAY_COUNT(DayThreeCables)==4;
    const FString Unwired=RunDayThreeCommand(TEXT("enable"));
    Passed&=Unwired.Contains(TEXT("Sessione console scollegata"))&&Phase==203;
    Phase=206;TerminalTarget=TEXT("LAPTOP");
    const FString LaptopPing=RunDayThreeCommand(TEXT("ping 192.168.2.2"));
    Passed&=!LaptopPing.Contains(TEXT("Reply from"))&&Phase==206&&GetDay()==3;
    Phase=211;RebootDayThree();Passed&=!ChallengeRebooted&&Phase==211;
    Passed&=!GetDayThreeNotebook().Contains(TEXT("Quattro guasti:"));
    Phase=212;Passed&=GetDayThreeNotebook().Contains(TEXT("Quattro guasti:"));
    EnterDayThreePhase(213);Passed&=Phase==211&&!Completed.Contains(TEXT("day3"));
    Phase=110;Passed&=!IsComplete();Phase=213;Passed&=IsComplete();
    UE_LOG(LogTemp,Display,TEXT("CLARA_DAY3_FLOW_%s: console ownership, laptop isolation, reboot gate, notebook unlock, final completion gate"),Passed?TEXT("PASS"):TEXT("FAIL"));
    return Passed;
}
