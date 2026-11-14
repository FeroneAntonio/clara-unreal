#include "ClaraPanels.h"
#include "ClaraPlayer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Paper(0.92f, 0.87f, 0.73f);
const FLinearColor PaperLight(0.97f, 0.93f, 0.83f);
const FLinearColor Ink(0.12f, 0.13f, 0.12f);
const FLinearColor Pencil(0.37f, 0.36f, 0.31f);
const FLinearColor Leather(0.16f, 0.10f, 0.065f);
const FLinearColor Ochre(0.50f, 0.29f, 0.11f);

class SClaraModal : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraModal) {}
        SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Close = Args._OnClose; ChildSlot[Args._Content.Widget]; }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
    {
        if (Event.GetKey() == EKeys::Escape) { Close.ExecuteIfBound(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    FSimpleDelegate Close;
};

FSlateFontInfo Font(int32 Size, bool Bold = false)
{
    return FCoreStyle::GetDefaultFontStyle(Bold ? TEXT("Bold") : TEXT("Regular"), Size);
}

const FButtonStyle& PaperButton()
{
    static const FButtonStyle Style = []()
    {
        FButtonStyle Result = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button"));
        Result.SetNormal(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
        FSlateBrush Hovered = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 0.88f, 0.64f));
        Result.SetHovered(Hovered);
        FSlateBrush Pressed = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        Pressed.TintColor = FSlateColor(FLinearColor(0.72f, 0.62f, 0.43f));
        Result.SetPressed(Pressed);
        return Result;
    }();
    return Style;
}

TSharedRef<STextBlock> NoteText(const FString& Text, int32 Size = 17, bool Bold = false, FLinearColor Color = Ink)
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(Size, Bold))
        .ColorAndOpacity(Color).AutoWrapText(true);
}

TSharedRef<SWidget> Action(const FString& Label, TFunction<FReply()> Callback, bool Enabled = true)
{
    return SNew(SButton).ButtonStyle(&PaperButton()).ButtonColorAndOpacity(FLinearColor(0.81f, 0.74f, 0.58f))
        .ContentPadding(FMargin(14, 12)).HAlign(HAlign_Left).IsEnabled(Enabled)
        .OnClicked_Lambda([Callback]() { return Callback(); })
        [NoteText(Label, 16)];
}

void Paragraph(const TSharedPtr<SVerticalBox>& Page, const FString& Text, int32 Size = 17, bool Bold = false, FLinearColor Color = Ink)
{
    Page->AddSlot().AutoHeight().Padding(0, 0, 0, 17)[NoteText(Text, Size, Bold, Color)];
}

void AddAction(const TSharedPtr<SVerticalBox>& Page, const FString& Label, TFunction<FReply()> Callback, bool Enabled = true)
{
    Page->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[Action(Label, Callback, Enabled)];
}

TSharedRef<SWidget> RuledPage(TSharedRef<SWidget> Content)
{
    TSharedRef<SVerticalBox> Rules = SNew(SVerticalBox);
    Rules->SetVisibility(EVisibility::HitTestInvisible);
    Rules->AddSlot().AutoHeight()[SNew(SSpacer).Size(FVector2D(0, 35))];
    for (int32 Index = 0; Index < 20; ++Index)
    {
        Rules->AddSlot().AutoHeight()
        [SNew(SBox).HeightOverride(1)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FLinearColor(0.40f, 0.49f, 0.49f, 0.14f)).Padding(0)]];
        Rules->AddSlot().AutoHeight()[SNew(SSpacer).Size(FVector2D(0, 27))];
    }
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(PaperLight).Padding(0)
        [SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Fill).Padding(24, 0)[Rules]
        + SOverlay::Slot().Padding(31, 28)[Content]];
}

class SClaraNotebook : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraNotebook) {}
        SLATE_ARGUMENT(AClaraPlayer*, Player)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        OnClose = Args._OnClose;
        TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
        const FString Names[] = {TEXT("Missione"), TEXT("Osservazioni"), TEXT("Strumenti"), TEXT("Cavi")};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Tabs->AddSlot().FillWidth(1).Padding(0, 0, 5, 0)
            [SNew(SButton).ButtonStyle(&PaperButton()).ContentPadding(FMargin(12, 12))
                .ButtonColorAndOpacity_Lambda([this, Index]() { return Tab == Index ? PaperLight : FLinearColor(0.69f, 0.61f, 0.45f); })
                .OnClicked_Lambda([this, Index]() { Tab = Index; Topic.Empty(); RebuildPages(); return FReply::Handled(); })
                [NoteText(Names[Index], 16, true)]];
        }
        Tabs->AddSlot().AutoWidth()
        [Action(TEXT("Chiudi  ×"), [this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })];
        ChildSlot
        [SNew(SOverlay)
        + SOverlay::Slot()
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FLinearColor(0.015f, 0.012f, 0.008f, 0.60f))]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(28)
            [SNew(SBox).WidthOverride(1100).HeightOverride(700).MaxDesiredWidth(1100).MaxDesiredHeight(700)
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(Leather).Padding(13)
                    [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(5, 0, 5, 9)[Tabs]
                    + SVerticalBox::Slot().FillHeight(1)
                        [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)
                            [RuledPage(SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(LeftPage, SVerticalBox)])]
                        + SHorizontalBox::Slot().AutoWidth()
                            [SNew(SBox).WidthOverride(20)
                                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                                    .BorderBackgroundColor(FLinearColor(0.63f, 0.54f, 0.39f)).Padding(0)]]
                        + SHorizontalBox::Slot().FillWidth(1)
                            [RuledPage(SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(RightPage, SVerticalBox)])]]]]]];
        RebuildPages();
    }

private:
    TWeakObjectPtr<AClaraPlayer> Player;
    FSimpleDelegate OnClose;
    TSharedPtr<SVerticalBox> LeftPage;
    TSharedPtr<SVerticalBox> RightPage;
    int32 Tab = 0;
    FString Topic;

    void TopicButton(const FString& Label, const FString& Name, bool MarkProbe = false)
    {
        AddAction(LeftPage, Label, [this, Name, MarkProbe]()
        {
            Topic = Name;
            if (MarkProbe && Player.IsValid()) Player->MarkProbeGuideRead();
            RebuildPages();
            return FReply::Handled();
        });
    }

    void Step(const FString& Label, bool Done)
    {
        Paragraph(RightPage, (Done ? TEXT("✓  ") : TEXT("○  ")) + Label, 17, false, Done ? Pencil : Ink);
    }

    void RebuildPages()
    {
        LeftPage->ClearChildren();
        RightPage->ClearChildren();
        if (!Player.IsValid()) return;
        const EClaraStage Stage = Player->GetStage();
        if (Tab == 0)
        {
            Paragraph(LeftPage, TEXT("Il mio primo giorno"), 25, true);
            Paragraph(LeftPage, TEXT("INCARICO ATTUALE"), 12, true, Ochre);
            Paragraph(LeftPage, Player->GetObjective(), 20, true);
            if (Stage >= EClaraStage::SwitchBrief)
            {
                Paragraph(LeftPage, TEXT("Ticket 003 · Gestione dello switch"), 16, true);
                Paragraph(LeftPage, TEXT("Prepara un'identita' e un indirizzo di gestione riconoscibili per il team."));
                Paragraph(LeftPage, TEXT("Nome  AULA-01\nVLAN di gestione  10"), 20, true);
                Paragraph(LeftPage, TEXT("Indirizzo della SVI\n192.168.10.2 /24\nMask 255.255.255.0"), 19, true);
                Paragraph(LeftPage, TEXT("Abilita la SVI e salva la configurazione. Poi torna da Luca per il controllo."), 16);
                Paragraph(RightPage, TEXT("Dal cavo alla configurazione"), 23, true);
                Step(TEXT("Completare le verifiche LAN e il primo rapporto"), Stage >= EClaraStage::SwitchBrief);
                Step(TEXT("Ricevere da Luca l'incarico switch"), Stage >= EClaraStage::SwitchConfig);
                Step(TEXT("Recuperare il cavo console"), Player->HasConsoleCable());
                Step(TEXT("Collegare switch e portatile, entrambi i capi"), Player->IsConsoleConnected());
                const auto* PC = Cast<AClaraPlayerController>(Player->GetController());
                Step(TEXT("Preparare e salvare la configurazione richiesta"), PC && PC->IsSwitchMissionConfigured());
                Step(TEXT("Verificare il risultato con Luca"), Stage == EClaraStage::Complete);
                Paragraph(RightPage, TEXT("La scheda Strumenti spiega come orientarti nella console. Non serve imparare una sequenza a memoria."), 15, false, Pencil);
            }
            else
            {
                Paragraph(LeftPage, TEXT("Ticket 001 · Aula formazione"), 16, true);
                Paragraph(LeftPage, TEXT("Il PC del docente non apre le dispense. In reception il materiale e' disponibile."));
                Paragraph(LeftPage, TEXT("Destinatario delle dispense\n192.168.10.20"), 20, true);
                if (Stage >= EClaraStage::Transfer)
                    Paragraph(LeftPage, TEXT("Ticket 002 · Prenotazioni\n192.168.10.30"), 18, true);
                Paragraph(LeftPage, TEXT("Un aiuto non annulla il tuo lavoro. Puoi tornare qui quando vuoi."), 15, false, Pencil);
                Paragraph(RightPage, TEXT("Una prova alla volta"), 23, true);
                Step(TEXT("Conoscere Alessia e Luca"), Stage >= EClaraStage::Ticket);
                Step(TEXT("Leggere la richiesta"), Stage >= EClaraStage::Cabling);
                Step(TEXT("Ripristinare il percorso fisico"), Player->IsCableConnected());
                Step(TEXT("Cercare uno strumento nella guida"), Player->HasReadProbeGuide());
                Step(TEXT("Verificare la risposta del destinatario"), Stage >= EClaraStage::Service);
                Step(TEXT("Aprire il materiale richiesto"), Stage >= EClaraStage::Transfer);
                Step(TEXT("Ripetere la prova su un altro destinatario"), Stage >= EClaraStage::Debrief);
                Step(TEXT("Spiegare le prove nel rapporto"), Stage >= EClaraStage::SwitchBrief);
            }
        }
        else if (Tab == 1)
        {
            Paragraph(LeftPage, TEXT("Quello che ho osservato"), 25, true);
            Paragraph(LeftPage, TEXT("Le note descrivono soltanto gli eventi gia' incontrati."), 15, false, Pencil);
            if (Stage < EClaraStage::Cabling)
                Paragraph(LeftPage, TEXT("Non ho ancora esaminato il guasto. Prima ascolto la richiesta."));
            else
            {
                Paragraph(LeftPage, TEXT("Sintomo"), 16, true);
                Paragraph(LeftPage, TEXT("Il materiale non si apre dalla postazione docente."));
                Paragraph(LeftPage, TEXT("Percorso fisico"), 16, true);
                Paragraph(LeftPage, Player->IsCableConnected() ? TEXT("Entrambe le estremita' Ethernet sono inserite.") : TEXT("Il collegamento del PC e' interrotto. Posso confrontarlo con quello della postazione funzionante."));
                if (Stage >= EClaraStage::Service) Paragraph(LeftPage, TEXT("Il destinatario .20 ha risposto alla prova del modello locale."));
                if (Stage >= EClaraStage::Transfer) Paragraph(LeftPage, TEXT("Le dispense sono state aperte nel modello locale."));
            }
            Paragraph(RightPage, TEXT("Tre domande diverse"), 23, true);
            Paragraph(RightPage, TEXT("1. Il percorso esiste?"), 19, true);
            Paragraph(RightPage, TEXT("Osservo prese, cavi ed estremita'."));
            Paragraph(RightPage, TEXT("2. Il dispositivo risponde?"), 19, true);
            Paragraph(RightPage, TEXT("Scelgo una prova di comunicazione e ne leggo il risultato."));
            Paragraph(RightPage, TEXT("3. Il servizio funziona?"), 19, true);
            Paragraph(RightPage, TEXT("Apro il materiale richiesto. Una risposta ICMP non basta."));
        }
        else if (Tab == 2)
        {
            Paragraph(LeftPage, TEXT("Che cosa voglio scoprire?"), 25, true);
            Paragraph(LeftPage, TEXT("Scegli una domanda. Il nome dello strumento viene dopo."), 16, false, Pencil);
            TopicButton(TEXT("Verificare se un PC risponde"), TEXT("probe"), true);
            TopicButton(TEXT("Trovare l'indirizzo del destinatario"), TEXT("address"));
            TopicButton(TEXT("Capire i limiti di una risposta"), TEXT("limits"));
            if (Stage >= EClaraStage::SwitchBrief) TopicButton(TEXT("Gestire lo switch"), TEXT("switch"));
            Paragraph(LeftPage, TEXT("Leggere la guida non esegue il comando: la prova la farai tu al PC."), 15, false, Pencil);
            if (Topic == TEXT("probe"))
            {
                Paragraph(RightPage, TEXT("Ping"), 27, true);
                Paragraph(RightPage, TEXT("La domanda: riesci a rispondermi?"), 19, true);
                Paragraph(RightPage, TEXT("Ping invia una richiesta ICMP Echo a un destinatario e osserva l'eventuale risposta."));
                Paragraph(RightPage, TEXT("Sul PC Linux del laboratorio"), 15, true, Ochre);
                Paragraph(RightPage, TEXT("ping -c 4 INDIRIZZO"), 22, true);
                Paragraph(RightPage, TEXT("ping = azione\n-c 4 = quattro richieste\nINDIRIZZO = a chi sono destinate"), 17);
                Paragraph(RightPage, TEXT("Chiudi il taccuino, avvicinati al monitor e apri il terminale. Recupera l'indirizzo dalla pagina Missione."), 16);
            }
            else if (Topic == TEXT("switch"))
            {
                Paragraph(RightPage, TEXT("Orientarsi nella console"), 25, true);
                Paragraph(RightPage, TEXT("Prima leggi il prompt"), 18, true);
                Paragraph(RightPage, TEXT(">  Osservazione come utente\n#  Gestione privilegiata\n(config)#  Configurazione dell'apparato\n(config-if)#  Configurazione di un'interfaccia"), 16);
                Paragraph(RightPage, TEXT("Una domanda alla volta"), 18, true);
                Paragraph(RightPage, TEXT("? mostra le opzioni nel punto in cui stai scrivendo. Uno spazio prima di ? chiede la parte successiva. TAB completa le parole non ambigue."), 16);
                Paragraph(RightPage, TEXT("Cambiare e salvare sono due azioni"), 18, true);
                Paragraph(RightPage, TEXT("La running-config e' in uso; la startup-config e' la copia salvata. Confrontale dopo una modifica. Luca puo' spiegare che cosa manca dal pulsante della console."), 16);
            }
            else if (Topic == TEXT("address"))
            {
                Paragraph(RightPage, TEXT("A chi mi rivolgo?"), 25, true);
                Paragraph(RightPage, TEXT("Il nome spiega il ruolo del dispositivo; l'indirizzo lo indica nella prova."));
                Paragraph(RightPage, TEXT("Dispense\n192.168.10.20"), 21, true);
                Paragraph(RightPage, TEXT("Prenotazioni\n192.168.10.30"), 21, true);
                Paragraph(RightPage, TEXT("Leggi il ticket prima di scegliere. Il secondo incarico richiede un destinatario diverso."), 16);
                if (Stage >= EClaraStage::SwitchBrief)
                    Paragraph(RightPage, TEXT("Attenzione: 192.168.10.2 e' l'indirizzo da assegnare alla gestione dello switch. Non e' il server delle dispense .20."), 17, true);
            }
            else if (Topic == TEXT("limits"))
            {
                Paragraph(RightPage, TEXT("Cosa posso concludere?"), 25, true);
                Paragraph(RightPage, TEXT("Se arriva una risposta: il dispositivo ha risposto a questa prova."), 19, true);
                Paragraph(RightPage, TEXT("Non dimostra che tutte le applicazioni funzionino o che la rete sia sicura."));
                Paragraph(RightPage, TEXT("Se non arriva: serve un'altra osservazione. Un cavo staccato e' una possibilita', non l'unica causa di un timeout."));
                Paragraph(RightPage, TEXT("Per chiudere il ticket delle dispense devi aprire anche il materiale."), 17, true);
            }
            else
            {
                Paragraph(RightPage, TEXT("Cerca per intenzione"), 25, true);
                Paragraph(RightPage, TEXT("Non devi conoscere una parola inglese per cominciare. Seleziona una domanda sulla pagina a sinistra."), 19);
                Paragraph(RightPage, TEXT("La guida resta consultabile anche nella prova finale."), 16, false, Pencil);
            }
        }
        else
        {
            Paragraph(LeftPage, TEXT("Prima di collegare"), 25, true);
            TopicButton(TEXT("Ethernet · percorso dei dati"), TEXT("ethernet"));
            TopicButton(TEXT("Console · gestione dell'apparato"), TEXT("console"));
            TopicButton(TEXT("Due estremita', un percorso"), TEXT("ends"));
            Paragraph(LeftPage, TEXT("Il colore aiuta a trovare un cavo. Etichette, connettori e funzione aiutano a sceglierlo."), 16, false, Pencil);
            if (Topic == TEXT("console"))
            {
                Paragraph(RightPage, TEXT("Console"), 26, true);
                Paragraph(RightPage, TEXT("Serve per gestire un apparato attraverso la sua console. Non sostituisce il cavo dati del PC."), 19);
                Paragraph(RightPage, TEXT("Connettori simili possono avere funzioni differenti. Cerca l'etichetta CONSOLE sul dispositivo."));
                Paragraph(RightPage, Player->IsConsoleConnected()
                    ? TEXT("Ora: entrambi i capi sono collegati, allo switch e al portatile.")
                    : (Player->HasConsoleCable() ? TEXT("Ora: il cavo e' stato raccolto, ma almeno un capo e' ancora libero.") : TEXT("Ora: il cavo e' nel laboratorio. Nell'incarico switch potrai raccoglierlo e collegarlo.")), 16, true);
                Paragraph(RightPage, TEXT("Un capo va alla porta CONSOLE dello switch; l'altro alla presa USB del portatile. Il ping del PC usa un percorso dati diverso."), 16);
            }
            else if (Topic == TEXT("ends"))
            {
                Paragraph(RightPage, TEXT("Segui tutto il percorso"), 26, true);
                Paragraph(RightPage, TEXT("Una presa inserita non basta. Segui il cavo fino all'altra estremita'."), 19);
                Paragraph(RightPage, Stage >= EClaraStage::SwitchBrief
                    ? TEXT("Per la console collega entrambi i capi: switch e portatile. Un solo inserimento non apre una sessione.")
                    : TEXT("Prendi il connettore libero, poi scegli la porta Ethernet. Puoi posarlo e riprovare."));
                Paragraph(RightPage, TEXT("Dopo il collegamento servono ancora le verifiche di comunicazione e servizio."));
            }
            else
            {
                Paragraph(RightPage, TEXT("Ethernet"), 26, true);
                Paragraph(RightPage, TEXT("Collega il PC alla rete dati attraverso lo switch."), 19);
                Paragraph(RightPage, TEXT("Esamina il connettore e la porta, poi confronta con la postazione funzionante."));
                Paragraph(RightPage, Player->IsCableConnected() ? TEXT("Ora: connettore inserito nel PC.") : (Player->IsHoldingCable() ? TEXT("Ora: connettore in mano.") : TEXT("Ora: cerca l'estremita' libera nel laboratorio.")), 17, true);
            }
        }
    }
};

void InventorySlot(const TSharedRef<SHorizontalBox>& Row, const FString& Name, const FString& State, const FString& Detail, bool Available)
{
    Row->AddSlot().FillWidth(1).Padding(5)
    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(Available ? PaperLight : FLinearColor(0.72f, 0.69f, 0.61f)).Padding(20)
        [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[NoteText(Name, 22, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[NoteText(State, 14, true, Ochre)]
        + SVerticalBox::Slot().AutoHeight()[NoteText(Detail, 16)]]];
}
}

TSharedRef<SWidget> MakeClaraNotebook(AClaraPlayer* Player, FSimpleDelegate OnClose)
{
    return SNew(SClaraModal).OnClose(OnClose)
        [SNew(SClaraNotebook).Player(Player).OnClose(OnClose)];
}

TSharedRef<SWidget> MakeClaraInventory(AClaraPlayer* Player, FSimpleDelegate OnClose)
{
    TWeakObjectPtr<AClaraPlayer> WeakPlayer(Player);
    TSharedRef<SHorizontalBox> Top = SNew(SHorizontalBox);
    TSharedRef<SHorizontalBox> Bottom = SNew(SHorizontalBox);
    InventorySlot(Top, TEXT("Badge"), Player && Player->HasBadge() ? TEXT("CONSEGNATO") : TEXT("DA RITIRARE"),
        Player && Player->HasBadge() ? TEXT("Tecnico in affiancamento. Il percorso scelto si puo' cambiare.") : TEXT("Alessia lo consegna dopo il colloquio."), Player && Player->HasBadge());
    InventorySlot(Top, TEXT("Taccuino"), TEXT("CON TE"), TEXT("Missione, osservazioni, strumenti e guida ai cavi. Puoi aprirlo in qualsiasi momento."), true);
    const bool Held = Player && Player->IsHoldingCable();
    const bool Connected = Player && Player->IsCableConnected();
    const bool Stored = Player && Player->IsCableStored();
    InventorySlot(Bottom, TEXT("Cavo Ethernet / estremita' libera"), Held ? TEXT("IN MANO") : (Stored ? TEXT("ALLA CINTURA") : (Connected ? TEXT("COLLEGATO AL PC") : TEXT("NEL LABORATORIO"))),
        Held ? TEXT("Hai in mano il connettore libero. L'altro capo resta collegato allo switch.") : (Stored ? TEXT("Il connettore e' agganciato alla cintura. Il cavo resta visibile fino allo switch.") : (Connected ? TEXT("Inserito nel PC. Per scollegarlo devi tornare alla presa.") : TEXT("Raccogli l'estremita' libera nella stanza."))), Held || Stored || Connected);
    const bool ConsoleOwned = Player && Player->HasConsoleCable();
    const bool ConsoleConnected = Player && Player->IsConsoleConnected();
    const bool ConsoleHeld = Player && Player->IsHoldingConsole();
    InventorySlot(Bottom, TEXT("Cavo console"), ConsoleConnected ? TEXT("SWITCH E PORTATILE COLLEGATI") : (ConsoleHeld ? TEXT("UN CAPO ANCORA LIBERO") : (ConsoleOwned ? TEXT("RACCOLTO") : TEXT("NEL LABORATORIO"))),
        ConsoleConnected ? TEXT("Entrambe le estremita' sono inserite. Il portatile puo' aprire la console dello switch.") : (ConsoleOwned ? TEXT("Collega entrambi i capi: CONSOLE sullo switch, USB sul portatile. Resta distinto dal cavo Ethernet.") : TEXT("Non ancora raccolto. Nell'incarico switch servira' per accedere alla gestione.")), ConsoleOwned);
    TSharedRef<SHorizontalBox> ItemActions = SNew(SHorizontalBox);
    if (Held || Stored)
    {
        ItemActions->AddSlot().FillWidth(1).Padding(0, 0, 12, 0)
        [Action(Held ? TEXT("Aggancia alla cintura") : TEXT("Prendi in mano"), [WeakPlayer, OnClose, Held]()
        {
            OnClose.ExecuteIfBound();
            if (WeakPlayer.IsValid())
            {
                if (Held) WeakPlayer->StowCable();
                else WeakPlayer->EquipCable();
            }
            return FReply::Handled();
        })];
        ItemActions->AddSlot().FillWidth(1)
        [Action(TEXT("Posa il connettore"), [WeakPlayer, OnClose]()
        {
            OnClose.ExecuteIfBound();
            if (WeakPlayer.IsValid()) WeakPlayer->DropCable();
            return FReply::Handled();
        })];
    }
    return SNew(SClaraModal).OnClose(OnClose)
    [SNew(SOverlay)
    + SOverlay::Slot()
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor(0.015f, 0.012f, 0.008f, 0.62f))]
    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(28)
        [SNew(SBox).WidthOverride(1000).HeightOverride(700).MaxDesiredWidth(1100).MaxDesiredHeight(700)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Paper).Padding(28)
                [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[NoteText(TEXT("Le tue cose"), 28, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)[NoteText(TEXT("Gli oggetti della stanza restano dove li hai lasciati."), 16, false, Pencil)]
                + SVerticalBox::Slot().FillHeight(1)[Top]
                + SVerticalBox::Slot().FillHeight(1).Padding(0, 10, 0, 15)[Bottom]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[ItemActions]
                + SVerticalBox::Slot().AutoHeight()
                    [Action(TEXT("Chiudi inventario"), [OnClose]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })]]]]];
}

TSharedRef<SWidget> MakeClaraDialogue(AClaraPlayer* Player)
{
    TWeakObjectPtr<AClaraPlayer> WeakPlayer(Player);
    TSharedRef<SVerticalBox> Dialogue = SNew(SVerticalBox);
    Dialogue->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
    [SNew(STextBlock).Text_Lambda([WeakPlayer]() { return FText::FromString(WeakPlayer.IsValid() ? WeakPlayer->GetSpeaker() : FString()); })
        .Font(Font(15, true)).ColorAndOpacity(FLinearColor(0.88f, 0.75f, 0.52f))];
    Dialogue->AddSlot().AutoHeight().Padding(0, 0, 0, 16)
    [SNew(STextBlock).Text_Lambda([WeakPlayer]() { return FText::FromString(WeakPlayer.IsValid() ? WeakPlayer->GetDialogue() : FString()); })
        .Font(Font(20)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f))];
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Dialogue->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [SNew(SButton).ButtonStyle(&PaperButton()).ContentPadding(FMargin(15, 11))
            .ButtonColorAndOpacity(FLinearColor(0.20f, 0.19f, 0.16f, 0.88f)).HAlign(HAlign_Left)
            .Visibility_Lambda([WeakPlayer, Index]() { return WeakPlayer.IsValid() && WeakPlayer->GetChoices().IsValidIndex(Index) ? EVisibility::Visible : EVisibility::Collapsed; })
            .OnClicked_Lambda([WeakPlayer, Index]() { if (WeakPlayer.IsValid()) WeakPlayer->Choose(Index + 1); return FReply::Handled(); })
            [SNew(STextBlock).Text_Lambda([WeakPlayer, Index]()
            {
                return FText::FromString(WeakPlayer.IsValid() && WeakPlayer->GetChoices().IsValidIndex(Index) ? FString::Printf(TEXT("%d   %s"), Index + 1, *WeakPlayer->GetChoices()[Index]) : FString());
            }).Font(Font(17)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f))]];
    }
    Dialogue->AddSlot().AutoHeight().HAlign(HAlign_Right)
    [SNew(SButton).ButtonStyle(&PaperButton()).ContentPadding(FMargin(20, 11)).ButtonColorAndOpacity(FLinearColor(0.63f, 0.51f, 0.32f))
        .Visibility_Lambda([WeakPlayer]() { return WeakPlayer.IsValid() && WeakPlayer->GetChoices().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
        .OnClicked_Lambda([WeakPlayer]() { if (WeakPlayer.IsValid()) WeakPlayer->ContinueDialogue(); return FReply::Handled(); })
        [NoteText(TEXT("Continua  →"), 17, true)]];
    return SNew(SOverlay)
    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(35, 0, 35, 45))
        [SNew(SBox).WidthOverride(1000).MaxDesiredWidth(1100).MaxDesiredHeight(700)
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FLinearColor(0.012f, 0.012f, 0.01f, 0.66f)).Padding(24)
                .Visibility_Lambda([WeakPlayer]() { return WeakPlayer.IsValid() && WeakPlayer->HasDialogue() ? EVisibility::Visible : EVisibility::Collapsed; })
                [Dialogue]]];
}
