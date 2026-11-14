#include "ClaraStoryDirector.h"
#include "ClaraRoomVisitor.h"
#include "ClaraNetworkDevice.h"
#include "ClaraStoryCast.h"
#include "ClaraDisplay.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Algo/Reverse.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
FString Edge(FString A,FString B){return A<B?A+TEXT("|")+B:B+TEXT("|")+A;}
FString Clean(FString T){T.TrimStartAndEndInline();return T;}
const TCHAR* ProgressFile=TEXT("ClaraStoryProgress.json");
}
AClaraStoryDirector::AClaraStoryDirector(){PrimaryActorTick.bCanEverTick=true;}
void AClaraStoryDirector::BeginPlay()
{
    Super::BeginPlay();Controller=Cast<AClaraRoomController>(GetWorld()->GetFirstPlayerController());
    CreateLabHosts();CreateProps();GatewayBegin();
    if(GetWorld()->GetGameViewport())
    {HUD=MakeClaraStoryHUD(this);GetWorld()->GetGameViewport()->AddViewportWidgetContent(HUD.ToSharedRef(),12);}
    Owned.Add(TEXT("notebook"));SetPhase(0);
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraCableDemo")))
    {
        Owned.Add(TEXT("badge"));Phase=3;PickupLaptop();ClosePage();
        if(Controller.IsValid()&&Controller->GetPawn())
        {Controller->GetPawn()->SetActorLocation({1120,1185,90});Controller->SetControlRotation({-38,90,0});}
        Notify(TEXT("Prova aggiornata: punta una bobina e premi E o clicca. T apre il portatile, I la borsa."));
        return;
    }
    FString Saved;
    if(!FApp::IsUnattended()&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraStoryCheck"))&&!FParse::Param(FCommandLine::Get(),TEXT("ClaraFreshStory"))&&FFileHelper::LoadFileToString(Saved,*(FPaths::ProjectSavedDir()/ProgressFile))&&Saved.Contains(TEXT("\"day1Complete\":true")))
    {Completed.Add(TEXT("day1"));for(const FString Item:{TEXT("badge"),TEXT("laptop"),TEXT("ethernet"),TEXT("console"),TEXT("fiber"),TEXT("sfp_module")})Owned.Add(Item);StartDay2();}
}
void AClaraStoryDirector::EndPlay(const EEndPlayReason::Type R)
{
    GatewayShutdown();
    if(GetWorld()&&GetWorld()->GetGameViewport())
    {if(HUD.IsValid())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(HUD.ToSharedRef());if(Panel.IsValid())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Panel.ToSharedRef());}
    if(ScreenOverlay.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ScreenOverlay.ToSharedRef());
    ScreenOverlay.Reset();HUD.Reset();Panel.Reset();
    Super::EndPlay(R);
}
void AClaraStoryDirector::CreateLabHosts()
{
    for(TActorIterator<AClaraNetworkDevice> It(GetWorld());It;++It)
        if(!It->IsPreview()&&It->ActorHasTag(TEXT("BenchDevice")))
            Devices.Add(It->GetModelId()==TEXT("Cisco1941")?TEXT("R1"):TEXT("SW1"),*It);
    auto Spawn=[this](const FString& Id,const FString& Model,FVector P,FRotator R=FRotator::ZeroRotator)
    {auto* D=GetWorld()->SpawnActor<AClaraNetworkDevice>(P,R);D->Initialize(Model);D->Tags.Add(TEXT("StoryNode"));D->Tags.Add(FName(*Id));Devices.Add(Id,D);return D;};
    auto* Laptop=Spawn(TEXT("LAPTOP"),TEXT("Laptop"),{1135,1120,77.2});Laptop->Tags.Add(TEXT("BenchDevice"));
    Spawn(TEXT("PC2"),TEXT("PC2"),{821,1312,77.1},{0,180,0});
    AClaraNetworkDevice* RackSlot=nullptr;
    for(TActorIterator<AClaraNetworkDevice> It(GetWorld());It;++It)
        if(!It->IsPreview()&&!It->ActorHasTag(TEXT("BenchDevice"))&&It->GetModelId()==TEXT("Cisco2960X")){RackSlot=*It;break;}
    const FVector Uplink=RackSlot?RackSlot->GetActorLocation():FVector(1505,1176,162.2);
    if(RackSlot)RackSlot->Destroy();
    Spawn(TEXT("UPLINK"),TEXT("FiberUplink"),Uplink);
    Display=NewObject<UClaraDisplay>(Laptop);Laptop->AddInstanceComponent(Display);Display->SetupAttachment(Laptop->GetRootComponent());Display->RegisterComponent();
    Display->SetDrawSize({1280,800});Display->SetRelativeLocation({0,10.1,12.4});Display->SetRelativeRotation({0,-90,0});Display->SetRelativeScale3D(FVector(.0234375));
}
void AClaraStoryDirector::CreateProps()
{
    const auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Office.M_Office"));
    const FString Kinds[]={TEXT("ethernet"),TEXT("console"),TEXT("fiber")};
    const FLinearColor Colors[]={FLinearColor(.8,.81,.76),FLinearColor(.08,.44,.69),FLinearColor(.78,.62,.035)};
    for(int32 I=0;I<3;++I)
    {
        const float X=1065+I*55;
        auto* Cable=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(X,1278,79),FRotator::ZeroRotator);
        Cable->Initialize(TEXT("Cable_")+Kinds[I],I==0?TEXT("CAT 6 / RJ45"):I==1?TEXT("USB / CONSOLE"):TEXT("FIBRA / LC"));
        auto* Bin=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(X,1321,79),FRotator::ZeroRotator);Bin->Initialize(TEXT("Box_")+Kinds[I],I==0?TEXT("ETHERNET"):I==1?TEXT("CONSOLE"):TEXT("FIBRA"));
        auto* M=UMaterialInstanceDynamic::Create(Base,Bin);M->SetVectorParameterValue(TEXT("Color"),Colors[I]);
        for(FVector Offset:{FVector(-21,0,4),FVector(21,0,4),FVector(0,12,4),FVector(0,-12,4)})
        {auto* C=NewObject<UStaticMeshComponent>(Bin);Bin->AddInstanceComponent(C);C->SetMobility(EComponentMobility::Movable);C->SetupAttachment(Bin->GetRootComponent());C->SetStaticMesh(const_cast<UStaticMesh*>(Cube));C->SetMaterial(0,M);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();C->SetRelativeLocation(Offset);C->SetRelativeScale3D(FVector(Offset.X!=0?2:44,Offset.X!=0?26:2,8)/100);}
    }
    auto* Module=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(1000,1107,79),FRotator::ZeroRotator);Module->Initialize(TEXT("SFP"),TEXT("MODULO SFP"));
    auto* Coffee=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(1200,1290,79),FRotator::ZeroRotator);Coffee->Initialize(TEXT("Coffee"),TEXT("ALIMENTAZIONE"));
}
void AClaraStoryDirector::Notify(const FString& T){Toast=T;ToastUntil=GetWorld()->GetTimeSeconds()+7;}
void AClaraStoryDirector::ShowPanel(const FString& Name)
{
    if(Panel.IsValid()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Panel.ToSharedRef());
    Panel.Reset();Page=Name;
    if(!Name.IsEmpty()&&GetWorld()->GetGameViewport())
    {Panel=MakeClaraStoryPanel(this);GetWorld()->GetGameViewport()->AddViewportWidgetContent(Panel.ToSharedRef(),22);}
    if(Controller.IsValid()){Controller->bShowMouseCursor=true;FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Controller->SetInputMode(Mode);}
}
void AClaraStoryDirector::Say(const FString& Who,const TArray<FString>& Lines,int32 Next,const TArray<FString>& Options)
{
    DialogueQueue.Empty();for(const auto& L:Lines)DialogueQueue.Add({Who,L});DialogueIndex=0;AfterDialogue=Next;Choices=Options;
    Speaker=Who;Dialogue=Lines.Num()?Lines[0]:FString();ShowPanel(TEXT("dialogue"));
}
void AClaraStoryDirector::ContinueDialogue()
{
    if(Page!=TEXT("dialogue"))return;
    if(DialogueIndex+1<DialogueQueue.Num()){++DialogueIndex;Speaker=DialogueQueue[DialogueIndex].Key;Dialogue=DialogueQueue[DialogueIndex].Value;return;}
    if(Choices.Num())return;
    const int32 Next=AfterDialogue;ClosePage();if(Next>=0)SetPhase(Next);
}
void AClaraStoryDirector::Choose(int32 I)
{
    if(!Choices.IsValidIndex(I))return;
    if(LaptopMenu){ClosePage();if(I==0)OpenLaptopScreen();else if(I==1){if(CarriedLaptop)PlaceLaptop();else PickupLaptop();}return;}
    if(Phase==0){Owned.Add(I==1?TEXT("career_cyber"):I==2?TEXT("career_both"):TEXT("career_network"));Choices.Empty();Say(TEXT("Alessia"),{TEXT("Perfetto. Il banco di oggi e' uguale per tutti: prima facciamo parlare due computer. Il badge e' qui sul tavolo. Luca ti aspetta nel laboratorio reti.")},1);}
    else {Choices.Empty();ContinueDialogue();}
}
void AClaraStoryDirector::ClosePage(){ShowPanel(FString());Dialogue.Empty();Choices.Empty();LaptopMenu=false;}
void AClaraStoryDirector::OpenNotebook(){if(Page==TEXT("notebook")){ClosePage();return;}ShowPanel(TEXT("notebook"));}
void AClaraStoryDirector::OpenInventory(){if(Page==TEXT("inventory")){ClosePage();return;}ShowPanel(TEXT("inventory"));}
FString AClaraStoryDirector::GetTitle()const{return FString::Printf(TEXT("Giorno %d · %s"),GetDay(),GetDay()==1?TEXT("Il primo messaggio"):TEXT("Indirizzi e configurazione"));}
FString AClaraStoryDirector::GetObjective()const
{
    switch(Phase)
    {
    case 0:return TEXT("Parla con Alessia alla reception");case 1:return TEXT("Prendi il badge sul banco reception");case 2:return TEXT("Raggiungi Luca nel laboratorio reti");case 3:return TEXT("Prendi il portatile dal banco · E oppure clic");
    case 4:return TEXT("Porta ordine nel kit: osserva i cavi e sistemali nelle tre scatole");case 5:return TEXT("Collega il portatile a SW1 con un cavo Ethernet");case 6:return TEXT("Apri il portatile con T e cerca un modo per provare PC2");case 7:return TEXT("Controlla il percorso: collega anche PC2 allo switch");case 8:return TEXT("Ripeti il test verso PC2: 192.168.1.11");case 9:return TEXT("Prendi il cavo console: portatile USB → CONSOLE di SW1");case 10:return TEXT("Leggi il MAC del portatile e ritrovalo nella tabella dello switch");case 11:return TEXT("Ricostruisci OSI: dal segnale al servizio");case 12:return TEXT("Associa i quattro livelli TCP/IP");case 13:return TEXT("Collega R1 a SW1 e prepara l'uplink ottico con il modulo SFP");case 14:return TEXT("Prova autonoma: ripristina i due PC e verifica la risposta di PC2");case 15:return TEXT("Ritrova la nuova porta del portatile nella tabella MAC");case 16:return TEXT("Giorno 1 completato · consulta il rapporto per iniziare il Giorno 2");
    case 100:return TEXT("Parla con Luca: oggi si configura");case 101:return TEXT("Metti in ordine la richiesta e la risposta ARP");case 102:return TEXT("Sul PC: svuota ARP con arp -d * e controlla con arp -a");case 103:return TEXT("Esegui ping a PC2 e rileggi arp -a");case 104:return TEXT("Risolvi il foglio /26: 192.168.1.70 / 255.255.255.192");case 105:return TEXT("Collega la console al banco e apri SW-PROVA");case 106:return TEXT("Configura SW-PROVA: nome, accessi, IP 192.168.1.2, SSH e salvataggio");case 107:return TEXT("Verifica la configurazione salvata e il ping al portatile");case 108:return TEXT("Parla con Gaia nell'area briefing");case 109:return TEXT("Ripristina il piano 2 dal simulatore del portatile e supera TEST e riavvio");case 110:return TEXT("Giorni 1 e 2 completati: rete ripristinata e configurazioni persistenti");
    }return TEXT("Esplora il laboratorio");
}
void AClaraStoryDirector::SetPhase(int32 N)
{
    Phase=N;HintLevel=0;
    if(N==6){Notify(TEXT("Apri il portatile. La scheda Comandi spiega gli strumenti, senza eseguirli al tuo posto."));}
    if(N==7)Notify(TEXT("Un LED acceso descrive un solo tratto del percorso. Guarda anche PC2."));
    if(N==9)Say(TEXT("Luca"),{TEXT("Eccolo. Adesso il messaggio arriva. Guarda pero': lo switch ha imparato da quale porta arrivano i due computer."),TEXT("Per leggerlo ci colleghiamo alla console. Il cavo azzurro porta la sessione di gestione; l'alimentazione dello switch e' separata.")});
    if(N==11){SetupPuzzle(TEXT("osi"));ScreenPage=TEXT("puzzle");OpenLaptopScreen();Say(TEXT("Luca"),{TEXT("Fermiamoci un momento su quello che hai appena visto. Il segnale passa nel cavo, il frame porta due MAC e il pacchetto contiene l'IP."),TEXT("Sul portatile ci sono sette schede. Rimettile dal livello fisico al servizio. Puoi rileggere i nomi e gli indizi: non e' una prova di memoria.")});}
    if(N==12){SetupPuzzle(TEXT("tcpip"));ScreenPage=TEXT("puzzle");OpenLaptopScreen();}
    if(N==13)Say(TEXT("Luca"),{TEXT("La prova tra i due PC era locale: il router non serviva. Ora aggiungiamo il collegamento verso R1."),TEXT("Per la fibra usiamo lo switch: il 1941 ha porte Ethernet in rame. Prendi il piccolo modulo SFP, inseriscilo nello slot e collega il cavo LC alla distribuzione ottica.")});
    if(N==14)
    {
        for(const FString Id:{TEXT("LAPTOP"),TEXT("PC2")})if(Devices.Contains(Id))
        {FString Error;Devices[Id]->DisconnectPort(TEXT("Ethernet"),Error);}
        Say(TEXT("Luca"),{TEXT("Facciamo l'ultima prova. Ho staccato soltanto i due collegamenti dei PC. Puoi cambiare porta: conta che tu sappia ricostruire il percorso."),TEXT("Questa volta scegli tu il cavo e il test. Se qualcosa non torna, gli strumenti e il taccuino restano qui.")});
    }
    if(N==16){Completed.Add(TEXT("day1"));SaveProgress();ShowPanel(TEXT("report"));}
    if(N==101){SetupPuzzle(TEXT("arp"));ScreenPage=TEXT("puzzle");OpenLaptopScreen();}
    if(N==104){SubnetStep=0;ScreenPage=TEXT("subnet");OpenLaptopScreen();}
    if(N==106){ScreenPage=TEXT("terminal");TerminalTarget=TEXT("SW-PROVA");OpenLaptopScreen();Say(TEXT("Luca"),{TEXT("Questo e' il banco di prova. Il comando help del terminale raccoglie i passi della scheda, mentre ? e Tab ti aiutano nel punto in cui sei."),TEXT("Parti dal nome e dagli accessi. Poi indirizzo di gestione e SSH. Quando hai finito, salva e verifica: non basta vedere un prompt nuovo.")});}
    if(N==108)Say(TEXT("Luca"),{TEXT("Il banco funziona. Gaia ha un ripristino da preparare: raggiungila nel briefing. La piantina non cambia, lavoreremo sulla copia del piano 2 dal portatile.")});
    if(N==109){SetupChallenge();ScreenPage=TEXT("challenge");OpenLaptopScreen();}
    if(N==110){Completed.Add(TEXT("day2"));SaveProgress();CloseLaptopScreen();ShowPanel(TEXT("report"));}
}
bool AClaraStoryDirector::HandleInteraction(AActor* Target)
{
    if(!Target||(!Target->IsA<AClaraStoryNPC>()&&!Target->IsA<AClaraStoryProp>()&&!Target->IsA<AClaraNetworkDevice>()))return false;
    if(Controller.IsValid()&&Controller->GetPawn()&&FVector::DistSquared(Controller->GetPawn()->GetActorLocation(),Target->GetActorLocation())>FMath::Square(340.f))
    {Notify(TEXT("Avvicinati per interagire con questo oggetto."));return true;}
    if(auto* NPC=Cast<AClaraStoryNPC>(Target))
    {
        const FString Id=NPC->GetCharacterId();
        if(Id==TEXT("Alessia")&&Phase==0)Say(Id,{TEXT("Ciao, ben arrivato. Io sono Alessia. Oggi lavori con Luca: una mattina al banco vale piu' di dieci domande sul curriculum."),TEXT("Prima di cominciare: ti interessa di piu' costruire le reti, capire come proteggerle, oppure entrambe le cose?")},-1,{TEXT("Partire dalle reti"),TEXT("Partire dalla sicurezza"),TEXT("Esplorare entrambe")});
        else if(Id==TEXT("Luca")&&Phase==2)Say(Id,{TEXT("Ciao. Vieni, ti faccio vedere il banco."),TEXT("Quel portatile lo usi tu. L'obiettivo e' semplice: far arrivare un messaggio al secondo PC. Non tocchiamo la rete dell'ufficio: questo laboratorio e' fatto per provare e ricominciare.")},3);
        else if(Id==TEXT("Luca")&&Phase==100)Say(Id,{TEXT("Buongiorno. Ieri hai fatto arrivare un messaggio. Oggi guardiamo come il PC trova il destinatario, poi configuriamo uno switch."),TEXT("Sul portatile trovi tre carte ARP da riordinare. Parti da chi sta facendo la domanda.")},101);
        else if(Id==TEXT("Gaia")&&Phase==108)Say(Id,{TEXT("Ciao, sono Gaia. Due switch del piano 2 sono stati sostituiti dopo un guasto. I nuovi sono ancora da configurare."),TEXT("Sul portatile c'e' la copia della rete: collega i dispositivi, correggi gli indirizzi, configura SW-A e SW-B. Quando e' pronta premi TEST. Dopo controlliamo che resista a un riavvio.")},109);
        else if(Id==TEXT("Jonny"))Say(Id,{Phase>=100?TEXT("Un ping che funziona non racconta tutta la sicurezza della rete. Quando attivi gli accessi remoti, tieni SSH e chiudi Telnet."):TEXT("Io sono Jonny. Quando hai un ping che risponde, chiamami: domani ti faccio vedere quante informazioni racconta quel piccolo messaggio.")});
        else Say(NPC->GetDisplayName(),{GetObjective()});return true;
    }
    if(auto* Prop=Cast<AClaraStoryProp>(Target))
    {
        const FString Id=Prop->GetPropId();
        if(Id==TEXT("Badge")){if(Phase<1){Notify(TEXT("Alessia ti aspetta: presentati prima di prendere il badge."));return true;}Owned.Add(TEXT("badge"));Prop->SetActorHiddenInGame(true);Prop->SetActorEnableCollision(false);if(Phase==1)SetPhase(2);Notify(TEXT("Badge riposto. I apre la borsa, N il taccuino."));}
        else if(Id.StartsWith(TEXT("Cable_"))){SelectSortingCable(Id.Mid(6));if(Phase==4)Notify(GetToast()+TEXT(" Ora clicca la scatola corrispondente."));else EquipCable(Id.Mid(6));}
        else if(Id.StartsWith(TEXT("Box_"))){if(Phase==4)SortCable(Id.Mid(4));else EquipCable(Id.Mid(4));}
        else if(Id==TEXT("SFP")){Owned.Add(TEXT("sfp_module"));EquipCable(TEXT("sfp_module"));Notify(TEXT("Modulo SFP pronto. Apri lo switch e seleziona uno slot SFP."));}
        else if(Id==TEXT("Coffee")){Notify(TEXT("Luca: questo alimenta la macchinetta del caffe'. Nessuna di quelle prese e' una porta dati."));}
        return true;
    }
    if(auto* D=Cast<AClaraNetworkDevice>(Target))
    {if(D->GetModelId()==TEXT("Laptop"))
        {if(!Owned.Contains(TEXT("laptop"))||Phase==3)PickupLaptop();
        else if(CarriedLaptop)OpenLaptopScreen();
        else {Say(TEXT("Portatile"),{TEXT("Puoi aprire la postazione virtuale qui oppure portare il computer con te.")},-1,{TEXT("Apri portatile"),TEXT("Prendi il portatile"),TEXT("Lascia sul banco")});LaptopMenu=true;}
        return true;}
    }
    return false;
}
AActor* AClaraStoryDirector::FindInteractionTarget()const
{
    if(!Controller.IsValid()||!Controller->GetPawn())return nullptr;
    FHitResult DirectHit;const bool HasDirect=Controller->GetHitResultUnderCursor(ECC_Visibility,false,DirectHit);
    AActor* Direct=DirectHit.GetActor();
    if(Direct&&(Direct->IsA<AClaraStoryNPC>()||Direct->IsA<AClaraStoryProp>()||Direct->IsA<AClaraNetworkDevice>()))return Direct;
    float MouseX=0,MouseY=0;if(!Controller->GetMousePosition(MouseX,MouseY))return nullptr;
    FVector RayStart,RayDirection;if(!Controller->DeprojectMousePositionToWorld(RayStart,RayDirection))return nullptr;
    AActor* Best=nullptr;float BestPixels=35.f;
    const FVector Player=Controller->GetPawn()->GetActorLocation();
    for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)
    {
        if(It->IsHidden()||!It->GetActorEnableCollision()||FVector::DistSquared(Player,It->GetActorLocation())>FMath::Square(340.f))continue;
        const UPrimitiveComponent* Shape=It->GetInteractionComponent();
        const FVector Position=Shape?Shape->GetComponentLocation():It->GetActorLocation();
        FVector2D Screen;if(!Controller->ProjectWorldLocationToScreen(Position,Screen))continue;
        const float Pixels=FVector2D::Distance(Screen,FVector2D(MouseX,MouseY));if(Pixels>BestPixels)continue;
        const float Distance=FVector::Distance(RayStart,Position);
        if(HasDirect&&FVector::Distance(RayStart,DirectHit.ImpactPoint)<Distance-35.f)continue;
        FCollisionQueryParams Params;Params.AddIgnoredActor(Controller->GetPawn());FHitResult Obstacle;
        if(GetWorld()->LineTraceSingleByChannel(Obstacle,RayStart,Position,ECC_Visibility,Params)&&Obstacle.GetActor()!=*It&&Obstacle.Distance<Distance-8.f)continue;
        Best=*It;BestPixels=Pixels;
    }
    return Best;
}
void AClaraStoryDirector::InteractFocused(){HandleInteraction(FindInteractionTarget());}
FString AClaraStoryDirector::GetInteractionLabel()const
{
    AActor* Target=FindInteractionTarget();
    if(!Target||!Controller.IsValid()||!Controller->GetPawn()||FVector::DistSquared(Controller->GetPawn()->GetActorLocation(),Target->GetActorLocation())>FMath::Square(340.f))return FString();
    if(auto* N=Cast<AClaraStoryNPC>(Target))return TEXT("Parla con ")+N->GetDisplayName();
    if(auto* P=Cast<AClaraStoryProp>(Target))
    {const FString Id=P->GetPropId();return Id.StartsWith(TEXT("Cable_"))?TEXT("Prendi ")+P->GetDisplayName():Id.StartsWith(TEXT("Box_"))&&!SortingCable.IsEmpty()?TEXT("Riponi il cavo · ")+P->GetDisplayName():P->GetDisplayName();}
    if(auto* D=Cast<AClaraNetworkDevice>(Target))return D->GetModelId()==TEXT("Laptop")?TEXT("Apri o prendi il portatile"):TEXT("Ispeziona ")+D->GetDisplayName();
    return FString();
}
TArray<AActor*> AClaraStoryDirector::GetImportantActors()const
{
    TArray<AActor*> Result;
    for(TActorIterator<AClaraStoryNPC> It(GetWorld());It;++It)if(IsMissionTarget(*It))Result.Add(*It);
    for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)if(IsMissionTarget(*It))Result.Add(*It);
    for(const auto& Pair:Devices)if(IsMissionTarget(Pair.Value.Get()))Result.Add(Pair.Value.Get());
    return Result;
}
bool AClaraStoryDirector::IsMissionTarget(AActor* Actor)const
{
    if(!Actor||Actor->IsHidden())return false;
    if(const auto* NPC=Cast<AClaraStoryNPC>(Actor))
    {
        const FString Id=NPC->GetCharacterId();
        return (Phase==0&&Id==TEXT("Alessia"))||((Phase==2||Phase==100)&&Id==TEXT("Luca"))||(Phase==108&&Id==TEXT("Gaia"));
    }
    if(const auto* Prop=Cast<AClaraStoryProp>(Actor))
    {
        const FString Id=Prop->GetPropId();
        if(Phase==1)return Id==TEXT("Badge");
        if(Phase==4)
        {if(Id.StartsWith(TEXT("Cable_")))return !Sorted.Contains(Id.Mid(6));if(Id.StartsWith(TEXT("Box_")))return !Sorted.Contains(Id.Mid(4));}
        if(Phase==5||Phase==7||Phase==14)return Id==TEXT("Cable_ethernet")||Id==TEXT("Box_ethernet");
        if(Phase==9||Phase==105)return Id==TEXT("Cable_console")||Id==TEXT("Box_console");
        if(Phase==13)return Id==TEXT("SFP")||Id==TEXT("Cable_fiber")||Id==TEXT("Box_fiber");
        return false;
    }
    const auto* Device=Cast<AClaraNetworkDevice>(Actor);if(!Device||Device->IsPreview())return false;
    auto Is=[&](const TCHAR* Id){return Devices.FindRef(Id).Get()==Actor;};
    if(Phase==3)return Is(TEXT("LAPTOP"));
    if(Phase==5||Phase==9||Phase==10||Phase==15||Phase==105||Phase==106||Phase==107)return Is(TEXT("LAPTOP"))||Is(TEXT("SW1"));
    if(Phase==7||Phase==14)return Is(TEXT("LAPTOP"))||Is(TEXT("PC2"))||Is(TEXT("SW1"));
    if(Phase==13)return Is(TEXT("SW1"))||Is(TEXT("R1"))||Is(TEXT("UPLINK"));
    return (Phase==6||Phase==8||Phase==11||Phase==12||Phase==101||Phase==102||Phase==103||Phase==104||Phase==109)&&Is(TEXT("LAPTOP"));
}
void AClaraStoryDirector::PickupLaptop()
{
    if(Phase<3){Notify(TEXT("Prima presentati ad Alessia e raggiungi Luca."));return;}
    if(!Devices.Contains(TEXT("LAPTOP"))||!Controller.IsValid()||!Controller->GetPawn())return;auto* L=Devices[TEXT("LAPTOP")].Get();
    ReturnHeldCable();
    for(const auto& P:L->GetPorts()){FString E;L->DisconnectPort(P.Id,E);}CloseLaptopScreen();
    Owned.Add(TEXT("laptop"));CarriedLaptop=true;L->SetLaptopCarried(true);
    auto* Camera=Controller->GetPawn()->FindComponentByClass<UCameraComponent>();
    L->AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);L->SetActorRelativeLocation({48,27,-38});L->SetActorRelativeRotation({0,90,0});
    for(UActorComponent* C:L->GetComponents())if(auto* P=Cast<UPrimitiveComponent>(C))P->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    if(Phase==3){SetPhase(4);Say(TEXT("Luca"),{TEXT("Puoi portarlo con te e aprirlo in mano con T, ovunque. Se vuoi appoggiarlo su un tavolo, guarda il piano e premi Q."),TEXT("Prima guarda i cavi sul tavolo vicino alla finestra. Le tre scatole hanno nomi diversi: osserva le spine e le scritte, poi sistemali. Il colore, da solo, non garantisce il tipo di un cavo.")});}
    else Notify(TEXT("Portatile in mano · T per aprirlo ovunque · Q per posarlo · I per la borsa"));
}
void AClaraStoryDirector::PlaceLaptop()
{
    if(!CarriedLaptop||!Devices.Contains(TEXT("LAPTOP")))return;
    auto* L=Devices[TEXT("LAPTOP")].Get();FVector P;FHitResult Hit;bool FoundSurface=false;
    if(Controller.IsValid())
    {FVector Eye;FRotator R;Controller->GetPlayerViewPoint(Eye,R);FCollisionQueryParams Params;Params.AddIgnoredActor(L);Params.AddIgnoredActor(Controller->GetPawn());
    if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+R.Vector()*250,ECC_Visibility,Params)&&Hit.ImpactNormal.Z>.7&&Hit.ImpactPoint.Z>30&&Hit.ImpactPoint.Z<120){P=Hit.ImpactPoint+FVector(0,0,.2);FoundSurface=true;}}
    if(!FoundSurface){Notify(TEXT("Guarda un piano libero del tavolo e premi Q per posare il portatile."));return;}
    L->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);L->SetActorLocationAndRotation(P,FRotator::ZeroRotator);L->SetLaptopCarried(false);CarriedLaptop=false;Notify(TEXT("Portatile posato. Clicca per aprire lo schermo; dalla borsa puoi riprenderlo."));
}
void AClaraStoryDirector::OpenLaptopScreen()
{
    if(!Owned.Contains(TEXT("laptop"))){Notify(TEXT("Prima prendi il portatile del laboratorio."));return;}
    if(!Devices.Contains(TEXT("LAPTOP"))||!Controller.IsValid())return;
    Controller->CloseDevice();
    if(CarriedLaptop)Devices[TEXT("LAPTOP")]->SetLaptopCarried(false);
    if(Display)Display->DeactivateDisplay();
    ScreenOpen=true;RefreshScreen();Controller->bShowMouseCursor=true;
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);Controller->SetInputMode(Mode);
}
void AClaraStoryDirector::RefreshScreen()
{
    if(!ScreenOpen||!GetWorld()->GetGameViewport())return;
    if(ScreenOverlay.IsValid())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ScreenOverlay.ToSharedRef());
    ScreenOverlay=MakeClaraStoryScreen(this);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(ScreenOverlay.ToSharedRef(),18);
}
void AClaraStoryDirector::CloseLaptopScreen()
{
    if(ScreenOverlay.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ScreenOverlay.ToSharedRef());
    ScreenOverlay.Reset();if(Display)Display->DeactivateDisplay();ScreenOpen=false;
    if(CarriedLaptop&&Devices.Contains(TEXT("LAPTOP")))Devices[TEXT("LAPTOP")]->SetLaptopCarried(true);
}
void AClaraStoryDirector::SetScreenPage(const FString& N){ScreenPage=N;if(N==TEXT("notebook")){OpenNotebook();return;}if(N==TEXT("inventory")){OpenInventory();return;}if(!ScreenOpen)OpenLaptopScreen();else RefreshScreen();}
void AClaraStoryDirector::Tick(float D)
{
    Super::Tick(D);TickGateway();if(GetWorld()->GetTimeSeconds()>ToastUntil)Toast.Empty();
    if(GetWorld()->GetTimeSeconds()>PollAt){PollAt=GetWorld()->GetTimeSeconds()+.25;PollLab();}
}
bool AClaraStoryDirector::PhysicalLinked(const FString& A,const FString& B,const FString& Kind)const
{
    if(!Devices.Contains(A)||!Devices.Contains(B))return false;
    for(const auto& P:Devices[A]->GetPorts())
    {FString PeerPort;auto* Peer=Devices[A]->GetPeerForPort(P.Id,PeerPort);if(Peer==Devices[B]&&
      (Kind==TEXT("ethernet")?P.Kind==TEXT("ethernet"):Kind==TEXT("console")?(P.Kind==TEXT("console_host")||P.Kind==TEXT("console")):Devices[A]->GetEffectivePortKind(P.Id)==TEXT("fiber")))return true;}
    return false;
}
bool AClaraStoryDirector::HostsReachable()const{return PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"))&&PhysicalLinked(TEXT("PC2"),TEXT("SW1"));}
void AClaraStoryDirector::LearnFromPing()
{
    for(const FString Id:{TEXT("LAPTOP"),TEXT("PC2")})if(Devices.Contains(Id))
    {FString PeerPort;if(Devices[Id]->GetPeerForPort(TEXT("Ethernet"),PeerPort)==Devices[TEXT("SW1")])LearnedMACs.Add(Id==TEXT("LAPTOP")?LaptopMAC:PC2MAC,PeerPort);}
    Day1CLI.SetLearnedMacTable(LearnedMACs);ArpCache.Add(PC2IP,PC2MAC);
}
void AClaraStoryDirector::PollLab()
{
    if(Phase==5&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1")))SetPhase(6);
    if(Phase==7&&HostsReachable())SetPhase(8);
    if(Phase==9&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console")))SetPhase(10);
    if(Phase==13&&PhysicalLinked(TEXT("SW1"),TEXT("R1"))&&PhysicalLinked(TEXT("SW1"),TEXT("UPLINK"),TEXT("fiber")))SetPhase(14);
    if(Phase==105&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console")))SetPhase(106);
}
TArray<FClaraStoryItem> AClaraStoryDirector::GetItems()const
{
    TArray<FClaraStoryItem> Items;
    auto Add=[&](const TCHAR* Id,const TCHAR* Label,const TCHAR* Detail){Items.Add({Id,Label,Detail,Owned.Contains(Id)});};
    Add(TEXT("badge"),TEXT("Badge CLARA"),TEXT("Il tuo primo giorno al laboratorio."));
    Add(TEXT("laptop"),TEXT("Portatile del laboratorio"),CarriedLaptop?TEXT("In mano. T apre il laboratorio virtuale ovunque; Q lo posa su un tavolo."):TEXT("Sul banco. Aprilo con T oppure prendilo e portalo alle console."));
    Add(TEXT("ethernet"),TEXT("Cavo Ethernet Cat 6"),TEXT("RJ-45 a entrambe le estremita': collega porte di rete in rame."));
    Add(TEXT("console"),TEXT("Cavo console"),TEXT("Adattatore USB del portatile verso CONSOLE RJ-45. Non alimenta lo switch."));
    Add(TEXT("fiber"),TEXT("Fibra LC duplex"),TEXT("Si collega a un modulo ottico compatibile, non alla gabbia SFP vuota."));
    Add(TEXT("sfp_module"),TEXT("Modulo SFP ottico"),TEXT("Inseriscilo nello slot SFP dello switch prima della fibra."));return Items;
}
void AClaraStoryDirector::UseItem(const FString& Id)
{ClosePage();if(Id==TEXT("laptop")){if(CarriedLaptop)PlaceLaptop();else PickupLaptop();}else if(Id==TEXT("badge"))Notify(TEXT("Badge gia' al sicuro nella borsa."));else EquipCable(Id);}
void AClaraStoryDirector::EquipCable(const FString& Kind)
{
    if(!Owned.Contains(Kind)){Notify(TEXT("Prima prendi il materiale dalla sua scatola."));return;}
    EquippedCable=Kind;if(Controller.IsValid())Controller->SetCableKind(Kind);
    Notify(Kind==TEXT("sfp_module")?TEXT("Modulo SFP in mano. Apri SW1 e seleziona uno slot SFP."):TEXT("Cavo selezionato. Apri un apparato e scegli i due capi."));ClosePage();
}
void AClaraStoryDirector::ReturnHeldCable()
{
    if(HeldCable.IsValid())
    {HeldCable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);HeldCable->SetActorTransform(HeldCableHome);HeldCable->SetActorEnableCollision(true);}
    HeldCable.Reset();
}
void AClaraStoryDirector::SelectSortingCable(const FString& K)
{ReturnHeldCable();SortingCable=K;
    if(Phase==4&&Controller.IsValid()&&Controller->GetPawn())
    {
        for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)if(It->GetPropId()==TEXT("Cable_")+K)
        {auto* Camera=Controller->GetPawn()->FindComponentByClass<UCameraComponent>();if(Camera){HeldCable=*It;HeldCableHome=It->GetActorTransform();It->SetActorEnableCollision(false);It->AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);It->SetActorRelativeLocation({50,-26,-24});It->SetActorRelativeRotation({20,0,0});}break;}
    }
Notify(K==TEXT("ethernet")?TEXT("Sulla guaina: CAT 6. Le due spine hanno otto contatti RJ-45."):K==TEXT("console")?TEXT("Una spina USB e un connettore RJ-45: il kit e' etichettato CONSOLE."):K==TEXT("fiber")?TEXT("Due piccoli connettori LC: la trasmissione e' ottica."):TEXT("Spina di alimentazione: non e' un cavo dati."));}
bool AClaraStoryDirector::SortCable(const FString& Bin)
{
    if(SortingCable.IsEmpty()){Notify(TEXT("Prendi prima un cavo e osserva le estremita'."));return false;}
    if(Bin!=SortingCable){Notify(TEXT("Qui non torna. Confronta le spine e la scritta sulla scatola."));return false;}
    if(HeldCable.IsValid()){HeldCableHome.SetLocation(FVector(Bin==TEXT("ethernet")?1065:Bin==TEXT("console")?1120:1175,1321,79));ReturnHeldCable();}
    Sorted.Add(Bin);Owned.Add(Bin);SortingCable.Empty();Notify(TEXT("Cavo riposto nella scatola corretta."));
    if(Sorted.Num()==3&&Phase==4){Owned.Add(TEXT("sfp_module"));ClosePage();SetPhase(5);Say(TEXT("Luca"),{TEXT("Adesso sai dove cercare. Prendi un Cat 6 dalla borsa e collega il portatile allo switch."),TEXT("Nel dettaglio puoi girare l'apparato, leggere la presa e scegliere il connettore. Un LED acceso ti dira' che quel tratto c'e'.")});}
    return true;
}
void AClaraStoryDirector::SetupPuzzle(const FString& K)
{
    if(K==TEXT("osi"))PuzzleExpected={TEXT("Physical · segnali e cavo"),TEXT("Data Link · frame e MAC"),TEXT("Network · IP e instradamento"),TEXT("Transport · TCP/UDP"),TEXT("Session · sessioni"),TEXT("Presentation · formati dei dati"),TEXT("Application · servizi applicativi")};
    else if(K==TEXT("tcpip"))PuzzleExpected={TEXT("Network Access · OSI 1 e 2"),TEXT("Internet · OSI 3"),TEXT("Transport · OSI 4"),TEXT("Application · OSI 5, 6 e 7")};
    else PuzzleExpected={TEXT("Il portatile domanda in broadcast: chi ha 192.168.1.11?"),TEXT("PC2 risponde al portatile con il proprio MAC"),TEXT("Il portatile memorizza IP/MAC e puo' inviare il frame")};
    Puzzle=PuzzleExpected;Algo::Reverse(Puzzle);ScreenPage=TEXT("puzzle");
}
void AClaraStoryDirector::MovePuzzleItem(int32 I,int32 Dir)
{const int32 J=I+Dir;if(Puzzle.IsValidIndex(I)&&Puzzle.IsValidIndex(J)){Puzzle.Swap(I,J);if(ScreenOpen)RefreshScreen();}}
bool AClaraStoryDirector::CheckPuzzle()
{
    if(Puzzle!=PuzzleExpected){Notify(TEXT("Riguarda il percorso: che cosa serve al passaggio successivo? Puoi chiedere un indizio."));return false;}
    if(Phase==11){SetPhase(12);Notify(TEXT("OSI ricostruito. Ora la stessa comunicazione nel modello TCP/IP."));}
    else if(Phase==12){CloseLaptopScreen();SetPhase(13);}
    else if(Phase==101){SetPhase(102);ScreenPage=TEXT("terminal");TerminalTarget=TEXT("LAPTOP");OpenLaptopScreen();Notify(TEXT("Ordine corretto. Ora osserva la cache ARP, prima e dopo un ping."));}
    return true;
}
FString AClaraStoryDirector::GetQuestion()const
{
    if(Phase==10||Phase==15)return TEXT("Quale porta dello switch ha imparato il MAC del portatile ")+LaptopMAC+TEXT("?");
    if(Phase!=104)return FString();
    const TCHAR* Q[]={TEXT("192.168.1.70/26 · Scrivi 70 in binario su 8 bit"),TEXT("255.255.255.192 · Scrivi 192 in binario su 8 bit"),TEXT("Esegui l'AND e scrivi l'indirizzo di rete completo"),TEXT("Metti a 1 i bit host: qual e' l'indirizzo di broadcast?"),TEXT("Quanti host utilizzabili ha questa /26?"),TEXT("192.168.1.130 appartiene alla stessa rete? Rispondi si oppure no")};
    return Q[FMath::Clamp(SubnetStep,0,5)];
}
FString AClaraStoryDirector::GetAnswerHint()const
{
    if(Phase==10||Phase==15)return TEXT("Leggi prima ipconfig /all sul PC, poi show mac address-table dalla console. Cerca lo stesso MAC.");
    const TCHAR* H[]={TEXT("Pesi: 128,64,32,16,8,4,2,1. Usa otto cifre."),TEXT("192 = 128 + 64. Gli altri bit valgono zero."),TEXT("L'AND tiene 1 solo quando entrambi i bit sono 1."),TEXT("La /26 lascia sei bit host: mettili a 1."),TEXT("2 elevato ai bit host, meno rete e broadcast."),TEXT("Confronta 130 con l'intervallo appena trovato.")};return H[FMath::Clamp(SubnetStep,0,5)];
}
bool AClaraStoryDirector::SubmitAnswer(const FString& Answer)
{
    FString A=Clean(Answer);A.ReplaceInline(TEXT(" "),TEXT(""));
    if(Phase==10||Phase==15)
    {
        if(!Completed.Contains(TEXT("mac-read"))||!Completed.Contains(TEXT("mac-table"))){Notify(TEXT("Apri prima le due viste: indirizzo fisico del PC e tabella dello switch."));return false;}
        A.ReplaceInline(TEXT("GigabitEthernet"),TEXT("Gi"));const FString* Port=LearnedMACs.Find(LaptopMAC);
        if(!Port||!A.Equals(*Port,ESearchCase::IgnoreCase)){Notify(TEXT("Confronta lo stesso MAC, non l'IP. La porta e' nella seconda colonna."));return false;}
        CloseLaptopScreen();if(Phase==10)SetPhase(11);else SetPhase(16);return true;
    }
    if(Phase==104)
    {
        const FString Expected[]={TEXT("01000110"),TEXT("11000000"),TEXT("192.168.1.64"),TEXT("192.168.1.127"),TEXT("62"),TEXT("no")};
        if(!A.Equals(Expected[SubnetStep],ESearchCase::IgnoreCase)){Notify(TEXT("Riprova: ")+GetAnswerHint());return false;}
        ++SubnetStep;Notify(TEXT("Passaggio corretto."));
        if(SubnetStep==6){CloseLaptopScreen();SetPhase(105);Say(TEXT("Luca"),{TEXT("Fatto: rete .64, broadcast .127, 62 host utilizzabili. L'indirizzo e la mask vanno letti insieme."),TEXT("Passiamo allo switch di prova. Il cavo console e' quello che ci permette di configurarlo da zero.")});}else RefreshScreen();return true;
    }
    return false;
}
FClaraSwitchCLI* AClaraStoryDirector::ActiveCLI()
{if(TerminalTarget==TEXT("SW-A"))return &SwitchA;if(TerminalTarget==TEXT("SW-B"))return &SwitchB;return Phase>=100?&TrialCLI:&Day1CLI;}
const FClaraSwitchCLI* AClaraStoryDirector::ActiveCLI()const{return const_cast<AClaraStoryDirector*>(this)->ActiveCLI();}
FString AClaraStoryDirector::GetTerminalTitle()const{return TerminalTarget==TEXT("LAPTOP")?TEXT("PC-LAB · terminale del computer"):TerminalTarget+TEXT(" · sessione console dello switch");}
FString AClaraStoryDirector::GetTerminalPrompt()const{if(TerminalTarget==TEXT("CML"))return GatewayNode+TEXT(" [CML]>");return TerminalTarget==TEXT("LAPTOP")?TEXT("tecnico@clara:~$"):ActiveCLI()->GetPrompt();}
void AClaraStoryDirector::SelectTerminalTarget(const FString& Id)
{
    const bool Available=Id==TEXT("LAPTOP")||(Phase==109?(Id==TEXT("SW-A")||Id==TEXT("SW-B")):(Id==(Phase>=100?TEXT("SW-PROVA"):TEXT("SW1"))));
    if(Id==TEXT("CML")){TerminalTarget=Id;ScreenPage=TEXT("gateway");RefreshScreen();return;}
    if(!Available){Notify(TEXT("Questa console non fa parte della prova corrente."));return;}
    if(Id!=TEXT("LAPTOP"))
    {
        const bool Connected=Phase==109?ConsoleTarget==Id:PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console"));
        if(!Connected){Notify(TEXT("Prima collega la USB console del portatile alla porta CONSOLE dello switch."));return;}
    }
    TerminalTarget=Id;Transcript.Empty();ScreenPage=TEXT("terminal");RefreshScreen();
}
FString AClaraStoryDirector::ExecutePC(const FString& Input)
{
    FString C=Clean(Input),Lower=C.ToLower();
    if(Lower==TEXT("help")||Lower==TEXT("?")||Lower==TEXT("aiuto"))return TEXT("STRUMENTI DI RETE\n\nip addr         Mostra indirizzo e MAC su Linux.\nipconfig /all    Mostra IPv4, mask, gateway e indirizzo fisico del PC.\nping <IP>        Invia una richiesta di risposta a quell'indirizzo.\narp -a           Mostra gli abbinamenti IP/MAC ricordati dal PC.\narp -d *         Svuota la cache ARP del laboratorio.\n\nUn comando e' composto dal nome dello strumento, uno spazio e gli eventuali argomenti. Leggi l'IP del destinatario sulla sua scheda: qui PC2 e' 192.168.1.11.\nLa console dello switch e' una sessione diversa: collegala e seleziona SW1.");
    if(Lower==TEXT("ip addr")||Lower==TEXT("ip a")||Lower==TEXT("ifconfig"))
    {Completed.Add(TEXT("mac-read"));return TEXT("2: eth0: <BROADCAST,MULTICAST,UP>\n    link/ether 02:aa:00:00:00:10\n    inet ")+LaptopIP+TEXT("/24 brd 192.168.1.255 scope global eth0\n\nMAC mostrato anche come 02-AA-00-00-00-10 nelle schede del laboratorio.");}
    if(Lower==TEXT("ip neigh")){Lower=TEXT("arp -a");}
    if(Lower==TEXT("ipconfig")||Lower==TEXT("ipconfig /all"))
    {if(Lower.EndsWith(TEXT("/all")))Completed.Add(TEXT("mac-read"));return TEXT("Scheda Ethernet PC-LAB\n  IPv4: ")+LaptopIP+TEXT("\n  Subnet mask: 255.255.255.0\n  Gateway: 192.168.1.1\n  Indirizzo fisico: ")+LaptopMAC;}
    if(Lower==TEXT("arp -d *")){ArpCache.Empty();Completed.Add(TEXT("arp-cleared"));return TEXT("Cache ARP del laboratorio svuotata.");}
    if(Lower==TEXT("arp -a"))
    {
        if(Phase==102&&Completed.Contains(TEXT("arp-cleared"))&&ArpCache.IsEmpty())SetPhase(103);
        if(Phase==103&&ArpCache.Contains(PC2IP)){CloseLaptopScreen();SetPhase(104);}
        FString R=TEXT("Indirizzo Internet       Indirizzo fisico         Tipo\n");for(const auto& E:ArpCache)R+=E.Key+TEXT("    ")+E.Value+TEXT("    dinamico\n");return ArpCache.IsEmpty()?TEXT("Nessuna voce ARP trovata."):R;
    }
    if(Lower.StartsWith(TEXT("ping ")))
    {
        FString IP=Clean(C.Mid(5));TArray<FString> Parts;IP.ParseIntoArray(Parts,TEXT("."));bool Valid=Parts.Num()==4;for(const auto& P:Parts)Valid=Valid&&P.IsNumeric()&&FCString::Atoi(*P)>=0&&FCString::Atoi(*P)<=255;if(!Valid)return TEXT("Indirizzo IPv4 non valido. Esempio di struttura: ping 192.168.1.11");
        bool OK=false;
        if(Phase==109)
        {
            const auto Nodes=GetSimNodes();const auto* Dest=Nodes.FindByPredicate([&](const auto& N){return N.IP==IP;});
            OK=Dest&&SimReachable(TEXT("LAPTOP"),Dest->Id)&&Dest->IP.StartsWith(TEXT("192.168.1."));
            if(Dest&&Dest->Kind==TEXT("switch"))OK=OK&&(Dest->Id==TEXT("SW-A")?SwitchA:SwitchB).IsSVIUp();
        }
        else if(IP==PC2IP){OK=HostsReachable();if(OK)LearnFromPing();}
        else if(IP==TEXT("192.168.1.2"))OK=HostsReachable()&&TrialCLI.IsSVIUp()&&TrialCLI.GetManagementAddress()==IP;
        if(Phase==6&&IP==PC2IP)SetPhase(OK?8:7);
        if(Phase==8&&IP==PC2IP&&OK){CloseLaptopScreen();SetPhase(9);}
        if(Phase==14&&IP==PC2IP&&OK){Completed.Remove(TEXT("mac-table"));SetPhase(15);Notify(TEXT("Risponde. Ora rileggi la tabella: la porta potrebbe essere cambiata."));}
        return OK?TEXT("Reply from ")+IP+TEXT(": bytes=32 time<1ms TTL=128\nReply from ")+IP+TEXT(": bytes=32 time<1ms TTL=128\nPacchetti: inviati=2, ricevuti=2, persi=0"):TEXT("Request timed out.\nNessuna risposta nel modello locale. Controlla percorso e indirizzi.");
    }
    if(Lower.StartsWith(TEXT("ssh ")))
    {
        const bool A=C.Contains(TEXT("192.168.1.2"));const auto& CLI=A?SwitchA:SwitchB;
        return Phase==109&&CLI.IsSSHConfigured()&&SimReachable(TEXT("LAPTOP"),A?TEXT("SW-A"):TEXT("SW-B"))?TEXT("Verifica del modello: accesso SSH configurato e raggiungibile. La sessione reale richiede il backend Cisco."):TEXT("SSH non disponibile: controlla rete, chiave, utente e linee VTY.");
    }
    if(Lower==TEXT("enable")||Lower.StartsWith(TEXT("conf")))return TEXT("Questo e' il terminale del PC. Collega il cavo console e seleziona lo switch per usare i comandi IOS.");
    return TEXT("Strumento non riconosciuto. Apri la scheda Comandi oppure scrivi help: cerca il compito che vuoi fare.");
}
FString AClaraStoryDirector::RunCommand(const FString& Input)
{
    if(TerminalTarget==TEXT("CML"))return RunGatewayCommand(Input);
    FString Out;
    const FString Prompt=GetTerminalPrompt();
    if(TerminalTarget==TEXT("LAPTOP"))Out=ExecutePC(Input);
    else
    {
        const bool Wired=Phase==109?ConsoleTarget==TerminalTarget:PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console"));
        if(!Wired)Out=TEXT("Sessione console scollegata. Controlla i due capi del cavo azzurro.");
        else
        {
            auto* CLI=ActiveCLI();CLI->SetLearnedMacTable(LearnedMACs);
            TSet<FString> Targets;if(HostsReachable()&&CLI->IsSVIUp())Targets.Add(LaptopIP);CLI->SetReachableTargets(Targets);
            if(Phase==109)ChallengeRebooted=false;
            Out=CLI->Execute(Input);
            FString N=Input.ToLower();if(N.Contains(TEXT("mac address-table"))&&!Out.StartsWith(TEXT("%"))&&!LearnedMACs.IsEmpty())Completed.Add(TEXT("mac-table"));
            if(Phase==107&&N.Contains(TEXT("show startup-config"))&&!Out.StartsWith(TEXT("%")))Completed.Add(TEXT("startup-read"));
            if(Phase==106&&TrialCLI.ValidateDay2(TEXT("SW-PROVA"),TEXT("192.168.1.2")).IsEmpty()){Completed.Remove(TEXT("startup-read"));SetPhase(107);Notify(TEXT("Configurazione completa e salvata. Rileggi show startup-config e prova ping 192.168.1.10."));}
            else if(Phase==107&&Completed.Contains(TEXT("startup-read"))&&TrialCLI.ValidateDay2(TEXT("SW-PROVA"),TEXT("192.168.1.2")).IsEmpty()&&N.StartsWith(TEXT("ping "))&&!Out.Contains(TEXT("%"))&&Out.Contains(TEXT("!"))){CloseLaptopScreen();SetPhase(108);}
        }
    }
    Transcript+=Prompt+TEXT(" ")+Input+TEXT("\n")+Out+TEXT("\n\n");if(Transcript.Len()>30000)Transcript.RightChopInline(Transcript.Len()-24000);
    return Out;
}
FString AClaraStoryDirector::CompleteCommand(const FString& Prefix)const
{
    if(TerminalTarget!=TEXT("LAPTOP"))return ActiveCLI()->Complete(Prefix);
    for(const FString C:{TEXT("ip addr"),TEXT("ip neigh"),TEXT("ipconfig /all"),TEXT("ping "),TEXT("arp -a"),TEXT("arp -d *"),TEXT("help")})if(C.StartsWith(Prefix,ESearchCase::IgnoreCase))return C;return Prefix;
}
FString AClaraStoryDirector::CommandHelp(const FString& Prefix)const
{return TerminalTarget==TEXT("LAPTOP")?TEXT("ip addr · leggi indirizzi e MAC (Linux)\nipconfig /all · compatibilita' con gli appunti\nping <IP> · prova un destinatario\narp -a · osserva la cache\narp -d * · svuota la cache\nhelp · manuale guidato"):ActiveCLI()->Help(Prefix);}
void AClaraStoryDirector::ResetTrialSwitch(){TrialCLI.Reset();Transcript.Empty();Completed.Remove(TEXT("startup-read"));if(Phase==107)SetPhase(106);Notify(TEXT("Solo lo switch di prova e' stato azzerato. Puoi ricominciare dal prompt Switch>."));}
void AClaraStoryDirector::RequestHint()
{
    ++HintLevel;
    if(Phase==10||Phase==15||Phase==104){Notify(GetAnswerHint());return;}
    if(Phase==4){Notify(TEXT("Osserva prima le spine: due RJ-45, USB→RJ-45, oppure due LC. Il colore e' solo l'etichetta del nostro kit."));return;}
    if(Phase==11||Phase==12||Phase==101){Notify(Phase==101?TEXT("Prima si domanda a tutti, poi risponde il proprietario, infine si memorizza."):TEXT("Parti dal cavo. Poi frame/MAC, pacchetto/IP e servizi via via piu' vicini all'applicazione."));return;}
    if(Phase==106||Phase==107){Notify(TrialCLI.ValidateDay2(TEXT("SW-PROVA"),TEXT("192.168.1.2")));return;}
    if(Phase==109){Notify(TEXT("Controlla in ordine: cavi, IP di PC3, configurazione di entrambi gli switch, SSH e salvataggio. TEST elenca cio' che manca."));return;}
    Notify(HintLevel==1?TEXT("Segui il percorso completo, non solo il primo LED. Puoi ispezionare ogni porta."):TEXT("La scheda Comandi del portatile spiega ping. Per PC2 cerca l'indirizzo 192.168.1.11."));
}
FString AClaraStoryDirector::GetNotebookText()const
{
    return GetTitle()+TEXT("\n\nOBIETTIVO\n")+GetObjective()+TEXT("\n\nIL BANCO\nPC-LAB 192.168.1.10 /24\nPC2 192.168.1.11 /24\nR1 192.168.1.1\n\nSTRUMENTI\nipconfig /all: indirizzi del PC\nping <IP>: richiesta di risposta\narp -a: cache IP/MAC del PC\nConsole: sessione separata sullo switch\n? e Tab: aiuto nel contesto IOS\n\nGIORNO 2 · SCHEDA SW-PROVA\nNome SW-PROVA; IP 192.168.1.2 /24; gateway 192.168.1.1.\nEnable secret e password console con login; offuscamento delle password di linea (non protezione forte). Banner. Dominio hacktheroute.local; utente admin; RSA almeno 2048 e SSHv2; VTY solo SSH con login local. Salva running-config in startup-config.\n\nCHALLENGE\nSW-A 192.168.1.2; SW-B 192.168.1.3; PC3 atteso 192.168.1.12; SRV1 192.168.1.20. Tutti /24, gateway 192.168.1.1. Le password del kit sono esempi di laboratorio.\n\nREGOLE\nUn MAC identifica un'interfaccia di livello 2 e puo' essere modificato. Il gateway serve per uscire dalla sottorete, non per il ping locale. Per la fibra servono moduli ottici compatibili.\n\nModalita' attuale: laboratorio didattico locale. Nessuna sessione Cisco CML viene dichiarata attiva.");
}
void AClaraStoryDirector::StartDay2()
{
    ClosePage();CloseLaptopScreen();
    for(const FString I:{TEXT("badge"),TEXT("laptop"),TEXT("ethernet"),TEXT("console"),TEXT("fiber"),TEXT("sfp_module")})Owned.Add(I);
    if(CarriedLaptop){auto* L=Devices.FindRef(TEXT("LAPTOP")).Get();if(L){L->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);L->SetActorLocationAndRotation({1135,1120,77.2},FRotator::ZeroRotator);L->SetLaptopCarried(false);}CarriedLaptop=false;}
    FString E;auto* SW=Devices.FindRef(TEXT("SW1")).Get();
    if(SW)for(const FString H:{TEXT("LAPTOP"),TEXT("PC2")})if(!PhysicalLinked(H,TEXT("SW1")))
        for(const auto& P:SW->GetPorts())if(P.Kind==TEXT("ethernet")&&!SW->IsPortOccupied(P.Id)){AClaraNetworkDevice::ConnectPorts(Devices[H],TEXT("Ethernet"),SW,P.Id,TEXT("ethernet"),E);break;}
    SetPhase(100);SaveProgress();
}
void AClaraStoryDirector::SaveProgress()
{if(!SelfTesting&&(Completed.Contains(TEXT("day1"))||Phase>=100))FFileHelper::SaveStringToFile(Phase==110?TEXT("{\"day1Complete\":true,\"day2Complete\":true}"):TEXT("{\"day1Complete\":true}"),*(FPaths::ProjectSavedDir()/ProgressFile));}
void AClaraStoryDirector::SetupChallenge()
{
    SimNodes={{TEXT("LAPTOP"),TEXT("Portatile"),TEXT("host"),TEXT("192.168.1.10"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{130,100}},
    {TEXT("R1"),TEXT("R1 · gia' pronto"),TEXT("router"),TEXT("192.168.1.1"),TEXT("255.255.255.0"),TEXT(""),{500,40}},
    {TEXT("SW-A"),TEXT("SW-A"),TEXT("switch"),TEXT(""),TEXT("255.255.255.0"),TEXT(""),{350,200}},
    {TEXT("SW-B"),TEXT("SW-B"),TEXT("switch"),TEXT(""),TEXT("255.255.255.0"),TEXT(""),{650,200}},
    {TEXT("PC2"),TEXT("PC2"),TEXT("host"),TEXT("192.168.1.11"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{180,370}},
    {TEXT("PC3"),TEXT("PC3"),TEXT("host"),TEXT("192.168.2.12"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{650,370}},
    {TEXT("SRV1"),TEXT("SRV1"),TEXT("host"),TEXT("192.168.1.20"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{850,370}}};
    SimLinks.Empty();SwitchA.Reset();SwitchB.Reset();ConsoleTarget.Empty();SimSelected.Empty();SimPending.Empty();SimCableKind.Empty();ChallengeReady=false;ChallengeRebooted=false;
    SimMessage=TEXT("Scheda Gaia: SW-A .2, SW-B .3, PC3 .12. Tutti /24, gateway .1. Collega Ethernet, poi console al portatile per configurare ogni switch.");
}
TArray<FClaraSimNode> AClaraStoryDirector::GetSimNodes()const
{
    TArray<FClaraSimNode> Result=SimNodes;
    for(auto& N:Result)if(N.Kind==TEXT("switch")){const auto& CLI=N.Id==TEXT("SW-A")?SwitchA:SwitchB;N.IP=CLI.GetManagementAddress();N.Gateway=CLI.GetGateway();}
    return Result;
}
TArray<TPair<FString,FString>> AClaraStoryDirector::GetSimLinks()const
{
    TArray<TPair<FString,FString>> L;
    if(Phase<109)
    {for(const auto& A:Devices)for(const auto& B:Devices)if(A.Key<B.Key&&PhysicalLinked(A.Key,B.Key))L.Add({A.Key,B.Key});return L;}
    for(const auto& E:SimLinks){FString A,B;if(E.Split(TEXT("|"),&A,&B))L.Add({A,B});}
    if(!ConsoleTarget.IsEmpty())L.Add({TEXT("LAPTOP"),ConsoleTarget});return L;
}
void AClaraStoryDirector::SetSimCableKind(const FString& K){SimCableKind=K;SimPending.Empty();SimMessage=K.IsEmpty()?TEXT("Seleziona un dispositivo."):TEXT("Scegli il primo dispositivo, poi il secondo.");}
void AClaraStoryDirector::SelectSimNode(const FString& Id)
{
    if(Phase!=109)
    {if(Devices.Contains(Id)&&Controller.IsValid()){CloseLaptopScreen();Controller->OpenDevice(Devices[Id]);}return;}
    if(!SimNodes.ContainsByPredicate([&](const auto& N){return N.Id==Id;}))return;
    SimSelected=Id;
    if(SimCableKind.IsEmpty())
    {if(Id.StartsWith(TEXT("SW-"))){SelectTerminalTarget(Id);return;}SimMessage=TEXT("Indirizzo attuale del dispositivo selezionato. Confrontalo con la scheda di Gaia.");return;}
    if(SimPending.IsEmpty()){SimPending=Id;SimMessage=TEXT("Prima estremita': ")+Id+TEXT(". Ora scegli il secondo capo.");return;}
    if(SimPending==Id){SimPending.Empty();SimMessage=TEXT("Collegamento annullato.");return;}
    if(SimCableKind==TEXT("console"))
    {
        const FString Switch=SimPending==TEXT("LAPTOP")?Id:Id==TEXT("LAPTOP")?SimPending:FString();
        if(!Switch.StartsWith(TEXT("SW-"))){SimMessage=TEXT("Console: collega il portatile alla console di uno switch.");SimPending.Empty();return;}
        ConsoleTarget=Switch;SimMessage=TEXT("Console collegata a ")+Switch+TEXT(". Passa al terminale e seleziona lo switch.");
    }
    else {const FString E=Edge(SimPending,Id);if(SimLinks.Contains(E)){SimLinks.Remove(E);SimMessage=TEXT("Cavo rimosso.");}else{SimLinks.Add(E);SimMessage=TEXT("Cavo Ethernet collegato.");}}
    SimPending.Empty();ChallengeReady=false;ChallengeRebooted=false;RefreshScreen();
}
bool AClaraStoryDirector::SimHasLink(const FString& A,const FString& B)const{return SimLinks.Contains(Edge(A,B));}
bool AClaraStoryDirector::SimReachable(const FString& A,const FString& B)const
{
    TSet<FString> Seen;TArray<FString> Queue;Seen.Add(A);Queue.Add(A);
    for(int32 I=0;I<Queue.Num();++I)
    {
        for(const auto& N:SimNodes)if(!Seen.Contains(N.Id)&&SimHasLink(Queue[I],N.Id))
        {Seen.Add(N.Id);Queue.Add(N.Id);}
    }
    return Seen.Contains(B);
}
bool AClaraStoryDirector::SetSimHostIP(const FString& Id,const FString& Address)
{
    auto* N=SimNodes.FindByPredicate([&](const auto& Item){return Item.Id==Id;});
    TArray<FString> Octets;Address.ParseIntoArray(Octets,TEXT("."));bool Valid=Octets.Num()==4;
    for(const auto& O:Octets)Valid=Valid&&!O.IsEmpty()&&O.IsNumeric()&&FCString::Atoi(*O)>=0&&FCString::Atoi(*O)<=255;
    if(!N||N->Kind!=TEXT("host")||!Valid){SimMessage=TEXT("Scegli un host e inserisci un IPv4 valido.");return false;}
    N->IP=Address;ChallengeReady=false;ChallengeRebooted=false;SimMessage=TEXT("Indirizzo aggiornato. Ora verifica la connettivita'.");return true;
}
FString AClaraStoryDirector::RunChallengeTest()
{
    if(Phase!=109)return TEXT("La challenge si apre con Gaia, dopo lo switch di prova.");
    TArray<FString> Problems;
    for(const auto& E:TArray<TPair<FString,FString>>{{TEXT("R1"),TEXT("SW-A")},{TEXT("SW-A"),TEXT("SW-B")},{TEXT("LAPTOP"),TEXT("SW-A")},{TEXT("PC2"),TEXT("SW-A")},{TEXT("PC3"),TEXT("SW-B")},{TEXT("SRV1"),TEXT("SW-B")}})
        if(!SimHasLink(E.Key,E.Value))Problems.Add(TEXT("Manca il collegamento ")+E.Key+TEXT(" ↔ ")+E.Value);
    const FString A=SwitchA.ValidateDay2(TEXT("SW-A"),TEXT("192.168.1.2"),false),B=SwitchB.ValidateDay2(TEXT("SW-B"),TEXT("192.168.1.3"),false);
    if(!A.IsEmpty())Problems.Add(TEXT("SW-A: ")+A);if(!B.IsEmpty())Problems.Add(TEXT("SW-B: ")+B);
    if(SwitchA.GetGateway()!=TEXT("192.168.1.1"))Problems.Add(TEXT("SW-A: gateway atteso 192.168.1.1."));
    if(SwitchB.GetGateway()!=TEXT("192.168.1.1"))Problems.Add(TEXT("SW-B: gateway atteso 192.168.1.1."));
    for(const auto& N:SimNodes)
    {
        const FString ExpectedIP=N.Id==TEXT("LAPTOP")?TEXT("192.168.1.10"):N.Id==TEXT("PC2")?TEXT("192.168.1.11"):N.Id==TEXT("PC3")?TEXT("192.168.1.12"):N.Id==TEXT("SRV1")?TEXT("192.168.1.20"):FString();
        if(!ExpectedIP.IsEmpty()&&N.IP!=ExpectedIP)Problems.Add(N.Id+TEXT(": l'IP non corrisponde alla scheda."));
        if(N.Kind==TEXT("host")&&!SimReachable(TEXT("LAPTOP"),N.Id))Problems.Add(TEXT("Il portatile non raggiunge ")+N.Id);
    }
    ChallengeReady=Problems.IsEmpty();
    if(ChallengeReady&&ChallengeRebooted){SimMessage=TEXT("TEST superato anche dopo il riavvio. SSH configurato, Telnet escluso, indirizzi e topologia corretti.");SetPhase(110);return SimMessage;}
    if(ChallengeReady)SimMessage=TEXT("Verifiche operative superate. Ora prova il riavvio: una configurazione non salvata verra' persa.");
    else SimMessage=TEXT("Da correggere:\n")+FString::Join(Problems,TEXT("\n"));
    return SimMessage;
}
void AClaraStoryDirector::RebootChallenge()
{
    if(!ChallengeReady){SimMessage=TEXT("Prima completa il TEST operativo.");return;}
    const bool Saved=SwitchA.IsStartupSaved()&&SwitchB.IsStartupSaved();SwitchA.ReloadFromStartup();SwitchB.ReloadFromStartup();ChallengeRebooted=true;
    if(!Saved){ChallengeReady=false;SimMessage=TEXT("Dopo il riavvio manca una configurazione salvata. Ricostruisci lo switch tornato nuovo e salva prima di riprovare.");}
    RunChallengeTest();
}
bool AClaraStoryDirector::RunSelfTest()
{
    TGuardValue<bool> TestGuard(SelfTesting,true);
    // Exercise the actual graph and validators; dialogue clicks alone never satisfy a lab gate.
    FString Error;bool OK=Devices.Contains(TEXT("LAPTOP"))&&Devices.Contains(TEXT("PC2"))&&Devices.Contains(TEXT("SW1"));
    if(!OK)return false;
    auto* SW=Devices[TEXT("SW1")].Get();auto* L=Devices[TEXT("LAPTOP")].Get();auto* PC=Devices[TEXT("PC2")].Get();
    int32 PickableCables=0;
    for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)if(It->GetPropId().StartsWith(TEXT("Cable_")))
    {
        FHitResult Hit;const FVector Center=It->GetInteractionComponent()->GetComponentLocation();FCollisionQueryParams Params;
        if(Controller.IsValid()&&Controller->GetPawn())Params.AddIgnoredActor(Controller->GetPawn());
        const bool HitCable=GetWorld()->LineTraceSingleByChannel(Hit,Center+FVector(0,0,80),Center-FVector(0,0,20),ECC_Visibility,Params)&&Hit.GetActor()==*It;
        PickableCables+=HitCable?1:0;
        UE_LOG(LogTemp,Display,TEXT("CLARA_CABLE_HIT_%s: %s"),HitCable?TEXT("PASS"):TEXT("FAIL"),*It->GetPropId());
    }
    OK=OK&&PickableCables==3;
    if(Controller.IsValid()&&Controller->GetPawn())
    {
        APawn* Pawn=Controller->GetPawn();const FTransform Start=Pawn->GetActorTransform();const FTransform LaptopStart=L->GetActorTransform();const FRotator Look=Controller->GetControlRotation();
        Phase=3;PickupLaptop();
        const bool PickedUp=CarriedLaptop&&L->IsLaptopCarried()&&L->GetAttachParentActor()==Pawn&&FMath::IsNearlyEqual(L->GetDeviceSize().Z,2.f,.1f);
        OK=OK&&PickedUp;ClosePage();
        const FTransform HeldTransform=L->GetActorTransform();AActor* PreviousView=Controller->GetViewTarget();
        OpenLaptopScreen();
        const bool OpenInHand=ScreenOpen&&CarriedLaptop&&!L->IsLaptopCarried()&&FMath::IsNearlyEqual(L->GetDeviceSize().Z,23.1f,.2f)&&L->GetAttachParentActor()==Pawn&&L->GetActorTransform().Equals(HeldTransform,.01f)&&Controller->GetViewTarget()==PreviousView&&ScreenOverlay.IsValid();
        OK=OK&&OpenInHand;CloseLaptopScreen();
        OK=OK&&CarriedLaptop&&L->IsLaptopCarried()&&L->GetAttachParentActor()==Pawn;
        UE_LOG(LogTemp,Display,TEXT("CLARA_LAPTOP_MOBILE_SCREEN_%s: attached and carried state unchanged; camera unchanged."),OpenInHand?TEXT("PASS"):TEXT("FAIL"));
        L->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);L->SetActorTransform(LaptopStart);L->SetLaptopCarried(false);CarriedLaptop=false;
        UE_LOG(LogTemp,Display,TEXT("CLARA_LAPTOP_CARRY_%s: pickup and opening in hand preserve attachment and camera."),(PickedUp&&OpenInHand)?TEXT("PASS"):TEXT("FAIL"));
        Pawn->SetActorTransform(Start);Controller->SetControlRotation(Look);if(Controller->PlayerCameraManager)Controller->PlayerCameraManager->UpdateCamera(.016f);
        Phase=0;Owned.Remove(TEXT("laptop"));Owned.Remove(TEXT("badge"));
    }
    else OK=false;
    OK=OK&&!HostsReachable();
    OK=OK&&AClaraNetworkDevice::ConnectPorts(L,TEXT("Ethernet"),SW,TEXT("Gi1/0/1"),TEXT("ethernet"),Error);
    OK=OK&&!HostsReachable();
    OK=OK&&AClaraNetworkDevice::ConnectPorts(PC,TEXT("Ethernet"),SW,TEXT("Gi1/0/2"),TEXT("ethernet"),Error)&&HostsReachable();
    LearnFromPing();OK=OK&&LearnedMACs.Num()==2&&ArpCache.Contains(PC2IP);
    OK=OK&&AClaraNetworkDevice::ConnectPorts(L,TEXT("ConsoleUSB"),SW,TEXT("Console"),TEXT("console"),Error);
    OK=OK&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console"));
    OK=OK&&!AClaraNetworkDevice::ConnectPorts(L,TEXT("ConsoleUSB"),SW,TEXT("Gi1/0/3"),TEXT("ethernet"),Error);
    OK=OK&&SW->InstallOpticalModule(TEXT("Gi1/0/25"));
    OK=OK&&AClaraNetworkDevice::ConnectPorts(SW,TEXT("Gi1/0/25"),Devices[TEXT("UPLINK")],TEXT("Optical"),TEXT("fiber"),Error);
    SetupPuzzle(TEXT("osi"));OK=OK&&Puzzle!=PuzzleExpected;Puzzle=PuzzleExpected;OK=OK&&Puzzle.Num()==7;
    SetupChallenge();Phase=109;OK=OK&&RunChallengeTest().Contains(TEXT("Manca il collegamento"))&&!ChallengeReady;
    const int32 LockedPhase=Phase;RebootChallenge();OK=OK&&Phase==LockedPhase&&!ChallengeRebooted;
    for(const auto& E:TArray<TPair<FString,FString>>{{TEXT("R1"),TEXT("SW-A")},{TEXT("SW-A"),TEXT("SW-B")},{TEXT("LAPTOP"),TEXT("SW-A")},{TEXT("PC2"),TEXT("SW-A")},{TEXT("PC3"),TEXT("SW-B")},{TEXT("SRV1"),TEXT("SW-B")}})SimLinks.Add(Edge(E.Key,E.Value));
    SetSimHostIP(TEXT("PC3"),TEXT("192.168.1.12"));
    auto Configure=[](FClaraSwitchCLI& CLI,const FString& Name,const FString& IP,bool Save)
    {
        CLI.Reset();CLI.Execute(TEXT("enable"));CLI.Execute(TEXT("conf t"));CLI.Execute(TEXT("hostname ")+Name);
        for(const TCHAR* C:{TEXT("interface vlan1"),TEXT("no shutdown")})CLI.Execute(C);
        CLI.Execute(TEXT("ip address ")+IP+TEXT(" 255.255.255.0"));CLI.Execute(TEXT("exit"));
        for(const TCHAR* C:{TEXT("enable secret Privato1"),TEXT("line console 0"),TEXT("password Console1"),TEXT("login"),TEXT("exit"),TEXT("ip domain-name clara.local"),TEXT("crypto key generate rsa"),TEXT("2048"),TEXT("username admin secret Personale1"),TEXT("ip ssh version 2"),TEXT("line vty 0 15"),TEXT("login local"),TEXT("transport input ssh"),TEXT("exit"),TEXT("banner motd #Accesso autorizzato#"),TEXT("service password-encryption"),TEXT("ip default-gateway 192.168.1.1"),TEXT("end")})CLI.Execute(C);
        if(Save)CLI.Execute(TEXT("write memory"));
    };
    Configure(SwitchA,TEXT("SW-A"),TEXT("192.168.1.2"),false);Configure(SwitchB,TEXT("SW-B"),TEXT("192.168.1.3"),false);
    RunChallengeTest();OK=OK&&ChallengeReady&&Phase==109;RebootChallenge();OK=OK&&!ChallengeReady&&Phase==109;
    Configure(SwitchA,TEXT("SW-A"),TEXT("192.168.1.2"),true);Configure(SwitchB,TEXT("SW-B"),TEXT("192.168.1.3"),true);ChallengeRebooted=false;
    RunChallengeTest();OK=OK&&ChallengeReady&&Phase==109;RebootChallenge();OK=OK&&Phase==110;ClosePage();
    Phase=4;SortingCable=TEXT("ethernet");OK=OK&&!SortCable(TEXT("console"))&&!Owned.Contains(TEXT("ethernet"));SortingCable.Empty();
    Phase=10;Completed.Remove(TEXT("mac-read"));Completed.Remove(TEXT("mac-table"));OK=OK&&!SubmitAnswer(TEXT("Gi1/0/1"))&&Phase==10;
    Phase=6;TerminalTarget=TEXT("LAPTOP");ExecutePC(TEXT("ping 192.168.1.11"));OK=OK&&Phase==9;ClosePage();
    OK=OK&&ExecutePC(TEXT("ping 300.1.1.1")).Contains(TEXT("non valido"));
    for(auto* D:{L,PC,SW,Devices[TEXT("UPLINK")].Get()})for(const auto& P:D->GetPorts()){FString E;D->DisconnectPort(P.Id,E);}
    LearnedMACs.Empty();ArpCache.Empty();SimNodes.Empty();Owned.Empty();Owned.Add(TEXT("notebook"));Completed.Empty();Sorted.Empty();Phase=0;Page.Empty();
    UE_LOG(LogTemp,Display,TEXT("CLARA_STORY_SELFTEST_%s: host path, MAC learning, console, optical module and locked challenge."),OK?TEXT("PASS"):TEXT("FAIL"));return OK;
}

bool AClaraStoryDirector::IsDeviceInspectionOpen()const{return Controller.IsValid()&&Controller->IsDeviceOpen();}
