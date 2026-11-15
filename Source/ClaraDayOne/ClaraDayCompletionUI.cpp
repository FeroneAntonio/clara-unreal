#include "ClaraStoryDirector.h"
#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> MakeClaraNotebookPaper(TSharedRef<SWidget> Content);

namespace
{
const FLinearColor Night(.018f,.041f,.048f,1.f);
const FLinearColor Ivory(.94f,.94f,.88f,1.f);
const FLinearColor Quiet(.62f,.74f,.72f,1.f);
const FLinearColor Mint(.43f,.84f,.70f,1.f);

FSlateFontInfo DayFont(int32 Size,bool Bold=false)
{
    return FCoreStyle::GetDefaultFontStyle(Bold?"Medium":"Regular",Size);
}

TSharedRef<SWidget> DayText(const FString& Value,int32 Size,FLinearColor Color=Ivory,bool Bold=false)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(DayFont(Size,Bold))
        .ColorAndOpacity(Color).AutoWrapText(true);
}

TSharedRef<SWidget> DayButton(const FString& Label,TFunction<void()> Action,bool Primary=false)
{
    static const FButtonStyle Style=FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor::White))
        .SetHovered(FSlateColorBrush(FLinearColor(.80f,.91f,.87f,1.f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.62f,.78f,.73f,1.f)));
    return SNew(SButton).ButtonStyle(&Style)
        .ButtonColorAndOpacity(Primary?Mint:FLinearColor(.09f,.16f,.17f,1.f))
        .ContentPadding(FMargin(22,14))
        .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Label)).Font(DayFont(14)).ColorAndOpacity(Primary?Night:Ivory).AutoWrapText(false)];
}

const FLinearColor NotebookInk(.12f,.23f,.30f,1.f);
const FLinearColor NotebookQuiet(.34f,.40f,.39f,1.f);
const FLinearColor NotebookTeal(.025f,.30f,.29f,1.f);

class SClaraDayReport final:public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraDayReport){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
    void Construct(const FArguments& Args){Director=Args._Director;Rebuild();}
    bool SupportsKeyboardFocus() const override{return true;}
    void Tick(const FGeometry& Geometry,double Time,float Delta) override
    {
        SCompoundWidget::Tick(Geometry,Time,Delta);
        if(!Focused){Focused=true;FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));}
    }
    FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override
    {
        if(Event.GetKey()==EKeys::Tab)return SCompoundWidget::OnKeyDown(Geometry,Event);
        if(Director.IsValid()&&!Event.IsRepeat())
        {
            if(Event.GetKey()==EKeys::Enter||Event.GetKey()==EKeys::SpaceBar)Advance();
            else if(Event.GetKey()==EKeys::Escape)Director->ClosePage();
            else if(Event.GetKey()==EKeys::Left)TurnPage(-1);
            else if(Event.GetKey()==EKeys::Right)TurnPage(1);
        }
        return FReply::Handled();
    }
private:
    TWeakObjectPtr<AClaraStoryDirector> Director;
    bool Focused=false;
    int32 Spread=0;

    void Advance()
    {
        if(!Director.IsValid())return;
        if(Director->GetDay()<3)Director->BeginNextDayTransition();
        else Director->ClosePage();
    }

    void TurnPage(int32 Direction)
    {
        const int32 Next=FMath::Clamp(Spread+Direction,0,2);
        if(Next!=Spread){Spread=Next;Rebuild();}
    }

    TSharedRef<SWidget> Link(const FString& Label,TFunction<void()> Action,bool Enabled=true)
    {
        static const FButtonStyle Style=FButtonStyle()
            .SetNormal(FSlateColorBrush(FLinearColor(1,1,1,0)))
            .SetHovered(FSlateColorBrush(FLinearColor(1,1,1,.09f)))
            .SetPressed(FSlateColorBrush(FLinearColor(1,1,1,.16f)));
        return SNew(SButton).ButtonStyle(&Style).IsEnabled(Enabled).ContentPadding(FMargin(12,12))
            .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(DayFont(13)).ColorAndOpacity(Enabled?Ivory:Quiet).AutoWrapText(false)];
    }

    TSharedRef<SWidget> Page(int32 Number)
    {
        const TCHAR* Titles[]={TEXT("Il primo messaggio"),TEXT("Cavi e ping"),TEXT("Console e indirizzi MAC"),
            TEXT("Due modi di leggere la rete"),TEXT("Rame, router e fibra"),TEXT("Ora posso farlo da solo")};
        auto Content=SNew(SVerticalBox);
        Content->AddSlot().AutoHeight().Padding(0,0,0,15)[DayText(Number==0?TEXT("GIORNO 1 · RETI · ORE 18:00"):TEXT("COSA HO IMPARATO"),11,NotebookQuiet,true)];
        Content->AddSlot().AutoHeight().Padding(0,0,0,24)[DayText(Titles[Number],22,NotebookInk,true)];
        auto Paragraph=[&](const TCHAR* Title,const TCHAR* Body)
        {
            Content->AddSlot().AutoHeight().Padding(0,0,0,8)[DayText(Title,15,NotebookTeal,true)];
            Content->AddSlot().AutoHeight().Padding(0,0,0,28)[DayText(Body,14,NotebookInk)];
        };
        if(Director.IsValid()&&Director->GetDay()>1)
        {
            const bool Last=Director->GetDay()==3;
            Content->ClearChildren();
            Content->AddSlot().AutoHeight().Padding(0,0,0,15)[DayText(Last?TEXT("GIORNO 3 · RETI · ORE 18:00"):TEXT("GIORNO 2 · RETI · ORE 18:00"),11,NotebookQuiet,true)];
            const TCHAR* Day2Titles[]={TEXT("DAY 2 COMPLETED!"),TEXT("ARP e sottoreti"),TEXT("Configurare lo switch"),TEXT("Verificare gli accessi"),TEXT("Ripristinare il piano 2"),TEXT("Pronto per il routing")};
            const TCHAR* Day3Titles[]={TEXT("DAY 3 COMPLETED!"),TEXT("Destinazione e gateway"),TEXT("Collegare le reti"),TEXT("Andata e ritorno"),TEXT("Diagnosticare i guasti"),TEXT("Grazie per aver giocato alla demo di TNES")};
            Content->AddSlot().AutoHeight().Padding(0,0,0,24)[DayText(Last?Day3Titles[Number]:Day2Titles[Number],22,NotebookInk,true)];
            if(!Last)switch(Number)
            {
            case 0:Paragraph(TEXT("Giornata completata"),TEXT("Hai ricostruito ARP, ragionato sulla mask e ripristinato la rete del piano 2 nel laboratorio locale."));break;
            case 1:Paragraph(TEXT("IP e MAC"),TEXT("ARP associa un IP locale al MAC. Leggere la cache prima e dopo un ping aiuta a osservare come viene appresa la coppia."));Paragraph(TEXT("Il foglio /26"),TEXT("La mask separa rete e host. Hai individuato rete, broadcast e intervallo degli host, prima di scegliere gli indirizzi."));break;
            case 2:Paragraph(TEXT("Una configurazione leggibile"),TEXT("Nome, accessi e indirizzo di gestione appartengono alla configurazione dello switch. La console consente di lavorare anche prima che la rete sia raggiungibile."));break;
            case 3:Paragraph(TEXT("Ping e autenticazione"),TEXT("Il ping verifica la raggiungibilità. La prova SSH verifica anche le credenziali nel modello locale del gioco: sono controlli diversi."));Paragraph(TEXT("Password di esercitazione"),TEXT("Usa credenziali dedicate al laboratorio. Il salvataggio della configurazione e la prova di accesso sono passaggi distinti."));break;
            case 4:Paragraph(TEXT("Dal cavo alla configurazione"),TEXT("Hai controllato collegamenti, indirizzi e switch del piano 2, poi ripetuto il test dopo il riavvio."));break;
            case 5:Paragraph(TEXT("Domani, ore 09:00"),TEXT("Porta con te il metodo: osserva, modifica una cosa alla volta e verifica. Il prossimo passo è collegare reti diverse."));break;
            }
            else switch(Number)
            {
            case 0:Paragraph(TEXT("Percorso reti completato"),TEXT("Hai costruito e verificato il percorso tra reti diverse, fino al collaudo dopo il riavvio."));break;
            case 1:Paragraph(TEXT("Locale o remoto?"),TEXT("La mask permette di distinguere una destinazione locale da una rete diversa. Per uscire dalla propria rete serve il gateway."));break;
            case 2:Paragraph(TEXT("Interfacce e collegamenti"),TEXT("Hai configurato le interfacce dei router e il collegamento /30 tra le reti. Un indirizzo corretto da solo non abilita una porta spenta."));break;
            case 3:Paragraph(TEXT("Due percorsi da controllare"),TEXT("Una rotta di andata non basta: anche la risposta deve trovare il percorso di ritorno. Le tabelle di routing spiegano dove viene inoltrato il pacchetto."));break;
            case 4:Paragraph(TEXT("Quattro guasti, un metodo"),TEXT("Hai confrontato ping, traceroute, stato delle interfacce e rotte per individuare i guasti. Dopo la correzione hai ripetuto le verifiche."));break;
            case 5:Paragraph(TEXT("Configurazioni persistenti"),TEXT("Il collaudo include il caricamento delle configurazioni salvate dopo il riavvio di R1, R2 e SW2."));Paragraph(TEXT("Nuovi capitoli — Coming soon"),TEXT("Conserva il metodo: osserva il risultato, formula un'ipotesi e verifica la correzione."));break;
            }
            Content->AddSlot().FillHeight(1)[SNew(SBox)];
            Content->AddSlot().AutoHeight().HAlign(HAlign_Right)[DayText(FString::FromInt(Number+1),11,NotebookQuiet)];
            return Content;
        }
        switch(Number)
        {
        case 0:
            Paragraph(TEXT("DAY 1 COMPLETED!"),TEXT("La rete funziona. Ho imparato a controllare il percorso di un messaggio, un passaggio alla volta."));
            Content->AddSlot().AutoHeight().Padding(0,0,0,10)[DayText(TEXT("Checkpoint completati"),15,NotebookTeal,true)];
            for(const TCHAR* Item:{TEXT("Cavi riconosciuti e ordinati"),TEXT("Ping tra i due PC riuscito"),TEXT("Console e tabella MAC lette"),TEXT("OSI e TCP/IP ricostruiti"),TEXT("Router e fibra collegati"),TEXT("Prova autonoma superata")})
                Content->AddSlot().AutoHeight().Padding(0,0,0,7)[DayText(FString(TEXT("✓  "))+Item,14,NotebookInk)];
            break;
        case 1:
            Paragraph(TEXT("Una presa, una funzione"),TEXT("Ethernet trasporta i dati. Il cavo console serve a configurare lo switch. La fibra usa la luce e richiede un modulo ottico compatibile."));
            Paragraph(TEXT("Leggere la risposta"),TEXT("Un LED acceso non basta: entrambi i PC devono essere collegati. Con ping verifico se il destinatario risponde; se non risponde, ricontrollo il percorso."));
            break;
        case 2:
            Paragraph(TEXT("Entrare nello switch"),TEXT("Collego il portatile alla porta CONSOLE. Il terminale del PC e la console dello switch sono due sessioni diverse, con comandi diversi."));
            Paragraph(TEXT("Trovare il dispositivo"),TEXT("Leggo il MAC della scheda del portatile e lo cerco con show mac address-table. La tabella mi dice su quale porta lo switch ha imparato quell'indirizzo."));
            break;
        case 3:
            Paragraph(TEXT("OSI: sette livelli"),TEXT("Dal segnale fisico fino all'applicazione, ogni livello ha un compito. Ricostruire la pila aiuta a capire dove cercare un problema."));
            Paragraph(TEXT("TCP/IP: quattro livelli"),TEXT("Network Access, Internet, Transport, Application. Il dato scende lungo la pila per essere trasmesso; chi riceve percorre la strada inversa."));
            break;
        case 4:
            Paragraph(TEXT("Collegare il router"),TEXT("Il router del banco usa porte in rame: lo collego allo switch con un Cat 6. Per un ping tra PC della stessa sottorete non serve attraversare il router."));
            Paragraph(TEXT("Completare il collegamento ottico"),TEXT("Inserisco prima il modulo SFP nello switch, poi collego la fibra al rack ottico. Uno slot vuoto e una porta pronta a ricevere il cavo non sono la stessa cosa."));
            break;
        case 5:
            Paragraph(TEXT("Ripetere la prova"),TEXT("Ho ricollegato i due PC senza la guida di Luca, ottenuto una risposta al ping e ritrovato la nuova porta del portatile nella tabella MAC."));
            Paragraph(TEXT("Domani, ore 09:00"),TEXT("«Domani si configura.»\n\nRipartirò da ARP, indirizzi e configurazione dello switch. Porto con me il metodo: osservo, provo e leggo il risultato."));
            break;
        }
        Content->AddSlot().FillHeight(1)[SNew(SBox)];
        Content->AddSlot().AutoHeight().HAlign(HAlign_Right)[DayText(FString::FromInt(Number+1),11,NotebookQuiet)];
        return Content;
    }

    void Rebuild()
    {
        const auto D=Director;
        const auto Pages=SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(63,37,34,30)[Page(Spread*2)]
            +SHorizontalBox::Slot().FillWidth(1).Padding(52,37,36,30)[Page(Spread*2+1)];
        ChildSlot[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0,0,0,.68f)).Padding(0)[
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::Both)[
                    SNew(SBox).WidthOverride(1120).HeightOverride(700)[
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(30,20,30,0)[
                            SNew(SBox).HeightOverride(570)[MakeClaraNotebookPaper(Pages)]]
                        +SVerticalBox::Slot().FillHeight(1).Padding(38,12,38,10).VAlign(VAlign_Center)[
                            SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Link(TEXT("← Indietro"),[this]{TurnPage(-1);},Spread>0)]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10,0)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d / 3"),Spread+1))).Font(DayFont(13)).ColorAndOpacity(Quiet).AutoWrapText(false)]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Link(TEXT("Avanti →"),[this]{TurnPage(1);},Spread<2)]
                            +SHorizontalBox::Slot().FillWidth(1)[SNew(SBox)]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10,0)[Link(TEXT("Torna in ufficio"),[D]{if(D.IsValid())D->ClosePage();})]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[
                                SNew(SButton).ContentPadding(FMargin(22,12)).ButtonColorAndOpacity(NotebookTeal)
                                .OnClicked_Lambda([this]{Advance();return FReply::Handled();})
                                [SNew(STextBlock).Text(FText::FromString(D->GetDay()==1?TEXT("Continua → Giorno 2"):D->GetDay()==2?TEXT("Continua → Giorno 3"):TEXT("Chiudi riepilogo"))).Font(DayFont(14)).ColorAndOpacity(Ivory).AutoWrapText(false)]]]
                    ]
                ]
            ]
        ];
    }
};

class SClaraDayTransition final:public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraDayTransition){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Director=Args._Director;
        const auto D=Director;
        ChildSlot[
            SNew(SOverlay)
            +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(0,0,0,0))]
            +SOverlay::Slot().VAlign(VAlign_Top)[
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.008f,.018f,.023f,.94f)).Padding(FMargin(40,22))[
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1)[DayText(TEXT("TNES  /  FINE GIORNATA"),14,Mint,true)]
                    +SHorizontalBox::Slot().AutoWidth()[DayText(D.IsValid()&&D->GetDay()==2?TEXT("GIORNO 2  →  GIORNO 3"):TEXT("GIORNO 1  →  GIORNO 2"),14,Ivory,true)]]]
            +SOverlay::Slot().VAlign(VAlign_Bottom)[
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.008f,.018f,.023f,.94f)).Padding(FMargin(40,22))[
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1)[
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                            .Text_Lambda([D]{return FText::FromString(D.IsValid()&&D->GetDayTransitionProgress()>.8f?TEXT("LA MATTINA SEGUENTE"):TEXT("FINE TURNO"));})
                            .Font(DayFont(13,true)).ColorAndOpacity(Mint)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(STextBlock)
                            .Text_Lambda([D]{return FText::FromString(D.IsValid()?D->GetOfficeClockTime():TEXT("18:00"));})
                            .Font(DayFont(32)).ColorAndOpacity(Ivory)]]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(20,0,24,0)[DayText(TEXT("18:00 → 09:00"),18,Quiet)]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[
                        DayButton(TEXT("Salta · Invio"),[D]{if(D.IsValid())D->FinishNextDayTransition();})]]]
        ];
    }

    bool SupportsKeyboardFocus() const override{return true;}
    void Tick(const FGeometry& Geometry,double Time,float Delta) override
    {
        SCompoundWidget::Tick(Geometry,Time,Delta);
        if(!Focused){Focused=true;FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));}
    }
    FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override
    {
        if(Event.GetKey()==EKeys::Tab)return SCompoundWidget::OnKeyDown(Geometry,Event);
        if(Director.IsValid()&&!Event.IsRepeat()&&
            (Event.GetKey()==EKeys::Enter||Event.GetKey()==EKeys::SpaceBar||Event.GetKey()==EKeys::Escape))
            Director->FinishNextDayTransition();
        return FReply::Handled();
    }
private:
    TWeakObjectPtr<AClaraStoryDirector> Director;
    bool Focused=false;
};
}

TSharedRef<SWidget> MakeClaraDayReport(AClaraStoryDirector* Director)
{
    return SNew(SClaraDayReport).Director(Director);
}

TSharedRef<SWidget> MakeClaraDayTransition(AClaraStoryDirector* Director)
{
    return SNew(SClaraDayTransition).Director(Director);
}
