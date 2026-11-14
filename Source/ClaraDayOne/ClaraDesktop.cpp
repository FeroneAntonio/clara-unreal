#include "ClaraDesktop.h"
#include "ClaraPlayer.h"

#include "Brushes/SlateColorBrush.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ClaraDesktop
{
    const FLinearColor Ink(0.89f, 0.91f, 0.92f);
    const FLinearColor Muted(0.62f, 0.68f, 0.69f);
    const FLinearColor Wallpaper(0.055f, 0.082f, 0.085f);
    const FLinearColor Chrome(0.035f, 0.045f, 0.047f);
    const FLinearColor Accent(0.82f, 0.70f, 0.48f);

    enum class EApp { Terminal, Material, Ticket, Notebook };

    const FButtonStyle& IconStyle()
    {
        static FButtonStyle Style = FButtonStyle()
            .SetNormal(FSlateColorBrush(FLinearColor(1, 1, 1, 0)))
            .SetHovered(FSlateColorBrush(FLinearColor(1, 1, 1, 0.08f)))
            .SetPressed(FSlateColorBrush(FLinearColor(1, 1, 1, 0.14f)))
            .SetNormalPadding(FMargin(8))
            .SetPressedPadding(FMargin(8));
        return Style;
    }

    TSharedRef<SWidget> Text(const FString& Value, int32 Size, FLinearColor Color = Ink, bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(FText::FromString(Value))
            .Font(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size))
            .ColorAndOpacity(Color);
    }

    class SClaraDesktop : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SClaraDesktop) : _Player(nullptr) {}
            SLATE_ARGUMENT(AClaraPlayer*, Player)
            SLATE_EVENT(FSimpleDelegate, OnClose)
        SLATE_END_ARGS()

        void Construct(const FArguments& Arguments)
        {
            Player = Arguments._Player;
            Close = Arguments._OnClose;
            ChildSlot
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(Wallpaper)
                .Padding(0)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        MakeTopBar()
                    ]
                    + SVerticalBox::Slot().FillHeight(1)
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(48, 20)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                            [ Text(TEXT("CLARA"), 48, FLinearColor(0.17f, 0.22f, 0.22f), true) ]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8)
                            [ Text(TEXT("LABORATORIO  /  NAPOLI"), 12, FLinearColor(0.24f, 0.30f, 0.29f)) ]
                        ]
                        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24, 28)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                            [ MakeDesktopIcon(TEXT(">_"), TEXT("Terminale"), TEXT("Apri il terminale Linux del PC docente."), EApp::Terminal) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                            [ MakeDesktopIcon(TEXT("Aa"), TEXT("Dispense"), TEXT("Verifica se il materiale del corso si apre da questa postazione."), EApp::Material) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                            [ MakeDesktopIcon(TEXT("#"), TEXT("Ticket"), TEXT("Leggi la richiesta e gli indirizzi dei destinatari."), EApp::Ticket) ]
                            + SVerticalBox::Slot().AutoHeight()
                            [ MakeDesktopIcon(TEXT("?"), TEXT("Taccuino"), TEXT("Cerca una spiegazione o uno strumento in base al tuo obiettivo."), EApp::Notebook) ]
                        ]
                        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(28)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                            [ Text(TEXT("PC DOCENTE"), 13, Muted, true) ]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 5)
                            [ Text(TEXT("192.168.10.10"), 16, Ink) ]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([WeakPlayer = Player]()
                                {
                                    const bool bConnected = WeakPlayer.IsValid() && WeakPlayer->IsCableConnected();
                                    return FText::FromString(bConnected ? TEXT("Ethernet collegata") : TEXT("Cavo Ethernet scollegato"));
                                })
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                .ColorAndOpacity(Muted)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ MakeTaskbar() ]
                ]
            ];
        }

        virtual bool SupportsKeyboardFocus() const override { return true; }

        virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
        {
            if (Event.GetKey() == EKeys::Escape)
            {
                Close.ExecuteIfBound();
                return FReply::Handled();
            }
            return SCompoundWidget::OnKeyDown(Geometry, Event);
        }

    private:
        TWeakObjectPtr<AClaraPlayer> Player;
        FSimpleDelegate Close;

        FOnClicked AppAction(EApp App) const
        {
            return FOnClicked::CreateLambda([WeakPlayer = Player, CloseDesktop = Close, App]()
            {
                // Close first so opening an app can take focus without competing utility panels.
                CloseDesktop.ExecuteIfBound();
                if (WeakPlayer.IsValid())
                {
                    switch (App)
                    {
                    case EApp::Terminal: WeakPlayer->RequestPCTerminal(); break;
                    case EApp::Material: WeakPlayer->OpenCourseMaterial(); break;
                    case EApp::Ticket: WeakPlayer->ReadCurrentTicket(); break;
                    case EApp::Notebook: WeakPlayer->OpenNotebook(); break;
                    }
                }
                return FReply::Handled();
            });
        }

        TSharedRef<SWidget> MakeDesktopIcon(const FString& Symbol, const FString& Label, const FString& Tip, EApp App)
        {
            return SNew(SBox).WidthOverride(116)
            [
                SNew(SButton)
                .ButtonStyle(&IconStyle())
                .HAlign(HAlign_Center)
                .ToolTipText(FText::FromString(Tip))
                .OnClicked(AppAction(App))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        SNew(SBox).WidthOverride(48).HeightOverride(44)
                        [
                            SNew(SBorder)
                            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                            .BorderBackgroundColor(FLinearColor(0.13f, 0.19f, 0.19f))
                            .HAlign(HAlign_Center).VAlign(VAlign_Center)
                            [ Text(Symbol, 24, Accent, true) ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)
                    [ Text(Label, 13) ]
                ]
            ];
        }

        TSharedRef<SWidget> MakeTopBar()
        {
            return SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(Chrome)
                .Padding(18, 9)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [ Text(TEXT("Postazione didattica / Linux"), 12, Muted) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([]() { return FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M"))); })
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                        .ColorAndOpacity(Ink)
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton).ButtonStyle(&IconStyle())
                        .ToolTipText(FText::FromString(TEXT("Torna nella stanza (Esc)")))
                        .OnClicked_Lambda([CloseDesktop = Close]()
                        {
                            CloseDesktop.ExecuteIfBound();
                            return FReply::Handled();
                        })
                        [ Text(TEXT("X"), 13, Ink, true) ]
                    ]
                ];
        }

        TSharedRef<SWidget> MakeTaskbar()
        {
            return SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(Chrome)
                .Padding(12, 5)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ TaskbarApp(TEXT(">_"), TEXT("Terminale del PC"), EApp::Terminal) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ TaskbarApp(TEXT("Aa"), TEXT("Dispense del corso"), EApp::Material) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ TaskbarApp(TEXT("#"), TEXT("Ticket di assistenza"), EApp::Ticket) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ TaskbarApp(TEXT("?"), TEXT("Taccuino e guida"), EApp::Notebook) ]
                    + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(12, 0)
                    [ Text(TEXT("Esc  torna nella stanza"), 11, Muted) ]
                ];
        }

        TSharedRef<SWidget> TaskbarApp(const FString& Symbol, const FString& Tip, EApp App)
        {
            return SNew(SButton)
                .ButtonStyle(&IconStyle())
                .HAlign(HAlign_Center)
                .ToolTipText(FText::FromString(Tip))
                .OnClicked(AppAction(App))
                [ SNew(SBox).WidthOverride(40).HAlign(HAlign_Center)[ Text(Symbol, 17, Accent, true) ] ];
        }
    };
}

TSharedRef<SWidget> MakeClaraDesktop(AClaraPlayer* Player, FSimpleDelegate OnClose)
{
    return SNew(ClaraDesktop::SClaraDesktop).Player(Player).OnClose(OnClose);
}
