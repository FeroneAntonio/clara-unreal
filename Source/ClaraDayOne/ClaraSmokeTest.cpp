#include "ClaraSmokeTest.h"
#include "ClaraPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

bool RunClaraDayOneSmokeTest(UWorld* World)
{
    auto Require = [](bool bCondition, const TCHAR* Description)
    {
        if (!bCondition) UE_LOG(LogTemp, Error, TEXT("CLARA_SMOKE_FAIL: %s"), Description);
        return bCondition;
    };
    if (!Require(World != nullptr, TEXT("World disponibile"))) return false;
    AClaraPlayer* Player = Cast<AClaraPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
    if (!Require(Player != nullptr, TEXT("Pawn del giocatore reale generato dal GameMode"))) return false;
    AClaraPlayerController* PC = Cast<AClaraPlayerController>(Player->GetController());
    if (!Require(PC != nullptr, TEXT("Controller del giocatore disponibile"))) return false;
    auto Find = [World](const TCHAR* Tag) -> AActor*
    {
        for (TActorIterator<AActor> It(World); It; ++It) if (It->ActorHasTag(FName(Tag))) return *It;
        return nullptr;
    };
    AActor* Reception = Find(TEXT("Clara.Reception"));
    AActor* Mentor = Find(TEXT("Clara.Mentor"));
    AActor* Ticket = Find(TEXT("Clara.Ticket"));
    AActor* Cable = Find(TEXT("Clara.Cable"));
    AActor* Socket = Find(TEXT("Clara.Socket"));
    AActor* Service = Find(TEXT("Clara.Service"));
    if (!Require(Reception && Mentor && Ticket && Cable && Socket && Service, TEXT("Sei attori interattivi presenti nella scena"))) return false;
    if (!Require(Player->GetStage() == EClaraStage::Reception, TEXT("Inizio al colloquio"))) return false;

    Player->InteractWithActor(Reception);
    if (!Require(Player->GetChoices().Num() == 4, TEXT("Quattro preferenze, compresa inesperienza"))) return false;
    Player->Choose(4);
    Player->ContinueDialogue();
    if (!Require(Player->GetStage() == EClaraStage::Welcome, TEXT("Nessuna esclusione per inesperienza"))) return false;
    Player->InteractWithActor(Mentor);
    Player->ContinueDialogue();
    Player->InteractWithActor(Ticket);
    Player->ContinueDialogue();
    if (!Require(Player->GetStage() == EClaraStage::Cabling, TEXT("Ticket porta all'osservazione fisica"))) return false;

    const FString Disconnected = Player->RunCommand(TEXT("ping -c 4 192.168.10.20"));
    if (!Require(Disconnected.Contains(TEXT("nessuna risposta")) && Player->GetStage() == EClaraStage::Cabling, TEXT("Cavo scollegato non genera successo"))) return false;
    Player->InteractWithActor(Cable);
    if (!Require(Player->IsHoldingCable() && !Player->IsCableConnected(), TEXT("Estremita' presa, collegamento ancora interrotto"))) return false;
    Player->StowCable();
    if (!Require(Player->IsCableStored() && !Player->IsHoldingCable() && Player->HasCableItem() && !Player->IsCableConnected(), TEXT("Aggancio alla cintura conserva il cavo senza creare un link"))) return false;
    Player->EquipCable();
    if (!Require(Player->IsHoldingCable() && !Player->IsCableStored() && Player->HasCableItem(), TEXT("Il connettore torna in mano dalla cintura"))) return false;
    Player->DropCable();
    if (!Require(!Player->IsHoldingCable() && !Player->IsCableConnected() && !Player->HasCableItem(), TEXT("Posa reversibile senza collegamento fantasma"))) return false;
    Player->InteractWithActor(Cable);
    Player->InteractWithActor(Socket);
    if (!Require(Player->IsCableConnected() && !Player->IsHoldingCable(), TEXT("Inserimento richiede il connettore in mano"))) return false;
    if (!Require(Player->GetStage() == EClaraStage::Meaning, TEXT("Collegamento introduce significato della prova"))) return false;
    Player->Choose(2);
    if (!Require(Player->GetStage() == EClaraStage::Meaning && Player->GetChoices().Num() == 3, TEXT("Risposta sbagliata non avanza la comprensione"))) return false;
    Player->Choose(1);
    Player->ContinueDialogue();
    Player->OpenNotebook();
    if (!Require(Player->GetStage() == EClaraStage::Guide, TEXT("Aprire il taccuino non equivale a leggere lo strumento"))) return false;
    Player->MarkProbeGuideRead();
    PC->CloseUtilityPanel();
    if (!Require(Player->GetStage() == EClaraStage::Terminal, TEXT("Guida disponibile prima della sintassi"))) return false;

    Player->RunCommand(TEXT("ping 192.168.10.999"));
    Player->RunCommand(TEXT("ping 192.168.10.99"));
    if (!Require(Player->GetStage() == EClaraStage::Terminal, TEXT("Indirizzo invalido o estraneo non soddisfa il ticket"))) return false;
    Player->RunCommand(TEXT("ping -c 4 192.168.10.20"));
    if (!Require(Player->GetStage() == EClaraStage::Service, TEXT("Ping riuscito richiede ancora verifica applicativa"))) return false;

    Player->InteractWithActor(Socket);
    if (!Require(!Player->IsCableConnected() && Player->IsHoldingCable(), TEXT("Sgancio interrompe il collegamento"))) return false;
    const FString AfterDetach = Player->RunCommand(TEXT("ping 192.168.10.20"));
    Player->OpenCourseMaterial();
    if (!Require(AfterDetach.Contains(TEXT("nessuna risposta")) && Player->GetStage() == EClaraStage::Service, TEXT("Prova precedente non abilita il servizio mentre il cavo e' staccato"))) return false;
    Player->InteractWithActor(Socket);
    Player->RunCommand(TEXT("ping 192.168.10.20"));
    Player->OpenCourseMaterial();
    Player->ContinueDialogue();
    if (!Require(Player->GetStage() == EClaraStage::Transfer, TEXT("Servizio separato e nuovo incarico"))) return false;
    Player->RunCommand(TEXT("ping 192.168.10.20"));
    if (!Require(Player->GetStage() == EClaraStage::Transfer, TEXT("Riutilizzare il vecchio indirizzo non completa il secondo ticket"))) return false;
    Player->RunCommand(TEXT("ping -c 4 192.168.10.30"));
    if (!Require(Player->GetStage() == EClaraStage::Debrief, TEXT("Nuovo destinatario richiede rapporto"))) return false;
    Player->InteractWithActor(Mentor);
    Player->Choose(1);
    if (!Require(Player->GetStage() == EClaraStage::Debrief && Player->GetChoices().Num() == 3, TEXT("Conclusione infondata non chiude il rapporto"))) return false;
    Player->Choose(2);
    Player->ContinueDialogue();
    if (!Require(Player->GetStage() == EClaraStage::SwitchBrief, TEXT("Il rapporto LAN introduce l'incarico switch senza completare il giorno"))) return false;
    Player->InteractWithActor(Mentor);
    Player->ContinueDialogue();
    if (!Require(Player->GetStage() == EClaraStage::SwitchConfig, TEXT("Luca assegna la configurazione dello switch"))) return false;
    AActor* ConsoleCable = Find(TEXT("Clara.ConsoleCable"));
    AActor* ConsolePort = Find(TEXT("Clara.ConsolePort"));
    AActor* ConsoleLaptopPort = Find(TEXT("Clara.ConsoleLaptopPort"));
    if (!Require(ConsoleCable && ConsolePort && ConsoleLaptopPort, TEXT("Cavo console e due porte reali della scena disponibili"))) return false;
    Player->InteractWithActor(ConsolePort);
    if (!Require(!Player->HasConsoleCable() && !Player->IsConsoleConnected(), TEXT("Una porta non produce un cavo o una connessione fantasma"))) return false;
    Player->InteractWithActor(ConsoleCable);
    if (!Require(Player->HasConsoleCable() && Player->IsHoldingConsole(), TEXT("Cavo console raccolto, distinto da Ethernet"))) return false;
    Player->InteractWithActor(ConsolePort);
    if (!Require(!Player->IsConsoleConnected(), TEXT("Un solo capo console non basta"))) return false;
    Player->InteractWithActor(ConsoleLaptopPort);
    if (!Require(Player->IsConsoleConnected() && !Player->IsHoldingConsole(), TEXT("Console collegata a switch e portatile"))) return false;
    Player->InteractWithActor(ConsoleLaptopPort);
    if (!Require(!Player->IsConsoleConnected(), TEXT("Sgancio di un capo interrompe la console"))) return false;
    Player->InteractWithActor(ConsoleLaptopPort);
    if (!Require(Player->IsConsoleConnected(), TEXT("Ricollegamento console reversibile"))) return false;
    PC->OpenTerminal(true);
    PC->ExecuteSwitchCommand(TEXT("enable"));
    PC->ExecuteSwitchCommand(TEXT("conf t"));
    PC->ExecuteSwitchCommand(TEXT("hostname WRONG-NAME"));
    PC->ExecuteSwitchCommand(TEXT("end"));
    if (!Require(PC->ExecuteSwitchCommand(TEXT("show run")).Contains(TEXT("hostname WRONG-NAME")) && Player->GetStage() == EClaraStage::SwitchConfig,
        TEXT("Hostname valido diverso dal ticket resta configurabile ma non completa l'incarico"))) return false;
    auto Configure = [PC]()
    {
        for (const TCHAR* Command : {TEXT("enable"), TEXT("conf t"), TEXT("hostname AULA-01"), TEXT("vlan 10"), TEXT("exit"),
            TEXT("int vlan10"), TEXT("ip address 192.168.10.2 255.255.255.0"), TEXT("no sh"), TEXT("end")})
            PC->ExecuteSwitchCommand(Command);
    };
    Configure();
    if (!Require(!PC->IsSwitchMissionConfigured() && Player->GetStage() == EClaraStage::SwitchConfig, TEXT("Configurazione corretta ma non salvata non basta"))) return false;
    PC->ExecuteSwitchCommand(TEXT("copy run start"));
    if (!Require(!PC->IsSwitchMissionConfigured() && Player->GetStage() == EClaraStage::SwitchConfig, TEXT("La copia richiede ancora conferma del nome"))) return false;
    PC->ExecuteSwitchCommand(TEXT(""));
    if (!Require(PC->IsSwitchMissionConfigured() && Player->GetStage() == EClaraStage::SwitchReport, TEXT("Conferma vuota salva e sblocca il rapporto switch"))) return false;
    PC->ResetSwitchWithMentor();
    if (!Require(PC->IsSwitchMissionConfigured() && Player->GetStage() == EClaraStage::SwitchReport, TEXT("Prima richiesta reset non cancella silenziosamente il lavoro"))) return false;
    PC->ResetSwitchWithMentor();
    if (!Require(!PC->IsSwitchMissionConfigured() && Player->GetStage() == EClaraStage::SwitchConfig && Player->IsConsoleConnected(),
        TEXT("Reset confermato ripristina solo esercizio switch, preservando cavo console"))) return false;
    Configure();
    PC->ExecuteSwitchCommand(TEXT("write memory"));
    if (!Require(PC->IsSwitchMissionConfigured() && Player->GetStage() == EClaraStage::SwitchReport, TEXT("Riconfigurazione dopo reset e salvataggio immediato funzionano"))) return false;
    PC->CloseTerminal();
    Player->InteractWithActor(Mentor);
    Player->ContinueDialogue();
    if (!Require(Player->GetStage() == EClaraStage::Complete, TEXT("Luca conclude dopo il controllo della configurazione salvata"))) return false;
    UE_LOG(LogTemp, Display, TEXT("CLARA_SMOKE_PASS: colloquio, taccuino, inventario cintura/presa/posa, LAN, servizio e trasferimento; console a due capi, hostname errato valido, configurazione, startup con conferma, reset esplicito e rapporto switch. Backend esclusivamente locale; input UI/rendering/CML non verificati da questo test."));
    return true;
}
