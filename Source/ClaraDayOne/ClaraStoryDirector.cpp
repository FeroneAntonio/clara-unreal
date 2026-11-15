#include "ClaraStoryDirector.h"
#include "ClaraFrontEnd.h"
#include "ClaraNarrativeCamera.h"
#include "ClaraRoomVisitor.h"
#include "ClaraNetworkDevice.h"
#include "ClaraStoryCast.h"
#include "ClaraDisplay.h"
#include "ClaraAudio.h"
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
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

namespace
{
FString Edge(FString A,FString B){return A<B?A+TEXT("|")+B:B+TEXT("|")+A;}
FString Clean(FString T){T.TrimStartAndEndInline();return T;}
const TCHAR* ProgressFile=TEXT("ClaraStoryProgress.json");
bool ValidIPv4(const FString& Address,uint32* Parsed=nullptr)
{
    TArray<FString> Octets;Address.ParseIntoArray(Octets,TEXT("."),false);
    if(Octets.Num()!=4)return false;
    uint32 Number=0;
    for(const FString& Octet:Octets)
    {
        if(Octet.IsEmpty()||Octet.Len()>3||(Octet.Len()>1&&Octet[0]==TEXT('0')))return false;
        for(TCHAR C:Octet)if(C<TEXT('0')||C>TEXT('9'))return false;
        const int32 Value=FCString::Atoi(*Octet);if(Value>255)return false;
        Number=(Number<<8)|static_cast<uint32>(Value);
    }
    if(Parsed)*Parsed=Number;return true;
}
bool SameSubnet(const FString& A,const FString& B,const FString& Mask)
{
    uint32 First=0,Second=0,Netmask=0;
    return ValidIPv4(A,&First)&&ValidIPv4(B,&Second)&&ValidIPv4(Mask,&Netmask)&&(First&Netmask)==(Second&Netmask);
}
}
AClaraStoryDirector::AClaraStoryDirector(){PrimaryActorTick.bCanEverTick=true;}
void AClaraStoryDirector::BeginPlay()
{
    Super::BeginPlay();Controller=Cast<AClaraRoomController>(GetWorld()->GetFirstPlayerController());
    CreateLabHosts();CreateProps();CreateOfficeClock();GatewayBegin();
    if(GetWorld()->GetGameViewport())
    {HUD=MakeClaraStoryHUD(this);GetWorld()->GetGameViewport()->AddViewportWidgetContent(HUD.ToSharedRef(),21);}
    Owned.Add(TEXT("notebook"));SetPhase(0);
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraTerminalPreview")))
    {
        Persist=false;StartDay2();ClosePage();Phase=5;
        ScreenPage=TEXT("terminal");SwitchTerminalSession(TEXT("LAPTOP"));OpenLaptopScreen();
        Notify(TEXT("Prova libera: ls; ping 192.168.1.11; ping 192.168.1.99. Ctrl+C interrompe. Luca ti guida."));
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraDayEndPreview")))
    {
        // Explicit UI preview: never read or replace the player's real checkpoint.
        for(const TCHAR* Id:{TEXT("badge"),TEXT("laptop"),TEXT("ethernet"),TEXT("console"),TEXT("fiber"),TEXT("sfp_module")})Owned.Add(Id);
        SetPhase(16);
        UE_LOG(LogTemp,Display,TEXT("CLARA_DAY_END_PREVIEW: user progress untouched."));
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraTowerPreview")))
    {Owned.Add(TEXT("notebook"));Owned.Add(TEXT("laptop"));SetPhase(11);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraCableDemo"))||FParse::Param(FCommandLine::Get(),TEXT("ClaraEthernetDemo")))
    {
        Owned.Add(TEXT("badge"));Phase=3;PickupLaptop();ClosePage();
        if(Controller.IsValid()&&Controller->GetPawn())
        {Controller->GetPawn()->SetActorLocation({1120,1185,90});Controller->SetControlRotation({-38,90,0});}
        if(FParse::Param(FCommandLine::Get(),TEXT("ClaraEthernetDemo")))
        {for(const FString Kind:{TEXT("ethernet"),TEXT("console"),TEXT("fiber")}){Owned.Add(Kind);Sorted.Add(Kind);}Owned.Add(TEXT("sfp_module"));SetPhase(5);}
        Notify(TEXT("Prova aggiornata: collega Ethernet fra portatile e switch. La presa del portatile e' gia' inquadrata nel primo piano."));
        return;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ClaraStoryDay3"))){StartDay3();return;}
    if(AClaraFrontEnd::NeedsArrivalInterview(GetWorld())){Persist=false;return;}
    if(AClaraFrontEnd::IsCyberStory(GetWorld()))
    {
        Persist=false;Owned.Add(TEXT("badge"));Owned.Add(TEXT("laptop"));
        SetScreenPage(TEXT("cyber"));return;
    }
    // Salvataggio completo solo nella partita normale: prove automatiche e punti demo non lo leggono ne' lo riscrivono.
    Persist=!FApp::IsUnattended();
    FString FrontEndTourDirectory;
    if(FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTour="),FrontEndTourDirectory))Persist=false;
    for(const TCHAR* Flag:{TEXT("ClaraStoryCheck"),TEXT("ClaraFreshStory"),TEXT("ClaraStoryDay2"),TEXT("ClaraStoryDay3"),TEXT("ClaraLayoutCheck"),TEXT("ClaraGatewayCheck"),TEXT("ClaraGatewayPortCheck")})
        if(FParse::Param(FCommandLine::Get(),Flag))Persist=false;
    FString TourFile;if(FParse::Value(FCommandLine::Get(),TEXT("ClaraTour="),TourFile))Persist=false;
    if(Persist)
    {
        const FString File=FPaths::ProjectSavedDir()/ProgressFile;bool Found=false,Restored=false;
        for(const FString Suffix:{FString(),FString(TEXT(".bak"))})
        {
            const FString Candidate=File+Suffix;if(!IFileManager::Get().FileExists(*Candidate))continue;Found=true;
            FString Saved;TSharedPtr<FJsonObject> Save;
            if(IFileManager::Get().FileSize(*Candidate)>2*1024*1024||!FFileHelper::LoadFileToString(Saved,*Candidate)||
               !FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Saved),Save)||!RestoreProgress(Save))continue;
            Restored=true;
            if(!Suffix.IsEmpty())
            {
                Persist=IFileManager::Get().Copy(*File,*Candidate,true,true)==COPY_OK;
                Notify(Persist?TEXT("Recuperato l'ultimo salvataggio valido."):TEXT("Partita recuperata; impossibile scrivere il salvataggio. Controlla lo spazio disponibile."));
            }
            break;
        }
        if(Found&&!Restored){Persist=false;Notify(TEXT("Salvataggio non leggibile. Il file originale è stato conservato; scegli Nuova partita per ricominciare."));}
    }
}
void AClaraStoryDirector::EndPlay(const EEndPlayReason::Type R)
{
    SaveProgress();
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
        Cable->Initialize(TEXT("Cable_")+Kinds[I],I==0?TEXT("Cavo bianco"):I==1?TEXT("Cavo azzurro"):TEXT("Cavo giallo"));
        auto* Bin=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(X,1321,79),FRotator::ZeroRotator);Bin->Initialize(TEXT("Box_")+Kinds[I],I==0?TEXT("ETHERNET CAT 6"):I==1?TEXT("CONSOLE"):TEXT("FIBRA"));
        auto* M=UMaterialInstanceDynamic::Create(Base,Bin);M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.17f,.19f,.20f));M->SetVectorParameterValue(TEXT("BaseColor"),FLinearColor(.17f,.19f,.20f));
        for(FVector Offset:{FVector(-21,0,4),FVector(21,0,4),FVector(0,12,4),FVector(0,-12,4)})
        {auto* C=NewObject<UStaticMeshComponent>(Bin);Bin->AddInstanceComponent(C);C->SetMobility(EComponentMobility::Movable);C->SetupAttachment(Bin->GetRootComponent());C->SetStaticMesh(const_cast<UStaticMesh*>(Cube));C->SetMaterial(0,M);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();C->SetRelativeLocation(Offset);C->SetRelativeScale3D(FVector(Offset.X!=0?2:44,Offset.X!=0?26:2,8)/100);}
    }
    auto* Module=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(1000,1107,79),FRotator::ZeroRotator);Module->Initialize(TEXT("SFP"),TEXT("MODULO SFP"));
    auto* Coffee=GetWorld()->SpawnActor<AClaraStoryProp>(FVector(1196,1268,79),FRotator::ZeroRotator);Coffee->Initialize(TEXT("Coffee"),TEXT("Cavo nero"));
    // I cartellini provvisori sopra i colleghi restano spenti: il nome compare gia' nel segnaposto e nell'invito a parlare.
    for(TActorIterator<AClaraStoryNPC> It(GetWorld());It;++It){TArray<UTextRenderComponent*> Plates;It->GetComponents(Plates);for(UTextRenderComponent* Plate:Plates)Plate->SetVisibility(false);}
}
void AClaraStoryDirector::Notify(const FString& T){Toast=T;ToastUntil=GetWorld()->GetTimeSeconds()+FMath::Clamp(3.f+T.Len()*.055f,6.f,15.f);}
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
    if(!SelfTesting&&!ScreenOpen&&!OpenLaptopAfterConversation&&!IsDayTransitionActive()&&Controller.IsValid()&&!Controller->IsDeviceOpen())
        for(TActorIterator<AClaraStoryNPC> It(GetWorld());It;++It)if(It->GetCharacterId()==Who&&!It->IsHidden())
        {if(auto* Camera=UClaraNarrativeCamera::Get(Controller.Get()))Camera->BeginConversation(*It,Who==TEXT("Gaia")&&Phase==108);break;}
    if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(Speaker,Dialogue);
}
void AClaraStoryDirector::ContinueDialogue()
{
    if(Page!=TEXT("dialogue"))return;
    if(DialogueIndex+1<DialogueQueue.Num())
    {
        ++DialogueIndex;Speaker=DialogueQueue[DialogueIndex].Key;Dialogue=DialogueQueue[DialogueIndex].Value;
        AClaraAudio::Cue(this,TEXT("click"),.45f);if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(Speaker,Dialogue);
        return;
    }
    if(Choices.Num())return;
    const int32 Next=AfterDialogue;ClosePage();if(Next>=0)SetPhase(Next);
    else if(Phase==104&&Completed.Contains(TEXT("day2-host-read"))&&ScreenPage==TEXT("terminal"))ScreenPage=TEXT("subnet");
}
void AClaraStoryDirector::FinishDialogue()
{
    if(Page!=TEXT("dialogue"))return;
    if(LaptopMenu){ClosePage();return;}
    if(DialogueQueue.Num()&&DialogueIndex!=DialogueQueue.Num()-1)
    {
        DialogueIndex=DialogueQueue.Num()-1;Speaker=DialogueQueue[DialogueIndex].Key;Dialogue=DialogueQueue[DialogueIndex].Value;
        if(Choices.Num())if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(Speaker,Dialogue);
    }
    if(Choices.Num())return; // la scelta resta a schermo: non si salta una decisione
    ContinueDialogue();
}
void AClaraStoryDirector::Choose(int32 I)
{
    if(!Choices.IsValidIndex(I))return;
    if(ChooseArrivalInterview(I))return;
    if(LaptopMenu){ClosePage();if(I==0)OpenLaptopScreen();else if(I==1){if(CarriedLaptop)PlaceLaptop();else PickupLaptop();}else if(I==3)UseItem(TEXT("inspect_laptop"));return;}
    Choices.Empty();ContinueDialogue();
}
void AClaraStoryDirector::ClosePage(){if(IsDayTransitionActive()){FinishNextDayTransition();return;}if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->StopVoice();if(Controller.IsValid())if(auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>())Camera->EndConversation();ShowPanel(FString());Dialogue.Empty();Choices.Empty();LaptopMenu=false;}
void AClaraStoryDirector::OpenNotebook(){if(Page==TEXT("dialogue")){Notify(TEXT("Termina prima il dialogo, poi apri il taccuino."));return;}if(IsDayTransitionActive())return;if(Page==TEXT("notebook")||Page==TEXT("report")){ClosePage();return;}AClaraAudio::Cue(this,TEXT("page"),.7f);ShowPanel(Phase==213?TEXT("day3report"):Phase==16||Phase==110?TEXT("report"):TEXT("notebook"));}
void AClaraStoryDirector::OpenInventory(){if(Page==TEXT("dialogue")){Notify(TEXT("Termina prima il dialogo, poi apri la borsa."));return;}if(IsDayTransitionActive())return;if(Page==TEXT("inventory")){ClosePage();return;}AClaraAudio::Cue(this,TEXT("zip"),.7f);ShowPanel(TEXT("inventory"));}
FString AClaraStoryDirector::GetTitle()const{return FString::Printf(TEXT("Giorno %d · %s"),GetDay(),GetDay()==1?TEXT("Il primo messaggio"):GetDay()==2?TEXT("Indirizzi e configurazione"):TEXT("Routing e guasti"));}
FString AClaraStoryDirector::GetObjective()const
{
    if(Phase>=200)return GetDayThreeObjective();
    // Obiettivi che dipendono da cio' che il giocatore ha gia' osservato dentro la stessa fase.
    if(Phase==6&&Completed.Contains(TEXT("day1-peer-unreachable")))return TEXT("PC2 non risponde: collega anche il secondo computer allo switch");
    if(Phase==5&&!Completed.Contains(TEXT("first-ping")))return TEXT("Chiama PC2: apri il portatile con T e nel Terminale scrivi ping 192.168.1.11");
    if(Phase==10)
    {
        if(!Completed.Contains(TEXT("mac-read")))return TEXT("Leggi il tuo MAC: nel Terminale del portatile scrivi ipconfig /all");
        if(LearnedMACs.IsEmpty())return Completed.Contains(TEXT("mac-empty"))?TEXT("La tabella è vuota: fai parlare i due PC con ping 192.168.1.11"):TEXT("Passa alla console di SW1 e scrivi show mac address-table");
        if(!Completed.Contains(TEXT("mac-table")))return TEXT("Riguarda la tabella di SW1: show mac address-table");
        return TEXT("Su che porta è il portatile? Confronta il MAC e scrivi la porta");
    }
    if(Phase==104&&!Completed.Contains(TEXT("day2-host-read")))return TEXT("Leggi IP, mask e gateway del portatile: nel Terminale scrivi ip addr");
    if(Phase==107)
    {
        if(!Completed.Contains(TEXT("startup-read")))return TEXT("Nella console SW-PROVA leggi show startup-config");
        if(!Completed.Contains(TEXT("trial-ping")))return TEXT("Da SW-PROVA esegui ping 192.168.1.10");
        return TEXT("Dal terminale del portatile prova ssh admin@192.168.1.2 con la password configurata");
    }
    switch(Phase)
    {
    case 0:return TEXT("Parla con Alessia alla reception");case 1:return TEXT("Prendi il badge sul banco reception");case 2:return TEXT("Raggiungi Luca nel laboratorio reti");case 3:return TEXT("Prendi il portatile dal banco · E oppure clic");
    case 4:return TEXT("Metti in ordine il tavolo dei cavi: prendi un cavo, provalo su una presa e riponilo nella scatola giusta");case 5:return TEXT("Collega il portatile a SW1 con un cavo Cat 6");case 6:return TEXT("Ripeti il ping a PC2: apri il portatile con T e scrivi ping 192.168.1.11");case 7:return TEXT("Un LED acceso non basta: collega anche PC2 a SW1 con un secondo Cat 6");case 8:return TEXT("Ripeti il ping verso PC2: ping 192.168.1.11");case 9:return TEXT("Collega il cavo console: presa USB del portatile → porta CONSOLE di SW1");case 10:return TEXT("Leggi il MAC del portatile e ritrovalo nella tabella dello switch SW1");case 11:return TEXT("Ricostruisci OSI: dal segnale al servizio");case 12:return TEXT("Associa i quattro livelli TCP/IP");case 13:return TEXT("Collega R1 a SW1 con un Cat 6, poi la fibra: modulo SFP in SW1 e cavo fino al rack della fibra");case 14:return TEXT("Ultima prova, da solo: ricollega i due PC e dimostra che PC2 risponde");case 15:return TEXT("Ritrova la nuova porta del portatile nella tabella MAC");case 16:return TEXT("Giorno 1 completato · N riapre il rapporto, da lì inizi il Giorno 2");
    case 100:return TEXT("Parla con Luca: oggi si configura");case 101:return TEXT("Metti in ordine la richiesta e la risposta ARP");case 102:return TEXT("Sul portatile: ip neigh flush all, poi ip neigh show");case 103:return TEXT("Esegui ping 192.168.1.11, poi ip neigh show");case 104:return TEXT("Risolvi il foglio /26: 192.168.1.70 / 255.255.255.192");case 105:return TEXT("Collega la console al banco e apri SW-PROVA");case 106:return TEXT("Configura SW-PROVA: nome, accessi, IP 192.168.1.2, SSH e salvataggio");case 107:return TEXT("Verifica la configurazione salvata e il ping al portatile");case 108:return TEXT("Parla con Gaia nell'area briefing");case 109:return TEXT("Ripristina il piano 2 dal simulatore del portatile e supera TEST e riavvio");case 110:return TEXT("Giorno 2 completato · N riapre il rapporto: continua verso il Giorno 3");
    }return TEXT("Esplora il laboratorio");
}
void AClaraStoryDirector::SetPhase(int32 N)
{
    Phase=N;HintLevel=0;SaveAt=0;
    SetOfficeClockMinutes(N>=100?540.f:N==16?1080.f:540.f+N*32.f);
    if(N>0&&!SelfTesting)AClaraAudio::Cue(this,N==7?TEXT("hint"):TEXT("success"),.5f);
    if(N>=200){EnterDayThreePhase(N);return;}
    if(N==6)Say(TEXT("Luca"),{TEXT("Visto il LED della porta? Prima ambra, poi verde."),
        TEXT("Impara a leggere i LED: è il modo più rapido per capire se qualcosa non va. Spento: nessun collegamento. Verde fisso: collegamento attivo. Verde lampeggiante: c'è traffico."),
        TEXT("Ambra: la porta sta aspettando, perché lo switch controlla che non ci siano anelli. Può durare fino a trenta secondi, non è un guasto. Verde e ambra alternati: c'è un problema sul collegamento."),
        TEXT("Il portatile adesso è collegato. Prova di nuovo il ping a PC2.")});
    if(N==7)Say(TEXT("Luca"),{TEXT("Ancora niente. Un LED acceso non basta: controlla l'altro cavo. Anche PC2 deve arrivare allo switch.")});
    if(N==8)Notify(TEXT("I LED di entrambe le porte sono verdi. Ripeti il ping: ping 192.168.1.11"));
    if(N==9)Say(TEXT("Luca"),{TEXT("Ecco qua: PC2 ha risposto."),
        TEXT("Adesso vediamo chi sta dove. Ogni scheda di rete ha un indirizzo fisico, il MAC: quarantotto bit scritti in esadecimale, tipo AA-BB-CC-DD-EE-FF."),
        TEXT("I primi tre byte dicono chi l'ha costruita, gli ultimi tre identificano la scheda. Il MAC serve a consegnare i frame dentro la rete locale; l'IP serve per andare da una rete all'altra."),
        TEXT("Per parlare con lo switch serve il cavo console: prendilo dalla scatola e collegalo tra il portatile e la porta CONSOLE di SW1.")});
    if(N==10)
    {
        // Le voci della tabella MAC scadono quando un host tace: la console trova la tabella vuota e il ping la riempie di nuovo.
        LearnedMACs.Empty();Day1CLI.SetLearnedMacTable(LearnedMACs);Completed.Remove(TEXT("mac-table"));
        Say(TEXT("Luca"),{TEXT("Stavolta il terminale dello switch si apre: prima era solo una prova del cavo, adesso il cavo serve davvero."),
            TEXT("Prima leggi il tuo MAC: nel terminale del portatile scrivi ipconfig /all e cerca la voce Indirizzo fisico."),
            TEXT("Poi passa alla console di SW1 e scrivi show mac address-table.")});
    }
    if(N==11){SetupPuzzle(TEXT("osi"));ScreenPage=TEXT("puzzle");OpenLaptopScreen();Say(TEXT("Luca"),{TEXT("Ok, adesso una cosa che ci serve per capire tutto il resto: come parlano tra loro i dispositivi."),
        TEXT("Dopo questa battuta avrai dieci secondi per osservare la torre. Poi ricostruiscila sul piolo destro: Physical in fondo, Application in cima. Si sposta soltanto il blocco superiore, senza limiti di dimensione.")});}
    if(N==12){SetupPuzzle(TEXT("tcpip"));ScreenPage=TEXT("puzzle");OpenLaptopScreen();Say(TEXT("Luca"),{TEXT("Allora. Un protocollo è un insieme di regole per comunicare. Uno stack è una pila di protocolli, uno per livello: ognuno fa un compito solo e si appoggia su quello sotto."),
        TEXT("Quando mandi un dato, lui scende lungo la pila. Ogni livello ci aggiunge la sua intestazione, e alla fine il Physical lo spedisce sul cavo come bit. Chi riceve fa il percorso contrario."),
        TEXT("Ok, adesso il modello che si usa davvero sul campo: TCP/IP. Stesso gioco di prima, con quattro livelli.")});}
    if(N==13)Say(TEXT("Luca"),{TEXT("Visto? TCP/IP è più semplice. OSI serve per capire e diagnosticare, TCP/IP è il modello che le reti usano davvero."),
        TEXT("Bene, ora completiamo il banco. Collegami switch e router con un Cat 6."),
        TEXT("Poi la fibra: è la nostra uscita verso l'esterno. Il router ha solo porte in rame, quindi parte dallo switch: modulo SFP nello slot, poi il cavo giallo fino al rack della fibra in sala server.")});
    if(N==14)
    {
        for(const FString Id:{TEXT("LAPTOP"),TEXT("PC2")})if(Devices.Contains(Id))
        {FString Error;Devices[Id]->DisconnectPort(TEXT("Ethernet"),Error);}
        Say(TEXT("Luca"),{TEXT("Il rame porta segnali elettrici, fino a cento metri. La fibra porta luce: arriva molto più lontano e non risente delle interferenze."),
            TEXT("Ultima prova, stavolta da solo. Ho staccato i cavi dei due PC."),
            TEXT("Rimetti in piedi il collegamento e dimostrami che PC2 risponde. Puoi usare porte diverse: conta il metodo.")});
    }
    if(N==16){Completed.Add(TEXT("day1"));CloseLaptopScreen();Toast.Empty();SaveProgress();ShowPanel(TEXT("report"));}
    if(N==101){SetupPuzzle(TEXT("arp"));ScreenPage=TEXT("puzzle");OpenLaptopScreen();}
    if(N==104)
    {
        Completed.Add(TEXT("day2-arp"));SubnetStep=0;ScreenPage=TEXT("terminal");SwitchTerminalSession(TEXT("LAPTOP"));OpenLaptopScreen();
        Say(TEXT("Luca"),{TEXT("Un IPv4 ha 32 bit: quattro gruppi da 8, ciascuno tra 0 e 255. La rete è come la via; l'host è il numero civico."),
            TEXT("La mask distingue le due parti: bit a 1 per la rete, a 0 per l'host. 255.255.255.0 ha 24 bit a 1: si scrive /24."),
            TEXT("Gli indirizzi privati sono 10.x.x.x, da 172.16 a 172.31 e 192.168.x.x. Si riusano nelle reti locali e non si instradano direttamente su Internet."),
            TEXT("Gli IP pubblici si instradano su Internet. Esistono anche intervalli speciali, come il loopback: fuori dagli intervalli privati non significa sempre pubblico."),
            TEXT("Ora guarda il portatile: scrivi ip addr. Leggi indirizzo, mask e gateway; il gateway serve per uscire dalla sottorete e deve essere raggiungibile nella rete locale.")});
    }
    if(N==106){ScreenPage=TEXT("terminal");SwitchTerminalSession(TEXT("SW-PROVA"));OpenLaptopScreen();Say(TEXT("Luca"),{TEXT("Il prompt indica il contesto: > utente, enable porta a #, configure terminal apre (config)#. help raccoglie la scheda; ? e Tab aiutano nel contesto corrente."),
        TEXT("Dai un nome allo switch, proteggi enable e la console. La password di linea serve solo con login. service password-encryption è offuscamento debole: non confonderlo con una protezione forte."),
        TEXT("L'IP di gestione va sulla VLAN 1: indirizzo e mask, no shutdown, poi gateway. Non serve un IP su ogni porta per inoltrare i frame."),
        TEXT("SSH cifra il trasporto sulla porta 22; Telnet, porta 23, manda le credenziali in chiaro. Prepara dominio, chiave RSA almeno 2048, SSH v2, utente e VTY con login local e solo SSH."),
        TEXT("La running-config è in uso adesso. La startup-config torna al riavvio: salva, rileggi e prova. In questo laboratorio useremo solo credenziali di esercitazione.")});}
    if(N==108)
    {
        Completed.Add(TEXT("day2-trial"));AClaraAudio::Cue(this,TEXT("hint"),.7f);
        Notify(TEXT("ALLERTA · Piano 2 senza rete. Gaia ti aspetta nell'area briefing."));
        Say(TEXT("Luca"),{TEXT("È arrivata un'allerta dal piano 2. Il banco di prova funziona: adesso mettiamo insieme quello che hai imparato."),TEXT("Raggiungi Gaia nel briefing. La rete da ripristinare è nel simulatore sul portatile.")});
    }
    if(N==109){SetupChallenge();ScreenPage=TEXT("challenge");OpenLaptopScreen();}
    if(N==110){Completed.Add(TEXT("day2"));SaveProgress();CloseLaptopScreen();ShowPanel(TEXT("report"));}
}
bool AClaraStoryDirector::HandleInteraction(AActor* Target)
{
    if(!Target||(!Target->IsA<AClaraStoryNPC>()&&!Target->IsA<AClaraStoryProp>()&&!Target->IsA<AClaraNetworkDevice>()))return false;
    if(Controller.IsValid()&&Controller->GetPawn()&&FVector::DistSquared(Controller->GetPawn()->GetActorLocation(),Target->GetActorLocation())>FMath::Square(340.f))
    {Notify(TEXT("Avvicinati per interagire con questo oggetto."));return true;}
    if(Phase>=200&&HandleDayThreeInteraction(Target))return true;
    if(auto* NPC=Cast<AClaraStoryNPC>(Target))
    {
        const FString Id=NPC->GetCharacterId();
        if(Id==TEXT("Alessia")&&Phase==0)Say(Id,{TEXT("Eccoti qui. Puntualissimo!"),
            TEXT("Benvenuto in TNES: reti costruite e protette, questo facciamo. Niente domande a raffica oggi, ti mettiamo subito al lavoro con la squadra."),
            TEXT("Guardi, provi, e se non capisci qualcosa chiedi: nessuno si aspetta che tu sappia già tutto."),
            TEXT("Il tuo badge è qui sul bancone. Prendilo.")},1);
        else if(Id==TEXT("Luca")&&Phase==2)Say(Id,{TEXT("Ciao! Luca, piacere."),TEXT("Il portatile laggiù sul banco è il tuo: prendilo, ti servirà.")},3);
        else if(Id==TEXT("Luca")&&Phase==100)Say(Id,{TEXT("Buongiorno. Oggi vediamo come il PC trova il destinatario, leggiamo gli indirizzi e configuriamo uno switch di prova."),
            TEXT("IP e MAC hanno compiti diversi. L'IP identifica l'interfaccia in una rete; il MAC serve a consegnare i frame sul collegamento locale. Il MAC può essere modificato: non è una prova d'identità."),
            TEXT("La subnet mask dice quale parte dell'IP identifica la rete. Destinatario nella stessa rete? Lo cerchi direttamente. Rete diversa? Cerchi il MAC del gateway."),
            TEXT("Qui usiamo 255.255.255.0, cioè /24. ARP trova il MAC partendo da un IP locale: richiesta a tutti, risposta dal proprietario."),
            TEXT("Il portatile vuole raggiungere PC2, 192.168.1.11. Rimetti in ordine le tre carte: domanda, risposta, memorizzazione.")},101);
        else if(Id==TEXT("Gaia")&&Phase==108)Say(Id,{TEXT("Il piano 2 è senza rete. Dopo l'interruzione di corrente abbiamo sostituito due switch: i nuovi non hanno configurazione."),
            TEXT("La scheda sul portatile indica nomi e indirizzi. Ricollega R1, SW-A, SW-B e i tre computer. Poi configura gli switch e verifica dal portatile sia ping sia accesso SSH."),
            TEXT("Premi TEST quando sei pronto. Controlliamo anche il riavvio: la configurazione deve restare. Nessuna corsa contro il tempo, ma voglio prove per ogni passaggio.")},109);
        else if(Id==TEXT("Luca")&&Phase>2&&Phase!=110){RequestHint();const FString Hint=Toast;Toast.Empty();Say(Id,{Hint});}
        else if(Id==TEXT("Jonny"))Say(Id,{Phase>=100?TEXT("Un ping che funziona non racconta tutta la sicurezza della rete. Quando attivi gli accessi remoti, tieni SSH e chiudi Telnet."):TEXT("Io sono Jonny. Quando hai un ping che risponde, chiamami: domani ti faccio vedere quante informazioni racconta quel piccolo messaggio.")});
        else Say(NPC->GetDisplayName(),{GetObjective()});return true;
    }
    if(auto* Prop=Cast<AClaraStoryProp>(Target))
    {
        const FString Id=Prop->GetPropId();
        if(Id==TEXT("Badge"))
        {
            if(Phase<1){Notify(TEXT("Alessia ti aspetta: presentati prima di prendere il badge."));return true;}
            Owned.Add(TEXT("badge"));Prop->SetActorHiddenInGame(true);Prop->SetActorEnableCollision(false);AClaraAudio::Cue(this,TEXT("pickup"),.7f);
            if(Phase==1){SetPhase(2);Say(TEXT("Alessia"),{TEXT("Adesso ti faccio conoscere Luca: ti accompagna lui per la giornata."),TEXT("Lo trovi nel Lab di Reti. Attraversa il corridoio: è la seconda porta sulla parete di fronte.")});}
            else Notify(TEXT("Badge riposto nella borsa."));
        }
        else if(Id.StartsWith(TEXT("Cable_"))){SelectSortingCable(Id.Mid(6));if(Phase==4)Notify(GetToast()+TEXT(" Provalo su una presa degli apparati, poi clicca la sua scatola."));else EquipCable(Id.Mid(6));}
        else if(Id.StartsWith(TEXT("Box_"))){if(Phase==4)SortCable(Id.Mid(4));else EquipCable(Id.Mid(4));}
        else if(Id==TEXT("SFP")){Owned.Add(TEXT("sfp_module"));AClaraAudio::Cue(this,TEXT("pickup"),.7f);EquipCable(TEXT("sfp_module"));Notify(TEXT("Modulo SFP in mano. Apri lo switch e clicca uno slot SFP."));}
        else if(Id==TEXT("Coffee"))
        {
            // Il cavo nero a tre poli: non entra in nessuna porta e Luca lo toglie dal tavolo.
            Completed.Add(TEXT("coffee"));Prop->SetActorHiddenInGame(true);Prop->SetActorEnableCollision(false);
            Say(TEXT("Luca"),{TEXT("Quello? Non entra in nessuna porta."),TEXT("È della macchinetta del caffè.")});
        }
        return true;
    }
    if(auto* D=Cast<AClaraNetworkDevice>(Target))
    {if(D->GetModelId()==TEXT("Laptop"))
        {if(!Owned.Contains(TEXT("laptop"))||Phase==3)PickupLaptop();
        else if(CarriedLaptop)OpenLaptopScreen();
        else {Say(TEXT("Portatile"),{TEXT("Puoi aprire la postazione virtuale qui oppure portare il computer con te.")},-1,{TEXT("Apri portatile"),TEXT("Prendi il portatile"),TEXT("Lascia sul banco"),TEXT("Esamina le porte")});LaptopMenu=true;}
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
    if(auto* N=Cast<AClaraStoryNPC>(Target))return N->GetCharacterId()==TEXT("Luca")&&Phase>2&&Phase<100?TEXT("Chiedi un consiglio a Luca"):TEXT("Parla con ")+N->GetDisplayName();
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
    if(Phase>=200)return IsDayThreeMissionTarget(Actor);
    if(!Actor||Actor->IsHidden())return false;
    const bool NeedsPeerCable=Phase==6&&Completed.Contains(TEXT("day1-peer-unreachable"));
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
        {
            // Un passo alla volta: prima i cavi da prendere, poi, con un cavo in mano, le scatole ancora vuote.
            if(Id.StartsWith(TEXT("Cable_")))return SortingCable.IsEmpty()&&!Sorted.Contains(Id.Mid(6));
            if(Id.StartsWith(TEXT("Box_")))return !SortingCable.IsEmpty()&&!Sorted.Contains(Id.Mid(4));
        }
        if(Phase==5||Phase==7||Phase==14||NeedsPeerCable)return Id==TEXT("Cable_ethernet")||Id==TEXT("Box_ethernet");
        if(Phase==9||Phase==105)return Id==TEXT("Cable_console")||Id==TEXT("Box_console");
        if(Phase==13)return Id==TEXT("SFP")||Id==TEXT("Cable_fiber")||Id==TEXT("Box_fiber");
        return false;
    }
    const auto* Device=Cast<AClaraNetworkDevice>(Actor);if(!Device||Device->IsPreview())return false;
    auto Is=[&](const TCHAR* Id){return Devices.FindRef(Id).Get()==Actor;};
    if(CarriedLaptop&&Is(TEXT("LAPTOP")))return false; // il portatile in mano non ha bisogno di un segnaposto
    if(Phase==4)return !SortingCable.IsEmpty()&&(Is(TEXT("SW1"))||Is(TEXT("R1")));
    if(Phase==3)return Is(TEXT("LAPTOP"));
    if(Phase==5||Phase==9||Phase==10||Phase==15||Phase==105||Phase==106||Phase==107)return Is(TEXT("LAPTOP"))||Is(TEXT("SW1"));
    if(Phase==7||Phase==14||NeedsPeerCable)return Is(TEXT("LAPTOP"))||Is(TEXT("PC2"))||Is(TEXT("SW1"));
    if(Phase==13)return Is(TEXT("SW1"))||Is(TEXT("R1"))||Is(TEXT("UPLINK"));
    return (Phase==6||Phase==8||Phase==11||Phase==12||Phase==101||Phase==102||Phase==103||Phase==104||Phase==109)&&Is(TEXT("LAPTOP"));
}
void AClaraStoryDirector::PickupLaptop()
{
    if(Phase<3){Notify(TEXT("Prima presentati ad Alessia e raggiungi Luca."));return;}
    if(!Devices.Contains(TEXT("LAPTOP"))||!Controller.IsValid()||!Controller->GetPawn())return;auto* L=Devices[TEXT("LAPTOP")].Get();
    ReturnHeldCable();
    for(const auto& P:L->GetPorts()){FString E;L->DisconnectPort(P.Id,E);}CloseLaptopScreen();
    Owned.Add(TEXT("laptop"));CarriedLaptop=true;AClaraAudio::Cue(this,TEXT("pickup"),.8f);L->SetLaptopCarried(true);
    auto* Camera=Controller->GetPawn()->FindComponentByClass<UCameraComponent>();
    L->AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);L->SetActorRelativeLocation({48,27,-38});L->SetActorRelativeRotation({0,90,0});
    for(UActorComponent* C:L->GetComponents())if(auto* P=Cast<UPrimitiveComponent>(C))P->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    if(Phase==3){SetPhase(4);Say(TEXT("Luca"),{TEXT("Dai, oggi il tuo compito consisterà nel montare la rete da zero."),
        TEXT("Il portatile puoi tenerlo in mano e aprirlo con T, ovunque. Se vuoi appoggiarlo, guarda un tavolo e premi Q."),
        TEXT("Quel tavolo dei cavi è un disastro: prima lo mettiamo in ordine. Prendi un cavo, provalo sugli apparati e mettilo nella scatola giusta.")});}
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
    L->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);L->SetActorLocationAndRotation(P,FRotator::ZeroRotator);L->SetLaptopCarried(false);CarriedLaptop=false;AClaraAudio::Cue(this,TEXT("drop"),.8f);Notify(TEXT("Portatile posato. Clicca per aprire lo schermo; dalla borsa puoi riprenderlo."));
}
void AClaraStoryDirector::OpenLaptopScreen()
{
    if(IsDayTransitionActive())return;
    if(Controller.IsValid())if(auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>();Camera&&Camera->IsActive())
    {OpenLaptopAfterConversation=true;Camera->EndConversation();return;}
    OpenLaptopAfterConversation=false;
    if(!Owned.Contains(TEXT("laptop"))){Notify(TEXT("Prima prendi il portatile del laboratorio."));return;}
    if(!Devices.Contains(TEXT("LAPTOP"))||!Controller.IsValid())return;
    Controller->CloseDevice();
    if(CarriedLaptop)Devices[TEXT("LAPTOP")]->SetLaptopCarried(false);
    if(Display)Display->DeactivateDisplay();
    if(!ScreenOpen)AClaraAudio::Cue(this,TEXT("laptop_open"),.6f);
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
    OpenLaptopAfterConversation=false;
    if(ScreenOverlay.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ScreenOverlay.ToSharedRef());
    if(ScreenOpen&&!SelfTesting)AClaraAudio::Cue(this,TEXT("laptop_close"),.6f);
    ScreenOverlay.Reset();if(Display)Display->DeactivateDisplay();ScreenOpen=false;
    if(CarriedLaptop&&Devices.Contains(TEXT("LAPTOP")))Devices[TEXT("LAPTOP")]->SetLaptopCarried(true);
}
void AClaraStoryDirector::SetScreenPage(const FString& N){if(N==TEXT("subnet")&&Phase==104&&!Completed.Contains(TEXT("day2-host-read"))){ScreenPage=TEXT("terminal");SwitchTerminalSession(TEXT("LAPTOP"));Notify(TEXT("Prima leggi IP, mask e gateway: scrivi ip addr nel terminale."));if(!ScreenOpen)OpenLaptopScreen();return;}ScreenPage=Phase>=200&&(N==TEXT("vision")||N==TEXT("challenge")||N==TEXT("gateway"))?TEXT("routing"):N;if(N==TEXT("notebook")){OpenNotebook();return;}if(N==TEXT("inventory")){OpenInventory();return;}if(!ScreenOpen)OpenLaptopScreen();}
void AClaraStoryDirector::Tick(float D)
{
    Super::Tick(D);TickTerminal(D);TickDayTransition(D);TickLayerTower(D);TickGateway();
    if(OpenLaptopAfterConversation&&Controller.IsValid())
    {const auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>();if(!Camera||!Camera->IsActive())OpenLaptopScreen();}if(GetWorld()->GetTimeSeconds()>ToastUntil)Toast.Empty();
    if(GetWorld()->GetTimeSeconds()>PollAt){PollAt=GetWorld()->GetTimeSeconds()+.25;PollLab();}
    // Salvataggio automatico: fase, borsa, cavi e posizione. Un riavvio non fa rifare il lavoro gia' svolto.
    if(Persist&&GetWorld()->GetTimeSeconds()>SaveAt){SaveAt=GetWorld()->GetTimeSeconds()+4;SaveProgress();}
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
    {
        FString PeerPort;
        if(Devices[Id]->GetPeerForPort(TEXT("Ethernet"),PeerPort)==Devices[TEXT("SW1")])
        {LearnedMACs.Add(Id==TEXT("LAPTOP")?LaptopMAC:PC2MAC,PeerPort);Devices[Id]->MarkTraffic(TEXT("Ethernet"));Devices[TEXT("SW1")]->MarkTraffic(PeerPort);}
    }
    Day1CLI.SetLearnedMacTable(LearnedMACs);ArpCache.Add(PC2IP,PC2MAC);
}
void AClaraStoryDirector::PollLab()
{
    if(Phase>=200){PollDayThree();return;}
    // Le fasi legate ai cavi avanzano quando il giocatore lascia il primo piano: prima guarda il LED, poi parla Luca.
    if(IsDeviceInspectionOpen()||Page==TEXT("dialogue"))return;
    if(Phase==5&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1")))SetPhase(6);
    if(Phase==6&&Completed.Contains(TEXT("day1-peer-unreachable"))&&HostsReachable())SetPhase(8);
    if(Phase==7&&HostsReachable())SetPhase(8);
    if(Phase==9&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console")))SetPhase(10);
    if(Phase==13&&PhysicalLinked(TEXT("SW1"),TEXT("R1"))&&PhysicalLinked(TEXT("SW1"),TEXT("UPLINK"),TEXT("fiber")))SetPhase(14);
    if(Phase==105&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console")))SetPhase(106);
}
TArray<FClaraStoryItem> AClaraStoryDirector::GetItems()const
{
    TArray<FClaraStoryItem> Items;
    auto Add=[&](const TCHAR* Id,const TCHAR* Label,const TCHAR* Detail){Items.Add({Id,Label,Detail,Owned.Contains(Id)});};
    Add(TEXT("badge"),TEXT("Badge TNES"),TEXT("Il tuo primo giorno al laboratorio."));
    Add(TEXT("laptop"),TEXT("Portatile del laboratorio"),CarriedLaptop?TEXT("In mano. T apre il laboratorio virtuale ovunque; Q lo posa su un tavolo."):TEXT("Sul banco. Aprilo con T oppure prendilo e portalo alle console."));
    Add(TEXT("ethernet"),TEXT("Cavo Ethernet Cat 6"),TEXT("RJ-45 a entrambe le estremita': collega porte di rete in rame."));
    Add(TEXT("console"),TEXT("Cavo console"),TEXT("Adattatore USB del portatile verso CONSOLE RJ-45. Non alimenta lo switch."));
    Add(TEXT("fiber"),TEXT("Fibra LC duplex"),TEXT("Si collega a un modulo ottico compatibile, non alla gabbia SFP vuota."));
    Add(TEXT("sfp_module"),TEXT("Modulo SFP ottico"),TEXT("Inseriscilo nello slot SFP dello switch prima della fibra."));return Items;
}
void AClaraStoryDirector::UseItem(const FString& Id)
{
    if(Id==TEXT("inspect_laptop"))
    {
        if(IsDayTransitionActive()||(Page==TEXT("dialogue")&&!LaptopMenu)){Notify(TEXT("Termina prima il dialogo."));return;}
        if(!Owned.Contains(TEXT("laptop"))){Notify(TEXT("Prima prendi il portatile del laboratorio."));return;}
        auto* Laptop=Devices.FindRef(TEXT("LAPTOP")).Get();
        if(!Laptop||!Controller.IsValid()){Notify(TEXT("Il portatile non è disponibile in questa scena."));return;}
        ClosePage();CloseLaptopScreen();Controller->OpenDevice(Laptop);return;
    }
    ClosePage();if(Id==TEXT("laptop")){if(CarriedLaptop)PlaceLaptop();else PickupLaptop();}else if(Id==TEXT("badge"))Notify(TEXT("Badge gia' al sicuro nella borsa."));else EquipCable(Id);}
void AClaraStoryDirector::EquipCable(const FString& Kind)
{
    if(!Owned.Contains(Kind)){Notify(TEXT("Prima prendi il materiale dalla sua scatola."));return;}
    EquippedCable=Kind;if(Controller.IsValid())Controller->SetCableKind(Kind);AClaraAudio::Cue(this,TEXT("click"),.5f);
    Notify(Kind==TEXT("sfp_module")?TEXT("Modulo SFP in mano. Apri SW1 e seleziona uno slot SFP."):TEXT("Cavo selezionato. Apri un apparato e scegli i due capi."));ClosePage();
}
void AClaraStoryDirector::ReturnHeldCable()
{
    if(HeldCable.IsValid())
    {HeldCable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);HeldCable->SetActorTransform(HeldCableHome);HeldCable->SetActorEnableCollision(true);}
    HeldCable.Reset();
}
void AClaraStoryDirector::SelectSortingCable(const FString& K)
{ReturnHeldCable();SortingCable=K;if(Controller.IsValid())Controller->SetCableKind(K);AClaraAudio::Cue(this,TEXT("pickup"),.7f);
    if(Phase==4&&Controller.IsValid()&&Controller->GetPawn())
    {
        for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)if(It->GetPropId()==TEXT("Cable_")+K)
        {auto* Camera=Controller->GetPawn()->FindComponentByClass<UCameraComponent>();if(Camera){HeldCable=*It;HeldCableHome=It->GetActorTransform();It->SetActorEnableCollision(false);It->AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);It->SetActorRelativeLocation({50,-26,-24});It->SetActorRelativeRotation({20,0,0});}break;}
    }
Notify(K==TEXT("ethernet")?TEXT("Sulla guaina: CAT 6. Le due spine hanno otto contatti RJ-45."):K==TEXT("console")?TEXT("Una spina USB e un connettore RJ-45: il kit e' etichettato CONSOLE."):K==TEXT("fiber")?TEXT("Due piccoli connettori LC: la trasmissione e' ottica."):TEXT("Spina di alimentazione: non e' un cavo dati."));}
bool AClaraStoryDirector::SortCable(const FString& Bin)
{
    if(SortingCable.IsEmpty()){Notify(TEXT("Prendi prima un cavo dal tavolo."));return false;}
    if(Bin!=SortingCable){AClaraAudio::Cue(this,TEXT("error"),.5f);Notify(TEXT("Questa scatola lo rifiuta. Guarda le spine del cavo e la scritta sulla scatola."));return false;}
    if(!Completed.Contains(TEXT("tested-")+SortingCable)){Notify(TEXT("Prima provalo sulla presa di un apparato: riconosci il connettore, poi scegli la scatola."));return false;}
    if(HeldCable.IsValid()){HeldCableHome.SetLocation(FVector(Bin==TEXT("ethernet")?1065:Bin==TEXT("console")?1120:1175,1321,79));ReturnHeldCable();}
    Sorted.Add(Bin);Owned.Add(Bin);SortingCable.Empty();AClaraAudio::Cue(this,TEXT("drop"),.7f);
    Notify(Bin==TEXT("ethernet")?TEXT("Cat 6 nella scatola ETHERNET."):Bin==TEXT("console")?TEXT("Cavo console nella sua scatola."):TEXT("Fibra nella sua scatola."));
    if(Sorted.Num()==3&&Phase==4)
    {
        Owned.Add(TEXT("sfp_module"));ClosePage();SetPhase(5);
        Say(TEXT("Luca"),{TEXT("Ok, adesso è tutto pronto: si costruisce per davvero."),
            TEXT("Il ping è la prima cosa che provi quando vuoi sapere se un altro dispositivo ti risponde. Usa il protocollo ICMP."),
            TEXT("Il tuo PC manda un echo request, una specie di «ci sei?». Se l'altro è raggiungibile, risponde con un echo reply."),
            TEXT("Proviamolo subito. Il tuo portatile è 192.168.1.10, PC2 è 192.168.1.11: apri il terminale e scrivi ping 192.168.1.11.")});
    }
    return true;
}
bool AClaraStoryDirector::HasItem(const FString& Id)const{return Owned.Contains(Id);}
bool AClaraStoryDirector::IsCableTest()const{return Phase==4&&!SortingCable.IsEmpty();}
AClaraNetworkDevice* AClaraStoryDirector::GetDevice(const FString& Id)const{return Devices.FindRef(Id).Get();}
void AClaraStoryDirector::DevSolvePuzzle(){if(IsTowerPuzzle())LayerTower.SolveForTest();Puzzle=PuzzleExpected;}
FString AClaraStoryDirector::TestHeldCable(AClaraNetworkDevice* Device,const FString& PortId)
{
    // Scena dei cavi: il cavo in mano si prova su una presa per riconoscerlo. Non resta collegato.
    if(!Device||SortingCable.IsEmpty())return FString();
    const FString Kind=Device->GetEffectivePortKind(PortId),Cable=SortingCable;
    bool Fits=false;FString Result=TEXT("Non entra.");
    if(Cable==TEXT("ethernet"))
    {
        if(Kind==TEXT("ethernet")){Fits=true;Result=TEXT("Entra con un clic: è una presa RJ-45 per i dati. Con l'altro capo collegato il LED passa da ambra a verde. Questo è un cavo di rete.");}
        else if(Kind==TEXT("console"))Result=TEXT("Nessun segnale. La spina entra, ma la porta CONSOLE non porta traffico di rete.");
    }
    else if(Cable==TEXT("console"))
    {
        if(Kind==TEXT("console")){Fits=true;Result=TEXT("Ok. Il capo RJ-45 entra nella porta CONSOLE. Il terminale per ora non si apre: è soltanto un controllo.");}
        else if(Kind==TEXT("console_host")){Fits=true;Result=TEXT("Ok. Il capo USB entra nella presa console del portatile.");}
        else if(Kind==TEXT("ethernet"))Result=TEXT("Nessun segnale. Il capo RJ-45 entra, ma una porta dati non è una console.");
    }
    else if(Cable==TEXT("fiber"))
    {
        if(Kind==TEXT("fiber")){Fits=true;Result=TEXT("Entra: due connettori LC in una presa ottica. Questa è fibra: qui dentro viaggia la luce.");}
        else if(Kind==TEXT("sfp"))Result=TEXT("Non entra ancora: nello slot SFP serve prima il modulo ottico.");
    }
    if(Fits){Completed.Add(TEXT("tested-")+Cable);AClaraAudio::Cue(this,TEXT("plug"),.8f);Result+=TEXT("\nLuca: «Bene, quello va nella sua scatola.»");}
    else AClaraAudio::Cue(this,TEXT("error"),.5f);
    return Result;
}
void AClaraStoryDirector::SetupPuzzle(const FString& K)
{
    if(K==TEXT("osi")||K==TEXT("tcpip")){LayerTower.Reset(K);TowerMnemonicVisible=false;}
    if(K==TEXT("osi"))PuzzleExpected={TEXT("Physical · segnali e cavo"),TEXT("Data Link · frame e MAC"),TEXT("Network · IP e instradamento"),TEXT("Transport · TCP/UDP"),TEXT("Session · sessioni"),TEXT("Presentation · formati dei dati"),TEXT("Application · servizi applicativi")};
    else if(K==TEXT("tcpip"))PuzzleExpected={TEXT("Network Access · OSI 1 e 2"),TEXT("Internet · OSI 3"),TEXT("Transport · OSI 4"),TEXT("Application · OSI 5, 6 e 7")};
    else PuzzleExpected={TEXT("Il portatile domanda in broadcast: chi ha 192.168.1.11?"),TEXT("PC2 risponde al portatile con il proprio MAC"),TEXT("Il portatile memorizza IP/MAC e puo' inviare il frame")};
    Puzzle=PuzzleExpected;Algo::Reverse(Puzzle);ScreenPage=TEXT("puzzle");
}
void AClaraStoryDirector::MovePuzzleItem(int32 I,int32 Dir)
{const int32 J=I+Dir;if(Puzzle.IsValidIndex(I)&&Puzzle.IsValidIndex(J)){AClaraAudio::Cue(this,TEXT("click"),.5f);Puzzle.Swap(I,J);}}
bool AClaraStoryDirector::CheckPuzzle()
{
    if(IsTowerPuzzle())return CheckLayerTower();
    if(Puzzle!=PuzzleExpected){AClaraAudio::Cue(this,TEXT("error"),.5f);Notify(TEXT("Riguarda il percorso: che cosa serve al passaggio successivo? Puoi chiedere un indizio."));return false;}
    if(Phase==11){SetPhase(12);Notify(TEXT("OSI ricostruito. Ora la stessa comunicazione nel modello TCP/IP."));}
    else if(Phase==12){CloseLaptopScreen();SetPhase(13);}
    else if(Phase==101){SetPhase(102);ScreenPage=TEXT("terminal");SwitchTerminalSession(TEXT("LAPTOP"));OpenLaptopScreen();Say(TEXT("Luca"),{TEXT("Ecco, proprio così. La richiesta ARP va in broadcast a FF-FF-FF-FF-FF-FF; PC2 risponde direttamente al portatile."),TEXT("Ora osserviamo la cache: ip neigh flush all la svuota; ip neigh show la mostra. Nel laboratorio funzionano anche gli alias arp -d * e arp -a.")});}
    return true;
}
FString AClaraStoryDirector::GetQuestion()const
{
    if(Phase==10||Phase==15)return TEXT("Su che porta è il portatile? Scrivila come compare nella tabella, nel formato Gi1/0/numero.");
    if(Phase!=104)return FString();
    const TCHAR* Q[]={TEXT("192.168.1.70/26 · Scrivi 70 in binario su 8 bit"),TEXT("255.255.255.192 · Scrivi 192 in binario su 8 bit"),TEXT("Esegui l'AND e scrivi l'indirizzo di rete completo"),TEXT("Metti a 1 i bit host: qual e' l'indirizzo di broadcast?"),TEXT("Quanti host utilizzabili ha questa /26?"),TEXT("192.168.1.130 appartiene alla stessa rete? Rispondi si oppure no")};
    return Q[FMath::Clamp(SubnetStep,0,5)];
}
FString AClaraStoryDirector::GetAnswerHint()const
{
    if(Phase==10||Phase==15)return TEXT("Con ipconfig /all leggi il MAC del portatile alla voce Indirizzo fisico. Dalla console di SW1, show mac address-table elenca i MAC visti da ogni porta. Cisco scrive le stesse dodici cifre a gruppi di quattro.");
    const TCHAR* H[]={TEXT("Pesi: 128,64,32,16,8,4,2,1. Usa otto cifre."),TEXT("192 = 128 + 64. Gli altri bit valgono zero."),TEXT("L'AND tiene 1 solo quando entrambi i bit sono 1."),TEXT("La /26 lascia sei bit host: mettili a 1."),TEXT("2 elevato ai bit host, meno rete e broadcast."),TEXT("Confronta 130 con l'intervallo appena trovato.")};return H[FMath::Clamp(SubnetStep,0,5)];
}
bool AClaraStoryDirector::SubmitAnswer(const FString& Answer)
{
    FString A=Clean(Answer);A.ReplaceInline(TEXT(" "),TEXT(""));
    if(Phase==10||Phase==15)
    {
        if(!Completed.Contains(TEXT("mac-read"))||!Completed.Contains(TEXT("mac-table"))){AClaraAudio::Cue(this,TEXT("error"),.5f);Notify(TEXT("Apri prima le due viste: indirizzo fisico del PC e tabella dello switch."));return false;}
        A.ReplaceInline(TEXT("GigabitEthernet"),TEXT("Gi"));const FString* Port=LearnedMACs.Find(LaptopMAC);
        if(!Port||!A.Equals(*Port,ESearchCase::IgnoreCase)){AClaraAudio::Cue(this,TEXT("error"),.5f);Notify(TEXT("Confronta le 12 cifre del MAC, non l'IP. La porta è nell'ultima colonna, Ports."));return false;}
        CloseLaptopScreen();
        if(Phase==10){Notify(TEXT("Esatto: è la porta dove hai collegato il portatile."));SetPhase(11);}
        else Say(TEXT("Luca"),{TEXT("Esatto: lo switch ti ha ritrovato sulla nuova porta."),TEXT("Bel lavoro oggi: cavi in ordine, rete su, MAC letto, ping fatto."),TEXT("Non è poco per una prima giornata. Domani si configura.")},16);
        return true;
    }
    if(Phase==104)
    {
        if(!Completed.Contains(TEXT("day2-host-read"))){Notify(TEXT("Prima leggi IP, mask e gateway nel terminale con ip addr."));return false;}
        const FString Expected[]={TEXT("01000110"),TEXT("11000000"),TEXT("192.168.1.64"),TEXT("192.168.1.127"),TEXT("62"),TEXT("no")};
        if(!A.Equals(Expected[SubnetStep],ESearchCase::IgnoreCase)){Notify(TEXT("Riprova: ")+GetAnswerHint());return false;}
        ++SubnetStep;Notify(TEXT("Passaggio corretto."));
        if(SubnetStep==6){Completed.Add(TEXT("day2-subnet"));CloseLaptopScreen();SetPhase(105);Say(TEXT("Luca"),{TEXT("Fatto: rete .64, broadcast .127, 62 host utilizzabili. L'indirizzo e la mask vanno letti insieme."),TEXT("Passiamo allo switch di prova. Il cavo console e' quello che ci permette di configurarlo da zero.")});}else RefreshScreen();return true;
    }
    return false;
}
FClaraSwitchCLI* AClaraStoryDirector::ActiveCLI()
{if(TerminalTarget==TEXT("SW-A"))return &SwitchA;if(TerminalTarget==TEXT("SW-B"))return &SwitchB;return Phase>=100?&TrialCLI:&Day1CLI;}
const FClaraSwitchCLI* AClaraStoryDirector::ActiveCLI()const{return const_cast<AClaraStoryDirector*>(this)->ActiveCLI();}
FString AClaraStoryDirector::GetTerminalTitle()const{if(Phase>=200)return TerminalTarget==TEXT("PC2")?TEXT("PC2 · rete A"):TerminalTarget==TEXT("LAPTOP")?TEXT("Portatile · solo console"):TerminalTarget+TEXT(" · console locale");return TerminalTarget==TEXT("LAPTOP")?TEXT("PC-LAB · terminale del computer"):TerminalTarget+TEXT(" · sessione console dello switch");}
FString AClaraStoryDirector::GetTerminalPrompt()const{if(Phase>=200)return RoutingLab.GetPrompt(TerminalTarget);if(TerminalTarget==TEXT("CML"))return GatewayNode+TEXT(" [CML]>");if(IsTerminalPasswordInput())return TEXT("Password (laboratorio):");return TerminalTarget==TEXT("LAPTOP")?TEXT("tecnico@clara:~$"):ActiveCLI()->GetPrompt();}
void AClaraStoryDirector::SelectTerminalTarget(const FString& Id)
{
    if(Phase>=200){SelectDayThreeTerminal(Id);return;}
    const bool Available=Id==TEXT("LAPTOP")||(Phase==109?(Id==TEXT("SW-A")||Id==TEXT("SW-B")):(Id==(Phase>=100?TEXT("SW-PROVA"):TEXT("SW1"))));
    if(Id==TEXT("CML")){SwitchTerminalSession(Id);ScreenPage=TEXT("gateway");return;}
    if(!Available){Notify(TEXT("Questa console non fa parte della prova corrente."));return;}
    if(Id!=TEXT("LAPTOP"))
    {
        const bool Connected=Phase==109?ConsoleTarget==Id:PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console"));
        if(!Connected){Notify(TEXT("Prima collega la USB console del portatile alla porta CONSOLE dello switch."));return;}
    }
    SwitchTerminalSession(Id);ScreenPage=TEXT("terminal");
}
void AClaraStoryDirector::BeginSubnetExercise()
{
    if(Phase!=104||Completed.Contains(TEXT("day2-host-read")))return;
    Completed.Add(TEXT("day2-host-read"));
    Say(TEXT("Luca"),{TEXT("Il portatile è 192.168.1.10/24; il gateway è 192.168.1.1. Entrambi sono nella rete 192.168.1.0. Per PC2, nella stessa rete, il router non serve."),
        TEXT("Proviamo una mask meno comoda: 192.168.1.70 con 255.255.255.192, cioè /26. Trova rete, broadcast e host utilizzabili, un passaggio alla volta."),
        TEXT("Parti dal binario. I pesi degli otto bit sono 128, 64, 32, 16, 8, 4, 2, 1. Se sbagli, ti indico dove riguardare senza penalità.")});
}
const FClaraSwitchCLI* AClaraStoryDirector::FindReachableSSH(const FString& Address,FString& Id)const
{
    Id.Empty();
    if(Phase==107&&TrialCLI.GetManagementAddress()==Address&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"))&&SameSubnet(LaptopIP,Address,TEXT("255.255.255.0")))
    {Id=TEXT("SW-PROVA");return TrialCLI.IsSVIUp()&&TrialCLI.IsSSHConfigured()?&TrialCLI:nullptr;}
    if(Phase!=109)return nullptr;
    const auto Nodes=GetSimNodes();const auto* Host=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("LAPTOP");});
    const auto* Target=Nodes.FindByPredicate([&](const auto& N){return N.Kind==TEXT("switch")&&N.IP==Address;});
    if(!Host||!Target)return nullptr;
    const auto* CLI=Target->Id==TEXT("SW-A")?&SwitchA:&SwitchB;
    if(!CLI->IsSVIUp()||!CLI->IsSSHConfigured()||!SimReachable(Host->Id,Target->Id)||!SameSubnet(Host->IP,Address,Host->Mask))return nullptr;
    Id=Target->Id;return CLI;
}
bool AClaraStoryDirector::HasVerifiedSSH(const FString& Id,const FClaraSwitchCLI& CLI)const
{return Completed.Contains(TEXT("ssh-verified:")+Id+TEXT(":")+CLI.GetRunningConfigurationFingerprint());}
void AClaraStoryDirector::CheckTrialComplete()
{
    if(Phase==107&&Completed.Contains(TEXT("startup-read"))&&Completed.Contains(TEXT("trial-ping"))&&HasVerifiedSSH(TEXT("SW-PROVA"),TrialCLI)&&TrialCLI.ValidateDay2(TEXT("SW-PROVA"),TEXT("192.168.1.2")).IsEmpty())SetPhase(108);
}
void AClaraStoryDirector::SwitchTerminalSession(const FString& Id)
{
    if(Id==TerminalTarget)return;
    PendingSSHUser.Empty();PendingSSHAddress.Empty();
    TerminalTranscripts.Add(TerminalTarget,Transcript);
    TerminalTarget=Id;Transcript=TerminalTranscripts.FindRef(Id);
}
TArray<FString> AClaraStoryDirector::GetTerminalHistory() const
{return TerminalHistories.FindRef(TerminalTarget);}
bool AClaraStoryDirector::IsTerminalReadingBanner() const
{if(Phase>=200)return RoutingLab.IsReadingBanner(TerminalTarget);return TerminalTarget!=TEXT("LAPTOP")&&TerminalTarget!=TEXT("CML")&&ActiveCLI()->IsReadingBanner();}
bool AClaraStoryDirector::IsTerminalAwaitingInput() const
{if(Phase>=200)return RoutingLab.IsAwaitingInput(TerminalTarget);return TerminalTarget!=TEXT("LAPTOP")&&TerminalTarget!=TEXT("CML")&&ActiveCLI()->IsAwaitingInput();}
FString AClaraStoryDirector::ExecutePC(const FString& Input)
{
    if(IsTerminalPasswordInput())
    {
        const FString User=PendingSSHUser,Address=PendingSSHAddress;PendingSSHUser.Empty();PendingSSHAddress.Empty();
        FString Id;const auto* CLI=FindReachableSSH(Address,Id);
        if(!CLI)return TEXT("Accesso annullato: destinazione non più raggiungibile. Ripeti il comando ssh.");
        if(!CLI->AuthenticateLocalSSH(User,Input))return TEXT("Accesso negato: utente o password non corrispondono alla configurazione locale. Ripeti ssh per riprovare.");
        Completed.Add(TEXT("ssh-verified:")+Id+TEXT(":")+CLI->GetRunningConfigurationFingerprint());CheckTrialComplete();
        return TEXT("Autenticazione SSH del laboratorio riuscita su ")+Id+TEXT(". Verificati utente, password e accesso configurato; prova conclusa, nessuna connessione esterna.");
    }
    FString C=Clean(Input),Lower=C.ToLower();
    if(Lower==TEXT("help")||Lower==TEXT("?")||Lower==TEXT("aiuto"))return TEXT("STRUMENTI DI RETE · LABORATORIO LOCALE\n\nip addr              Mostra IPv4 e MAC; il riepilogo del gioco aggiunge mask e gateway.\nping <IP>            Verifica se il destinatario risponde.\nip neigh show        Mostra gli abbinamenti IP/MAC ricordati.\nip neigh flush all   Svuota la cache ARP.\nssh utente@IP        Verifica le credenziali configurate sullo switch del laboratorio.\n\nAlias dagli appunti Windows: ipconfig /all, arp -a e arp -d *. Funzionano solo nel modello del gioco, senza modificare la rete del computer.\nPC2 è 192.168.1.11. La console è una sessione separata: collega il cavo e scegli lo switch della prova corrente.");
    if(Lower==TEXT("ip addr")||Lower==TEXT("ip addr show")||Lower==TEXT("ip address show")||Lower==TEXT("ip a")||Lower==TEXT("ifconfig"))
    {Completed.Add(TEXT("mac-read"));BeginSubnetExercise();return TEXT("2: eth0: <BROADCAST,MULTICAST,UP>\n    link/ether 02:aa:00:00:00:10\n    inet ")+LaptopIP+TEXT("/24 brd 192.168.1.255 scope global eth0\n    mask 255.255.255.0 · gateway predefinito 192.168.1.1 (riepilogo del laboratorio)\n\nlink/ether è il MAC di questa scheda di rete. Gli apparati Cisco scrivono le stesse 12 cifre a gruppi di quattro: 02aa.0000.0010.");}
    if(Lower==TEXT("ip neigh")||Lower==TEXT("ip neigh show")||Lower==TEXT("ip neighbor show"))Lower=TEXT("arp -a");
    if(Lower==TEXT("ip neigh flush all")||Lower==TEXT("ip neighbor flush all")||Lower==TEXT("sudo ip neigh flush all"))Lower=TEXT("arp -d *");
    if(Lower==TEXT("ipconfig")||Lower==TEXT("ipconfig /all"))
    {const bool All=Lower.EndsWith(TEXT("/all"));if(All)Completed.Add(TEXT("mac-read"));BeginSubnetExercise();return FString(TEXT("Scheda Ethernet PC-LAB:\n"))+(All?TEXT("   Indirizzo fisico. . . . . . . : ")+LaptopMAC+TEXT("\n"):FString())+TEXT("   Indirizzo IPv4. . . . . . . . : ")+LaptopIP+TEXT("\n   Subnet mask . . . . . . . . . : 255.255.255.0\n   Gateway predefinito . . . . . : 192.168.1.1");}
    if(Lower==TEXT("arp -d *")){ArpCache.Empty();Completed.Add(TEXT("arp-cleared"));return TEXT("Cache ARP del laboratorio svuotata.");}
    if(Lower==TEXT("arp -a"))
    {
        if(Phase==102&&Completed.Contains(TEXT("arp-cleared"))&&ArpCache.IsEmpty())SetPhase(103);
        if(Phase==103&&ArpCache.Contains(PC2IP)&&Page!=TEXT("dialogue"))
            Say(TEXT("Luca"),{TEXT("La cache ora contiene l'IP e il MAC di PC2. Il portatile li ricorda per un po', poi la voce scade e deve chiedere di nuovo."),
                TEXT("ARP non autentica chi risponde. Una risposta falsa può associare l'IP della vittima al MAC di un altro dispositivo: si chiama ARP spoofing."),
                TEXT("Il ping potrebbe funzionare anche mentre il traffico passa da un intruso. La cifratura può proteggere i contenuti: non basta ARP per leggerli. Oggi riconosciamo il rischio; lo approfondiremo nel percorso Cyber.")},104);
        FString R=TEXT("Indirizzo Internet       Indirizzo fisico         Tipo\n");for(const auto& E:ArpCache)R+=E.Key+TEXT("    ")+E.Value+TEXT("    dinamico\n");return ArpCache.IsEmpty()?TEXT("Nessuna voce ARP trovata."):R;
    }
    if(Lower.StartsWith(TEXT("ping ")))
    {
        FString IP=Clean(C.Mid(5));if(!ValidIPv4(IP))return TEXT("Indirizzo IPv4 non valido. Esempio di struttura: ping 192.168.1.11");
        const bool OK=PCPingReachable(IP);if(OK&&IP==PC2IP&&Phase!=109)LearnFromPing();
        if(Phase==6&&IP==PC2IP)
        {if(OK)SetPhase(8);else{Completed.Add(TEXT("day1-peer-unreachable"));Notify(TEXT("PC2 non risponde. Controlla il suo cavo: anche il secondo computer deve arrivare allo switch."));}}
        if(Phase==8&&IP==PC2IP&&OK)SetPhase(9); // il terminale resta aperto: la risposta va letta
        if(Phase==14&&IP==PC2IP&&OK){Completed.Remove(TEXT("mac-table"));SetPhase(15);Notify(TEXT("Risponde. Ora rileggi la tabella: la porta potrebbe essere cambiata."));}
        if(Phase==5&&IP==PC2IP&&!OK&&!Completed.Contains(TEXT("first-ping")))
        {Completed.Add(TEXT("first-ping"));Say(TEXT("Luca"),{TEXT("Request timed out: nessuno risponde."),TEXT("Tranquillo, è normale: mancano ancora i collegamenti. Uno alla volta. Prendi un Cat 6 dalla scatola e collega lo switch al portatile.")});}
        if(Phase==10&&IP==PC2IP&&OK)Notify(TEXT("PC2 risponde. Ora riguarda la tabella dello switch: show mac address-table."));
        // Stessa forma del copione del gruppo e dei laboratori Cisco: Reply from oppure Request timed out.
        return OK?TEXT("Reply from ")+IP+TEXT(": bytes=32 time<1ms TTL=128\nReply from ")+IP+TEXT(": bytes=32 time<1ms TTL=128\n\nPacchetti: inviati = 2, ricevuti = 2, persi = 0")
            :FString(TEXT("Request timed out.\nRequest timed out.\n\nPacchetti: inviati = 2, ricevuti = 0, persi = 2"));
    }
    if(Lower.StartsWith(TEXT("ssh ")))
    {
        const FString Destination=Clean(C.Mid(4));FString User,IP;
        if(!Destination.Split(TEXT("@"),&User,&IP)||User.IsEmpty()||!ValidIPv4(IP))
            return TEXT("Formato: ssh utente@indirizzo. Esempio: ssh admin@192.168.1.2");
        FString Id;if(!FindReachableSSH(IP,Id))return TEXT("SSH non disponibile: controlla destinatario, rete, chiave e linee VTY.");
        PendingSSHUser=User;PendingSSHAddress=IP;
        return TEXT("Prova SSH locale su ")+Id+TEXT(" · inserisci la password dell'utente configurato nel gioco. Il testo non viene mostrato né inserito nella cronologia.");
    }
    if(Lower.StartsWith(TEXT("telnet ")))return TEXT("Telnet non è ammesso nella prova: trasmette le credenziali in chiaro. Configura le VTY con transport input ssh e usa ssh utente@IP.");
    if(Lower==TEXT("ls")||Lower==TEXT("ls -l")||Lower==TEXT("ls -la"))return TEXT("appunti/  scheda-rete.txt  topologia.txt");
    if(Lower==TEXT("pwd"))return TEXT("/home/tecnico/laboratorio");
    if(Lower==TEXT("whoami"))return TEXT("tecnico");
    if(Lower==TEXT("enable")||Lower.StartsWith(TEXT("conf")))return TEXT("Questo e' il terminale del PC. Collega il cavo console e seleziona lo switch per usare i comandi IOS.");
    return TEXT("Comando non riconosciuto in questo laboratorio. Scrivi help o apri Manuale per vedere gli strumenti disponibili.");
}
bool AClaraStoryDirector::PCPingReachable(const FString& IP)const
{
    if(IP==TEXT("127.0.0.1"))return true;
    if(Phase==109)
    {
        const auto Nodes=GetSimNodes();const auto* Dest=Nodes.FindByPredicate([&](const auto& N){return N.IP==IP;});
        const auto* Source=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("LAPTOP");});
        bool OK=Dest&&Source&&SimReachable(Source->Id,Dest->Id)&&SameSubnet(Source->IP,Dest->IP,Source->Mask);
        if(Dest&&Dest->Kind==TEXT("switch"))OK=OK&&(Dest->Id==TEXT("SW-A")?SwitchA:SwitchB).IsSVIUp();return OK;
    }
    if(IP==LaptopIP)return true;
    if(IP==PC2IP)return HostsReachable();
    return IP==TEXT("192.168.1.2")&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"))&&TrialCLI.IsSVIUp()&&TrialCLI.GetManagementAddress()==IP;
}
void AClaraStoryDirector::PrepareCLIReachability(FClaraSwitchCLI& CLI,const FString& Session)const
{
    TSet<FString> Targets;
    if(Phase==109&&CLI.IsSVIUp())
    {
        for(const auto& Node:GetSimNodes())if(!Node.IP.IsEmpty()&&SimReachable(Session,Node.Id)&&SameSubnet(CLI.GetManagementAddress(),Node.IP,TEXT("255.255.255.0")))
            if(Node.Kind!=TEXT("switch")||(Node.Id==TEXT("SW-A")?SwitchA:SwitchB).IsSVIUp())Targets.Add(Node.IP);
    }
    else if(Phase!=109&&PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"))&&CLI.IsSVIUp()&&SameSubnet(LaptopIP,CLI.GetManagementAddress(),TEXT("255.255.255.0")))Targets.Add(LaptopIP);
    CLI.SetReachableTargets(Targets);
}
bool AClaraStoryDirector::TryPredictTerminalPing(const FString& Input,FString& Preview)const
{
    if(Phase>=200)return !IsTerminalAwaitingInput()&&PredictDayThreePing(Input,Preview);
    // Reti3/Cyber extend this pure prediction point; CML always retains its native gateway execution.
    if(TerminalTarget==TEXT("CML")||IsTerminalPasswordInput()||IsTerminalAwaitingInput())return false;
    TArray<FString> Words;Input.ParseIntoArrayWS(Words);
    const bool Ping=Words.Num()==2&&Words[0].Equals(TEXT("ping"),ESearchCase::IgnoreCase);
    const bool DoPing=Words.Num()==3&&Words[0].Equals(TEXT("do"),ESearchCase::IgnoreCase)&&Words[1].Equals(TEXT("ping"),ESearchCase::IgnoreCase);
    if((!Ping&&!DoPing)||!ValidIPv4(Words.Last()))return false;
    if(TerminalTarget==TEXT("LAPTOP"))
    {if(!Ping)return false;Preview=PCPingReachable(Words.Last())?TEXT("Reply from"):TEXT("Request timed out.");return true;}
    const bool Wired=Phase==109?ConsoleTarget==TerminalTarget:PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console"));
    if(!Wired)return false;
    FClaraSwitchCLI Copy=*ActiveCLI();PrepareCLIReachability(Copy,TerminalTarget);Preview=Copy.Execute(Input);
    return Preview.Contains(TEXT("Success rate"));
}

FString AClaraStoryDirector::RunImmediate(const FString& Input,bool RecordOutput)
{
    if(Phase>=200)return RunDayThreeCommand(Input,RecordOutput);
    if(TerminalTarget==TEXT("CML"))return RunGatewayCommand(Input);
    const FString Command=Input.TrimStartAndEnd();
    if(TerminalTarget==TEXT("LAPTOP")&&!IsTerminalPasswordInput()&&Command.Equals(TEXT("clear"),ESearchCase::IgnoreCase))
    {Transcript.Empty();return FString();}
    AClaraAudio::Cue(this,TEXT("enter"),.6f);
    FString Out;
    const FString Prompt=GetTerminalPrompt();const bool PasswordInput=IsTerminalPasswordInput();
    if(TerminalTarget==TEXT("LAPTOP"))Out=ExecutePC(Input);
    else
    {
        const bool Wired=Phase==109?ConsoleTarget==TerminalTarget:PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"),TEXT("console"));
        if(!Wired)Out=TEXT("Sessione console scollegata. Controlla i due capi del cavo azzurro.");
        else
        {
            auto* CLI=ActiveCLI();CLI->SetLearnedMacTable(LearnedMACs);PrepareCLIReachability(*CLI,TerminalTarget);
            const FString BeforeConfiguration=CLI->GetRunningConfigurationFingerprint();
            Out=CLI->Execute(Input);
            if(Phase==109&&BeforeConfiguration!=CLI->GetRunningConfigurationFingerprint()){ChallengeReady=false;ChallengeRebooted=false;}
            FString N=Input.ToLower();const bool MacTable=Out.Contains(TEXT("Mac Address Table"));
            if(MacTable&&!LearnedMACs.IsEmpty())Completed.Add(TEXT("mac-table"));
            if(MacTable&&Phase==10)
            {
                if(LearnedMACs.IsEmpty()&&!Completed.Contains(TEXT("mac-empty")))
                {Completed.Add(TEXT("mac-empty"));Say(TEXT("Luca"),{TEXT("Lo switch ha una tabella: MAC e porta. Quando arriva un frame, legge il MAC di origine e lo associa alla porta da cui è arrivato."),
                    TEXT("Per inoltrare guarda il MAC di destinazione. Se lo conosce, manda il frame solo su quella porta. Se non lo conosce, lo manda su tutte le altre: si chiama flooding."),
                    TEXT("Adesso è vuota perché nessuno sta parlando: lo switch dimentica chi tace da un po'. Facciamo parlare qualcuno. Torna al terminale del portatile e rifai il ping a PC2.")});}
                else if(!LearnedMACs.IsEmpty()&&!Completed.Contains(TEXT("mac-filled")))
                {Completed.Add(TEXT("mac-filled"));Say(TEXT("Luca"),{TEXT("Visto? Il ping ha fatto parlare i due dispositivi, e lo switch si è imparato chi sta dove."),
                    TEXT("Su che porta è il portatile? Confronta il MAC di ipconfig /all con la tabella e scrivi la porta.")});}
            }
            if(Phase==107&&Out.StartsWith(TEXT("Startup configuration (copia locale salvata):")))Completed.Add(TEXT("startup-read"));
            if(Phase==106&&TrialCLI.ValidateDay2(TEXT("SW-PROVA"),TEXT("192.168.1.2")).IsEmpty()){Completed.Remove(TEXT("startup-read"));SetPhase(107);Notify(TEXT("Configurazione completa e salvata. Rileggi show startup-config e prova ping 192.168.1.10."));}
            else if(Phase==107)
            {
                if(N.StartsWith(TEXT("ping "))&&Clean(N.Mid(5))==LaptopIP&&!Out.Contains(TEXT("%"))&&Out.Contains(TEXT("!")))Completed.Add(TEXT("trial-ping"));
                CheckTrialComplete();
            }
        }
    }
    if(RecordOutput)AppendTerminalOutput(TerminalTarget,Prompt+TEXT(" ")+(PasswordInput?TEXT("[nascosta]"):Input)+TEXT("\n")+Out+TEXT("\n\n"));
    return Out;
}
FString AClaraStoryDirector::CompleteCommand(const FString& Prefix)const
{
    if(Phase>=200)return CompleteDayThreeCommand(Prefix);
    if(IsTerminalPasswordInput())return Prefix;
    if(TerminalTarget!=TEXT("LAPTOP"))return ActiveCLI()->Complete(Prefix);
    if(Prefix.TrimStartAndEnd().IsEmpty())return Prefix;
    FString Common;int32 Matches=0;
    for(const FString C:{TEXT("ip addr"),TEXT("ip neigh show"),TEXT("ip neigh flush all"),TEXT("sudo ip neigh flush all"),TEXT("ipconfig /all"),TEXT("ping "),TEXT("arp -a"),TEXT("arp -d *"),TEXT("ssh "),TEXT("clear"),TEXT("help")})
    {
        if(!C.StartsWith(Prefix,ESearchCase::IgnoreCase))continue;
        if(Matches++==0){Common=C;continue;}
        int32 Count=0;while(Count<Common.Len()&&Count<C.Len()&&FChar::ToLower(Common[Count])==FChar::ToLower(C[Count]))++Count;
        Common.LeftInline(Count);
    }
    return Matches?Common:Prefix;
}
FString AClaraStoryDirector::CommandHelp(const FString& Prefix)const
{if(Phase>=200)return TEXT("Console: enable → configure terminal → interfaccia/rotta → end → salva.\nshow running-config · show startup-config · show ip route · show ip interface brief\nPC2: ping <IP> · traceroute <IP> · ipconfig /all\nScheda guidata: Network Vision. Il portatile non è nella rete.");if(IsTerminalPasswordInput())return TEXT("Inserisci la password dell'utente configurato nel gioco. Non usare credenziali personali.");return TerminalTarget==TEXT("LAPTOP")?TEXT("ip addr · leggi indirizzi e MAC (Linux)\nip neigh show · osserva la cache ARP\nip neigh flush all · svuota la cache ARP\nping <IP> · prova un destinatario\nssh utente@IP · prova autenticazione nel laboratorio\nipconfig /all, arp -a, arp -d * · alias compatibili con gli appunti\nhelp · manuale guidato"):ActiveCLI()->Help(Prefix);}
void AClaraStoryDirector::ResetTrialSwitch(){TrialCLI.Reset();PendingPings.Remove(TEXT("SW-PROVA"));TerminalTranscripts.Remove(TEXT("SW-PROVA"));TerminalHistories.Remove(TEXT("SW-PROVA"));PendingSSHUser.Empty();PendingSSHAddress.Empty();if(TerminalTarget==TEXT("SW-PROVA"))Transcript.Empty();Completed.Remove(TEXT("startup-read"));Completed.Remove(TEXT("trial-ping"));if(Phase==107)SetPhase(106);Notify(TEXT("Solo lo switch di prova e' stato azzerato. Puoi ricominciare dal prompt Switch>."));}
namespace
{
// Indizi del Giorno 1 in tre passi: prima dove guardare, poi che cosa fare, solo alla fine il gesto esatto.
FString Day1Hint(int32 Phase,int32 Level)
{
    TArray<FString> H;
    switch(Phase)
    {
    case 0:H={TEXT("Alessia è seduta al banco della reception, in fondo alla prima stanza."),TEXT("WASD per camminare, tieni premuto il tasto destro del mouse per guardarti intorno. Avvicinati ad Alessia e clicca su di lei.")};break;
    case 1:H={TEXT("Il badge è sul banco della reception."),TEXT("Avvicinati al banco: quando in basso compare l'etichetta del badge clicca oppure premi E.")};break;
    case 2:H={TEXT("Luca è nel laboratorio reti, oltre il corridoio."),TEXT("Esci dalla reception e attraversa il corridoio: il laboratorio è la seconda porta sulla parete di fronte.")};break;
    case 3:H={TEXT("Il portatile è sul banco centrale del laboratorio, vicino a switch e router."),TEXT("Avvicinati e clicca sul portatile, oppure premi E quando compare l'etichetta.")};break;
    case 4:H={TEXT("I tre cavi sono sul tavolo vicino alla finestra, davanti alle tre scatole. Clicca un cavo per prenderlo in mano."),TEXT("Con il cavo in mano clicca un apparato e prova una presa: se entra, hai capito che cavo è. Sulla guaina del cavo bianco c'è scritto CAT 6, l'azzurro ha un capo USB, il giallo ha due connettori LC."),TEXT("Bianco con due RJ-45: scatola ETHERNET. Azzurro con USB e RJ-45: scatola CONSOLE. Giallo con due LC: scatola FIBRA. Con il cavo in mano clicca la scatola.")};break;
    case 5:H={TEXT("Il ping si scrive nel Terminale del portatile: premi T e apri Terminale."),TEXT("Scrivi ping 192.168.1.11 e premi Invio. Se nessuno risponde manca un collegamento: serve un Cat 6 tra una porta dello switch SW1 e la presa di rete del portatile."),TEXT("Per collegare clicca lo switch SW1 sul banco e scegli una porta libera. Poi dal pannello a sinistra passa a «Portatile» e clicca la sua presa Ethernet.")};break;
    case 6:H={TEXT("Il portatile ora è collegato: ripeti la prova."),TEXT("Premi T, apri il Terminale e scrivi ping 192.168.1.11")};break;
    case 7:H={TEXT("Il portatile è collegato allo switch. E PC2?"),TEXT("PC2 è il computer sul banco in fondo a sinistra. Serve un secondo cavo Ethernet da PC2 a una porta libera di SW1."),TEXT("Clicca PC2 e poi la sua presa Ethernet; dal pannello a sinistra passa a «Switch» e clicca una porta libera.")};break;
    case 8:H={TEXT("Ora il percorso è completo: ripeti la prova."),TEXT("Apri il portatile con T e nel Terminale scrivi: ping 192.168.1.11")};break;
    case 9:H={TEXT("Il cavo console è quello azzurro, con una spina USB e una RJ-45."),TEXT("Sceglilo dalla borsa (I). Primo capo: la presa USB console del portatile. Secondo capo: la porta con la scritta CONSOLE su SW1."),TEXT("La porta CONSOLE non è una delle porte dati numerate: ruota lo switch trascinando con il mouse e leggi le etichette.")};break;
    case 10:case 15:H={TEXT("Ti servono due letture: il MAC del tuo portatile e la tabella dello switch."),TEXT("Nel Terminale del portatile scrivi ipconfig /all e annota l'Indirizzo fisico. Poi scegli la console SW1 e scrivi show mac address-table. Se la tabella è vuota fai un ping a PC2: si riempie quando i dispositivi parlano."),TEXT("Cerca nella tabella la riga con le stesse dodici cifre: Cisco le scrive a gruppi di quattro. La porta è nell'ultima colonna: scrivila nella risposta.")};break;
    case 11:case 12:H={TEXT("Parti da quello che hai toccato con mano: il cavo è il livello più basso."),TEXT("Dopo il cavo viene il frame con i MAC, poi il pacchetto con l'IP, poi il trasporto; in cima ci sono i servizi usati dalle persone.")};break;
    case 13:H={TEXT("Due collegamenti: R1 e SW1 con un cavo in rame; SW1 e la distribuzione ottica con la fibra."),TEXT("La fibra non entra in uno slot vuoto: prendi il modulo SFP sul banco, apri SW1 e clicca uno dei quattro slot SFP, dopo le 24 porte in rame."),TEXT("Poi scegli «Fibra LC»: primo capo sullo slot con il modulo, secondo capo su «Distribuzione ottica» dal pannello a sinistra.")};break;
    case 14:H={TEXT("Luca ha staccato i cavi dei due computer. Che cosa serviva perché PC2 rispondesse?"),TEXT("Ricollega il portatile e PC2 a due porte libere di SW1 con cavi Ethernet: puoi usare porte diverse da prima."),TEXT("Poi apri il Terminale e prova: ping 192.168.1.11")};break;
    case 16:H={TEXT("Premi N per riaprire il rapporto della giornata: da lì inizi il Giorno 2.")};break;
    }
    return H.Num()?H[FMath::Clamp(Level-1,0,H.Num()-1)]:FString();
}
}
void AClaraStoryDirector::RequestHint()
{
    if(IsTowerPuzzle()){PreviewLayerTower();return;}
    if(Phase>=200){RequestDayThreeHint();return;}
    ++HintLevel;AClaraAudio::Cue(this,TEXT("hint"),.5f);
    if(Phase==100){Notify(TEXT("Parla con Luca: ti aspetta nel laboratorio reti per iniziare il Giorno 2."));return;}
    if(Phase<100){const FString Hint=Day1Hint(Phase==6&&Completed.Contains(TEXT("day1-peer-unreachable"))?7:Phase,HintLevel);if(!Hint.IsEmpty()){Notify(Hint);if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(TEXT("Luca"),Hint);return;}}
    if(Phase==104&&!Completed.Contains(TEXT("day2-host-read"))){Notify(GetObjective());return;}
    if(Phase==10||Phase==15||Phase==104){Notify(GetAnswerHint());return;}
    if(Phase==4){Notify(TEXT("Osserva prima le spine: due RJ-45, USB→RJ-45, oppure due LC. Il colore e' solo l'etichetta del nostro kit."));return;}
    if(Phase==11||Phase==12||Phase==101){Notify(Phase==101?TEXT("Prima si domanda a tutti, poi risponde il proprietario, infine si memorizza."):TEXT("Parti dal cavo. Poi frame/MAC, pacchetto/IP e servizi via via piu' vicini all'applicazione."));return;}
    if(Phase==102){Notify(TEXT("Svuota la cache con ip neigh flush all, poi osservala con ip neigh show. Deve risultare vuota."));return;}
    if(Phase==103){Notify(TEXT("Prima ping 192.168.1.11, poi ip neigh show. Cerca la coppia IP/MAC di PC2."));return;}
    if(Phase==105){Notify(TEXT("Il cavo azzurro collega USB console del portatile alla presa CONSOLE dello switch. Lo switch ha alimentazione propria."));return;}
    if(Phase==106||Phase==107){const FString Missing=TrialCLI.ValidateDay2(TEXT("SW-PROVA"),TEXT("192.168.1.2"));Notify(Missing.IsEmpty()?GetObjective():Missing);return;}
    if(Phase==108){Notify(TEXT("Gaia ti aspetta nell'area briefing con la scheda del piano 2."));return;}
    if(Phase==109){Notify(TEXT("Controlla in ordine: cavi, IP di PC3, configurazione di entrambi gli switch, SSH e salvataggio. TEST elenca cio' che manca."));return;}
    Notify(HintLevel==1?TEXT("Segui il percorso completo, non solo il primo LED. Puoi ispezionare ogni porta."):TEXT("La scheda Comandi del portatile spiega ping. Per PC2 cerca l'indirizzo 192.168.1.11."));
}
FString AClaraStoryDirector::GetNotebookText()const
{
    if(Phase>=200)return GetDayThreeNotebook();
    // Il taccuino riceve le note quando Luca le spiega (le voci «da inserire nel taccuino» dei copioni): niente risposte in anticipo.
    FString T=GetTitle()+TEXT("\n\nOBIETTIVO\n")+GetObjective();
    const int32 P=Phase>=100?99:Phase;
    if(P>=3)T+=TEXT("\n\nLA RETE DI OGGI\nIl mio portatile: 192.168.1.10\nPC2: 192.168.1.11\nLa rete è già impostata: oggi si lavora su cavi e collegamenti.");
    if(P>=5)T+=TEXT("\n\nCAVI\nCat 6, bianco: due spine RJ-45, porta i dati. In rame si arriva fino a 100 metri.\nConsole, azzurro: USB da un lato, RJ-45 dall'altro. Porta i comandi di gestione, non il traffico.\nFibra, giallo: due connettori LC. Porta luce: arriva molto più lontano e non risente delle interferenze.");
    if(P>=5)T+=TEXT("\n\nPING\nTest per sapere se un altro dispositivo risponde. Usa ICMP.\nIl PC manda un echo request, l'altro risponde con un echo reply.\nReply from: risponde.\nRequest timed out: nessuno risponde.\nDestination host unreachable: la rete non sa come arrivarci.\nIl ping è il primo test da fare.");
    if(P>=6)T+=TEXT("\n\nLED DELLO SWITCH\nSpento: nessun collegamento.\nVerde fisso: collegamento attivo. Verde lampeggiante: c'è traffico.\nAmbra: la porta sta aspettando, fino a circa 30 secondi dopo il collegamento. Non è un guasto.\nVerde e ambra alternati: problema sul collegamento.\nLED SYST: verde = switch funzionante, ambra = guasto.");
    if(P>=8)T+=TEXT("\nUn LED acceso non basta: serve il collegamento anche dall'altra parte.");
    if(P>=9)T+=TEXT("\n\nMAC\nIndirizzo fisico della scheda di rete: 48 bit in esadecimale, per esempio AA-BB-CC-DD-EE-FF.\nPrimi 3 byte: il costruttore. Ultimi 3 byte: la scheda singola.\nServe a consegnare i frame nella rete locale: ogni frame ha un MAC di origine e uno di destinazione.\nIl MAC vale nella rete locale e di norma resta lo stesso. L'IP cambia e serve tra reti diverse.\nSi legge con ipconfig /all, alla voce Indirizzo fisico.");
    if(P>=11||(P==10&&Completed.Contains(TEXT("mac-empty"))))T+=TEXT("\n\nTABELLA MAC DELLO SWITCH\nMAC e porta.\nImpara: legge il MAC di origine e lo associa alla porta da cui è arrivato il frame.\nInoltra: se conosce il MAC di destinazione manda il frame solo su quella porta. Se non lo conosce lo manda su tutte le altre (flooding).\nSi vede con show mac address-table. Cisco scrive il MAC a gruppi di quattro cifre: 02aa.0000.0010.");
    if(P>=12)T+=TEXT("\n\nSTACK\nProtocollo: insieme di regole per comunicare. Stack: pila di protocolli, uno per livello.\nIncapsulamento: scendendo, ogni livello aggiunge la sua intestazione. Chi riceve fa il percorso inverso e la toglie.\nOSI: 7 Application (il servizio che usi) · 6 Presentation (formato, compressione, cifratura) · 5 Session (apre e chiude il dialogo) · 4 Transport (segmenti e consegna) · 3 Network (IP e percorso tra reti) · 2 Data Link (MAC e frame) · 1 Physical (segnali sul cavo).");
    if(P>=13)T+=TEXT("\n\nTCP/IP\nApplication (OSI 7, 6, 5) · Transport (4) · Internet (3) · Network Access (2 e 1).\nOSI serve per capire e diagnosticare. TCP/IP è il modello usato davvero.");
    if(P>=14)T+=TEXT("\n\nROUTER E FIBRA\nIl router 1941 ha solo porte in rame. La fibra parte dallo switch: prima il modulo SFP nello slot, poi il cavo.\nIl router serve per uscire dalla rete, non per un ping tra vicini.");
    if(Phase>=101)T+=TEXT("\n\nGIORNO 2 · IP, MAC E MASK\nL'IP identifica l'interfaccia in una rete; il MAC consegna i frame sul collegamento locale e può essere modificato. La mask separa rete e host. Stessa sottorete: destinazione diretta; rete diversa: prossimo passo verso il gateway.");
    if(Phase>=102)T+=TEXT("\n\nARP\nAssocia un IP locale al MAC: richiesta broadcast FF-FF-FF-FF-FF-FF, risposta diretta al richiedente, coppia memorizzata nella cache. Le voci dinamiche scadono.\nip neigh show mostra la cache; ip neigh flush all la svuota. Alias del copione: arp -a / arp -d *.");
    if(Phase>=104)T+=TEXT("\n\nARP SPOOFING\nARP non autentica le risposte: un'associazione falsa può deviare il traffico anche se il ping risponde. La deviazione non elimina la cifratura dei contenuti.\n\nINDIRIZZI E SUBNET MASK\nIPv4: 32 bit in quattro ottetti 0–255. Nella mask, 1 = rete e 0 = host. 255.255.255.0 = /24. IP e gateway devono essere nella stessa rete locale; il gateway serve per destinazioni esterne.\nPrivati: 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16. Non instradati direttamente su Internet. Pubblici: instradabili globalmente; esistono anche intervalli speciali, per esempio loopback. Non tutti gli indirizzi non privati sono pubblici.");
    if(Completed.Contains(TEXT("day2-subnet")))T+=TEXT("\n\nIL FOGLIO /26\nRete = IP AND mask. Broadcast = tutti i bit host a 1. In questa /26, 2^6 − 2 = 62 host.\n192.168.1.70/26: rete 192.168.1.64, broadcast 192.168.1.127, host da .65 a .126. .130 è in un'altra rete.");
    if(Phase>=105)T+=TEXT("\n\nSCHEDA SW-PROVA\nNome SW-PROVA; IP 192.168.1.2 /24; gateway 192.168.1.1.\nPrompt >: utente; enable → # privilegiata; configure terminal → (config)# configurazione.\nEnable secret e password console con login; service password-encryption offusca le password di linea (non protezione forte). Banner MOTD. IP di gestione sulla SVI interface vlan 1; no shutdown la abilita. La console porta comandi, non alimentazione.\nSSH: dominio hacktheroute.local, RSA almeno 2048, ip ssh version 2, utente admin, VTY con login local e transport input ssh. SSH cifra il trasporto, porta 22; Telnet usa la porta 23 e trasmette credenziali in chiaro.\nSalva running-config in startup-config, poi rileggi la copia e verifica il ping. Dal portatile prova ssh admin@192.168.1.2 e la password che hai impostato: è un'autenticazione nel modello locale del gioco. Usa solo password di esercitazione.");
    if(Phase>=109)T+=TEXT("\n\nSCHEDA DI GAIA · PIANO 2\nR1 192.168.1.1; SW-A .2; SW-B .3; portatile .10; PC2 .11; PC3 atteso .12. Tutti /24, gateway .1.\nCavi: R1↔SW-A, SW-A↔SW-B, portatile e PC2↔SW-A, PC3↔SW-B. Console separata per configurare ciascuno switch.\nVerifica ping e autenticazione SSH su entrambi gli switch, quindi TEST e riavvio. Una modifica alla configurazione richiede una nuova prova SSH. Le password del kit sono esempi di laboratorio.");
    if(Phase>=110)T+=TEXT("\n\nRETE RIPRISTINATA\nDiagnosi dal basso verso l'alto: cavi, IP e mask, configurazione di ogni switch, ping, autenticazione SSH, salvataggio e prova dopo il riavvio. Il giorno seguente parte da un ambiente nuovo.");
    return T;
}
void AClaraStoryDirector::StartDay2()
{
    CancelAllTerminalCommands();
    if(Controller.IsValid())if(auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>())Camera->EndConversation(true);
    ResetDayTransition();ClosePage();CloseLaptopScreen();
    TerminalTranscripts.Empty();TerminalHistories.Empty();Transcript.Empty();TerminalTarget=TEXT("LAPTOP");
    ReturnHeldCable();SortingCable.Empty();TrialCLI.Reset();Day1CLI.Reset();ArpCache.Empty();LearnedMACs.Empty();
    for(const FString Key:Completed.Array())if(Key.StartsWith(TEXT("day2-"))||Key.StartsWith(TEXT("ssh-verified:"))||Key==TEXT("trial-ping"))Completed.Remove(Key);
    PendingSSHUser.Empty();PendingSSHAddress.Empty();
    Completed.Remove(TEXT("arp-cleared"));Completed.Remove(TEXT("startup-read"));SubnetStep=0;
    for(const auto& Entry:Devices)if(Entry.Value)for(const auto& Port:Entry.Value->GetPorts()){FString Error;Entry.Value->DisconnectPort(Port.Id,Error);}
    for(const FString I:{TEXT("badge"),TEXT("laptop"),TEXT("ethernet"),TEXT("console"),TEXT("fiber"),TEXT("sfp_module")})Owned.Add(I);
    if(CarriedLaptop){auto* L=Devices.FindRef(TEXT("LAPTOP")).Get();if(L){L->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);L->SetActorLocationAndRotation({1135,1120,77.2},FRotator::ZeroRotator);L->SetLaptopCarried(false);}CarriedLaptop=false;}
    FString E;auto* SW=Devices.FindRef(TEXT("SW1")).Get();
    if(SW)for(const FString H:{TEXT("LAPTOP"),TEXT("PC2")})if(!PhysicalLinked(H,TEXT("SW1")))
        for(const auto& P:SW->GetPorts())if(P.Kind==TEXT("ethernet")&&!SW->IsPortOccupied(P.Id)){AClaraNetworkDevice::ConnectPorts(Devices[H],TEXT("Ethernet"),SW,P.Id,TEXT("ethernet"),E);break;}
    SetPhase(100);SaveProgress();
}
TSharedRef<FJsonObject> AClaraStoryDirector::CaptureProgress() const
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"),2);
    Root->SetBoolField(TEXT("day1Complete"),Completed.Contains(TEXT("day1"))||Phase>=100);
    Root->SetBoolField(TEXT("day2Complete"),Completed.Contains(TEXT("day2"))||Phase==110||Phase>=200);
    Root->SetNumberField(TEXT("phase"),Phase);
    if(Phase>=200)Root->SetObjectField(TEXT("routingLab"),RoutingLab.ExportState());
    auto Names=[](const TSet<FString>& Set){TArray<FString> Sorted=Set.Array();Sorted.Sort();TArray<TSharedPtr<FJsonValue>> A;for(const FString& S:Sorted)A.Add(MakeShared<FJsonValueString>(S));return A;};
    Root->SetArrayField(TEXT("owned"),Names(Owned));Root->SetArrayField(TEXT("sorted"),Names(Sorted));Root->SetArrayField(TEXT("completed"),Names(Completed));
    TArray<TSharedPtr<FJsonValue>> Links,Modules;
    for(const auto& A:Devices)if(A.Value)for(const auto& Port:A.Value->GetPorts())
    {
        if(A.Value->HasOpticalModule(Port.Id)){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("d"),A.Key);M->SetStringField(TEXT("p"),Port.Id);Modules.Add(MakeShared<FJsonValueObject>(M));}
        FString PeerPort,PeerId;const AClaraNetworkDevice* Peer=A.Value->GetPeerForPort(Port.Id,PeerPort);
        if(Peer)for(const auto& B:Devices)if(B.Value.Get()==Peer){PeerId=B.Key;break;}
        if(PeerId.IsEmpty()||!(A.Key<PeerId))continue;
        const FString Kind=A.Value->GetEffectivePortKind(Port.Id);
        auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("a"),A.Key);L->SetStringField(TEXT("pa"),Port.Id);L->SetStringField(TEXT("b"),PeerId);L->SetStringField(TEXT("pb"),PeerPort);
        L->SetStringField(TEXT("kind"),Kind==TEXT("ethernet")?TEXT("ethernet"):Kind.StartsWith(TEXT("console"))?TEXT("console"):TEXT("fiber"));Links.Add(MakeShared<FJsonValueObject>(L));
    }
    Root->SetArrayField(TEXT("links"),Links);Root->SetArrayField(TEXT("sfp"),Modules);
    TArray<TSharedPtr<FJsonValue>> Macs;for(const auto& M:LearnedMACs){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("m"),M.Key);O->SetStringField(TEXT("p"),M.Value);Macs.Add(MakeShared<FJsonValueObject>(O));}Root->SetArrayField(TEXT("macs"),Macs);
    if(const auto* L=Devices.FindRef(TEXT("LAPTOP")).Get())
    {auto O=MakeShared<FJsonObject>();const FVector At=L->GetActorLocation();O->SetBoolField(TEXT("carried"),CarriedLaptop);O->SetNumberField(TEXT("x"),FMath::RoundToInt(At.X));O->SetNumberField(TEXT("y"),FMath::RoundToInt(At.Y));O->SetNumberField(TEXT("z"),At.Z);Root->SetObjectField(TEXT("laptop"),O);}
    if(Controller.IsValid()&&Controller->GetPawn())
    {
        auto O=MakeShared<FJsonObject>();const auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>();
        const bool Narrative=Camera&&Camera->IsActive();const FVector At=Narrative?Camera->GetResumeTransform().GetLocation():Controller->GetPawn()->GetActorLocation();
        const FRotator Look=Narrative?Camera->GetResumeLook():Controller->GetControlRotation();
        O->SetNumberField(TEXT("x"),Narrative?At.X:FMath::RoundToInt(At.X/20)*20);O->SetNumberField(TEXT("y"),Narrative?At.Y:FMath::RoundToInt(At.Y/20)*20);
        O->SetNumberField(TEXT("z"),Narrative?At.Z:90.);O->SetNumberField(TEXT("yaw"),Narrative?Look.Yaw:FMath::RoundToInt(Look.Yaw/15)*15);
        O->SetNumberField(TEXT("pitch"),Look.Pitch);O->SetNumberField(TEXT("roll"),Look.Roll);
        O->SetNumberField(TEXT("actorYaw"),Narrative?Camera->GetResumeTransform().Rotator().Yaw:Controller->GetPawn()->GetActorRotation().Yaw);Root->SetObjectField(TEXT("player"),O);
    }
    if(IsTowerPuzzle())Root->SetObjectField(TEXT("layerTower"),LayerTower.ExportState());
    Root->SetObjectField(TEXT("day1CLI"),Day1CLI.ExportState());Root->SetObjectField(TEXT("trialCLI"),TrialCLI.ExportState());
    Root->SetObjectField(TEXT("switchA"),SwitchA.ExportState());Root->SetObjectField(TEXT("switchB"),SwitchB.ExportState());
    auto Arp=MakeShared<FJsonObject>();for(const auto& Entry:ArpCache)Arp->SetStringField(Entry.Key,Entry.Value);Root->SetObjectField(TEXT("arp"),Arp);
    Root->SetNumberField(TEXT("subnetStep"),SubnetStep);Root->SetNumberField(TEXT("hintLevel"),HintLevel);
    Root->SetStringField(TEXT("equippedCable"),EquippedCable);Root->SetStringField(TEXT("sortingCable"),SortingCable);
    Root->SetStringField(TEXT("screenPage"),ScreenPage);Root->SetStringField(TEXT("terminalTarget"),TerminalTarget);
    Root->SetStringField(TEXT("transcript"),Transcript);Root->SetBoolField(TEXT("screenOpen"),ScreenOpen);Root->SetBoolField(TEXT("terminalWasBusy"),IsTerminalBusy());
    TArray<TSharedPtr<FJsonValue>> SavedPuzzle;for(const FString& Item:Puzzle)SavedPuzzle.Add(MakeShared<FJsonValueString>(Item));Root->SetArrayField(TEXT("puzzle"),SavedPuzzle);
    auto Hosts=MakeShared<FJsonObject>();for(const auto& Node:SimNodes)if(Node.Kind==TEXT("host"))Hosts->SetStringField(Node.Id,Node.IP);
    Root->SetObjectField(TEXT("simHosts"),Hosts);Root->SetArrayField(TEXT("simLinks"),Names(SimLinks));
    Root->SetStringField(TEXT("consoleTarget"),ConsoleTarget);Root->SetBoolField(TEXT("challengeReady"),ChallengeReady);Root->SetBoolField(TEXT("challengeRebooted"),ChallengeRebooted);
    if(Page==TEXT("dialogue")&&!LaptopMenu)
    {
        auto D=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Lines;
        for(const auto& Line:DialogueQueue){auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("speaker"),Line.Key);L->SetStringField(TEXT("text"),Line.Value);Lines.Add(MakeShared<FJsonValueObject>(L));}
        D->SetArrayField(TEXT("lines"),Lines);D->SetNumberField(TEXT("index"),DialogueIndex);D->SetNumberField(TEXT("next"),AfterDialogue);
        TArray<TSharedPtr<FJsonValue>> Options;for(const FString& Choice:Choices)Options.Add(MakeShared<FJsonValueString>(Choice));D->SetArrayField(TEXT("choices"),Options);
        Root->SetObjectField(TEXT("dialogue"),D);
    }
    return Root;
}
void AClaraStoryDirector::SaveProgress()
{
    if(SelfTesting||!Persist||Phase<=0)return;
    FString Out;const auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
    if(!FJsonSerializer::Serialize(CaptureProgress(),Writer)||Out==LastSave)return;
    const FString File=FPaths::ProjectSavedDir()/ProgressFile,Temporary=File+TEXT(".tmp");
    const FString Backup=File+TEXT(".bak");
    // Unreal's replace operation deletes the destination first; retain a recovery copy across that gap.
    const bool Written=FFileHelper::SaveStringToFile(Out,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    const bool BackedUp=Written&&(!IFileManager::Get().FileExists(*File)||IFileManager::Get().Copy(*Backup,*File,true,true)==COPY_OK);
    if(BackedUp&&IFileManager::Get().Move(*File,*Temporary,true,true,false,true))LastSave=Out;
    else
    {
        if(!IFileManager::Get().FileExists(*File)&&IFileManager::Get().FileExists(*Backup))IFileManager::Get().Copy(*File,*Backup,true,true);
        UE_LOG(LogTemp,Warning,TEXT("CLARA_SAVE: checkpoint not written; recovery file retained."));
    }
}
bool AClaraStoryDirector::RestoreProgress(const TSharedPtr<FJsonObject>& Save)
{
    if(!Save.IsValid())return false;
    bool Day1=false,Day2=false;int32 SavedPhase=0,Version=1;
    Save->TryGetNumberField(TEXT("version"),Version);if(Version<1||Version>2)return false;
    Save->TryGetBoolField(TEXT("day1Complete"),Day1);Save->TryGetBoolField(TEXT("day2Complete"),Day2);Save->TryGetNumberField(TEXT("phase"),SavedPhase);
    TGuardValue<bool> RestoreGuard(Persist,false);
    if(Version==1&&(SavedPhase>=100||(Day1&&SavedPhase!=16)))
    {
        // Old saves contained no Day 2 configuration to recover.
        Completed.Add(TEXT("day1"));StartDay2();if(Day2)SetPhase(110);
        else Notify(TEXT("Questo salvataggio precedente conserva il Giorno 1. Il Giorno 2 riparte dall'inizio."));return true;
    }
    if(!((SavedPhase>=0&&SavedPhase<=16)||(SavedPhase>=100&&SavedPhase<=110)||(SavedPhase>=200&&SavedPhase<=213)))return false;
    FClaraRoutingLab LoadedRouting;
    if(SavedPhase>=200){const TSharedPtr<FJsonObject>* Object=nullptr;if(!Save->TryGetObjectField(TEXT("routingLab"),Object)||!LoadedRouting.ImportState(*Object))return false;}
    if(SavedPhase>=212)
    {
        const TArray<TSharedPtr<FJsonValue>>* Proofs=nullptr;bool Verified=false;
        if(Save->TryGetArrayField(TEXT("completed"),Proofs))for(const auto& Proof:*Proofs){FString Value;if(Proof->TryGetString(Value)&&Value==TEXT("d3-reboot-verified"))Verified=true;}
        if(!Verified||!LoadedRouting.Validate(true).IsEmpty())return false;
    }
    FClaraLayerTower LoadedTower;bool LegacyTower=false;
    if(SavedPhase==11||SavedPhase==12)
    {
        const TSharedPtr<FJsonObject>* TowerState=nullptr;
        if(Save->TryGetObjectField(TEXT("layerTower"),TowerState))
        {if(!LoadedTower.ImportState(*TowerState)||LoadedTower.Kind!=(SavedPhase==11?TEXT("osi"):TEXT("tcp")))return false;}
        else{LoadedTower.Reset(SavedPhase==11?TEXT("osi"):TEXT("tcpip"));LegacyTower=true;}
    }
    FClaraSwitchCLI LoadedDay1,LoadedTrial,LoadedA,LoadedB;
    if(Version==2)for(const auto& Entry:TArray<TPair<FString,FClaraSwitchCLI*>>{{TEXT("day1CLI"),&LoadedDay1},{TEXT("trialCLI"),&LoadedTrial},{TEXT("switchA"),&LoadedA},{TEXT("switchB"),&LoadedB}})
    {const TSharedPtr<FJsonObject>* O=nullptr;if(!Save->TryGetObjectField(Entry.Key,O)||!Entry.Value->ImportState(*O))return false;}
    if(Controller.IsValid())if(auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>())Camera->EndConversation(true);
    ResetDayTransition();ClosePage();CloseLaptopScreen();ReturnHeldCable();SortingCable.Empty();
    for(const auto& Entry:Devices)if(Entry.Value)for(const auto& Port:Entry.Value->GetPorts()){FString Error;Entry.Value->DisconnectPort(Port.Id,Error);}
    if(auto* L=Devices.FindRef(TEXT("LAPTOP")).Get()){L->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);L->SetLaptopCarried(false);}
    CarriedLaptop=false;Owned.Empty();Sorted.Empty();Completed.Empty();LearnedMACs.Empty();ArpCache.Empty();
    auto ReadSet=[&Save](const TCHAR* Field,TSet<FString>& Out){const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(Save->TryGetArrayField(Field,Values))for(const auto& V:*Values){FString S;if(V->TryGetString(S))Out.Add(S);}};
    ReadSet(TEXT("owned"),Owned);ReadSet(TEXT("sorted"),Sorted);ReadSet(TEXT("completed"),Completed);Owned.Add(TEXT("notebook"));
    CancelAllTerminalCommands();Phase=SavedPhase;HintLevel=0;SubnetStep=0;PendingSSHUser.Empty();PendingSSHAddress.Empty();
    // Earlier checkpoints already passed these lessons before reaching the later phase.
    if(Phase>=104&&Phase<200)Completed.Add(TEXT("day2-arp"));
    if(Phase>=105&&Phase<200){Completed.Add(TEXT("day2-host-read"));Completed.Add(TEXT("day2-subnet"));}
    if(Phase>=108&&Phase<200)Completed.Add(TEXT("day2-trial"));
    SetOfficeClockMinutes(Phase>=100?540.f:Phase==16?1080.f:540.f+Phase*32.f);
    Save->TryGetNumberField(TEXT("hintLevel"),HintLevel);Save->TryGetNumberField(TEXT("subnetStep"),SubnetStep);SubnetStep=FMath::Clamp(SubnetStep,0,5);
    if(Phase>=200){SetupDayThreeLab(false);RoutingLab=MoveTemp(LoadedRouting);SimNodes.Empty();SimLinks.Empty();ChallengeReady=false;ChallengeRebooted=false;}else if(Phase>=109)SetupChallenge();else{SimNodes.Empty();SimLinks.Empty();ChallengeReady=false;ChallengeRebooted=false;}
    Day1CLI=MoveTemp(LoadedDay1);TrialCLI=MoveTemp(LoadedTrial);SwitchA=MoveTemp(LoadedA);SwitchB=MoveTemp(LoadedB);
    for(TActorIterator<AClaraStoryProp> It(GetWorld());It;++It)
    {
        const FString Id=It->GetPropId();
        if(Id==TEXT("Badge")||Id==TEXT("Coffee")){const bool Hidden=Id==TEXT("Badge")?Owned.Contains(TEXT("badge")):Completed.Contains(TEXT("coffee"));It->SetActorHiddenInGame(Hidden);It->SetActorEnableCollision(!Hidden);}
        if(Id.StartsWith(TEXT("Cable_"))){FVector At=It->GetActorLocation();At.Y=Sorted.Contains(Id.Mid(6))?1321:1278;It->SetActorLocation(At);}
    }
    const TSharedPtr<FJsonObject>* State=nullptr;
    if(Controller.IsValid()&&Controller->GetPawn()&&Save->TryGetObjectField(TEXT("player"),State)&&State)
    {
        double X=0,Y=0,Z=90,Yaw=0,Pitch=-3,Roll=0,ActorYaw=0;
        if((*State)->TryGetNumberField(TEXT("x"),X)&&(*State)->TryGetNumberField(TEXT("y"),Y)&&X>20&&X<1880&&Y>20&&Y<1420)
        {(*State)->TryGetNumberField(TEXT("yaw"),Yaw);ActorYaw=Yaw;(*State)->TryGetNumberField(TEXT("actorYaw"),ActorYaw);(*State)->TryGetNumberField(TEXT("z"),Z);(*State)->TryGetNumberField(TEXT("pitch"),Pitch);(*State)->TryGetNumberField(TEXT("roll"),Roll);
            Controller->GetPawn()->TeleportTo(FVector(X,Y,FMath::Clamp(Z,88.,200.)),FRotator(0,ActorYaw,0),false,false);Controller->SetControlRotation(FRotator(FMath::Clamp(Pitch,-89.,89.),Yaw,Roll));}
    }
    if(auto* L=Devices.FindRef(TEXT("LAPTOP")).Get();L&&Owned.Contains(TEXT("laptop")))
    {
        bool Carried=true;double X=0,Y=0,Z=0;State=nullptr;
        if(Save->TryGetObjectField(TEXT("laptop"),State)&&State){(*State)->TryGetBoolField(TEXT("carried"),Carried);(*State)->TryGetNumberField(TEXT("x"),X);(*State)->TryGetNumberField(TEXT("y"),Y);(*State)->TryGetNumberField(TEXT("z"),Z);}
        if(Carried||X<20||X>1880||Y<20||Y>1420||Z<30||Z>120)PickupLaptop();else L->SetActorLocationAndRotation(FVector(X,Y,Z),FRotator::ZeroRotator);
    }
    const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if(Save->TryGetArrayField(TEXT("sfp"),Values))for(const auto& V:*Values)
        {const TSharedPtr<FJsonObject>* O=nullptr;FString D,P;if(V->TryGetObject(O)&&(*O)->TryGetStringField(TEXT("d"),D)&&(*O)->TryGetStringField(TEXT("p"),P))if(auto* Device=Devices.FindRef(D).Get())Device->InstallOpticalModule(P);}
    Values=nullptr;
    if(Save->TryGetArrayField(TEXT("links"),Values))for(const auto& V:*Values)
        {const TSharedPtr<FJsonObject>* O=nullptr;
        if(V->TryGetObject(O))
        {
            FString AId,BId,PA,PB,Kind,Error;
            if(!(*O)->TryGetStringField(TEXT("a"),AId)||!(*O)->TryGetStringField(TEXT("b"),BId)||!(*O)->TryGetStringField(TEXT("pa"),PA)||
               !(*O)->TryGetStringField(TEXT("pb"),PB)||!(*O)->TryGetStringField(TEXT("kind"),Kind))continue;
            auto* A=Devices.FindRef(AId).Get();auto* B=Devices.FindRef(BId).Get();
            if(A&&B)AClaraNetworkDevice::ConnectPorts(A,PA,B,PB,Kind,Error);
        }}
    Values=nullptr;
    if(Save->TryGetArrayField(TEXT("macs"),Values))for(const auto& V:*Values)
        {const TSharedPtr<FJsonObject>* O=nullptr;FString M,P;if(V->TryGetObject(O)&&(*O)->TryGetStringField(TEXT("m"),M)&&(*O)->TryGetStringField(TEXT("p"),P))LearnedMACs.Add(M,P);}
    Day1CLI.SetLearnedMacTable(LearnedMACs);
    const TSharedPtr<FJsonObject>* Arp=nullptr;
    if(Save->TryGetObjectField(TEXT("arp"),Arp))for(const auto& Entry:(*Arp)->Values){FString M;const FString Address(*Entry.Key);if(ValidIPv4(Address)&&Entry.Value->TryGetString(M))ArpCache.Add(Address,M);}
    else if(Version==1&&LearnedMACs.Num())ArpCache.Add(PC2IP,PC2MAC);
    if(Phase==11)SetupPuzzle(TEXT("osi"));else if(Phase==12)SetupPuzzle(TEXT("tcpip"));else if(Phase==101)SetupPuzzle(TEXT("arp"));else{Puzzle.Empty();PuzzleExpected.Empty();}
    if(Save->TryGetArrayField(TEXT("puzzle"),Values)&&Values->Num()==PuzzleExpected.Num())
    {
        TArray<FString> Order;for(const auto& V:*Values){FString S;if(V->TryGetString(S)&&PuzzleExpected.Contains(S)&&!Order.Contains(S))Order.Add(S);}
        if(Order.Num()==PuzzleExpected.Num())Puzzle=MoveTemp(Order);
    }
    if(Phase==11||Phase==12){LayerTower=MoveTemp(LoadedTower);TowerMnemonicVisible=false;}
    Save->TryGetStringField(TEXT("equippedCable"),EquippedCable);if(Controller.IsValid()&&Owned.Contains(EquippedCable))Controller->SetCableKind(EquippedCable);
    FString SavedCable;if(Phase==4&&Save->TryGetStringField(TEXT("sortingCable"),SavedCable)&&
       (SavedCable==TEXT("ethernet")||SavedCable==TEXT("console")||SavedCable==TEXT("fiber"))&&!Sorted.Contains(SavedCable))SelectSortingCable(SavedCable);
    const TSharedPtr<FJsonObject>* Hosts=nullptr;
    if(Save->TryGetObjectField(TEXT("simHosts"),Hosts))for(auto& Node:SimNodes)if(Node.Kind==TEXT("host")){FString IP;if((*Hosts)->TryGetStringField(Node.Id,IP)&&ValidIPv4(IP))Node.IP=IP;}
    TSet<FString> Links;ReadSet(TEXT("simLinks"),Links);
    for(const FString& Link:Links){FString A,B;if(Link.Split(TEXT("|"),&A,&B)&&A!=B&&SimNodes.ContainsByPredicate([&](const auto& N){return N.Id==A;})&&SimNodes.ContainsByPredicate([&](const auto& N){return N.Id==B;}))SimLinks.Add(Edge(A,B));}
    Save->TryGetStringField(TEXT("consoleTarget"),ConsoleTarget);if(ConsoleTarget!=TEXT("SW-A")&&ConsoleTarget!=TEXT("SW-B"))ConsoleTarget.Empty();
    Save->TryGetBoolField(TEXT("challengeReady"),ChallengeReady);Save->TryGetBoolField(TEXT("challengeRebooted"),ChallengeRebooted);
    Save->TryGetStringField(TEXT("screenPage"),ScreenPage);Save->TryGetStringField(TEXT("terminalTarget"),TerminalTarget);Save->TryGetStringField(TEXT("transcript"),Transcript);
    bool InterruptedPing=false;Save->TryGetBoolField(TEXT("terminalWasBusy"),InterruptedPing);if(InterruptedPing)Transcript+=TEXT("\nPing interrotto dalla chiusura: ripeti il comando.\n");
    bool ReopenScreen=false;Save->TryGetBoolField(TEXT("screenOpen"),ReopenScreen);
    if(ReopenScreen&&Phase!=16&&Phase!=110&&Phase!=213&&Owned.Contains(TEXT("laptop"))&&!SelfTesting)OpenLaptopScreen();
    Toast.Empty();
    if(Phase==213){Completed.Add(TEXT("day3"));ShowPanel(TEXT("day3report"));return true;}
    if(Phase==16||Phase==110){Completed.Add(TEXT("day1"));if(Phase==110)Completed.Add(TEXT("day2"));ShowPanel(TEXT("report"));return true;}
    if(LegacyTower)Notify(TEXT("Il vecchio esercizio a lista è diventato una torre a tre pioli. Riparti soltanto da questo puzzle: il resto della partita è conservato."));
    const TSharedPtr<FJsonObject>* DialogueState=nullptr;
    if(Save->TryGetObjectField(TEXT("dialogue"),DialogueState)&&(*DialogueState)->TryGetArrayField(TEXT("lines"),Values))
    {
        DialogueQueue.Empty();Choices.Empty();DialogueIndex=0;AfterDialogue=-1;
        for(const auto& V:*Values){const TSharedPtr<FJsonObject>* O=nullptr;FString Who,Line;if(V->TryGetObject(O)&&(*O)->TryGetStringField(TEXT("speaker"),Who)&&(*O)->TryGetStringField(TEXT("text"),Line))DialogueQueue.Add({Who,Line});}
        (*DialogueState)->TryGetNumberField(TEXT("index"),DialogueIndex);(*DialogueState)->TryGetNumberField(TEXT("next"),AfterDialogue);
        if((*DialogueState)->TryGetArrayField(TEXT("choices"),Values))for(const auto& V:*Values){FString Choice;if(V->TryGetString(Choice))Choices.Add(Choice);}
        if(DialogueQueue.IsValidIndex(DialogueIndex))
        {Speaker=DialogueQueue[DialogueIndex].Key;Dialogue=DialogueQueue[DialogueIndex].Value;ShowPanel(TEXT("dialogue"));if(!SelfTesting)if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(Speaker,Dialogue);return true;}
    }
    if(!LegacyTower)Notify(TEXT("Partita ripresa: ")+GetObjective());return true;
}
void AClaraStoryDirector::RestartStory()
{
    // Nuova partita esplicita: cancella il salvataggio e ricarica la stanza dal Giorno 1.
    if(Controller.IsValid())if(auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>())Camera->EndConversation(true);
    CancelAllTerminalCommands();OpenLaptopAfterConversation=false;Persist=false;for(const FString Suffix:{FString(),FString(TEXT(".bak")),FString(TEXT(".tmp"))})IFileManager::Get().Delete(*(FPaths::ProjectSavedDir()/ProgressFile+Suffix));
    UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true)));
}
void AClaraStoryDirector::SetupChallenge()
{
    SimNodes={{TEXT("LAPTOP"),TEXT("Portatile"),TEXT("host"),TEXT("192.168.1.10"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{130,100}},
    {TEXT("R1"),TEXT("R1 · gia' pronto"),TEXT("router"),TEXT("192.168.1.1"),TEXT("255.255.255.0"),TEXT(""),{500,40}},
    {TEXT("SW-A"),TEXT("SW-A"),TEXT("switch"),TEXT(""),TEXT("255.255.255.0"),TEXT(""),{350,200}},
    {TEXT("SW-B"),TEXT("SW-B"),TEXT("switch"),TEXT(""),TEXT("255.255.255.0"),TEXT(""),{650,200}},
    {TEXT("PC2"),TEXT("PC2"),TEXT("host"),TEXT("192.168.1.11"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{180,370}},
    {TEXT("PC3"),TEXT("PC3"),TEXT("host"),TEXT("192.168.2.12"),TEXT("255.255.255.0"),TEXT("192.168.1.1"),{650,370}}};
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
    uint32 IPv4=0;const bool Valid=ValidIPv4(Address,&IPv4)&&(IPv4>>24)>0&&(IPv4>>24)!=127&&(IPv4>>24)<224&&(IPv4&255)>0&&(IPv4&255)<255;
    if(!N||N->Kind!=TEXT("host")||!Valid){SimMessage=TEXT("Scegli un host e inserisci un IPv4 unicast valido per una rete /24.");return false;}
    N->IP=Address;ChallengeReady=false;ChallengeRebooted=false;SimMessage=TEXT("Indirizzo aggiornato. Ora verifica la connettivita'.");return true;
}
FString AClaraStoryDirector::RunChallengeTest()
{
    if(Phase!=109)return TEXT("La challenge si apre con Gaia, dopo lo switch di prova.");
    TArray<FString> Problems;
    if(!Completed.Contains(TEXT("day2-arp"))||!Completed.Contains(TEXT("day2-subnet"))||!Completed.Contains(TEXT("day2-trial")))Problems.Add(TEXT("Completa prima ARP, subnetting e verifica dello switch di prova con Luca."));
    for(const auto& E:TArray<TPair<FString,FString>>{{TEXT("R1"),TEXT("SW-A")},{TEXT("SW-A"),TEXT("SW-B")},{TEXT("LAPTOP"),TEXT("SW-A")},{TEXT("PC2"),TEXT("SW-A")},{TEXT("PC3"),TEXT("SW-B")}})
        if(!SimHasLink(E.Key,E.Value))Problems.Add(TEXT("Manca il collegamento ")+E.Key+TEXT(" ↔ ")+E.Value);
    const FString A=SwitchA.ValidateDay2(TEXT("SW-A"),TEXT("192.168.1.2"),false),B=SwitchB.ValidateDay2(TEXT("SW-B"),TEXT("192.168.1.3"),false);
    if(!A.IsEmpty())Problems.Add(TEXT("SW-A: ")+A);if(!B.IsEmpty())Problems.Add(TEXT("SW-B: ")+B);
    if(SwitchA.GetGateway()!=TEXT("192.168.1.1"))Problems.Add(TEXT("SW-A: gateway atteso 192.168.1.1."));
    if(SwitchB.GetGateway()!=TEXT("192.168.1.1"))Problems.Add(TEXT("SW-B: gateway atteso 192.168.1.1."));
    for(const auto& N:SimNodes)
    {
        const FString ExpectedIP=N.Id==TEXT("LAPTOP")?TEXT("192.168.1.10"):N.Id==TEXT("PC2")?TEXT("192.168.1.11"):N.Id==TEXT("PC3")?TEXT("192.168.1.12"):FString();
        if(!ExpectedIP.IsEmpty()&&N.IP!=ExpectedIP)Problems.Add(N.Id+TEXT(": l'IP non corrisponde alla scheda."));
        if(N.Kind==TEXT("host")&&!SimReachable(TEXT("LAPTOP"),N.Id))Problems.Add(TEXT("Il portatile non raggiunge ")+N.Id);
    }
    if(!HasVerifiedSSH(TEXT("SW-A"),SwitchA))Problems.Add(TEXT("SW-A: manca una prova SSH con utente e password sulla configurazione attuale."));
    if(!HasVerifiedSSH(TEXT("SW-B"),SwitchB))Problems.Add(TEXT("SW-B: manca una prova SSH con utente e password sulla configurazione attuale."));
    ChallengeReady=Problems.IsEmpty();
    if(ChallengeReady&&ChallengeRebooted){SimMessage=TEXT("TEST superato anche dopo il riavvio. Credenziali SSH verificate nel laboratorio, Telnet escluso, indirizzi e topologia corretti.");SetPhase(110);return SimMessage;}
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
    if(!RunClaraLayerTowerFlowSelfTest())return false;
    if(!RunDayThreeSelfTest())return false;
    TGuardValue<bool> TestGuard(SelfTesting,true);
    // Exercise the actual graph and validators; dialogue clicks alone never satisfy a lab gate.
    FString Error;bool OK=RunGatewayPortRepairSelfTest()&&Devices.Contains(TEXT("LAPTOP"))&&Devices.Contains(TEXT("PC2"))&&Devices.Contains(TEXT("SW1"));
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
        Phase=5;Controller->SetCableKind(TEXT("ethernet"));Controller->OpenDevice(SW);
        Controller->SelectDevicePort(TEXT("Gi1/0/1"));Controller->OpenDevice(L);
        Controller->SetCableKind(TEXT("ethernet"));Controller->SelectDevicePort(TEXT("Ethernet"));
        const bool ConnectedInHand=PhysicalLinked(TEXT("LAPTOP"),TEXT("SW1"));Controller->CloseDevice();PollLab();
        const bool Advanced=Phase==6;ClosePage();
        UE_LOG(LogTemp,Display,TEXT("CLARA_ETHERNET_INTERACTION_%s: switch to carried laptop through inspector, repeated cable selection preserves first end, mission advances"),(ConnectedInHand&&Advanced)?TEXT("PASS"):TEXT("FAIL"));
        OK&=ConnectedInHand&&Advanced;FString UnplugError;L->DisconnectPort(TEXT("Ethernet"),UnplugError);
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
    Phase=100;OK&=!GetNotebookText().Contains(TEXT("SCHEDA SW-PROVA"));
    SetPhase(101);Puzzle=PuzzleExpected;OK&=CheckPuzzle()&&Phase==102;FinishDialogue();
    TerminalTarget=TEXT("LAPTOP");ExecutePC(TEXT("ip neigh flush all"));ExecutePC(TEXT("ip neigh show"));OK&=Phase==103;
    ExecutePC(TEXT("ip neigh show"));OK&=Phase==103;
    ExecutePC(TEXT("ping 192.168.1.11"));ExecutePC(TEXT("ip neigh show"));FinishDialogue();FinishDialogue();
    OK&=Phase==104&&!SubmitAnswer(TEXT("01000110"));
    ExecutePC(TEXT("ip addr"));FinishDialogue();OK&=ScreenPage==TEXT("subnet");
    for(const TCHAR* Answer:{TEXT("01000110"),TEXT("11000000"),TEXT("192.168.1.64"),TEXT("192.168.1.127"),TEXT("62"),TEXT("no")})OK&=SubmitAnswer(Answer);
    OK&=Phase==105&&Completed.Contains(TEXT("day2-arp"))&&Completed.Contains(TEXT("day2-subnet"));FinishDialogue();
    SetupChallenge();Phase=109;OK=OK&&RunChallengeTest().Contains(TEXT("Manca il collegamento"))&&!ChallengeReady;
    OK&=RunChallengeTest().Contains(TEXT("Completa prima"));Completed.Add(TEXT("day2-trial"));
    const int32 LockedPhase=Phase;RebootChallenge();OK=OK&&Phase==LockedPhase&&!ChallengeRebooted;
    for(const auto& E:TArray<TPair<FString,FString>>{{TEXT("R1"),TEXT("SW-A")},{TEXT("SW-A"),TEXT("SW-B")},{TEXT("LAPTOP"),TEXT("SW-A")},{TEXT("PC2"),TEXT("SW-A")},{TEXT("PC3"),TEXT("SW-B")}})SimLinks.Add(Edge(E.Key,E.Value));
    SetSimHostIP(TEXT("PC3"),TEXT("192.168.1.12"));
    auto Configure=[](FClaraSwitchCLI& CLI,const FString& Name,const FString& IP,bool Save)
    {
        CLI.Reset();CLI.Execute(TEXT("enable"));CLI.Execute(TEXT("conf t"));CLI.Execute(TEXT("hostname ")+Name);
        for(const TCHAR* C:{TEXT("interface vlan1"),TEXT("no shutdown")})CLI.Execute(C);
        CLI.Execute(TEXT("ip address ")+IP+TEXT(" 255.255.255.0"));CLI.Execute(TEXT("exit"));
        for(const TCHAR* C:{TEXT("enable secret Privato1"),TEXT("line console 0"),TEXT("password Console1"),TEXT("login"),TEXT("exit"),TEXT("ip domain-name clara.local"),TEXT("crypto key generate rsa"),TEXT("2048"),TEXT("username admin secret Personale1"),TEXT("ip ssh version 2"),TEXT("line vty 0 15"),TEXT("login local"),TEXT("transport input ssh"),TEXT("exit"),TEXT("banner motd #Accesso autorizzato#"),TEXT("service password-encryption"),TEXT("ip default-gateway 192.168.1.1"),TEXT("end")})CLI.Execute(C);
        if(Save)CLI.Execute(TEXT("write memory"));
    };
    Configure(TrialCLI,TEXT("SW-PROVA"),TEXT("192.168.1.2"),true);Phase=107;TerminalTarget=TEXT("SW-PROVA");
    RunCommand(TEXT("show startup-config"));RunCommand(TEXT("ping 192.168.1.10"));TickTerminal(4.f);OK&=Phase==107;
    TerminalTarget=TEXT("LAPTOP");RunCommand(TEXT("ssh admin@192.168.1.2"));OK&=IsTerminalPasswordInput();
    RunCommand(TEXT("Personale1"));OK&=Phase==108&&Completed.Contains(TEXT("day2-trial"));FinishDialogue();Phase=109;
    auto VerifySSH=[this,&OK]()
    {
        TerminalTarget=TEXT("LAPTOP");
        for(const TCHAR* Address:{TEXT("192.168.1.2"),TEXT("192.168.1.3")})
        {RunCommand(FString(TEXT("ssh admin@"))+Address);OK&=IsTerminalPasswordInput();RunCommand(TEXT("Personale1"));OK&=!IsTerminalPasswordInput();}
    };
    Configure(SwitchA,TEXT("SW-A"),TEXT("192.168.1.2"),false);Configure(SwitchB,TEXT("SW-B"),TEXT("192.168.1.3"),false);
    RunChallengeTest();OK&=!ChallengeReady&&SimMessage.Contains(TEXT("prova SSH"));
    TerminalTarget=TEXT("LAPTOP");RunCommand(TEXT("ssh admin@192.168.1.99"));OK&=!IsTerminalPasswordInput();
    RunCommand(TEXT("ssh admin@192.168.1.2"));OK&=IsTerminalPasswordInput();
    RunCommand(TEXT("IncorrectSecret"));OK&=!HasVerifiedSSH(TEXT("SW-A"),SwitchA)&&!Transcript.Contains(TEXT("IncorrectSecret"))&&!GetTerminalHistory().Contains(TEXT("IncorrectSecret"));
    VerifySSH();OK&=HasVerifiedSSH(TEXT("SW-A"),SwitchA)&&HasVerifiedSSH(TEXT("SW-B"),SwitchB)&&!Transcript.Contains(TEXT("Personale1"))&&!GetTerminalHistory().Contains(TEXT("Personale1"));
    RunChallengeTest();OK=OK&&ChallengeReady&&Phase==109;RebootChallenge();OK=OK&&!ChallengeReady&&Phase==109;
    Configure(SwitchA,TEXT("SW-A"),TEXT("192.168.1.2"),true);Configure(SwitchB,TEXT("SW-B"),TEXT("192.168.1.3"),true);ChallengeRebooted=false;VerifySSH();
    SwitchA.Execute(TEXT("conf t"));SwitchA.Execute(TEXT("username admin secret Nuova2"));
    RunChallengeTest();OK&=!ChallengeReady&&!HasVerifiedSSH(TEXT("SW-A"),SwitchA);
    SwitchA.Execute(TEXT("username admin secret Personale1"));SwitchA.Execute(TEXT("end"));
    RunChallengeTest();OK=OK&&ChallengeReady&&Phase==109;RebootChallenge();OK=OK&&Phase==110;ClosePage();
    Phase=4;SortingCable=TEXT("ethernet");OK=OK&&!SortCable(TEXT("console"))&&!Owned.Contains(TEXT("ethernet"));SortingCable.Empty();
    Phase=10;Completed.Remove(TEXT("mac-read"));Completed.Remove(TEXT("mac-table"));OK=OK&&!SubmitAnswer(TEXT("Gi1/0/1"))&&Phase==10;
    Phase=6;TerminalTarget=TEXT("LAPTOP");ExecutePC(TEXT("ping 192.168.1.11"));OK=OK&&Phase==9;ClosePage();
    OK=OK&&ExecutePC(TEXT("ping 300.1.1.1")).Contains(TEXT("non valido"));
    for(const TCHAR* Bad:{TEXT("192..168.1.11"),TEXT("192.168.1.11."),TEXT("-1.168.1.11"),TEXT("192.168.01.11")})
        OK&=ExecutePC(FString(TEXT("ping "))+Bad).Contains(TEXT("non valido"));
    for(auto* D:{L,PC,SW,Devices[TEXT("UPLINK")].Get()})for(const auto& P:D->GetPorts()){FString E;D->DisconnectPort(P.Id,E);}
    LearnedMACs.Empty();ArpCache.Empty();SimNodes.Empty();Owned.Empty();Owned.Add(TEXT("notebook"));Completed.Empty();Sorted.Empty();Phase=0;Page.Empty();
    // Session changes must preserve output and history without mixing PC and IOS.
    TerminalHistories.Empty();TerminalTranscripts.Empty();Transcript.Empty();TerminalTarget=TEXT("LAPTOP");
    RunCommand(TEXT("help"));const FString PCOutput=Transcript;
    SwitchTerminalSession(TEXT("SW1"));OK&=Transcript.IsEmpty()&&GetTerminalHistory().IsEmpty();
    Transcript=TEXT("Switch session evidence");SwitchTerminalSession(TEXT("LAPTOP"));
    OK&=Transcript==PCOutput&&GetTerminalHistory().Num()==1;
    OK&=CompleteCommand(TEXT("ip"))==TEXT("ip")&&CompleteCommand(TEXT("ip n"))==TEXT("ip neigh ")&&CompleteCommand(TEXT("")).IsEmpty();
    RunCommand(TEXT("clear"));OK&=Transcript.IsEmpty()&&GetTerminalHistory().Num()==2;
    SwitchTerminalSession(TEXT("SW1"));OK&=Transcript==TEXT("Switch session evidence");
    SwitchTerminalSession(TEXT("LAPTOP"));
    OK&=ExecutePC(TEXT("ping 127.0.0.1")).Contains(TEXT("Reply from"));
    TerminalHistories.Empty();TerminalTranscripts.Empty();Transcript.Empty();
    OK&=RunPersistenceSelfTest();OK&=RunTerminalSelfTest();
    UE_LOG(LogTemp,Display,TEXT("CLARA_STORY_SELFTEST_%s: host path, MAC learning, console, optical module and locked challenge."),OK?TEXT("PASS"):TEXT("FAIL"));return OK;
}

bool AClaraStoryDirector::DevRoundTripCheckpoint(const FString& Action)
{
    FString Tour;if(!FParse::Value(FCommandLine::Get(),TEXT("ClaraTour="),Tour)||Persist)return false;
    if(Action==TEXT("load"))return DevCheckpoint.IsValid()&&RestoreProgress(DevCheckpoint);
    const auto Snapshot=CaptureProgress();FString Text;TSharedPtr<FJsonObject> Parsed;
    if(Controller.IsValid())if(const auto* Camera=Controller->FindComponentByClass<UClaraNarrativeCamera>();Camera&&Camera->IsActive())
    {
        const auto Player=Snapshot->GetObjectField(TEXT("player"));const FVector Resume=Camera->GetResumeTransform().GetLocation();
        if(!FMath::IsNearlyEqual(Player->GetNumberField(TEXT("x")),Resume.X,.01)||!FMath::IsNearlyEqual(Player->GetNumberField(TEXT("y")),Resume.Y,.01)||
            !FMath::IsNearlyEqual(Player->GetNumberField(TEXT("yaw")),double(Camera->GetResumeLook().Yaw),.01))return false;
    }
    if(!FJsonSerializer::Serialize(Snapshot,TJsonWriterFactory<TCHAR>::Create(&Text))||!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Text),Parsed))return false;
    if(Action==TEXT("save")){DevCheckpoint=Parsed;return true;}
    return Action==TEXT("roundtrip")&&RestoreProgress(Parsed);
}

bool AClaraStoryDirector::RunPersistenceSelfTest()
{
    const auto Original=CaptureProgress();
    auto RoundTrip=[this]()
    {
        FString Text;auto Writer=TJsonWriterFactory<TCHAR>::Create(&Text);TSharedPtr<FJsonObject> Parsed;
        return FJsonSerializer::Serialize(CaptureProgress(),Writer)&&FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Text),Parsed)&&RestoreProgress(Parsed);
    };
    Phase=11;Owned.Add(TEXT("badge"));Owned.Add(TEXT("laptop"));SetupPuzzle(TEXT("osi"));Puzzle.Swap(0,1);
    const auto Order=Puzzle;ArpCache.Empty();bool OK=RoundTrip()&&Phase==11&&Puzzle==Order&&ArpCache.IsEmpty();
    Phase=104;SubnetStep=3;Completed.Add(TEXT("day2-host-read"));OK&=RoundTrip()&&Phase==104&&SubnetStep==3&&Completed.Contains(TEXT("day2-host-read"));
    Phase=109;SetupChallenge();SimLinks.Add(Edge(TEXT("LAPTOP"),TEXT("SW-A")));SetSimHostIP(TEXT("PC3"),TEXT("192.168.1.12"));
    SwitchA.Execute(TEXT("enable"));SwitchA.Execute(TEXT("conf t"));SwitchA.Execute(TEXT("hostname PIANO-SALVATO"));SwitchA.Execute(TEXT("end"));SwitchA.Execute(TEXT("write memory"));
    SwitchA.Execute(TEXT("conf t"));SwitchA.Execute(TEXT("hostname PIANO-IN-CORSO"));ConsoleTarget=TEXT("SW-A");
    OK&=RoundTrip()&&Phase==109&&SimNodes.Num()==6&&SimHasLink(TEXT("LAPTOP"),TEXT("SW-A"))&&ConsoleTarget==TEXT("SW-A")&&
        SwitchA.GetHostname()==TEXT("PIANO-IN-CORSO")&&!SwitchA.IsStartupSaved()&&GetSimNodes().ContainsByPredicate([](const auto& N){return N.Id==TEXT("PC3")&&N.IP==TEXT("192.168.1.12");});
    Completed.Add(TEXT("ssh-verified:SW-A:")+SwitchA.GetRunningConfigurationFingerprint());
    PendingSSHUser=TEXT("admin");PendingSSHAddress=TEXT("192.168.1.2");
    OK&=RoundTrip()&&PendingSSHUser.IsEmpty()&&HasVerifiedSSH(TEXT("SW-A"),SwitchA);
    SwitchA.ReloadFromStartup();OK&=SwitchA.GetHostname()==TEXT("PIANO-SALVATO")&&!HasVerifiedSSH(TEXT("SW-A"),SwitchA);
    Phase=15;Page=TEXT("dialogue");DialogueQueue={{TEXT("Luca"),TEXT("Controllo ripresa")}};DialogueIndex=0;AfterDialogue=16;Choices.Empty();
    OK&=RoundTrip()&&Page==TEXT("dialogue")&&AfterDialogue==16;ContinueDialogue();OK&=Phase==16;
    OK&=RoundTrip()&&Phase==16&&Page==TEXT("report")&&!ScreenOpen&&GetOfficeClockTime()==TEXT("18:00");
    const auto CompletedDay=CaptureProgress();
    BeginNextDayTransition();BeginNextDayTransition();
    OK&=IsDayTransitionActive()&&Phase==16&&Page==TEXT("daytransition")&&CaptureProgress()->GetNumberField(TEXT("phase"))==16;
    OpenNotebook();OpenInventory();OpenLaptopScreen();
    OK&=Page==TEXT("daytransition")&&!ScreenOpen;
    OK&=RestoreProgress(CompletedDay)&&Phase==16&&!IsDayTransitionActive()&&Page==TEXT("report");
    BeginNextDayTransition();ClosePage(); // Esc skips the sequence, never the day's work.
    OK&=Phase==100&&!IsDayTransitionActive()&&GetOfficeClockTime()==TEXT("09:00");
    FinishNextDayTransition();BeginNextDayTransition();OK&=Phase==100&&!IsDayTransitionActive();
    OK&=RestoreProgress(CompletedDay);BeginNextDayTransition();TickDayTransition(2.f);
    OK&=Phase==16&&GetOfficeClockTime()==TEXT("18:00");
    TickDayTransition(10.f);OK&=Phase==16&&GetOfficeClockTime()==TEXT("09:00");
    TickDayTransition(1.f);
    OK&=Phase==100&&!IsDayTransitionActive()&&GetOfficeClockTime()==TEXT("09:00");
    OK&=RoundTrip()&&Phase==100&&!IsDayTransitionActive();
    Phase=110;OK&=RoundTrip()&&Phase==110&&Page==TEXT("report");
    auto Invalid=MakeShared<FJsonObject>();Invalid->SetNumberField(TEXT("version"),999);OK&=!RestoreProgress(Invalid)&&Phase==110;
    OK&=RestoreProgress(Original);
    UE_LOG(LogTemp,Display,TEXT("CLARA_PERSISTENCE_SELFTEST_%s: puzzle, subnet, Day 2 configuration, topology, pending dialogue and final report."),OK?TEXT("PASS"):TEXT("FAIL"));
    return OK;
}

bool AClaraStoryDirector::IsDeviceInspectionOpen()const{return Controller.IsValid()&&Controller->IsDeviceOpen();}
