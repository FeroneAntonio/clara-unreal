#include "ClaraStoryDirector.h"
#include "Brushes/SlateColorBrush.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Ink(.055f,.12f,.15f),Paper(.96f,.95f,.90f),Teal(.02f,.35f,.36f);
const TCHAR* OSINames[]={TEXT("Physical"),TEXT("Data Link"),TEXT("Network"),TEXT("Transport"),TEXT("Session"),TEXT("Presentation"),TEXT("Application")};
const TCHAR* TCPNames[]={TEXT("Network Access"),TEXT("Internet"),TEXT("Transport"),TEXT("Application")};
const FLinearColor OSIColors[]={FLinearColor(FColor(216,151,63)),FLinearColor(FColor(187,118,36)),FLinearColor(FColor(57,133,157)),FLinearColor(FColor(54,139,100)),FLinearColor(FColor(111,93,155)),FLinearColor(FColor(132,108,173)),FLinearColor(FColor(93,73,139))};
FString LayerName(bool TCP,int32 Layer){return TCP?FString(TCPNames[Layer]):FString(OSINames[Layer]);}
FLinearColor LayerColor(bool TCP,int32 Layer){const int32 Map[]={1,2,3,6};return OSIColors[TCP?Map[Layer]:Layer];}
TSharedRef<SWidget> TowerText(const FString& Value,int32 Size=17,FLinearColor Color=Ink,bool Bold=false)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true);
}
TSharedRef<SWidget> TowerButton(const FString& Label,TFunction<void()> Action,bool Enabled=true)
{
    static const FButtonStyle Style=FButtonStyle().SetNormal(FSlateColorBrush(FLinearColor::White)).SetHovered(FSlateColorBrush(FLinearColor(.82f,.9f,.87f))).SetPressed(FSlateColorBrush(FLinearColor(.64f,.79f,.74f)));
    return SNew(SButton).ButtonStyle(&Style).ButtonColorAndOpacity(Teal).IsEnabled(Enabled).ContentPadding(FMargin(14,12))
        .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})[SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).ColorAndOpacity(FLinearColor::White).AutoWrapText(false)];
}

class SClaraLayerTower final:public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraLayerTower){}SLATE_ARGUMENT(AClaraStoryDirector*,Director)SLATE_END_ARGS()
    void Construct(const FArguments& Args){Director=Args._Director;Rebuild();}
    bool SupportsKeyboardFocus()const override{return true;}
    FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)override
    {
        if(Director.IsValid()&&!Event.IsRepeat())
        {
            const FKey Key=Event.GetKey();
            if(Key==EKeys::One||Key==EKeys::Two||Key==EKeys::Three)
            {Director->SelectTowerPeg(Key==EKeys::One?0:Key==EKeys::Two?1:2);return FReply::Handled();}
        }
        return SCompoundWidget::OnKeyDown(Geometry,Event);
    }
    void Tick(const FGeometry& Geometry,double Time,float Delta)override
    {
        SCompoundWidget::Tick(Geometry,Time,Delta);
        if(Director.IsValid())
        {
            const auto& Tower=Director->GetLayerTower();
            if(Revision!=Tower.Revision||Kind!=Tower.Kind||Mnemonic!=Director->IsTowerMnemonicVisible())Rebuild();
        }
    }
private:
    TWeakObjectPtr<AClaraStoryDirector> Director;
    int32 Revision=-1;FString Kind;bool Mnemonic=false;

    TSharedRef<SWidget> Block(bool TCP,int32 Layer,bool Selected=false,bool Reference=false)
    {
        const FString Label=(Reference?FString::FromInt(Layer+1)+TEXT("  "):FString())+LayerName(TCP,Layer);
        const FLinearColor TextColor=LayerColor(TCP,Layer).GetLuminance()>.38f?Ink:FLinearColor::White;
        auto Stripes=SNew(SHorizontalBox);
        const TArray<int32> Map=TCP?(Layer==0?TArray<int32>{0,1}:Layer==1?TArray<int32>{2}:Layer==2?TArray<int32>{3}:TArray<int32>{4,5,6}):TArray<int32>{Layer};
        for(int32 Index:Map)Stripes->AddSlot().FillWidth(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(OSIColors[Index]).Padding(0)];
        return SNew(SBox).HeightOverride(40)[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Selected?FLinearColor(FColor(255,222,106)):Paper).Padding(Selected?3.f:1.f)[
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(LayerColor(TCP,Layer)).Padding(FMargin(10,3))[
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[TowerText(Label,16,TextColor,true)]
                    +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(4)[Stripes]]
                ]
            ]
        ];
    }

    TSharedRef<SWidget> Reference(bool TCP)
    {
        auto Stack=SNew(SVerticalBox);
        const int32 Count=TCP?4:7;
        for(int32 I=Count-1;I>=0;--I)Stack->AddSlot().AutoHeight().Padding(0,1)[Block(TCP,I,false,true)];
        auto D=Director;
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,9)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold",18)).ColorAndOpacity(Teal)
                .Text_Lambda([D]{return FText::FromString(D.IsValid()?FString::Printf(TEXT("Osserva l’ordine · %d s"),FMath::CeilToInt(D->GetLayerTower().PreviewRemaining)):FString());})]
            +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(TCP?.58f:1.f).Padding(0,0,TCP?20:0,0)[Stack]
                +SHorizontalBox::Slot().FillWidth(TCP?.42f:0.f)[TCP?
                    TowerText(TEXT("STESSI COLORI, DUE MODELLI\n\nApplication ← OSI 7, 6, 5\nTransport ← OSI 4\nInternet ← OSI 3\nNetwork Access ← OSI 2, 1\n\nLa base sostiene tutti i livelli sopra."),17):SNew(SBox)]
            ];
    }

    TSharedRef<SWidget> Peg(int32 Index,bool TCP)
    {
        const auto& Tower=Director->GetLayerTower();auto D=Director;
        auto Blocks=SNew(SVerticalBox);
        Blocks->AddSlot().FillHeight(1)[SNew(SBox)];
        for(int32 I=Tower.Pegs[Index].Num()-1;I>=0;--I)
        {
            const bool Top=I==Tower.Pegs[Index].Num()-1;
            const auto Tile=Block(TCP,Tower.Pegs[Index][I],Tower.Selected==Index&&Top);
            if(Top)
                Blocks->AddSlot().AutoHeight().Padding(0,1)[SNew(SButton).ButtonStyle(FCoreStyle::Get(),"NoBorder").ContentPadding(0)
                    .IsEnabled(!Tower.IsPreview()&&!Tower.Completed)
                    .ToolTipText(FText::FromString(TEXT("Seleziona questo blocco superiore, oppure posa qui il blocco selezionato.")))
                    .OnClicked_Lambda([D,Index]{if(D.IsValid())D->SelectTowerPeg(Index);return FReply::Handled();})[Tile]];
            else Blocks->AddSlot().AutoHeight().Padding(0,1)[Tile];
        }
        const FString Name=Index==0?TEXT("A"):Index==1?TEXT("B"):TEXT("C");
        const FString Action=Tower.Selected<0?TEXT("Prendi da ")+Name:Tower.Selected==Index?TEXT("Annulla selezione"):TEXT("Posa su ")+Name;
        const bool Enabled=!Tower.IsPreview()&&!Tower.Completed&&(Tower.Selected>=0||!Tower.Pegs[Index].IsEmpty());
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,8)[TowerText(Name+(Index==2?TEXT(" · ARRIVO"):FString()),15,Teal,true)]
            +SVerticalBox::Slot().FillHeight(1)[SNew(SOverlay)
                +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)[SNew(SBox).WidthOverride(7).HeightOverride(285)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.62f,.65f,.60f)).Padding(0)]]
                +SOverlay::Slot()[Blocks]
            ]
            +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(7)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Tower.Completed?FLinearColor(.25f,.7f,.47f):Teal).Padding(0)]]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[TowerButton(Action,[D,Index]{if(D.IsValid())D->SelectTowerPeg(Index);},Enabled)];
    }

    void Rebuild()
    {
        if(!Director.IsValid()){ChildSlot[SNew(SBox)];return;}
        const auto D=Director;const auto& Tower=D->GetLayerTower();const bool TCP=Tower.Kind==TEXT("tcp");
        Revision=Tower.Revision;Kind=Tower.Kind;Mnemonic=D->IsTowerMnemonicVisible();
        auto Pegs=SNew(SHorizontalBox).RenderTransform_Lambda([D]
        {
            const float Shake=D.IsValid()?D->GetLayerTower().ShakeRemaining:0.f;
            return FSlateRenderTransform(FVector2D(FMath::Sin(Shake*90.f)*FMath::Min(1.f,Shake*3.f)*6.f,0));
        });
        for(int32 I=0;I<3;++I)Pegs->AddSlot().FillWidth(1).Padding(I?10:0,0,I<2?10:0,0)[Peg(I,TCP)];
        auto Actions=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding({10,10});
        Actions->AddSlot()[TowerButton(Tower.Completed?TEXT("Continua con Luca"):TEXT("Verifica la torre"),[D]{if(D.IsValid())D->CheckPuzzle();},!Tower.IsPreview())];
        Actions->AddSlot()[TowerButton(TEXT("Rivedi · 5 secondi"),[D]{if(D.IsValid())D->PreviewLayerTower();},!Tower.Completed)];
        Actions->AddSlot()[TowerButton(Mnemonic?TEXT("Chiudi frase"):TEXT("Frase per ricordare"),[D]{if(D.IsValid())D->ShowTowerMnemonic();})];
        ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Paper).Padding(20)[
            SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[TowerText(TCP?TEXT("TCP/IP · quattro livelli"):TEXT("OSI · sette livelli"),25,Ink,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,16)[TowerText(TEXT("Prendi soltanto il blocco in cima e scegli dove posarlo. Ricostruisci sul piolo C, a destra. Puoi appoggiare ogni blocco su qualsiasi altro blocco."),16)]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(385)[SNew(SOverlay)
                    +SOverlay::Slot()[SNew(SBox).Visibility_Lambda([D]{return D.IsValid()&&D->GetLayerTower().PreviewPending?EVisibility::Visible:EVisibility::Collapsed;})
                        .VAlign(VAlign_Center).HAlign(HAlign_Center)[TowerText(TEXT("Ascolta Luca. Poi avrai dieci secondi per osservare la torre."),21,Teal,true)]]
                    +SOverlay::Slot()[SNew(SBox).Visibility_Lambda([D]{return D.IsValid()&&!D->GetLayerTower().PreviewPending&&D->GetLayerTower().IsPreview()?EVisibility::Visible:EVisibility::Collapsed;})[Reference(TCP)]]
                    +SOverlay::Slot()[SNew(SBox).Visibility_Lambda([D]{return D.IsValid()&&!D->GetLayerTower().IsPreview()?EVisibility::Visible:EVisibility::Collapsed;})[Pegs]]
                ]]
                +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)[Actions]
                +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[SNew(SBox).Visibility(Tower.OffersHint()&&!Tower.Completed?EVisibility::Visible:EVisibility::Collapsed)[TowerText(TEXT("Luca: «Puoi rivedere l’immagine oppure usare la frase. Nessuna penalità: riprova quando vuoi.»"),16,Teal)]]
                +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[SNew(SBox).Visibility(Mnemonic?EVisibility::Visible:EVisibility::Collapsed)[TowerText(D->GetTowerMnemonic(),17,Teal)]]
                +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[TowerText(TCP?TEXT("Base: Network Access · cima: Application. Tasti 1, 2, 3 oppure clic sui pulsanti dei pioli."):TEXT("Base: Physical · cima: Application. Tasti 1, 2, 3 oppure clic sui pulsanti dei pioli."),14)]
            ]
        ]];
    }
};
}

TSharedRef<SWidget> MakeClaraLayerTower(AClaraStoryDirector* Director)
{
    return SNew(SClaraLayerTower).Director(Director);
}
