#include "ClaraPlayer.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

FClaraProbeCommand ParseClaraProbe(const FString& Command)
{
    FClaraProbeCommand Result;
    TArray<FString> Parts;
    Command.TrimStartAndEnd().ParseIntoArrayWS(Parts);
    if (Parts.IsEmpty() || Parts[0] != TEXT("ping"))
    {
        Result.Error = TEXT("Strumento non riconosciuto. Scrivi help per cercare per obiettivo.");
        return Result;
    }
    if (Parts.Num() == 1)
    {
        Result.Error = TEXT("Manca il destinatario. Consulta l'indirizzo nella scheda del ticket.");
        return Result;
    }
    if (Parts.Num() == 2) Result.Destination = Parts[1];
    else if (Parts.Num() == 4 && Parts[1] == TEXT("-c") && Parts[2] == TEXT("4")) Result.Destination = Parts[3];
    else
    {
        Result.Error = TEXT("In questa postazione: ping INDIRIZZO oppure ping -c 4 INDIRIZZO. La prova locale e' limitata a quattro richieste.");
        return Result;
    }
    TArray<FString> Octets;
    Result.Destination.ParseIntoArray(Octets, TEXT("."), false);
    if (Octets.Num() != 4)
    {
        Result.Error = TEXT("Il destinatario deve essere un indirizzo IPv4 con quattro numeri separati da punti.");
        return Result;
    }
    for (const FString& Octet : Octets)
    {
        if (Octet.IsEmpty() || Octet.Len() > 3)
        {
            Result.Error = TEXT("Indirizzo IPv4 non valido: ogni parte deve essere un numero da 0 a 255.");
            return Result;
        }
        for (TCHAR Character : Octet)
        {
            if (!FChar::IsDigit(Character))
            {
                Result.Error = TEXT("Indirizzo IPv4 non valido: usa soltanto cifre e punti.");
                return Result;
            }
        }
        if (FCString::Atoi(*Octet) > 255 || (Octet.Len() > 1 && Octet[0] == TEXT('0')))
        {
            Result.Error = TEXT("Indirizzo IPv4 non valido: valori 0-255, senza zeri iniziali.");
            return Result;
        }
    }
    Result.bValid = true;
    return Result;
}

AClaraPlayer::AClaraPlayer()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(30, 88);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0, 0, 66));
    Camera->bUsePawnControlRotation = true;
    Camera->SetFieldOfView(82);
    GetCharacterMovement()->MaxWalkSpeed = 245;
    GetCharacterMovement()->BrakingDecelerationWalking = 1800;
    bUseControllerRotationYaw = true;
}

void AClaraPlayer::BeginPlay()
{
    Super::BeginPlay();
    Notify(TEXT("Benvenuto. Raggiungi il banco accoglienza e premi E per parlare con Alessia."));
}

void AClaraPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Toast.IsEmpty() && GetWorld()->GetTimeSeconds() > ToastUntil) Toast.Empty();
    if (GetActorLocation().Z < -500) SetActorLocation(FVector(140, 0, 120));
}

void AClaraPlayer::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"), this, &AClaraPlayer::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &AClaraPlayer::MoveRight);
    Input->BindAxis(TEXT("Turn"), this, &AClaraPlayer::Turn);
    Input->BindAxis(TEXT("LookUp"), this, &AClaraPlayer::LookUp);
}

bool AClaraPlayer::IsInputBlocked() const
{
    const auto* PC = Cast<AClaraPlayerController>(Controller);
    return HasDialogue() || (PC && (PC->IsTerminalOpen() || PC->IsPanelOpen()));
}
void AClaraPlayer::MoveForward(float Value)
{
    if (!IsInputBlocked() && !bInspectingPC && !bInspectingMonitor) AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value);
}
void AClaraPlayer::MoveRight(float Value)
{
    if (!IsInputBlocked() && !bInspectingPC && !bInspectingMonitor) AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value);
}
void AClaraPlayer::Turn(float Value)
{
    const auto* PC = Cast<AClaraPlayerController>(Controller);
    if (!IsInputBlocked() && !bInspectingPC && !bInspectingMonitor && PC && PC->IsLookEnabled()) AddControllerYawInput(Value);
}
void AClaraPlayer::LookUp(float Value)
{
    const auto* PC = Cast<AClaraPlayerController>(Controller);
    if (!IsInputBlocked() && !bInspectingPC && !bInspectingMonitor && PC && PC->IsLookEnabled()) AddControllerPitchInput(Value);
}

AActor* AClaraPlayer::GetFocusedActor() const
{
    if (!Camera || IsInputBlocked()) return nullptr;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ClaraInteraction), false, this);
    if (CableActor.IsValid() && (bHoldingCable || bCableStored)) Params.AddIgnoredActor(CableActor.Get());
    FVector Start = Camera->GetComponentLocation();
    FVector Direction = Camera->GetForwardVector();
    if (const auto* PC = Cast<AClaraPlayerController>(Controller); PC && PC->bShowMouseCursor)
    {
        float X = 0, Y = 0;
        if (!PC->GetMousePosition(X, Y) || !PC->DeprojectScreenPositionToWorld(X, Y, Start, Direction)) return nullptr;
    }
    GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * 380, ECC_Visibility, Params);
    AActor* Actor = Hit.GetActor();
    if (!Actor) return nullptr;
    for (const FName& Tag : Actor->Tags)
        if (Tag != TEXT("Clara.Office") && Tag.ToString().StartsWith(TEXT("Clara."))) return Actor;
    return nullptr;
}

FString AClaraPlayer::GetInteractionLabel() const
{
    const AActor* Actor = GetFocusedActor();
    if (!Actor) return FString();
    if (Actor->ActorHasTag(TEXT("Clara.Reception"))) return TEXT("E  Parla con Alessia · accoglienza");
    if (Actor->ActorHasTag(TEXT("Clara.Mentor"))) return TEXT("E  Contatta Luca · tutor reti");
    if (Actor->ActorHasTag(TEXT("Clara.Ticket"))) return TEXT("E  Leggi la richiesta del docente");
    if (Actor->ActorHasTag(TEXT("Clara.InspectPC"))) return bInspectingPC ? TEXT("E  Torna alla postazione") : TEXT("E  Esamina il retro del case");
    if (Actor->ActorHasTag(TEXT("Clara.Desktop"))) return TEXT("E  Usa il desktop del PC");
    if (Actor->ActorHasTag(TEXT("Clara.Cable"))) return bCableConnected ? TEXT("E  Sgancia il connettore Ethernet") : TEXT("E  Prendi il connettore Ethernet");
    if (Actor->ActorHasTag(TEXT("Clara.ConsoleCable"))) return Stage >= EClaraStage::SwitchConfig ? TEXT("E  Prendi il cavo console") : TEXT("E  Esamina il cavo console");
    if (Actor->ActorHasTag(TEXT("Clara.ConsolePort"))) return TEXT("E  Collega la console allo switch");
    if (Actor->ActorHasTag(TEXT("Clara.ConsoleLaptopPort"))) return TEXT("E  Collega la console al portatile");
    if (Actor->ActorHasTag(TEXT("Clara.SwitchTerminal"))) return TEXT("E  Apri la console dello switch");
    if (Actor->ActorHasTag(TEXT("Clara.Socket"))) return bCableConnected ? TEXT("E  Scollega Ethernet dal PC") : (bHoldingCable ? TEXT("E  Inserisci il connettore nella porta Ethernet") : TEXT("E  Esamina la porta Ethernet"));
    if (Actor->ActorHasTag(TEXT("Clara.Guide"))) return TEXT("E  Apri il taccuino");
    if (Actor->ActorHasTag(TEXT("Clara.Transfer"))) return TEXT("E  Leggi il ticket della reception");
    if (Actor->ActorHasTag(TEXT("Clara.Service"))) return TEXT("E  Apri le dispense · T  Terminale");
    if (Actor->ActorHasTag(TEXT("Clara.Terminal"))) return TEXT("E  Usa il terminale");
    return TEXT("E  Esamina");
}

void AClaraPlayer::Say(const FString& Name, const FString& Text, EClaraStage Next)
{
    if (bInspectingPC) EndPCInspection();
    Speaker = Name;
    Dialogue = Text;
    Choices.Empty();
    StageAfterDialogue = Next;
    GetCharacterMovement()->StopMovementImmediately();
}

void AClaraPlayer::Notify(const FString& Text)
{
    Toast = Text;
    ToastUntil = GetWorld()->GetTimeSeconds() + 8;
}

void AClaraPlayer::ContinueDialogue()
{
    if (!HasDialogue() || !Choices.IsEmpty()) return;
    Dialogue.Empty();
    Stage = StageAfterDialogue;
    HintLevel = 0;
}

void AClaraPlayer::ClosePanel()
{
    if (HasDialogue())
    {
        if (Choices.IsEmpty()) ContinueDialogue();
        else Notify(TEXT("Scegli una risposta con i tasti 1-4. Puoi chiedere un indizio con H."));
    }
}

void AClaraPlayer::Interact()
{
    if (HasDialogue()) { ContinueDialogue(); return; }
    InteractWithActor(GetFocusedActor());
}

void AClaraPlayer::InteractWithActor(AActor* Actor)
{
    if (!Actor || IsInputBlocked()) return;
    if (Actor->ActorHasTag(TEXT("Clara.InspectPC"))) { if (bInspectingPC) EndPCInspection(); else BeginPCInspection(); return; }
    if (Actor->ActorHasTag(TEXT("Clara.Desktop"))) { if (auto* PC = Cast<AClaraPlayerController>(Controller)) PC->OpenDesktop(); return; }
    if (Actor->ActorHasTag(TEXT("Clara.Switch")))
    {
        if (Stage >= EClaraStage::SwitchConfig) { RequestSwitchTerminal(); return; }
        Say(TEXT("Ispezione dello switch"), TEXT("Le porte Ethernet collegano i dispositivi della LAN. Le prese SFP servono agli uplink; la console serve a gestire l'apparato. Segui il cavo dati gia' inserito nella porta 1 fino alla postazione del docente. In questa build l'apparato e' un modello didattico locale, non un dispositivo CML avviato."), Stage);
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.Reception")))
    {
        if (Stage == EClaraStage::Reception)
        {
            Say(TEXT("Alessia"), TEXT("Ciao, sono Alessia. Oggi lavorerai con Luca. Prima di darti il badge: che cosa ti incuriosisce di piu'?"), Stage);
            Choices = {TEXT("Reti: collegare e far comunicare i sistemi"), TEXT("Cyber: osservare e proteggere i servizi"), TEXT("Entrambi i percorsi"), TEXT("Non conosco ancora la differenza")};
        }
        else Say(TEXT("Alessia"), Stage == EClaraStage::Complete ? TEXT("Il corso e' iniziato. Grazie per il rapporto: domani ripartiremo da quello che hai scoperto.") : TEXT("Luca ti aspetta al laboratorio reti. Guarda le targhette sulle postazioni."), Stage);
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.Mentor")))
    {
        if (Stage == EClaraStage::Welcome)
            Say(TEXT("Luca"), TEXT("Eccoti. Il docente dell'aula non riesce ad aprire le dispense. Ho lasciato la richiesta sul banco: dagli un'occhiata, poi vediamo insieme."), EClaraStage::Ticket);
        else if (Stage == EClaraStage::Debrief)
        {
            Say(TEXT("Luca · rapporto di intervento"), TEXT("Prima di chiudere: quale rapporto descrive le prove che hai fatto?"), Stage);
            Choices = {TEXT("Il LED acceso dimostra che tutte le applicazioni funzionano."), TEXT("Ho collegato Ethernet, verificato una risposta e aperto le dispense."), TEXT("Il ping dimostra che la rete e' sicura.")};
        }
        else if (Stage == EClaraStage::SwitchBrief)
            Say(TEXT("Luca"), TEXT("Ora dobbiamo riconoscere e gestire lo switch. Collega il cavo CONSOLE al portatile e allo switch. L'incarico e' nel taccuino: nome AULA-01, VLAN 10, indirizzo 192.168.10.2 con maschera 255.255.255.0. Alla fine salva. La guida e l'aiuto della console restano a disposizione."), EClaraStage::SwitchConfig);
        else if (Stage == EClaraStage::SwitchConfig || Stage == EClaraStage::SwitchReport)
        {
            if (auto* PC = Cast<AClaraPlayerController>(Controller))
                Say(TEXT("Luca"), PC->GetSwitchMentorFeedback(), PC->IsSwitchMissionConfigured() ? EClaraStage::Complete : EClaraStage::SwitchConfig);
        }
        else ShowHint();
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.Ticket")))
    {
        if (Stage == EClaraStage::Reception || Stage == EClaraStage::Welcome)
        { Notify(TEXT("Prima ritira il badge alla reception e conosci Luca.")); return; }
        Say(TEXT("Ticket #001 · aula formazione"), TEXT("Le dispense non si aprono dal PC docente. In reception il materiale e' disponibile. Destinatario delle dispense: 192.168.10.20. Confronta le postazioni e cerca una prova utile. Nessuna fretta: il laboratorio e' didattico."), Stage == EClaraStage::Ticket ? EClaraStage::Cabling : Stage);
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.ConsoleCable")))
    {
        if (Stage >= EClaraStage::SwitchConfig)
        {
            if (bHoldingCable) StowCable();
            bHasConsoleCable = true;
            bHoldingConsole = !IsConsoleConnected();
            Notify(TEXT("Cavo console raccolto. Collega un capo alla presa CONSOLE e l'altro alla USB del portatile."));
            return;
        }
        Say(TEXT("Etichetta del cavo"), TEXT("CONSOLE · gestione dell'apparato. Un connettore simile non rende questo cavo Ethernet. Oggi cerchiamo il percorso dei dati; useremo la console quando dovremo configurare il dispositivo."), Stage);
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.ConsolePort")) || Actor->ActorHasTag(TEXT("Clara.ConsoleLaptopPort")))
    {
        if (!bHasConsoleCable) { Notify(TEXT("Serve il cavo console azzurro, sul banco vicino al portatile.")); return; }
        if (Actor->ActorHasTag(TEXT("Clara.ConsolePort"))) bConsoleAtSwitch = !bConsoleAtSwitch;
        else bConsoleAtLaptop = !bConsoleAtLaptop;
        bHoldingConsole = !IsConsoleConnected();
        Notify(IsConsoleConnected() ? TEXT("Collegamento console pronto. Clicca sullo schermo del portatile.") : TEXT("Un capo e' libero. Collega entrambe le estremita': switch e portatile."));
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.SwitchTerminal"))) { RequestSwitchTerminal(); return; }
    if (Actor->ActorHasTag(TEXT("Clara.Cable")) || Actor->ActorHasTag(TEXT("Clara.Socket")))
    {
        if (Stage < EClaraStage::Cabling) { Notify(TEXT("Leggi prima il ticket: osserva che cosa manca al docente.")); return; }
        if (bCableConnected)
        {
            bCableConnected = false;
            bHoldingCable = true;
            if (CableActor.IsValid())
            {
                CableActor->SetActorEnableCollision(false);
                CableActor->AttachToComponent(Camera, FAttachmentTransformRules::KeepWorldTransform);
                CableActor->SetActorRelativeLocation(FVector(55, 22, -22));
            }
            Notify(TEXT("Connettore sganciato. Nel modello locale il collegamento e' interrotto. Q posa il cavo."));
            return;
        }
        if (Actor->ActorHasTag(TEXT("Clara.Socket")))
        {
            if (bHoldingConsole) { Notify(TEXT("Questo e' il cavo console: non trasporta i dati Ethernet. Prendi l'estremita' blu del cavo che arriva dalla porta 1 dello switch.")); return; }
            if (!bHoldingCable) { Say(TEXT("Porta Ethernet del PC"), TEXT("La porta dati e' libera. Segui il cavo che arriva dall'armadio: una delle sue estremita' non e' inserita."), Stage); return; }
            bHoldingCable = false;
            bCableConnected = true;
            if (CableActor.IsValid())
            {
                CableActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
                CableActor->SetActorLocation(Actor->GetActorLocation());
                CableActor->SetActorRotation(Actor->GetActorRotation());
                CableActor->SetActorEnableCollision(false);
            }
            if (Stage == EClaraStage::Cabling)
            {
                Stage = EClaraStage::Meaning;
                Say(TEXT("Luca"), TEXT("Bene, adesso il cavo e' inserito. Se chiedessimo una risposta all'altro computer e ci rispondesse, che cosa avremmo scoperto?"), Stage);
                Choices = {TEXT("Il dispositivo ha risposto a questa prova."), TEXT("Tutte le applicazioni funzionano."), TEXT("La rete e' sicuramente protetta.")};
            }
            else Notify(TEXT("Collegamento ripristinato nel modello locale. Ripeti la verifica che ti serve."));
            return;
        }
        if (!bHoldingCable)
        {
            CableActor = Actor;
            CableRestTransform = Actor->GetActorTransform();
            bHoldingCable = true;
            Actor->SetActorEnableCollision(false);
            Actor->AttachToComponent(Camera, FAttachmentTransformRules::KeepWorldTransform);
            Actor->SetActorRelativeLocation(FVector(55, 22, -22));
            Actor->SetActorRelativeRotation(FRotator::ZeroRotator);
            Notify(TEXT("Connettore Ethernet in mano. Cerca la porta libera sul retro del PC; E collega, Q posa."));
        }
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.Guide"))) { OpenNotebook(); return; }
    if (Actor->ActorHasTag(TEXT("Clara.Transfer")))
    {
        Say(TEXT("Ticket #002 · reception"), TEXT("Il dispositivo delle prenotazioni ha indirizzo 192.168.10.30. Puoi verificare se risponde? Scegli lo strumento: il taccuino resta disponibile."), Stage);
        return;
    }
    if (Actor->ActorHasTag(TEXT("Clara.Terminal"))) { RequestTerminal(); return; }
    if (Actor->ActorHasTag(TEXT("Clara.Service")))
    {
        OpenCourseMaterial();
    }
}

void AClaraPlayer::OpenCourseMaterial()
{
    if (IsInputBlocked()) return;
    if (Stage < EClaraStage::Cabling) { Notify(TEXT("Le dispense non si aprono. Leggi il ticket sul banco.")); return; }
    if (!bCableConnected) { Notify(TEXT("Materiale non disponibile nel modello locale: collegamento dati interrotto. Quale confronto puoi fare?")); return; }
    if (!bFirstPing) { Notify(TEXT("Prima raccogli una prova di risposta dal terminale del desktop.")); return; }
    bServiceOpened = true;
    Say(TEXT("Dispense · anteprima del servizio locale"), TEXT("CLARA · Lezione introduttiva: osservare, formulare un'ipotesi, verificare. Il materiale e' disponibile nel modello locale. Il ping verificava una risposta; aprire questo documento verifica separatamente la richiesta. Ora la reception chiede una prova sul destinatario 192.168.10.30."), Stage <= EClaraStage::Service ? EClaraStage::Transfer : Stage);
}

void AClaraPlayer::ReadCurrentTicket()
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("Clara.Ticket"))) { InteractWithActor(*It); return; }
}

void AClaraPlayer::BeginPCInspection()
{
    if (bInspectingPC || IsInputBlocked()) return;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(TEXT("Clara.Socket"))) continue;
        const FVector Target = It->GetActorLocation();
        if (FVector::DistSquared(Target, GetActorLocation()) > FMath::Square(380.f)) return;
        bInspectingPC = true;
        SavedCameraLocation = Camera->GetRelativeLocation();
        SavedViewRotation = GetControlRotation();
        Camera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        Camera->SetWorldLocation(Target + FVector(-110, -55, 8));
        Controller->SetControlRotation((Target - Camera->GetComponentLocation()).Rotation());
        GetCharacterMovement()->StopMovementImmediately();
        Notify(TEXT("Retro del case. La presa RETE ha otto contatti e due piccoli LED. Esc torna alla postazione."));
        return;
    }
}

void AClaraPlayer::EndPCInspection()
{
    if (!bInspectingPC) return;
    bInspectingPC = false;
    Camera->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
    Camera->SetRelativeLocation(SavedCameraLocation);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    if (Controller) Controller->SetControlRotation(SavedViewRotation);
}

void AClaraPlayer::BeginMonitorInspection(FVector ScreenPosition, bool bLaptop)
{
    if (bInspectingMonitor) return;
    if (bInspectingPC) EndPCInspection();
    bInspectingMonitor = true;
    SavedMonitorCameraLocation = Camera->GetRelativeLocation();
    SavedMonitorViewRotation = GetControlRotation();
    SavedMonitorFieldOfView = Camera->FieldOfView;
    Camera->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Camera->SetWorldLocation(ScreenPosition + FVector(bLaptop ? -44 : -75, 0, 1));
    Camera->SetFieldOfView(55);
    if (Controller) Controller->SetControlRotation((ScreenPosition - Camera->GetComponentLocation()).Rotation());
    GetCharacterMovement()->StopMovementImmediately();
}

void AClaraPlayer::EndMonitorInspection()
{
    if (!bInspectingMonitor) return;
    bInspectingMonitor = false;
    Camera->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
    Camera->SetRelativeLocation(SavedMonitorCameraLocation);
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
    Camera->SetFieldOfView(SavedMonitorFieldOfView);
    if (Controller) Controller->SetControlRotation(SavedMonitorViewRotation);
}

void AClaraPlayer::Choose(int32 Choice)
{
    if (Choices.IsEmpty() || Choice < 1 || Choice > Choices.Num()) return;
    if (Stage == EClaraStage::Reception)
    {
        Career = Choices[Choice - 1];
        Say(TEXT("Alessia"), TEXT("Ecco il badge. Luca ti aspetta in laboratorio; Jonny lo conoscerai piu' tardi. Per il percorso hai tempo di decidere strada facendo."), EClaraStage::Welcome);
    }
    else if (Stage == EClaraStage::Meaning)
    {
        if (Choice != 1) { Notify(TEXT("Una singola prova non verifica ogni programma e non certifica la sicurezza. Cerca la conclusione piu' limitata.")); return; }
        Say(TEXT("Luca"), TEXT("Esatto. Nel taccuino cerca 'verificare se un dispositivo risponde'. Troverai il nome dello strumento e il suo significato. Premi N dopo questo dialogo."), EClaraStage::Guide);
    }
    else if (Stage == EClaraStage::Debrief)
    {
        if (Choice != 2) { Notify(TEXT("Il rapporto deve distinguere collegamento, risposta ICMP e servizio. Una prova non dimostra tutto.")); return; }
        if (!bServiceOpened || !bFirstPing || !bTransferPing) return;
        Say(TEXT("Luca"), TEXT("Le dispense funzionano e il rapporto e' pronto. C'e' ancora una cosa: dobbiamo preparare lo switch per chi gestira' l'aula. Vieni al portatile e vediamo la console."), EClaraStage::SwitchBrief);
    }
}

void AClaraPlayer::OpenNotebook()
{
    if (HasDialogue()) return;
    if (auto* PC = Cast<AClaraPlayerController>(Controller)) PC->OpenNotebookPanel();
}

void AClaraPlayer::MarkProbeGuideRead()
{
    bGuideRead = true;
    if (Stage == EClaraStage::Guide) Stage = EClaraStage::Terminal;
}

void AClaraPlayer::ShowHint()
{
    TArray<FString> Hints;
    switch (Stage)
    {
    case EClaraStage::Reception: Hints = {TEXT("Raggiungi il banco accoglienza. Mira alla targa di Alessia e premi E.")}; break;
    case EClaraStage::Welcome: Hints = {TEXT("Il punto tutor di Luca e' nel laboratorio. Cerca il contrassegno RETI e premi E.")}; break;
    case EClaraStage::Ticket: Hints = {TEXT("Sul banco c'e' la richiesta dell'aula: leggila con E.")}; break;
    case EClaraStage::Cabling: Hints = {TEXT("Confronta il percorso del PC con una connessione completa."), TEXT("Guarda dietro il PC: una porta dati e' libera."), TEXT("Segui il cavo Ethernet dall'armadio e prendi la sua estremita' libera."), TEXT("Con il connettore in mano, mira alla porta Ethernet sul retro del PC e premi E. La console serve a un altro scopo.")}; break;
    case EClaraStage::Meaning: Hints = {TEXT("Cerca la conclusione che riguarda soltanto la prova eseguita, senza estenderla a sicurezza o applicazioni.")}; break;
    case EClaraStage::Guide: Hints = {TEXT("Premi N. Cerca per intenzione: verificare se un dispositivo risponde.")}; break;
    case EClaraStage::Terminal: Hints = {TEXT("Apri la guida con N e recupera il destinatario dal ticket."), TEXT("Avvicinati al monitor e premi T. Il terminale accetta help."), TEXT("La prova usa azione, limite e destinatario: ping -c 4 192.168.10.20.")}; break;
    case EClaraStage::Service: Hints = {TEXT("Ora clicca Dispense nel desktop del PC. Il successo del ping e' solo una delle verifiche.")}; break;
    case EClaraStage::Transfer: Hints = {TEXT("Il nuovo ticket indica 192.168.10.30. Quale strumento hai appena usato?"), TEXT("T apre il terminale vicino al monitor; N conserva la guida. Cambia il destinatario.")}; break;
    case EClaraStage::Debrief: Hints = {TEXT("Torna da Luca e ricostruisci sintomo, intervento e prove senza attribuire al ping risultati che non verifica.")}; break;
    case EClaraStage::SwitchBrief: Hints = {TEXT("Luca ha un secondo incarico: parla con lui vicino al rack.")}; break;
    case EClaraStage::SwitchConfig: Hints = {TEXT("Collega il cavo console fra switch e portatile. Il taccuino contiene l'incarico."), TEXT("Nella console ? mostra l'aiuto e Tab completa le parole. Parti leggendo il prompt."), TEXT("Puoi chiedere a Luca una verifica dal pulsante nella console. Il ripristino ti fa ricominciare solo questo esercizio.")}; break;
    case EClaraStage::SwitchReport: Hints = {TEXT("Configurazione verificata e salvata. Torna da Luca per il controllo finale.")}; break;
    default: Hints = {TEXT("Primo giorno concluso. Puoi continuare a esplorare e provare il cablaggio.")}; break;
    }
    Notify(Hints[FMath::Min(HintLevel++, Hints.Num() - 1)]);
}

bool AClaraPlayer::IsNearTerminal() const
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if ((It->ActorHasTag(TEXT("Clara.Terminal")) || It->ActorHasTag(TEXT("Clara.Service"))) && FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) < FMath::Square(380.f)) return true;
    return false;
}
void AClaraPlayer::RequestTerminal()
{
    if (Stage >= EClaraStage::SwitchConfig) { RequestSwitchTerminal(); return; }
    if (IsInputBlocked()) return;
    if (!IsNearTerminal()) { Notify(TEXT("Avvicinati alla postazione del docente per usare il suo terminale.")); return; }
    if (Stage < EClaraStage::Guide) { Notify(TEXT("Prima osserva il problema e il collegamento; il terminale arriva con una domanda da verificare.")); return; }
    if (auto* PC = Cast<AClaraPlayerController>(Controller)) PC->OpenTerminal();
}

void AClaraPlayer::RequestPCTerminal()
{
    if (IsInputBlocked()) return;
    if (!IsNearTerminal()) { Notify(TEXT("Avvicinati al monitor del PC docente.")); return; }
    if (Stage < EClaraStage::Guide) { Notify(TEXT("Prima osserva il collegamento e cerca nella guida la domanda da verificare.")); return; }
    if (auto* PC = Cast<AClaraPlayerController>(Controller)) PC->OpenTerminal(false);
}

void AClaraPlayer::RequestSwitchTerminal()
{
    if (IsInputBlocked()) return;
    if (Stage < EClaraStage::SwitchConfig) { Notify(TEXT("Prima chiedi a Luca l'incarico di configurazione.")); return; }
    if (!IsConsoleConnected()) { Notify(TEXT("La console non e' collegata: servono entrambi i capi, switch e USB del portatile.")); return; }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("Clara.SwitchTerminal")) && FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) < FMath::Square(380.f))
        {
            if (auto* PC = Cast<AClaraPlayerController>(Controller)) PC->OpenTerminal(true);
            return;
        }
    }
    Notify(TEXT("Avvicinati al portatile collegato alla console dello switch."));
}

void AClaraPlayer::OnSwitchConfigurationChecked(bool bCorrect)
{
    if (bCorrect && Stage == EClaraStage::SwitchConfig)
    {
        Stage = EClaraStage::SwitchReport;
        Notify(TEXT("Configurazione e salvataggio verificati. Torna da Luca per il controllo finale."));
    }
    else if (!bCorrect && Stage == EClaraStage::SwitchReport) Stage = EClaraStage::SwitchConfig;
}

void AClaraPlayer::RestartSwitchExercise()
{
    if (Stage >= EClaraStage::SwitchConfig)
    {
        Stage = EClaraStage::SwitchConfig;
        Notify(TEXT("Luca ha ripristinato solo lo switch di questa esercitazione. Il collegamento console resta pronto."));
    }
}

void AClaraPlayer::ToggleVision()
{
    if (IsInputBlocked()) return;
    bVisionEnabled = !bVisionEnabled;
    Notify(bVisionEnabled ? TEXT("Network Vision: illustrazione del percorso, non cattura di pacchetti. Origine: modello locale.") : TEXT("Network Vision disattivata."));
}
void AClaraPlayer::DropCable()
{
    if ((!bHoldingCable && !bCableStored) || HasDialogue()) return;
    bHoldingCable = false;
    bCableStored = false;
    if (CableActor.IsValid())
    {
        CableActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        CableActor->SetActorTransform(CableRestTransform);
        CableActor->SetActorEnableCollision(true);
    }
    Notify(TEXT("Connettore posato nel punto di raccolta."));
}

void AClaraPlayer::StowCable()
{
    if (!bHoldingCable || !CableActor.IsValid() || HasDialogue()) return;
    bHoldingCable = false;
    bCableStored = true;
    CableActor->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
    CableActor->SetActorRelativeLocation(FVector(12, 28, -20));
    Notify(TEXT("Estremita' libera agganciata alla cintura. L'altro capo resta nello switch."));
}

void AClaraPlayer::EquipCable()
{
    if (!bCableStored || !CableActor.IsValid() || HasDialogue()) return;
    bCableStored = false;
    bHoldingCable = true;
    CableActor->AttachToComponent(Camera, FAttachmentTransformRules::KeepWorldTransform);
    CableActor->SetActorRelativeLocation(FVector(55, 22, -22));
    Notify(TEXT("Connettore in mano. Clicca sulla porta Ethernet per inserirlo."));
}

FString AClaraPlayer::RunCommand(const FString& Command)
{
    const FString Clean = Command.TrimStartAndEnd();
    if (Clean == TEXT("enable") || Clean.StartsWith(TEXT("conf")))
        return TEXT("Luca: questa e' la shell Linux del PC, non la console dello switch. Qui puoi provare ping o leggere help.\nPer i comandi IOS collega il cavo CONSOLE al portatile vicino allo switch e apri quella sessione.");
    if (Clean == TEXT("help"))
        return TEXT("Guida della postazione locale Linux\nObiettivo: verificare se un dispositivo risponde.\nStrumento: ping -c 4 INDIRIZZO\nAzione: richiesta ICMP Echo; -c 4: quattro prove.\nTicket dispense: 192.168.10.20\nTicket prenotazioni: 192.168.10.30\nScrivi clear per pulire. Questo non e' un terminale Cisco CML.");
    const FClaraProbeCommand Probe = ParseClaraProbe(Clean);
    if (!Probe.bValid) return Probe.Error;
    if (Probe.Destination != TEXT("192.168.10.20") && Probe.Destination != TEXT("192.168.10.30"))
        return TEXT("Destinazione non inclusa nel modello locale. Non viene generato un esito di rete inventato.");
    if (!bCableConnected)
        return TEXT("[MODELLO LOCALE] Collegamento dati interrotto.\n4 richieste simulate, nessuna risposta. Questo esito non e' una misura CML.\nUn timeout reale puo' avere molte cause: qui il guasto controllato e' il collegamento fisico.");
    FString Output = FString::Printf(TEXT("[MODELLO LOCALE · nessuna rete Cisco collegata]\nDestinatario: %s\n4 richieste simulate, 4 risposte.\nTempi e TTL non sono generati: non sono stati misurati."), *Probe.Destination);
    if (Probe.Destination == TEXT("192.168.10.20"))
    {
        bFirstPing = true;
        if (Stage == EClaraStage::Terminal || Stage == EClaraStage::Guide)
        {
            Stage = EClaraStage::Service;
            Output += TEXT("\n\nLuca: la risposta riguarda questa comunicazione. Ora chiudi il terminale e clicca Dispense nel desktop del PC.");
        }
    }
    else if (Stage == EClaraStage::Transfer && bServiceOpened)
    {
        bTransferPing = true;
        Stage = EClaraStage::Debrief;
        Output += TEXT("\n\nHai verificato il nuovo destinatario. Torna al punto tutor di Luca per il rapporto.");
    }
    return Output;
}

FString AClaraPlayer::GetStageTitle() const
{
    static const TCHAR* Titles[] = {TEXT("Il colloquio"), TEXT("Il primo affiancamento"), TEXT("Una richiesta concreta"), TEXT("Segui il collegamento"), TEXT("Una domanda, una risposta"), TEXT("Trova lo strumento"), TEXT("La prima istruzione"), TEXT("Verifica il servizio"), TEXT("Ora scegli tu"), TEXT("Il rapporto"), TEXT("Preparare lo switch"), TEXT("La console dell'aula"), TEXT("Il controllo finale"), TEXT("Primo giorno completato")};
    return Titles[static_cast<uint8>(Stage)];
}
FString AClaraPlayer::GetObjective() const
{
    static const TCHAR* Objectives[] = {TEXT("Parla con Alessia al banco accoglienza."), TEXT("Raggiungi Luca nel laboratorio."), TEXT("Leggi il ticket sul banco del docente."), TEXT("Esamina il retro del PC e il percorso fino allo switch."), TEXT("Che cosa dimostra una risposta del dispositivo?"), TEXT("Nel taccuino cerca come verificare una risposta."), TEXT("Trova il destinatario e usa il terminale vicino al monitor."), TEXT("Apri le dispense sul monitor."), TEXT("Verifica il dispositivo delle prenotazioni: 192.168.10.30."), TEXT("Torna da Luca e ricostruisci le prove eseguite."), TEXT("Chiedi a Luca il nuovo incarico."), TEXT("Collega la console e prepara lo switch. Consulta l'incarico nel taccuino."), TEXT("Torna da Luca per verificare il lavoro."), TEXT("Hai ripristinato il servizio e preparato lo switch.")};
    return Objectives[static_cast<uint8>(Stage)];
}

void AClaraPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ConfigureCMLBridge();
    RestorePointer();
    CreateToolbar();
}
bool AClaraPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (Params.Key == EKeys::RightMouseButton && Params.Event == IE_Released)
    {
        RestorePointer();
        return true;
    }
    if (Params.Event != IE_Pressed || IsTerminalOpen()) return Super::InputKey(Params);
    AClaraPlayer* Player = GetClaraPlayer();
    if (!Player) return Super::InputKey(Params);
    if (Params.Key == EKeys::RightMouseButton && !Player->IsInputBlocked())
    {
        bLookEnabled = true;
        LookReadyTime = GetWorld()->GetTimeSeconds() + .1f;
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());
        return true;
    }
    if (Params.Key == EKeys::LeftMouseButton && !Player->IsInputBlocked()) { Player->Interact(); return true; }
    if (Params.Key == EKeys::I) { OpenInventory(); return true; }
    if (Params.Key == EKeys::E) { Player->Interact(); return true; }
    if (Params.Key == EKeys::H) { Player->ShowHint(); return true; }
    if (Params.Key == EKeys::N) { Player->OpenNotebook(); return true; }
    if (Params.Key == EKeys::T) { Player->RequestTerminal(); return true; }
    if (Params.Key == EKeys::V) { Player->ToggleVision(); return true; }
    if (Params.Key == EKeys::Q) { Player->DropCable(); return true; }
    if (Params.Key == EKeys::Enter || Params.Key == EKeys::SpaceBar) { Player->ContinueDialogue(); return true; }
    if (Params.Key == EKeys::Escape) { if (IsPanelOpen()) CloseUtilityPanel(); else if (Player->IsInspectingPC()) Player->EndPCInspection(); else Player->ClosePanel(); return true; }
    if (Params.Key == EKeys::One) { Player->Choose(1); return true; }
    if (Params.Key == EKeys::Two) { Player->Choose(2); return true; }
    if (Params.Key == EKeys::Three) { Player->Choose(3); return true; }
    if (Params.Key == EKeys::Four) { Player->Choose(4); return true; }
    return Super::InputKey(Params);
}

void AClaraPlayerController::OpenTerminal(bool bForSwitch)
{
    if (TerminalRoot.IsValid() || !GetWorld()->GetGameViewport()) return;
    bSwitchTerminal = bForSwitch;
    if (!bSwitchTerminal) TerminalOutput.Empty();
    bResetConfirmation = false;
    TerminalHistory.Empty();
    HistoryIndex = 0;
    if (bSwitchTerminal) TerminalOutput = bCMLMode
        ? TEXT("[CML] Apertura della console reale in corso. Il modello IOS locale e' disattivato.")
        : TEXT("CONSOLE LOCALE · sottoinsieme didattico IOS, non un'immagine Cisco eseguita.\nTab completa; ? apre l'aiuto contestuale; frecce richiamano i comandi.\nLeggi l'incarico nel taccuino e prova. Puoi sbagliare e chiedere aiuto a Luca.\n");
    if (TerminalOutput.IsEmpty()) TerminalOutput = TEXT("POSTAZIONE DIDATTICA · MODELLO LOCALE\nCML non collegato. Nessun comando viene eseguito su una rete reale.\nNon conosci lo strumento? Scrivi help e premi Invio.");
    TWeakObjectPtr<AClaraPlayerController> WeakThis(this);
    const FLinearColor Background = bSwitchTerminal
        ? FLinearColor(0.96f, 0.96f, 0.94f, 1.f) : FLinearColor(0.016f, 0.023f, 0.026f, 1.f);
    const FLinearColor Foreground = bSwitchTerminal
        ? FLinearColor(0.005f, 0.007f, 0.005f, 1.f) : FLinearColor(0.89f, 0.93f, 0.88f, 1.f);
    const FSlateFontInfo MonoFont(FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansMono.ttf"), 14);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
    static const FEditableTextBoxStyle InlineInputStyle = []()
    {
        FEditableTextBoxStyle Style = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
        const FSlateBrush& Flat = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        Style.SetBackgroundImageNormal(Flat).SetBackgroundImageHovered(Flat)
            .SetBackgroundImageFocused(Flat).SetBackgroundImageReadOnly(Flat).SetPadding(FMargin(0));
        return Style;
    }();
    TerminalRoot = SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .Padding(16)
        .BorderBackgroundColor(Background)
        [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
        [SNew(STextBlock).Font(ButtonFont).Text_Lambda([WeakThis]()
            {
                if (!WeakThis.IsValid()) return FText::GetEmpty();
                return FText::FromString(WeakThis->bSwitchTerminal
                    ? (WeakThis->bCMLMode ? TEXT("Console CML · apparato remoto / output reale") : TEXT("Console · switch aula / esercitazione locale"))
                    : TEXT("Terminale · PC docente / Linux · 192.168.10.10"));
            }).ColorAndOpacity(Foreground)]
        + SVerticalBox::Slot().FillHeight(1).Padding(0, 0, 0, 10)
        [SAssignNew(TerminalScroll, SScrollBox)
            + SScrollBox::Slot()
            [SNew(STextBlock).Font(MonoFont).AutoWrapText(true)
                .Text_Lambda([WeakThis]() { return FText::FromString(WeakThis.IsValid() ? WeakThis->TerminalOutput : FString()); })
                .ColorAndOpacity(Foreground)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
            [SNew(STextBlock).Font(MonoFont).ColorAndOpacity(Foreground)
                .Text_Lambda([WeakThis]()
                {
                    if (!WeakThis.IsValid()) return FText::GetEmpty();
                    if (WeakThis->bSwitchTerminal && WeakThis->bCMLMode) return FText::GetEmpty();
                    return FText::FromString(WeakThis->bSwitchTerminal ? WeakThis->SwitchCLI.GetPrompt() : TEXT("tecnico@pc-docente:~$"));
                })]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [SAssignNew(TerminalInput, SEditableTextBox)
                .Style(&InlineInputStyle).Font(MonoFont)
                .ForegroundColor(Foreground).FocusedForegroundColor(Foreground).BackgroundColor(Background)
                .HintText(FText::GetEmpty())
                .IsEnabled_Lambda([WeakThis]() { return WeakThis.IsValid()
                    && (!WeakThis->bSwitchTerminal || !WeakThis->bCMLMode || WeakThis->CanUseCMLInput()); })
                .ClearKeyboardFocusOnCommit(false)
                .SelectAllTextOnCommit(false)
                .OnTextCommitted(FOnTextCommitted::CreateUObject(this, &AClaraPlayerController::SubmitTerminal))
                .OnTextChanged(FOnTextChanged::CreateUObject(this, &AClaraPlayerController::TerminalTextChanged))
                .OnKeyDownHandler(FOnKeyDown::CreateUObject(this, &AClaraPlayerController::TerminalKeyDown))]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
            [SNew(SButton).ContentPadding(FMargin(12, 8))
                .Visibility(bSwitchTerminal && !bCMLMode ? EVisibility::Visible : EVisibility::Collapsed)
                .OnClicked_Lambda([WeakThis]() { if (WeakThis.IsValid()) WeakThis->AskSwitchMentor(); return FReply::Handled(); })
                [SNew(STextBlock).Font(ButtonFont).Text(FText::FromString(TEXT("Chiedi a Luca"))).ColorAndOpacity(FLinearColor(0.08f, 0.09f, 0.08f))]]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).ContentPadding(FMargin(12, 8))
                .Visibility(bSwitchTerminal && !bCMLMode ? EVisibility::Visible : EVisibility::Collapsed)
                .OnClicked_Lambda([WeakThis]() { if (WeakThis.IsValid()) WeakThis->ResetSwitchWithMentor(); return FReply::Handled(); })
                [SNew(STextBlock).Font(ButtonFont).ColorAndOpacity(FLinearColor(0.08f, 0.09f, 0.08f))
                    .Text_Lambda([WeakThis]() { return FText::FromString(WeakThis.IsValid() && WeakThis->bResetConfirmation
                        ? TEXT("Conferma: ripristina lo switch") : TEXT("Ricomincia con Luca")); })]]
            + SHorizontalBox::Slot().FillWidth(1)[SNew(SBox)]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SButton).ContentPadding(FMargin(12, 8))
                .OnClicked_Lambda([WeakThis]() { if (WeakThis.IsValid()) WeakThis->CloseTerminal(); return FReply::Handled(); })
                [SNew(STextBlock).Font(ButtonFont).Text(FText::FromString(TEXT("Chiudi · Esc"))).ColorAndOpacity(FLinearColor(0.08f, 0.09f, 0.08f))]]]];
    if (!ShowOnDisplay(TerminalRoot, bSwitchTerminal))
    {
        TerminalRoot.Reset();
        TerminalInput.Reset();
        TerminalScroll.Reset();
        return;
    }
    bTerminalOnDisplay = true;
    if (bSwitchTerminal && bCMLMode) BeginCMLConsole();
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetWidgetToFocus(TerminalInput);
    SetInputMode(Mode);
    FSlateApplication::Get().SetKeyboardFocus(TerminalInput);
    if (ACharacter* Character = Cast<ACharacter>(GetPawn())) Character->GetCharacterMovement()->StopMovementImmediately();
}

void AClaraPlayerController::SubmitTerminal(const FText& Text, ETextCommit::Type Commit)
{
    if (Commit != ETextCommit::OnEnter || !TerminalInput.IsValid()) return;
    const FString Command = bSwitchTerminal && bCMLMode ? Text.ToString() : Text.ToString().TrimStartAndEnd();
    if (bSwitchTerminal && bCMLMode && !CanUseCMLInput()) return;
    if (Command.IsEmpty() && !(bSwitchTerminal && (bCMLMode || SwitchCLI.IsAwaitingSaveConfirmation()))) return;
    if (!Command.IsEmpty()) TerminalHistory.Add(Command);
    HistoryIndex = TerminalHistory.Num();
    if (bSwitchTerminal && bCMLMode) SendCMLConsoleInput(Command, ECMLAction::Submit);
    else if (Command == TEXT("clear")) TerminalOutput.Empty();
    else if (bSwitchTerminal)
    {
        const FString Prompt = SwitchCLI.GetPrompt();
        TerminalOutput += TEXT("\n") + Prompt + Command + TEXT("\n") + ExecuteSwitchCommand(Command) + TEXT("\n");
        if (TerminalOutput.Len() > 24000) TerminalOutput.RightInline(24000);
    }
    else if (AClaraPlayer* Player = Cast<AClaraPlayer>(GetPawn())) TerminalOutput = TEXT("tecnico@pc-docente:~$ ") + Command + TEXT("\n\n") + Player->RunCommand(Command);
    TerminalInput->SetText(FText::GetEmpty());
    if (TerminalScroll.IsValid()) TerminalScroll->ScrollToEnd();
    FSlateApplication::Get().SetKeyboardFocus(TerminalInput);
}

void AClaraPlayerController::CloseTerminal()
{
    if (bSwitchTerminal && bCMLMode) CloseCMLConsole();
    const bool bReturnDesktop = bTerminalOnDisplay && !bSwitchTerminal && !bEndingPlay;
    if (bTerminalOnDisplay) ClearDisplay();
    else if (TerminalRoot.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(TerminalRoot.ToSharedRef());
    TerminalRoot.Reset();
    TerminalInput.Reset();
    TerminalScroll.Reset();
    bTerminalOnDisplay = false;
    RestorePointer();
    FSlateApplication::Get().SetAllUserFocusToGameViewport();
    if (bReturnDesktop) OpenDesktop();
}
void AClaraPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    if (TerminalRoot.IsValid()) CloseTerminal();
    CloseUtilityPanel();
    if (DialogueRoot.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(DialogueRoot.ToSharedRef());
    if (ToolbarRoot.IsValid() && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ToolbarRoot.ToSharedRef());
    DialogueRoot.Reset();
    ToolbarRoot.Reset();
    Super::EndPlay(Reason);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraProbeParserTest, "Clara.DayOne.ProbeParser", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraProbeParserTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Simple destination"), ParseClaraProbe(TEXT("ping 192.168.10.20")).bValid);
    TestEqual(TEXT("Bounded probe destination"), ParseClaraProbe(TEXT("ping -c 4 192.168.10.30")).Destination, FString(TEXT("192.168.10.30")));
    TestTrue(TEXT("Whitespace accepted"), ParseClaraProbe(TEXT("  ping   -c  4  192.168.10.20  ")).bValid);
    for (const FString& Bad : {FString(TEXT("ping")), FString(TEXT("ping 256.1.2.3")), FString(TEXT("ping 1..2.3")), FString(TEXT("ping 01.2.3.4")), FString(TEXT("ping 1.2.3.4;reboot")), FString(TEXT("ping -c 999 1.2.3.4")), FString(TEXT("ping 1.2.3.4 extra"))})
        TestFalse(*Bad, ParseClaraProbe(Bad).bValid);
    return true;
}
#endif
