#include "ClaraFrontEnd.h"
#include "ClaraNarrativeCamera.h"
#include "ClaraStoryDirector.h"
#include "ClaraAudioSettings.h"
#include "Widgets/Input/SComboButton.h"
#include "ClaraCyberUI.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

namespace ClaraStoryUI
{
    const FLinearColor Ink(.065f,.14f,.16f,1.f), Muted(.31f,.40f,.42f,1.f);
    const FLinearColor Paper(.95f,.925f,.865f,.98f), Card(.99f,.975f,.93f,1.f);
    const FLinearColor Teal(.025f,.34f,.35f,1.f), Sand(.75f,.57f,.30f,1.f), Night(.025f,.06f,.075f,1.f);
    FSlateFontInfo Font(int32 Size,bool Bold=false)
    {
        return FCoreStyle::GetDefaultFontStyle(Bold?"Medium":"Regular",Size);
    }
    FSlateFontInfo TerminalFont(int32 Size)
    {
        static const auto Mono=MakeShared<FCompositeFont>(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansMono.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
        return FSlateFontInfo(Mono,Size);
    }
    TSharedRef<SWidget> Text(const FString& Value,int32 Size=18,FLinearColor Color=Ink,bool Bold=false)
    {
        return SNew(STextBlock).Text(FText::FromString(Value)).Font(Font(Size,Bold)).ColorAndOpacity(Color).AutoWrapText(true);
    }
    TSharedRef<SWidget> Live(TFunction<FString()> Getter,int32 Size=18,FLinearColor Color=Ink,bool Bold=false)
    {
        return SNew(STextBlock).Text_Lambda([Getter=MoveTemp(Getter)]{return FText::FromString(Getter());})
            .Font(Font(Size,Bold)).ColorAndOpacity(Color).AutoWrapText(true);
    }
    TSharedRef<SWidget> Label(const FString& Value,int32 Size=14,FLinearColor Color=Ink,bool Bold=false)
    {
        return SNew(STextBlock).Text(FText::FromString(Value)).Font(Font(Size,Bold)).ColorAndOpacity(Color).AutoWrapText(false);
    }
    TSharedRef<SWidget> LiveLabel(TFunction<FString()> Getter,int32 Size=14,FLinearColor Color=Ink,bool Bold=false)
    {
        return SNew(STextBlock).Text_Lambda([Getter=MoveTemp(Getter)]{return FText::FromString(Getter());})
            .Font(Font(Size,Bold)).ColorAndOpacity(Color).AutoWrapText(false);
    }
    TSharedRef<SWidget> Button(const FString& Caption,TFunction<void()> Action,bool Primary=false)
    {
        static const FButtonStyle Flat=FButtonStyle()
            .SetNormal(FSlateColorBrush(FLinearColor::White))
            .SetHovered(FSlateColorBrush(FLinearColor(.86f,.93f,.92f,1)))
            .SetPressed(FSlateColorBrush(FLinearColor(.72f,.83f,.81f,1)));
        return SNew(SButton).ButtonStyle(&Flat).ButtonColorAndOpacity(Primary?Teal:FLinearColor(.85f,.88f,.85f,1.f))
            .ForegroundColor(Primary?FLinearColor::White:Ink).ContentPadding(FMargin(16,11))
            .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
            [Label(Caption,15,Primary?FLinearColor::White:Ink,true)];
    }
    TSharedRef<SWidget> CardWrap(TSharedRef<SWidget> Content,FMargin Padding=FMargin(22))
    {
        return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(Card).Padding(Padding)[Content];
    }
    TSharedRef<SWidget> Heading(const FString& Kicker,const FString& Title)
    {
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,7)[Text(Kicker.ToUpper(),13,Teal,true)]
            +SVerticalBox::Slot().AutoHeight()[Text(Title,28,Ink,true)];
    }

    TSharedRef<SEditableTextBox> AnswerInput(TWeakObjectPtr<AClaraStoryDirector> D,int32 Size)
    {
        auto WeakInput=MakeShared<TWeakPtr<SEditableTextBox>>();
        TSharedRef<SEditableTextBox> Input=SNew(SEditableTextBox).Font(Font(Size))
            .HintText(FText::FromString(TEXT("Scrivi la tua risposta"))).ClearKeyboardFocusOnCommit(false)
            .OnTextCommitted_Lambda([D,WeakInput](const FText& Value,ETextCommit::Type Type)
            {
                if(Type!=ETextCommit::OnEnter||!D.IsValid())return;
                D->SubmitAnswer(Value.ToString());
                if(auto Widget=WeakInput->Pin())Widget->SetText(FText::GetEmpty());
            });
        *WeakInput=Input;return Input;
    }

    class SNetworkMap final:public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SNetworkMap){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
        void Construct(const FArguments& Args){Director=Args._Director;}
        FVector2D ComputeDesiredSize(float) const override{return FVector2D(760,440);}
        bool SupportsKeyboardFocus() const override{return true;}
        TArray<FClaraSimNode> Nodes() const
        {
            if(Director.IsValid())
            {
                auto Result=Director->GetSimNodes();
                if(!Result.IsEmpty())return Result;
            }
            TArray<FClaraSimNode> Result;
            auto Add=[&](const TCHAR* Id,const TCHAR* Label,const TCHAR* Kind,float X,float Y)
            {FClaraSimNode N;N.Id=Id;N.Label=Label;N.Kind=Kind;N.Position=FVector2D(X,Y);Result.Add(N);};
            Add(TEXT("LAPTOP"),TEXT("Portatile"),TEXT("pc"),160,250);
            Add(TEXT("SW1"),TEXT("SW1"),TEXT("switch"),500,250);
            Add(TEXT("PC2"),TEXT("PC2"),TEXT("pc"),840,250);
            return Result;
        }
        FVector2D Point(const FClaraSimNode& N,const FVector2D& Size)const
        {
            return FVector2D(70+FMath::Clamp(N.Position.X/1000.,0.,1.)*FMath::Max(0.,Size.X-140.),
                             65+FMath::Clamp(N.Position.Y/500.,0.,1.)*FMath::Max(0.,Size.Y-130.));
        }
        FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)override
        {
            if(Event.GetEffectingButton()!=EKeys::LeftMouseButton||!Director.IsValid())return FReply::Unhandled();
            const FVector2D Local=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
            for(const auto& N:Nodes())if((Point(N,Geometry.GetLocalSize())-Local).Size()<55)
            {Director->SelectSimNode(N.Id);return FReply::Handled().SetUserFocus(SharedThis(this));}
            return FReply::Unhandled();
        }
        int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,
            int32 Layer,const FWidgetStyle&,bool)const override
        {
            const auto NS=Nodes();const FVector2D Size=G.GetLocalSize();
            const FSlateBrush* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.91f,.925f,.89f,1));
            TMap<FString,FVector2D> Positions;for(const auto& N:NS)Positions.Add(N.Id,Point(N,Size));
            if(Director.IsValid())for(const auto& Link:Director->GetSimLinks())
            {
                const auto* A=Positions.Find(Link.Key);const auto* B=Positions.Find(Link.Value);if(!A||!B)continue;
                TArray<FVector2D> Points{*A,*B};
                FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Teal,true,4.f);
            }
            const FString Selected=Director.IsValid()?Director->GetSimSelected():FString();
            for(const auto& N:NS)
            {
                const FVector2D P=Point(N,Size), BoxSize(106,64), Origin=P-BoxSize/2;
                const bool IsSelected=N.Id==Selected;
                FSlateDrawElement::MakeBox(Out,Layer+2,G.ToPaintGeometry(BoxSize,FSlateLayoutTransform(Origin)),Brush,
                    ESlateDrawEffect::None,IsSelected?Sand:Teal);
                FString Symbol=N.Kind.Contains(TEXT("switch"))?TEXT("SW"):N.Kind.Contains(TEXT("router"))?TEXT("R"):N.Kind.Contains(TEXT("server"))?TEXT("SRV"):TEXT("PC");
                FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(85,30),FSlateLayoutTransform(P+FVector2D(-22,-15))),Symbol,Font(22,true),ESlateDrawEffect::None,FLinearColor::White);
                FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(180,30),FSlateLayoutTransform(P+FVector2D(-50,39))),N.Label,Font(18,true),ESlateDrawEffect::None,Ink);
                if(!N.IP.IsEmpty())FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(190,25),FSlateLayoutTransform(P+FVector2D(-58,63))),N.IP,Font(15),ESlateDrawEffect::None,Muted);
            }
            return Layer+4;
        }
    private:TWeakObjectPtr<AClaraStoryDirector> Director;
    };

    const FLinearColor TerminalBackground(FColor(14,17,20));
    const FLinearColor TerminalText(FColor(221,225,228));
    const FLinearColor TerminalMuted(FColor(147,157,165));
    const FLinearColor TerminalPrompt(FColor(112,200,149));
    const FButtonStyle& TerminalButtonStyle()
    {
        static const FButtonStyle Style=FButtonStyle()
            .SetNormal(FSlateColorBrush(FLinearColor(1,1,1,0)))
            .SetHovered(FSlateColorBrush(FLinearColor(1,1,1,.065f)))
            .SetPressed(FSlateColorBrush(FLinearColor(1,1,1,.10f)))
            .SetNormalPadding(FMargin(8,5)).SetPressedPadding(FMargin(8,5));
        return Style;
    }
    const FEditableTextBoxStyle& TerminalInputStyle()
    {
        static const FEditableTextBoxStyle Style=FEditableTextBoxStyle()
            .SetBackgroundImageNormal(FSlateColorBrush(TerminalBackground))
            .SetBackgroundImageHovered(FSlateColorBrush(TerminalBackground))
            .SetBackgroundImageFocused(FSlateColorBrush(TerminalBackground))
            .SetBackgroundImageReadOnly(FSlateColorBrush(TerminalBackground))
            .SetBackgroundColor(FLinearColor::White).SetForegroundColor(TerminalText)
            .SetFocusedForegroundColor(TerminalText).SetReadOnlyForegroundColor(TerminalText)
            .SetPadding(FMargin(1,4)).SetFont(TerminalFont(18));
        return Style;
    }
    TSharedRef<SWidget> TerminalLive(TFunction<FString()> Getter,int32 Size=18,FLinearColor Color=TerminalText,bool Wrap=true)
    {
        return SNew(STextBlock).Text_Lambda([Getter=MoveTemp(Getter)]{return FText::FromString(Getter());})
            .Font(TerminalFont(Size)).ColorAndOpacity(Color).AutoWrapText(Wrap);
    }
    TSharedRef<SWidget> TerminalButton(const FString& Label,TFunction<void()> Action,bool Primary=false)
    {
        return SNew(SButton).ButtonStyle(&TerminalButtonStyle()).ContentPadding(FMargin(8,5))
            .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
            [ClaraStoryUI::Label(Label,14,Primary?TerminalPrompt:TerminalMuted)];
    }

    class STerminal final:public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(STerminal){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
        void Construct(const FArguments& Args)
        {
            Director=Args._Director;
            ActiveTarget=Director.IsValid()?Director->GetTerminalTarget():FString();
            HistoryIndex=Director.IsValid()?Director->GetTerminalHistory().Num():0;
            TSharedRef<SHorizontalBox> Targets=SNew(SHorizontalBox);
            TArray<FString> Sessions{TEXT("LAPTOP")};
            if(Director.IsValid()&&Director->GetDay()==3)Sessions={TEXT("LAPTOP"),TEXT("PC2"),TEXT("R1"),TEXT("R2"),TEXT("SW2")};
            else if(Director.IsValid()&&Director->GetPhase()==109){Sessions.Add(TEXT("SW-A"));Sessions.Add(TEXT("SW-B"));}
            else Sessions.Add(Director.IsValid()&&Director->GetDay()==2?TEXT("SW-PROVA"):TEXT("SW1"));
            for(const FString& Id:Sessions)
                Targets->AddSlot().AutoWidth().Padding(0,0,12,0)[SNew(SButton)
                    .ButtonStyle(&TerminalButtonStyle()).ContentPadding(FMargin(8,5))
                    .OnClicked_Lambda([this,Id]{if(Director.IsValid()){if(Director->IsTerminalPasswordInput())Fill(TEXT(""),false);Director->SelectTerminalTarget(Id);SyncSession();Help.Reset();}FocusInput();return FReply::Handled();})[
                        SNew(STextBlock).Text(FText::FromString(Id==TEXT("PC2")?TEXT("PC2 · rete"):Id==TEXT("LAPTOP")?(Director.IsValid()&&Director->GetDay()==3?TEXT("Portatile · console"):TEXT("PC · Linux")):TEXT("Console · ")+Id))
                        .Font(Font(14)).ColorAndOpacity_Lambda([this,Id]{return Director.IsValid()&&Director->GetTerminalTarget()==Id?TerminalPrompt:TerminalMuted;})
                    ]];
            const auto D=Director;
            TSharedRef<SEditableTextBox> PortAnswer=AnswerInput(D,16);
            PortAnswer->SetStyle(&TerminalInputStyle());
            ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TerminalBackground).Padding(0)[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1)[Targets]
                    +SHorizontalBox::Slot().AutoWidth()[TerminalButton(TEXT("Manuale"),[this]{if(Director.IsValid())Help=Director->CommandHelp(TEXT(""));FocusInput();})]
                ]
                +SVerticalBox::Slot().FillHeight(1.f)[
                    SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[
                        SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Director.IsValid()?Director->GetTranscript():FString());})
                        .Font(TerminalFont(18)).ColorAndOpacity(TerminalText).AutoWrapText(true)
                    ]
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(0,9,0,7)[
                    SNew(SBox).MaxDesiredHeight(125).Visibility_Lambda([this]{return Help.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;})[
                        SNew(SScrollBox)+SScrollBox::Slot()[TerminalLive([this]{return Help;},16,TerminalMuted)]
                    ]
                ]
                +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[TerminalLive([this]{return Director.IsValid()?Director->GetTerminalPrompt():TEXT(">");},18,TerminalPrompt,false)]
                    +SHorizontalBox::Slot().FillWidth(1.f)[
                        SAssignNew(Input,SEditableTextBox).Style(&TerminalInputStyle()).Font(TerminalFont(18)).MinDesiredWidth(250).ClearKeyboardFocusOnCommit(false)
                        .IsPassword_Lambda([D]{return D.IsValid()&&D->IsTerminalPasswordInput();})
                        .IsReadOnly_Lambda([D]{return D.IsValid()&&D->IsTerminalBusy();})
                        .HintText_Lambda([D]{return FText::FromString(D.IsValid()&&D->IsTerminalBusy()?TEXT("Ping in corso · Ctrl+C per interrompere"):D.IsValid()&&D->IsTerminalPasswordInput()?TEXT("Password del laboratorio"):TEXT("Scrivi un comando · help per iniziare"));})
                        .OnTextCommitted(this,&STerminal::Commit)
                        .OnTextChanged(this,&STerminal::Changed)
                        .OnKeyDownHandler(this,&STerminal::KeyDown)
                    ]
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)[
                    SNew(SBox).Visibility_Lambda([D]{return D.IsValid()&&(D->GetPhase()==10||D->GetPhase()==15)&&!D->GetQuestion().IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;})[
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[TerminalLive([D]{return D.IsValid()?D->GetQuestion():FString();},16,TerminalMuted)]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[Label(TEXT("Porta >"),16,TerminalPrompt)]
                            +SHorizontalBox::Slot().FillWidth(1)[PortAnswer]
                            +SHorizontalBox::Slot().AutoWidth().Padding(10,0,0,0)[TerminalButton(TEXT("Verifica"),[D,PortAnswer]{if(D.IsValid()){D->SubmitAnswer(PortAnswer->GetText().ToString());PortAnswer->SetText(FText::GetEmpty());}},true)]
                        ]
                    ]
                ]
            ]];
        }
        void Tick(const FGeometry& G,const double Time,const float Delta)override
        {
            SCompoundWidget::Tick(G,Time,Delta);
            if(Director.IsValid())
            {
                if(!Director->GetPage().IsEmpty())InitialFocusPending=true;
                if(InitialFocusPending&&Director->GetPage().IsEmpty())
                {
                    InitialFocusPending=false;
                    FocusInput();
                }
                SyncSession();
                const bool Password=Director->IsTerminalPasswordInput();
                if(Password!=PasswordMode){PasswordMode=Password;Drafts.Remove(ActiveTarget);Help.Reset();Fill(TEXT(""),false);}
                const FString Current=Director->GetTranscript();
                if(Current!=LastTranscript){LastTranscript=Current;if(Scroll.IsValid())Scroll->ScrollToEnd();}
            }
        }
    private:
        TWeakObjectPtr<AClaraStoryDirector> Director;
        TSharedPtr<SEditableTextBox> Input;
        TSharedPtr<SScrollBox> Scroll;
        TMap<FString,FString> Drafts;
        int32 HistoryIndex=0;
        FString Help,LastTranscript,ActiveTarget;
        bool Updating=false,InitialFocusPending=true,PasswordMode=false;
        void FocusInput(){if(Input.IsValid())FSlateApplication::Get().SetKeyboardFocus(Input);}
        void Fill(const FString& Value,bool Focus=true){if(!Input.IsValid())return;Updating=true;Input->SetText(FText::FromString(Value));Input->GoTo(ETextLocation::EndOfDocument);Updating=false;if(Focus)FocusInput();}
        void SyncSession()
        {
            if(!Director.IsValid()||ActiveTarget==Director->GetTerminalTarget())return;
            if(Input.IsValid()&&!PasswordMode)Drafts.Add(ActiveTarget,Input->GetText().ToString());
            ActiveTarget=Director->GetTerminalTarget();HistoryIndex=Director->GetTerminalHistory().Num();Help.Reset();
            Fill(Drafts.FindRef(ActiveTarget),Director->GetPage().IsEmpty());
        }
        void Submit()
        {
            if(!Director.IsValid()||!Input.IsValid())return;
            SyncSession();if(Director->IsTerminalBusy())return;const bool PasswordInput=Director->IsTerminalPasswordInput();
            FString Command=Input->GetText().ToString();if(!PasswordInput)Command.TrimStartAndEndInline();if(Command.IsEmpty()&&!PasswordInput&&Director->GetTerminalTarget()==TEXT("LAPTOP"))return;
            Director->RunCommand(Command);PasswordMode=Director->IsTerminalPasswordInput();HistoryIndex=Director->GetTerminalHistory().Num();
            Drafts.Remove(ActiveTarget);Help.Reset();Fill(TEXT(""),Director->GetPage().IsEmpty());
            if(Scroll.IsValid())Scroll->ScrollToEnd();
        }
        void Commit(const FText&,ETextCommit::Type Type){if(Type==ETextCommit::OnEnter)Submit();}
        void Changed(const FText& Value)
        {
            if(Updating||!Director.IsValid()||Director->IsTerminalReadingBanner()||Director->IsTerminalPasswordInput()||Director->IsTerminalBusy())return;
            int32 Index;const FString TextValue=Value.ToString();
            if(TextValue.TrimStart().StartsWith(TEXT("banner motd "),ESearchCase::IgnoreCase))return;
            if(TextValue.FindChar(TEXT('?'),Index)){const FString Prefix=TextValue.Left(Index);Help=Director->CommandHelp(Prefix);Fill(Prefix);}
        }
        FReply KeyDown(const FGeometry&,const FKeyEvent& Event)
        {
            if(!Director.IsValid()||!Input.IsValid())return FReply::Unhandled();
            if(Event.GetKey()==EKeys::C&&Event.IsControlDown()&&Director->CancelTerminalCommand()){Help.Reset();Fill(TEXT(""));return FReply::Handled();}
            if((Director->IsTerminalPasswordInput()||Director->IsTerminalBusy())&&(Event.GetKey()==EKeys::Tab||Event.GetKey()==EKeys::Up||Event.GetKey()==EKeys::Down))return FReply::Handled();
            if(Event.GetKey()==EKeys::Tab)
            {
                const FString Prefix=Input->GetText().ToString(),Complete=Director->CompleteCommand(Prefix);
                if(Complete==Prefix)Help=Director->CommandHelp(Prefix);
                Fill(Complete);return FReply::Handled();
            }
            if(Event.GetKey()==EKeys::Up||Event.GetKey()==EKeys::Down)
            {
                const auto History=Director->GetTerminalHistory();
                if(HistoryIndex==History.Num())Drafts.Add(ActiveTarget,Input->GetText().ToString());
                HistoryIndex=FMath::Clamp(HistoryIndex+(Event.GetKey()==EKeys::Up?-1:1),0,History.Num());
                Fill(HistoryIndex<History.Num()?History[HistoryIndex]:Drafts.FindRef(ActiveTarget));return FReply::Handled();
            }
            return FReply::Unhandled();
        }
    };

    TSharedRef<SWidget> ModalFrame(const FString& Kicker,const FString& Title,TSharedRef<SWidget> Body,TWeakObjectPtr<AClaraStoryDirector> D)
    {
        return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Paper).Padding(28)[
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[Heading(Kicker,Title)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[Button(TEXT("Chiudi · Esc"),[D]{if(D.IsValid())D->ClosePage();})]
            ]
            +SVerticalBox::Slot().FillHeight(1)[Body]
        ];
    }

    TSharedRef<SWidget> ActivityBody(TWeakObjectPtr<AClaraStoryDirector> D,const FString& Page)
    {
            if(Page==TEXT("puzzle")&&D.IsValid()&&D->IsTowerPuzzle())return MakeClaraLayerTower(D.Get());
            TSharedRef<SVerticalBox> Body=SNew(SVerticalBox);
            if(Page==TEXT("notebook"))
            {
                Body->AddSlot().AutoHeight()[Live([D]{return D.IsValid()?D->GetNotebookText():FString();},19)];
            }
            else if(Page==TEXT("inventory"))
            {
                Body->AddSlot().AutoHeight()[Text(TEXT("Scegli un oggetto dalla borsa."))];
            }
            else if(Page==TEXT("sorting"))
            {
                Body->AddSlot().AutoHeight().Padding(0,0,0,14)[Text(TEXT("Esamina forma, etichette e funzione. I colori di questi cavi appartengono al laboratorio: non sono uno standard universale."),18,Muted)];
                struct FCable {const TCHAR* Kind;const TCHAR* Label;};
                for(const FCable Cable: {FCable{TEXT("ethernet"),TEXT("Bianco · CAT 6 · dati Ethernet")},FCable{TEXT("console"),TEXT("Azzurro · USB–RJ45 · console")},FCable{TEXT("fiber"),TEXT("Giallo · LC · fibra ottica")},FCable{TEXT("coffee"),TEXT("Nero · alimentazione")}})
                {
                    const FString Kind=Cable.Kind;
                    Body->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1)[Button(Cable.Label,[D,Kind]{if(D.IsValid())D->SelectSortingCable(Kind);})]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14,0)[LiveLabel([D,Kind]{return D.IsValid()&&D->IsCableSorted(Kind)?TEXT("Classificato"):TEXT("Da esaminare");},16,Teal)]];
                }
                Body->AddSlot().AutoHeight().Padding(0,16,0,12)[Live([D]{return FString(TEXT("Cavo selezionato: "))+(D.IsValid()?D->GetSortingCable():FString());},16,Teal)];
                TSharedRef<SHorizontalBox> Bins=SNew(SHorizontalBox);
                for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("ethernet"),TEXT("Scatola Ethernet")},{TEXT("console"),TEXT("Scatola console")},{TEXT("fiber"),TEXT("Scatola fibra")},{TEXT("coffee"),TEXT("Restituisci a Luca")}})
                    Bins->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(Pair.Value,[D,Kind=Pair.Key]{if(D.IsValid())D->SortCable(Kind);},true)];
                Body->AddSlot().AutoHeight()[Bins];
            }
            else if(Page==TEXT("puzzle"))
            {
                Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Metti i passaggi nell’ordine che descrive il fenomeno. Puoi spostare una carta e controllare la sequenza senza penalità."),18,Muted)];
                const auto Items=D->GetPuzzleItems();
                for(int32 I=0;I<Items.Num();++I)
                {
                    const FLinearColor RowColor=I%2?FLinearColor(.86f,.91f,.87f,1):FLinearColor(.94f,.88f,.75f,1);
                    Body->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(RowColor).Padding(12)[
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,14,0)[Label(FString::FromInt(I+1),24,Teal,true)]
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Live([D,I]{const auto Values=D.IsValid()?D->GetPuzzleItems():TArray<FString>();return Values.IsValidIndex(I)?Values[I]:FString();},17,Ink)]
                        +SHorizontalBox::Slot().AutoWidth().Padding(10,0,6,0)[Button(TEXT("Su"),[D,I]{if(D.IsValid())D->MovePuzzleItem(I,-1);})]
                        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Giù"),[D,I]{if(D.IsValid())D->MovePuzzleItem(I,1);})]
                    ]];
                }
                Body->AddSlot().AutoHeight().Padding(0,12,0,0)[Button(TEXT("Verifica la sequenza"),[D]{if(D.IsValid())D->CheckPuzzle();},true)];
            }
            else if(Page==TEXT("subnet"))
            {
                TSharedRef<SEditableTextBox> Answer=AnswerInput(D,23);
                Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Live([D]{return D.IsValid()?D->GetQuestion():FString();},20,Ink,true)];
                Body->AddSlot().AutoHeight().Padding(0,0,0,20)[Live([D]{return D.IsValid()?D->GetAnswerHint():FString();},18,Muted)];
                Body->AddSlot().AutoHeight().Padding(0,0,0,15)[Answer];
                Body->AddSlot().AutoHeight()[Button(TEXT("Controlla il passaggio"),[D,Answer]{if(D.IsValid()){D->SubmitAnswer(Answer->GetText().ToString());Answer->SetText(FText::GetEmpty());}},true)];
            }
            else if(Page==TEXT("report"))
            {
                Body->AddSlot().AutoHeight().Padding(0,0,0,14)[Text(D->GetDay()==1?TEXT("Bel lavoro oggi: cavi in ordine, rete su, MAC letto, ping fatto."):TEXT("Rete su. Configurazioni salvate, anche dopo il riavvio."),23,Ink,true)];
                TSharedRef<SWrapBox> Next=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12,10));
                if(D->GetPhase()==16)Next->AddSlot()[Button(TEXT("Inizia il Giorno 2"),[D]{if(D.IsValid())D->BeginNextDayTransition();},true)];
                if(D->GetPhase()==110)Next->AddSlot()[Button(TEXT("Continua → Giorno 3"),[D]{if(D.IsValid())D->BeginNextDayTransition();},true)];
                Next->AddSlot()[Button(TEXT("Nuova partita dal Giorno 1"),[D]{if(D.IsValid())D->RestartStory();})];
                Body->AddSlot().AutoHeight().Padding(0,0,0,18)[Next];
                Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Le prove contano: un collegamento fisico, una risposta di rete e un accesso autenticato verificano cose diverse. Conserva il metodo, non soltanto la sequenza dei comandi."),17,Muted)];
                Body->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("I TUOI APPUNTI"),13,Muted,true)];
                Body->AddSlot().AutoHeight()[Live([D]{return D.IsValid()?D->GetNotebookText():FString();},17)];
            }
            else Body->AddSlot().AutoHeight()[Live([D]{return D.IsValid()?D->GetObjective():FString();},20)];
            return SNew(SScrollBox)+SScrollBox::Slot()[Body];
        }


    class SNotebookPaper final:public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SNotebookPaper){} SLATE_DEFAULT_SLOT(FArguments,Content) SLATE_END_ARGS()
        void Construct(const FArguments& Args){ChildSlot[Args._Content.Widget];}
        int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled)const override
        {
            const FVector2D Size=G.GetLocalSize();const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Brush,ESlateDrawEffect::None,FLinearColor(.19f,.14f,.105f,1));
            for(int32 Page=0;Page<2;++Page)
            {
                const float X=Page==0?14.f:Size.X/2+9;
                const FVector2D Origin(X,12),Extent(Size.X/2-23,Size.Y-24);
                FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(Extent,FSlateLayoutTransform(Origin)),Brush,ESlateDrawEffect::None,FLinearColor(.96f,.94f,.85f,1));
                for(float Y=102;Y<Size.Y-26;Y+=31)
                {TArray<FVector2D> Points{FVector2D(X+16,Y),FVector2D(X+Extent.X-15,Y)};FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor(.42f,.56f,.63f,.22f),true,1);}
                TArray<FVector2D> Margin{FVector2D(X+35,80),FVector2D(X+35,Size.Y-30)};
                FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Margin,ESlateDrawEffect::None,FLinearColor(.66f,.22f,.19f,.32f),true,1);
            }
            for(float Y=58;Y<Size.Y-32;Y+=48)
                FSlateDrawElement::MakeBox(Out,Layer+3,G.ToPaintGeometry(FVector2D(29,5),FSlateLayoutTransform(FVector2D(Size.X/2-14,Y))),Brush,ESlateDrawEffect::None,FLinearColor(.48f,.47f,.42f,1));
            return SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer+4,Style,ParentEnabled);
        }
    };
    class SItemSketch final:public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SItemSketch){} SLATE_ARGUMENT(FString,Item) SLATE_END_ARGS()
        void Construct(const FArguments& A){Item=A._Item;}
        FVector2D ComputeDesiredSize(float)const override{return {110,64};}
        int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& O,int32 L,const FWidgetStyle&,bool)const override
        {
            const auto* B=FCoreStyle::Get().GetBrush("WhiteBrush");const FVector2D C=G.GetLocalSize()/2;
            auto Box=[&](FVector2D P,FVector2D S,FLinearColor Color){FSlateDrawElement::MakeBox(O,L,G.ToPaintGeometry(S,FSlateLayoutTransform(C+P)),B,ESlateDrawEffect::None,Color);};
            if(Item==TEXT("laptop"))
            {Box({-43,-31},{86,54},FLinearColor(.11,.14,.16));Box({-38,-26},{76,43},FLinearColor(.17,.35,.42));Box({-50,23},{100,7},FLinearColor(.55,.58,.60));}
            else if(Item==TEXT("badge"))
            {Box({-22,-31},{44,65},FLinearColor(.86,.86,.77));Box({-22,-31},{44,12},Teal);Box({-12,-12},{13,17},Muted);Box({6,-8},{10,3},Teal);Box({-13,14},{27,3},Muted);}
            else if(Item==TEXT("sfp_module"))
            {Box({-39,-10},{75,21},FLinearColor(.56,.58,.58));Box({26,-10},{15,21},FLinearColor(.10,.16,.18));Box({-44,-7},{14,15},FLinearColor(.75,.66,.32));}
            else
            {
                FLinearColor Color=Item==TEXT("console")?FLinearColor(.18,.57,.78):Item==TEXT("fiber")?FLinearColor(.89,.69,.16):FLinearColor(.77,.80,.77);
                TArray<FVector2D> Points;
                for(int32 I=0;I<=90;++I){const float A=I*.17f,R=17.f+I*.19f;Points.Add(C+FVector2D(FMath::Cos(A)*R,FMath::Sin(A)*R*.62f));}
                FSlateDrawElement::MakeLines(O,L,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,4);
                Box({-46,-6},{12,15},Color);Box({36,10},{13,11},Color);
            }
            return L+1;
        }
    private:FString Item;
    };
    class SStoryPanel final:public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SStoryPanel){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
        void Construct(const FArguments& Args){Director=Args._Director;Rebuild();}
        bool SupportsKeyboardFocus()const override{return true;}
        FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)override
        {
            if(Director.IsValid()&&Director->GetPage()==TEXT("dialogue"))
            {
                const FKey Key=Event.GetKey();
                if(Key==EKeys::V){if(auto* PC=Director->GetWorld()->GetFirstPlayerController())UClaraNarrativeCamera::Get(PC)->ToggleReducedMotion();}
                else if(Key==EKeys::Escape)Director->FinishDialogue();
                else if(Key==EKeys::Enter||Key==EKeys::SpaceBar||Key==EKeys::E)Director->ContinueDialogue();
                else if(Key==EKeys::Tab)return FReply::Unhandled();
                return FReply::Handled();
            }
            return SCompoundWidget::OnKeyDown(Geometry,Event);
        }
        void Tick(const FGeometry& G,double Time,float Delta)override
        {
            SCompoundWidget::Tick(G,Time,Delta);if(!Director.IsValid())return;
            FString Next=Director->GetPage()+TEXT("|")+Director->GetDialogue()+TEXT("|")+FString::FromInt(Director->GetPhase());
            for(const auto& Choice:Director->GetChoices())Next+=TEXT("|")+Choice;
            if(Director->GetPage()==TEXT("notebook"))Next+=TEXT("|")+FString::FromInt(NotebookPage)+TEXT("|")+Director->GetNotebookText();
            if(Next!=Signature){Signature=Next;Rebuild();}
            if(Director->GetPage()==TEXT("dialogue")&&!DialogueFocused)
            {
                DialogueFocused=true;
                FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
            }
        }
    private:
        TWeakObjectPtr<AClaraStoryDirector> Director;
        FString Signature,SelectedItem=TEXT("laptop");
        bool DialogueFocused=false;
        int32 NotebookPage=0,NotebookChapterPage=1;
        FString NotebookTitle,NotebookContent,NotebookChapter;
        TSharedRef<SWidget> Notebook()
        {
            auto D=Director;
            FString Notes=D->GetNotebookText();
            Notes.RemoveFromStart(D->GetTitle()+TEXT("\n\n"));
            TArray<FString> Blocks;Notes.ParseIntoArray(Blocks,TEXT("\n\n"),true);
            struct FNotebookPage { FString Chapter,Body; int32 ChapterPage=1; };
            TArray<FNotebookPage> Pages;
            TArray<TPair<FString,int32>> Chapters;
            const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            // Match the fixed paper column; keep each sheet short without shrinking the font.
            constexpr float TextWidth=416.f;
            constexpr int32 LinesPerPage=9;
            auto AddChapter=[&](const FString& Title,const FString& Body)
            {
                Chapters.Add({Title,Pages.Num()});
                TArray<FString> Paragraphs;Body.ParseIntoArrayLines(Paragraphs,false);
                FString Sheet;int32 Lines=0,ChapterPage=1;
                auto AddLine=[&](const FString& Line)
                {
                    if(Lines==LinesPerPage)
                    {
                        Pages.Add({Title,Sheet.TrimEnd(),ChapterPage++});
                        Sheet.Empty();Lines=0;
                    }
                    if(Line.IsEmpty()&&Lines==0)return;
                    if(Lines>0)Sheet+=TEXT("\n");
                    Sheet+=Line;++Lines;
                };
                for(const FString& Paragraph:Paragraphs)
                {
                    TArray<FString> Words;Paragraph.ParseIntoArrayWS(Words);
                    FString Line;
                    for(const FString& Word:Words)
                    {
                        const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
                        if(!Line.IsEmpty()&&Measure->Measure(Candidate,Font(18)).X>TextWidth)
                        {AddLine(Line);Line=Word;}
                        else Line=Candidate;
                    }
                    if(!Line.IsEmpty())AddLine(Line);
                    AddLine(TEXT(""));
                }
                if(!Sheet.TrimStartAndEnd().IsEmpty())Pages.Add({Title,Sheet.TrimEnd(),ChapterPage});
                if(Pages.Num()==Chapters.Last().Value)Pages.Add({Title,TEXT("Le osservazioni di questo capitolo compariranno qui."),1});
#if DO_CHECK
                TArray<FString> OriginalWords,PrintedWords;
                Body.ParseIntoArrayWS(OriginalWords);
                for(int32 I=Chapters.Last().Value;I<Pages.Num();++I)
                {
                    TArray<FString> Words;Pages[I].Body.ParseIntoArrayWS(Words);PrintedWords.Append(Words);
                }
                check(OriginalWords.IsEmpty()||OriginalWords==PrintedWords);
#endif
            };
            for(const FString& Block:Blocks)
            {
                int32 Break=INDEX_NONE;
                if(Block.FindChar(TEXT('\n'),Break))AddChapter(Block.Left(Break),Block.Mid(Break+1));
                else AddChapter(TEXT("APPUNTI"),Block);
            }
            if(Pages.IsEmpty())AddChapter(TEXT("APPUNTI"),TEXT("Le osservazioni raccolte durante le attività compariranno qui."));
            AddChapter(TEXT("DA RICORDARE"),TEXT("Osserva le prese.\nRicostruisci il percorso.\nProva, poi leggi la risposta.\nUn errore è un indizio."));
            if(NotebookTitle!=D->GetTitle()){NotebookTitle=D->GetTitle();NotebookPage=0;}
            else if(NotebookContent!=Notes&&!NotebookChapter.IsEmpty())
            {
                for(int32 I=0;I<Pages.Num();++I)
                    if(Pages[I].Chapter==NotebookChapter&&Pages[I].ChapterPage<=NotebookChapterPage)NotebookPage=I;
            }
            NotebookContent=Notes;
            NotebookPage=FMath::Clamp(NotebookPage,0,Pages.Num()-1);
            const int32 Count=Pages.Num();
            const FNotebookPage& Current=Pages[NotebookPage];
            NotebookChapter=Current.Chapter;NotebookChapterPage=Current.ChapterPage;
            int32 ChapterIndex=0;
            for(int32 I=0;I<Chapters.Num();++I)if(Chapters[I].Value<=NotebookPage)ChapterIndex=I;
            const int32 ChapterEnd=Chapters.IsValidIndex(ChapterIndex+1)?Chapters[ChapterIndex+1].Value:Count;
            static const FButtonStyle PaperButton=FButtonStyle()
                .SetNormal(FSlateColorBrush(FLinearColor::White))
                .SetHovered(FSlateColorBrush(FLinearColor(.87f,.93f,.88f)))
                .SetPressed(FSlateColorBrush(FLinearColor(.74f,.84f,.78f)));
            const auto PageButton=[&](const FString& Caption,int32 Index,bool Enabled=true,bool Active=false)
            {
                return SNew(SButton).ButtonStyle(&PaperButton).IsEnabled(Enabled)
                    .ButtonColorAndOpacity(Active?FLinearColor(.77f,.87f,.79f):FLinearColor(.96f,.94f,.85f))
                    .ContentPadding(FMargin(10,10)).ToolTipText(FText::FromString(Caption))
                    // Rebuild on the following Tick, after Slate finishes this input callback.
                    .OnClicked_Lambda([this,Index]{NotebookPage=Index;return FReply::Handled();})
                    [SNew(STextBlock).Text(FText::FromString(Caption)).Font(Font(15,Active))
                        .ColorAndOpacity(Ink).AutoWrapText(false).OverflowPolicy(ETextOverflowPolicy::Ellipsis)];
            };
            auto Index=SNew(SVerticalBox);
            TSharedPtr<SWidget> SelectedChapter;
            for(int32 I=0;I<Chapters.Num();++I)
            {
                auto ChapterButton=PageButton(FString::Printf(TEXT("%02d   %s"),I+1,*Chapters[I].Key),Chapters[I].Value,true,I==ChapterIndex);
                Index->AddSlot().AutoHeight().Padding(0,0,0,4)[ChapterButton];
                if(I==ChapterIndex)SelectedChapter=ChapterButton;
            }
            auto IndexScroll=SNew(SScrollBox)+SScrollBox::Slot()[Index];
            IndexScroll->ScrollDescendantIntoView(SelectedChapter,false);
            auto Contents=SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("TNES / APPUNTI DI LABORATORIO"),12,Muted,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)[Text(D->GetTitle(),22,Ink,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("Capitoli"),18,Teal,true)]
                +SVerticalBox::Slot().FillHeight(1)[IndexScroll]
                +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)[Text(TEXT("Scegli un capitolo. Le note si aggiungono mentre impari."),13,Muted)];
            return SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(30)[
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[SNew(SBox).WidthOverride(1060).HeightOverride(650)[SNew(SNotebookPaper)[
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1).Padding(58,38,34,34)[Contents]
                    +SHorizontalBox::Slot().FillWidth(1).Padding(50,32,34,30)[SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("TACCUINO PERSONALE"),12,Muted,true)]
                            +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Chiudi"),[D]{if(D.IsValid())D->ClosePage();})]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Text(Current.Chapter,22,Ink,true)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[Text(
                            FString::Printf(TEXT("Capitolo %d · pagina %d di %d"),ChapterIndex+1,Current.ChapterPage,ChapterEnd-Chapters[ChapterIndex].Value),13,Muted)]
                        +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[
                            SNew(STextBlock).Text(FText::FromString(Current.Body))
                                .Font(Font(18)).ColorAndOpacity(Ink).WrapTextAt(TextWidth).LineHeightPercentage(1.25f)]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,10)[SNew(SSeparator)]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().AutoWidth()[PageButton(TEXT("Precedente"),NotebookPage-1,NotebookPage>0)]
                            +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[Text(FString::Printf(TEXT("%d / %d"),NotebookPage+1,Count),14,Muted)]
                            +SHorizontalBox::Slot().AutoWidth()[PageButton(TEXT("Successiva"),NotebookPage+1,NotebookPage+1<Count)]]]
                ]]]];
        }
        TSharedRef<SWidget> Inventory()
        {
            auto D=Director;
            const auto Items=D->GetItems();
            if(!Items.ContainsByPredicate([this](const FClaraStoryItem& Item){return Item.Id==SelectedItem;})&&!Items.IsEmpty())SelectedItem=Items[0].Id;
            static const FButtonStyle ItemStyle=FButtonStyle()
                .SetNormal(FSlateColorBrush(FLinearColor(.075f,.105f,.11f)))
                .SetHovered(FSlateColorBrush(FLinearColor(.13f,.23f,.23f)))
                .SetPressed(FSlateColorBrush(FLinearColor(.09f,.29f,.28f)))
                .SetNormalPadding(FMargin(10)).SetPressedPadding(FMargin(10));
            const FLinearColor Selection(.32f,.78f,.71f),Quiet(.70f,.77f,.76f);
            auto Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(5));
            FClaraStoryItem Selected;int32 Index=0,OwnedCount=0;
            for(const auto& Item:Items)
            {
                if(Item.Owned)++OwnedCount;
                const bool Active=Item.Id==SelectedItem;
                if(Active)Selected=Item;
                Grid->AddSlot(Index%2,Index/2)[SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(Active?Selection:FLinearColor(.19f,.25f,.26f)).Padding(2)[
                    SNew(SButton).ButtonStyle(&ItemStyle).ContentPadding(10)
                    .ToolTipText(FText::FromString(Item.Label))
                    .OnClicked_Lambda([this,Id=Item.Id]{SelectedItem=Id;Rebuild();return FReply::Handled();})[
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[SNew(SBox).WidthOverride(100).HeightOverride(80)[SNew(SItemSketch).Item(Item.Id)]]
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,7)[SNew(STextBlock).Text(FText::FromString(Item.Label)).Font(Font(14)).ColorAndOpacity(FLinearColor::White).AutoWrapText(false).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText(FText::FromString(Item.Label))]
                            +SVerticalBox::Slot().AutoHeight()[Text(Item.Owned?TEXT("NEL KIT"):TEXT("DA RACCOGLIERE"),11,Item.Owned?Selection:Quiet)]]
                    ]]];++Index;
            }
            auto Details=SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(Selected.Owned?TEXT("PRONTO ALL’USO"):TEXT("DA RACCOGLIERE"),12,Selection,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Text(Selected.Label,20,FLinearColor::White,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[Text(Selected.Detail,15,Quiet)];
            if(!Selected.Owned)
                Details->AddSlot().AutoHeight()[Text(TEXT("Segui l’obiettivo per aggiungerlo al tuo kit."),15,Quiet)];
            else if(Selected.Id==TEXT("laptop"))
            {
                Details->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("Apri portatile"),[D]{if(D.IsValid()){D->ClosePage();D->OpenLaptopScreen();}},true)];
                Details->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("Esamina le porte"),[D]{if(D.IsValid())D->UseItem(TEXT("inspect_laptop"));})];
                if(!D->IsCarryingLaptop())Details->AddSlot().AutoHeight()[Button(TEXT("Prendi e porta con te"),[D]{if(D.IsValid())D->UseItem(TEXT("laptop"));})];
            }
            else if(Selected.Id==TEXT("badge"))Details->AddSlot().AutoHeight()[Text(TEXT("Il badge resta nella tua borsa."),15,Quiet)];
            else Details->AddSlot().AutoHeight()[Button(Selected.Id==TEXT("sfp_module")?TEXT("Prendi modulo SFP"):TEXT("Prendi cavo"),[D,Id=Selected.Id]{if(D.IsValid())D->UseItem(Id);},true)];
            return SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(32)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[SNew(SBox).WidthOverride(1120).HeightOverride(620)[
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.045f,.065f,.068f,.98f)).Padding(28)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("LA TUA BORSA"),23,FLinearColor::White,true)]
                        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Chiudi · I / Esc"),[D]{if(D.IsValid())D->ClosePage();})]]
                    +SVerticalBox::Slot().AutoHeight().Padding(5,0,0,16)[Text(FString::Printf(TEXT("%d / %d oggetti nel kit · Seleziona un oggetto per usarlo o conoscerlo."),OwnedCount,Items.Num()),15,Quiet)]
                    +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1.8f).Padding(0,0,22,0)[Grid]
                        +SHorizontalBox::Slot().FillWidth(1).Padding(14,8,0,0)[SNew(SScrollBox)+SScrollBox::Slot()[Details]]]
                ]]]];
        }
        void Rebuild()
        {
            if(!Director.IsValid()){ChildSlot[SNew(SBox)];return;}
            auto D=Director;const FString Page=D->GetPage();
            if(Page.IsEmpty()){ChildSlot[SNew(SBox)];return;}
            if(Page==TEXT("notebook")){ChildSlot[Notebook()];return;}
            if(Page==TEXT("inventory")){ChildSlot[Inventory()];return;}
            if(Page==TEXT("day3report")){ChildSlot[MakeClaraDayThreeReport(D.Get())];return;}
            if(Page==TEXT("report")&&(D->GetPhase()==16||D->GetPhase()==110)){ChildSlot[MakeClaraDayReport(D.Get())];return;}
            if(Page==TEXT("daytransition")){ChildSlot[MakeClaraDayTransition(D.Get())];return;}
            if(Page==TEXT("dialogue"))
            {
                // Battuta in basso, scura e compatta: si legge sopra la scena e sopra il terminale senza coprirli.
                static const FButtonStyle Ghost=FButtonStyle()
                    .SetNormal(FSlateColorBrush(FLinearColor(1,1,1,.10f))).SetHovered(FSlateColorBrush(FLinearColor(1,1,1,.22f))).SetPressed(FSlateColorBrush(FLinearColor(1,1,1,.30f)));
                const FLinearColor Speech(.95f,.96f,.94f,1.f),Voice(.42f,.84f,.78f,1.f);
                const auto Choice=[Speech](const FString& Label,TFunction<void()> Action)
                {
                    return SNew(SButton).ButtonStyle(&Ghost).ContentPadding(FMargin(18,9))
                        .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})[SNew(SBox).MaxDesiredWidth(760)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(16)).ColorAndOpacity(Speech).AutoWrapText(false).ToolTipText(FText::FromString(Label))]]];
                };
                const auto Choices=D->GetChoices();
                TSharedRef<SWrapBox> Actions=SNew(SWrapBox).UseAllottedSize(true)
                    .HAlign(Choices.IsEmpty()?HAlign_Right:HAlign_Left).InnerSlotPadding(FVector2D(10,8));
                if(Choices.IsEmpty())Actions->AddSlot()[Choice(TEXT("Continua · Invio"),[D]{if(D.IsValid())D->ContinueDialogue();})];
                else for(int32 I=0;I<Choices.Num();++I)Actions->AddSlot()[Choice(Choices[I],[D,I]{if(D.IsValid())D->Choose(I);})];
                ChildSlot[SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(38,0,38,30)[
                    SNew(SBox).WidthOverride(900)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(5)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Voice)]]
                        +SHorizontalBox::Slot().FillWidth(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.012f,.03f,.038f,.94f)).Padding(FMargin(24,16,20,14))[
                            SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Live([D]{return D.IsValid()?D->GetSpeaker():FString();},15,Voice,true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(SBox).MaxDesiredHeight(220)[SNew(SScrollBox)
                                +SScrollBox::Slot()[Live([D]{return D.IsValid()?D->GetDialogue():FString();},21,Speech)]]]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(SBox)
                                .Visibility_Lambda([D]{return D.IsValid()&&D->IsScreenOpen()&&D->GetScreenPage()==TEXT("terminal")?EVisibility::HitTestInvisible:EVisibility::Collapsed;})[
                                    Text(TEXT("Continua il dialogo per tornare al terminale."),13,FLinearColor(.68f,.76f,.76f))]]
                            +SVerticalBox::Slot().AutoHeight()[Actions]]]]
                ]];return;
            }
            const FString Title=Page==TEXT("notebook")?TEXT("Il tuo taccuino"):Page==TEXT("inventory")?TEXT("Inventario"):Page==TEXT("sorting")?TEXT("Riconosci e ordina i cavi"):Page==TEXT("puzzle")?TEXT("Una sequenza da capire"):Page==TEXT("subnet")?TEXT("Indirizzi, un passo alla volta"):TEXT("Rapporto di fine giornata");
            ChildSlot[SNew(SOverlay)+SOverlay::Slot().Padding(36)[
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[
                    SNew(SBox).WidthOverride(1010).HeightOverride(690)[ModalFrame(TEXT("Network Vision / Affiancamento"),Title,ActivityBody(D,Page),D)]]
            ]];
        }
    };

    const FLinearColor DesktopInk(.89f,.92f,.92f), DesktopMuted(.57f,.65f,.65f);
    const FLinearColor DesktopChrome(.035f,.055f,.065f), DesktopWallpaper(.045f,.095f,.105f);
    const FButtonStyle& DesktopButtonStyle()
    {
        static const FButtonStyle Style=FButtonStyle()
            .SetNormal(FSlateColorBrush(FLinearColor(1,1,1,0)))
            .SetHovered(FSlateColorBrush(FLinearColor(1,1,1,.09f)))
            .SetPressed(FSlateColorBrush(FLinearColor(1,1,1,.16f)))
            .SetNormalPadding(FMargin(8,5)).SetPressedPadding(FMargin(8,5));
        return Style;
    }
    TSharedRef<SWidget> DesktopButton(const FString& Label,TFunction<void()> Action,const FString& Tip=FString())
    {
        return SNew(SButton).ButtonStyle(&DesktopButtonStyle()).ToolTipText(FText::FromString(Tip))
            .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})
            [ClaraStoryUI::Label(Label,14,DesktopInk)];
    }
    class SDesktopIcon final:public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SDesktopIcon){} SLATE_ARGUMENT(FString,Kind) SLATE_END_ARGS()
        void Construct(const FArguments& Args){Kind=Args._Kind;}
        FVector2D ComputeDesiredSize(float)const override{return FVector2D(48,44);}
        int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,
            int32 Layer,const FWidgetStyle&,bool)const override
        {
            const auto* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
            auto Box=[&](FVector2D P,FVector2D Size,FLinearColor Color)
            {FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,Color);};
            auto Line=[&](FVector2D A,FVector2D B,FLinearColor Color,float Width=2.f)
            {FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),TArray<FVector2D>{A,B},ESlateDrawEffect::None,Color,true,Width);};
            if(Kind==TEXT("files"))
            {
                Box({4,9},{19,8},FLinearColor(.55f,.37f,.12f));
                Box({4,15},{40,26},FLinearColor(.84f,.64f,.27f));
                Box({5,17},{38,3},FLinearColor(.97f,.79f,.42f));
            }
            else if(Kind==TEXT("vision"))
            {
                const FLinearColor Color(.34f,.75f,.72f);
                Line({24,10},{9,31},Color);Line({24,10},{40,31},Color);Line({9,31},{40,31},Color);
                for(FVector2D P:TArray<FVector2D>{{18,4},{2,26},{34,26}})Box(P,{12,10},Color);
            }
            else if(Kind==TEXT("terminal"))
            {
                Box({2,5},{44,35},FLinearColor(.34f,.42f,.43f));
                Box({4,9},{40,29},FLinearColor(.018f,.031f,.035f));
                Line({10,18},{17,23},DesktopInk);Line({17,23},{10,28},DesktopInk);Line({21,29},{31,29},DesktopInk);
            }
            else if(Kind==TEXT("gateway"))
            {
                const FLinearColor Blue(.29f,.58f,.83f);
                Box({4,8},{40,30},FLinearColor(.09f,.2f,.3f));
                for(int32 I=0;I<3;++I){Box({9.f,13.f+I*8},{30,4},Blue);Box({11.f,14.f+I*8},{2,2},DesktopInk);}
            }
            else
            {
                Box({9,4},{31,37},FLinearColor(.87f,.85f,.74f));
                Box({7,4},{5,37},FLinearColor(.6f,.43f,.24f));
                for(int32 I=0;I<4;++I)Line({17.f,14.f+I*6},{34.f,14.f+I*6},FLinearColor(.38f,.45f,.43f),1.f);
            }
            return Layer+2;
        }
    private:FString Kind;
    };

    class SStoryScreen final:public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SStoryScreen){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
        void Construct(const FArguments& Args){Director=Args._Director;Rebuild();}
        void Tick(const FGeometry& G,double Time,float Delta)override
        {
            SCompoundWidget::Tick(G,Time,Delta);
            if(Director.IsValid()&&Page!=Director->GetScreenPage()){Page=Director->GetScreenPage();Rebuild();}
        }
    private:
        TWeakObjectPtr<AClaraStoryDirector> Director;
        FString Page,LastApp=TEXT("terminal");
        bool WindowMaximized=false;
        FString GatewayTab=TEXT("console"),ScenarioPrompt;
        TSharedPtr<SEditableTextBox> ScenarioInput;
        TSharedRef<SWidget> MapBody(bool Challenge)
        {
            auto D=Director;
            TSharedRef<SVerticalBox> Tools=SNew(SVerticalBox);
            Tools->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(Challenge?TEXT("Ricostruisci e verifica"):TEXT("Osserva i collegamenti"),19,Ink,true)];
            Tools->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(Challenge?TEXT("Scegli un cavo, poi due dispositivi. Per configurare uno switch collega la console dal portatile e apri il terminale."):TEXT("I collegamenti disegnati provengono dallo stato del laboratorio. Una linea non dimostra da sola il funzionamento del servizio."),15,Muted)];
            if(Challenge)
            {
                for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("ethernet"),TEXT("Cavo Ethernet")},{TEXT("console"),TEXT("Cavo console")},{TEXT(""),TEXT("Solo selezione")}})
                    Tools->AddSlot().AutoHeight().Padding(0,0,0,7)[Button(Pair.Value,[D,Kind=Pair.Key]{if(D.IsValid())D->SetSimCableKind(Kind);})];
                Tools->AddSlot().AutoHeight().Padding(0,10,0,12)[Live([D]{return FString(TEXT("Strumento: "))+(D.IsValid()?D->GetSimCableKind():FString())+TEXT("\nSelezione: ")+(D.IsValid()?D->GetSimSelected():FString());},17,Teal,true)];
            }
            const auto Nodes=D->GetSimNodes();
            if(!Nodes.IsEmpty())for(const auto& N:Nodes)Tools->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(N.Label,[D,Id=N.Id]{if(D.IsValid())D->SelectSimNode(Id);})];
            if(Challenge)
            {
                TSharedRef<SEditableTextBox> IP=SNew(SEditableTextBox).Font(Font(20)).HintText(FText::FromString(TEXT("Nuovo IP host selezionato")));
                Tools->AddSlot().AutoHeight().Padding(0,12,0,7)[IP];
                Tools->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("Applica IP all’host"),[D,IP]{if(D.IsValid())D->SetSimHostIP(D->GetSimSelected(),IP->GetText().ToString());})];
                Tools->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("TEST rete"),[D]{if(D.IsValid())D->RunChallengeTest();},true)];
                Tools->AddSlot().AutoHeight()[SNew(SButton).IsEnabled_Lambda([D]{return D.IsValid()&&D->CanRebootChallenge();}).ButtonColorAndOpacity(Sand).ContentPadding(FMargin(16,11))
                    .OnClicked_Lambda([D]{if(D.IsValid())D->RebootChallenge();return FReply::Handled();})[Label(TEXT("Riavvio di verifica"),15,Ink,true)]];
            }
            return SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,20,0)[
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().FillHeight(1.f)[SNew(SNetworkMap).Director(D.Get())]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[Live([D]{return D.IsValid()?D->GetSimMessage():FString();},19,Teal,true)]
                ]
                +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(300)[SNew(SScrollBox)+SScrollBox::Slot()[Tools]]];
        }
        TSharedRef<SWidget> Body()
        {
            auto D=Director;
            if(Page==TEXT("routing"))return MakeClaraDayThreeLab(D.Get());
            if(Page==TEXT("cyber"))return MakeClaraCyberUI(D.Get());
            if(Page==TEXT("terminal"))return SNew(STerminal).Director(D.Get());
            if(Page==TEXT("puzzle")||Page==TEXT("sorting"))return ActivityBody(D,Page);
            if(Page==TEXT("vision")||Page==TEXT("challenge"))return MapBody(Page==TEXT("challenge"));
            if(Page==TEXT("subnet"))
            {
                TSharedRef<SEditableTextBox> Answer=AnswerInput(D,26);
                return SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,22)[Live([D]{return D.IsValid()?D->GetQuestion():FString();},23,Ink,true)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[Live([D]{return D.IsValid()?D->GetAnswerHint():FString();},17,Muted)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Answer]
                    +SVerticalBox::Slot().AutoHeight()[Button(TEXT("Verifica questo passaggio"),[D,Answer]{if(D.IsValid()){D->SubmitAnswer(Answer->GetText().ToString());Answer->SetText(FText::GetEmpty());}},true)];
            }
            if(Page==TEXT("files"))return FilesBody();
            if(Page==TEXT("notes")||Page==TEXT("document"))return SNew(SScrollBox)+SScrollBox::Slot()[
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Card).Padding(24)[
                    Live([D]{return D.IsValid()?D->GetNotebookText():FString();},18,Ink)]];
            if(Page==TEXT("mission"))return SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Text(TEXT("Richiesta di lavoro"),24,Ink,true)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)[Live([D]{return D.IsValid()?D->GetObjective():FString();},21,Ink)]
                +SVerticalBox::Slot().AutoHeight()[Text(TEXT("Usa il terminale per osservare gli indirizzi e provare la rete. Nel taccuino ritrovi gli appunti raccolti al banco."),18,Muted)];
            if(Page==TEXT("gateway"))return GatewayBody();
            return SNew(SBox);
        }
        FString AppTitle(const FString& App)const
        {
            if(App==TEXT("cyber"))return TEXT("Laboratorio Cyber");
            if(App==TEXT("terminal"))return TEXT("Terminale");
            if(App==TEXT("files"))return TEXT("File — /home/tecnico");
            if(App==TEXT("notes"))return TEXT("Taccuino — appunti.txt");
            if(App==TEXT("document"))return TEXT("Documenti — reti.txt");
            if(App==TEXT("mission"))return TEXT("Ticket — richiesta.txt");
            if(App==TEXT("routing"))return TEXT("Network Vision — routing e collaudo");
            if(App==TEXT("vision"))return TEXT("Network Vision");
            if(App==TEXT("challenge"))return TEXT("Network Vision — ripristino piano 2");
            if(App==TEXT("gateway"))return TEXT("Laboratorio Cisco — Gateway");
            if(App==TEXT("subnet"))return TEXT("Esercitazione — indirizzi e sottoreti");
            return TEXT("Laboratorio — esercitazione");
        }
        void OpenApp(const FString& App,bool PCSession=false)
        {
            if(!Director.IsValid())return;
            if(PCSession)Director->SelectTerminalTarget(TEXT("LAPTOP"));
            const FString Target=App==TEXT("vision")&&Director->GetDay()==2&&Director->GetSimNodes().Num()>3?TEXT("challenge"):App;
            Director->SetScreenPage(Target);
        }
        TSharedRef<SWidget> DesktopIcon(const FString& Kind,const FString& Label,const FString& App,const FString& Tip)
        {
            return SNew(SBox).WidthOverride(128).HeightOverride(95)[
                SNew(SButton).ButtonStyle(&DesktopButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                .ToolTipText(FText::FromString(Tip))
                .OnClicked_Lambda([this,App]{OpenApp(App,App==TEXT("terminal"));return FReply::Handled();})[
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SDesktopIcon).Kind(Kind)]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill).Padding(0,7,0,0)[
                        SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(12)).ColorAndOpacity(DesktopInk)
                            .AutoWrapText(false).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText(FText::FromString(Label)).Justification(ETextJustify::Center)]
                ]
            ];
        }
        TSharedRef<SWidget> FilesBody()
        {
            auto D=Director;
            TSharedRef<SVerticalBox> Rows=SNew(SVerticalBox);
            struct FFile{const TCHAR* Name;const TCHAR* Type;const TCHAR* Page;};
            for(const FFile& File:TArray<FFile>{{TEXT("Documenti"),TEXT("Cartella"),TEXT("document")},{TEXT("appunti.txt"),TEXT("Documento di testo"),TEXT("notes")},{TEXT("richiesta.txt"),TEXT("Ticket di lavoro"),TEXT("mission")},{TEXT("topologia.net"),TEXT("Network Vision"),TEXT("vision")},{TEXT("laboratorio.cml"),TEXT("Lab Cisco"),TEXT("gateway")}})
            {
                Rows->AddSlot().AutoHeight()[SNew(SButton).ButtonStyle(&DesktopButtonStyle())
                    .OnClicked_Lambda([this,App=FString(File.Page)]{OpenApp(App);return FReply::Handled();})[
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,14,0)[Label(FString(File.Type)==TEXT("Cartella")?TEXT("▸"):TEXT("·"),22,Teal)]
                        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(File.Name,16,Ink)]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(File.Type,15,Muted)]
                    ]];
            }
            return SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Text(TEXT("Home  /  tecnico"),17,Muted)]
                +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,24,0)[SNew(SBox).WidthOverride(145)[SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("Posizioni"),15,Muted,true)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("Home"),18,Teal,true)]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("Documenti"),18,Ink)]
                        +SVerticalBox::Slot().AutoHeight()[Text(TEXT("Laboratorio"),18,Ink)]]]
                    +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(8,0,8,10)[SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().FillWidth(1)[Text(TEXT("Nome"),14,Muted,true)]
                            +SHorizontalBox::Slot().AutoWidth()[Label(TEXT("Tipo"),14,Muted,true)]]
                        +SVerticalBox::Slot().AutoHeight()[SNew(SSeparator)]
                        +SVerticalBox::Slot().AutoHeight()[Rows]
                    ]
                ];
        }
        TSharedRef<SWidget> GatewayBody()
        {
            auto D=Director;
            TSharedRef<SHorizontalBox> Tabs=SNew(SHorizontalBox);
            for(const auto& Tab:TArray<TPair<FString,FString>>{{TEXT("console"),TEXT("Console")},{TEXT("verify"),TEXT("Verifiche")},{TEXT("security"),TEXT("Sicurezza")},{TEXT("scenario"),TEXT("Scenario")}})
                Tabs->AddSlot().AutoWidth().Padding(0,0,7,0)[SNew(SButton)
                    .ButtonColorAndOpacity(GatewayTab==Tab.Key?FLinearColor(.06f,.11f,.09f):TerminalBackground)
                    .ButtonStyle(&TerminalButtonStyle()).ForegroundColor(TerminalText).ContentPadding(FMargin(8,5))
                    .OnClicked_Lambda([this,Id=Tab.Key]{GatewayTab=Id;Rebuild();return FReply::Handled();})
                    [Label(Tab.Value,14,GatewayTab==Tab.Key?TerminalPrompt:TerminalMuted,true)]];
            TSharedRef<SVerticalBox> Content=SNew(SVerticalBox);
            if(GatewayTab==TEXT("console"))
            {
                TSharedRef<SEditableTextBox> Node=SNew(SEditableTextBox).Style(&TerminalInputStyle()).Font(Font(17))
                    .Text(FText::FromString(D->GetGatewayNode())).HintText(FText::FromString(TEXT("Nome nodo · SW-PIANO1")));
                auto WeakCommand=MakeShared<TWeakPtr<SEditableTextBox>>();
                TSharedRef<SEditableTextBox> Command=SNew(SEditableTextBox).Style(&TerminalInputStyle()).Font(TerminalFont(18)).ClearKeyboardFocusOnCommit(false)
                    .HintText(FText::FromString(TEXT("Comando sul nodo selezionato")))
                    .OnTextCommitted_Lambda([D,WeakCommand](const FText& Value,ETextCommit::Type Type)
                    {
                        if(Type!=ETextCommit::OnEnter||!D.IsValid())return;
                        D->RunGatewayCommand(Value.ToString());
                        if(auto Input=WeakCommand->Pin())Input->SetText(FText::GetEmpty());
                    });
                *WeakCommand=Command;
                Content->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[Label(TEXT("Nodo CML"),16,TerminalMuted)]
                    +SHorizontalBox::Slot().FillWidth(1)[Node]
                    +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[TerminalButton(TEXT("Seleziona"),[D,Node]{if(D.IsValid())D->GatewaySetNode(Node->GetText().ToString());})]
                ];
                Content->AddSlot().FillHeight(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TerminalBackground).Padding(14)[
                    SNew(SScrollBox)+SScrollBox::Slot()[SNew(STextBlock).Text_Lambda([D]{return FText::FromString(D.IsValid()?D->GetGatewayTranscript():FString());}).Font(TerminalFont(17)).ColorAndOpacity(TerminalText).AutoWrapText(true)]
                ]];
                Content->AddSlot().AutoHeight().Padding(0,10,0,0)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1)[Command]
                    +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[TerminalButton(TEXT("Invio"),[D,Command]{if(D.IsValid()){D->RunGatewayCommand(Command->GetText().ToString());Command->SetText(FText::GetEmpty());}},true)]
                ];
            }
            else
            {
                if(GatewayTab==TEXT("verify"))
                {
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("Verifica una missione sul laboratorio collegato."),18,TerminalText)];
                    TSharedRef<SUniformGridPanel> Missions=SNew(SUniformGridPanel).SlotPadding(FMargin(0,0,10,10));
                    const TArray<TPair<FString,FString>> Tests={{TEXT("rack"),TEXT("Rack e collegamenti")},{TEXT("terminal"),TEXT("Console e configurazione")},{TEXT("umbrella"),TEXT("Protezione Umbrella")},{TEXT("wireshark"),TEXT("Analisi Wireshark")}};
                    for(int32 I=0;I<Tests.Num();++I)Missions->AddSlot(I%2,I/2)[TerminalButton(Tests[I].Value,[D,Id=Tests[I].Key]{if(D.IsValid())D->GatewayVerifyMission(Id);})];
                    Content->AddSlot().AutoHeight()[Missions];
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[TerminalButton(TEXT("Chiedi un indizio"),[D]{if(D.IsValid())D->GatewayHint();})];
                }
                else if(GatewayTab==TEXT("security"))
                {
                    TSharedRef<SEditableTextBox> Domain=SNew(SEditableTextBox).Style(&TerminalInputStyle()).Font(Font(19)).HintText(FText::FromString(TEXT("Dominio · esempio.test")));
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("Politica del dominio nel laboratorio"),20,TerminalText,true)];
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Domain];
                    Content->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[TerminalButton(TEXT("Blocca dominio"),[D,Domain]{if(D.IsValid())D->GatewayBlockDomain(Domain->GetText().ToString(),true);},true)]
                        +SHorizontalBox::Slot().AutoWidth()[TerminalButton(TEXT("Rimuovi blocco"),[D,Domain]{if(D.IsValid())D->GatewayBlockDomain(Domain->GetText().ToString(),false);})]
                    ];
                    TSharedRef<SEditableTextBox> Port=SNew(SEditableTextBox).Style(&TerminalInputStyle()).Font(Font(17)).HintText(FText::FromString(TEXT("Identificativo porta o collegamento")));
                    Content->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1)[Port]
                        +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[TerminalButton(TEXT("Abilita porta"),[D,Port]{if(D.IsValid())D->GatewayPatchPort(Port->GetText().ToString(),true);})]
                        +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[TerminalButton(TEXT("Disabilita"),[D,Port]{if(D.IsValid())D->GatewayPatchPort(Port->GetText().ToString(),false);})]
                    ];
                }
                else if(GatewayTab==TEXT("scenario"))
                {
                    ScenarioInput=SNew(SEditableTextBox).Style(&TerminalInputStyle()).Font(Font(18)).Text(FText::FromString(ScenarioPrompt)).HintText(FText::FromString(TEXT("Descrivi la rete o la prova da generare")))
                        .OnTextChanged_Lambda([this](const FText& Value){ScenarioPrompt=Value.ToString();});
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("Prepara uno scenario"),20,TerminalText,true)];
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[ScenarioInput.ToSharedRef()];
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[TerminalButton(TEXT("Genera anteprima"),[this,D]{if(D.IsValid())D->GatewayGenerate(ScenarioInput->GetText().ToString(),false);},true)]
                        +SHorizontalBox::Slot().AutoWidth()[TerminalButton(TEXT("Applica al laboratorio"),[this,D]{if(D.IsValid())D->GatewayGenerate(ScenarioInput->GetText().ToString(),true);})]
                    ];
                    Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Text(TEXT("Genera l’anteprima per leggere il risultato; applica lo scenario con il pulsante dedicato."),16,TerminalMuted)];
                }
                Content->AddSlot().FillHeight(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(TerminalBackground).Padding(14)[
                    SNew(SScrollBox)+SScrollBox::Slot()[Live([D]{return D.IsValid()?D->GetGatewayResult():FString();},17,TerminalText)]
                ]];
            }
            return SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Live([D]{return D.IsValid()?D->GetGatewayStatus():TEXT("Gateway non verificato");},15,TerminalPrompt)]
                    +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[TerminalButton(TEXT("Aggiorna"),[D]{if(D.IsValid())D->GatewayRefresh();})]
                    +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[TerminalButton(TEXT("Prepara lab"),[D]{if(D.IsValid())D->GatewayBootstrap();})]
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[Tabs]
                +SVerticalBox::Slot().FillHeight(1)[Content];
        }
        TSharedRef<SWidget> TopBar()
        {
            auto D=Director;
            return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(DesktopChrome).Padding(FMargin(12,3))[
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[DesktopButton(TEXT("Applicazioni"),[this]{OpenApp(TEXT("desktop"));})]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12,0)[Label(TEXT("tecnico@tnes-lab"),13,DesktopMuted)]
                +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(8,0)[Text(TEXT("Linux · postazione didattica"),12,DesktopMuted)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12,0)[LiveLabel([D]{return D.IsValid()?D->GetOfficeClockTime():FString(TEXT("09:00"));},13,DesktopInk)]
                +SHorizontalBox::Slot().AutoWidth()[DesktopButton(TEXT("Esci"),[D]{if(D.IsValid())D->CloseLaptopScreen();},TEXT("Torna alla stanza · Esc"))]
            ];
        }
        TSharedRef<SWidget> Taskbar()
        {
            TSharedRef<SHorizontalBox> Apps=SNew(SHorizontalBox);
            for(const auto& App:TArray<TPair<FString,FString>>{{TEXT("terminal"),TEXT(">_")},{TEXT("files"),TEXT("File")},{TEXT("vision"),TEXT("Rete")},{TEXT("gateway"),TEXT("CML")},{TEXT("notes"),TEXT("Appunti")}})
                Apps->AddSlot().AutoWidth().Padding(1,0)[DesktopButton(App.Value,[this,PageId=App.Key]{OpenApp(PageId);},AppTitle(App.Key))];
            Apps->AddSlot().AutoWidth().Padding(18,0,0,0)[DesktopButton(AppTitle(LastApp),[this]{OpenApp(Page==TEXT("desktop")?LastApp:TEXT("desktop"));},TEXT("Mostra o riduci la finestra"))];
            Apps->AddSlot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(15,0)[Text(Director.IsValid()&&Director->GetDay()==3?TEXT("Portatile · console locale / PC2 · rete A"):TEXT("PC-LAB  ·  192.168.1.10"),12,DesktopMuted)];
            return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(DesktopChrome).Padding(FMargin(9,4))[Apps];
        }
        TSharedRef<SWidget> AppWindow()
        {
            auto D=Director;
            return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.18f,.25f,.27f)).Padding(1)[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(DesktopChrome).Padding(FMargin(12,3))[
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(AppTitle(Page),14,DesktopInk,true)]
                    +SHorizontalBox::Slot().AutoWidth()[DesktopButton(TEXT("—"),[this]{OpenApp(TEXT("desktop"));},TEXT("Riduci a icona"))]
                    +SHorizontalBox::Slot().AutoWidth()[DesktopButton(WindowMaximized?TEXT("❐"):TEXT("□"),[this]{WindowMaximized=!WindowMaximized;Rebuild();},TEXT("Ingrandisci / ripristina"))]
                    +SHorizontalBox::Slot().AutoWidth()[DesktopButton(TEXT("×"),[this]{OpenApp(TEXT("desktop"));},TEXT("Chiudi la finestra"))]
                ]]
                +SVerticalBox::Slot().FillHeight(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Page==TEXT("terminal")||Page==TEXT("gateway")?TerminalBackground:Paper).Padding(Page==TEXT("terminal")?12:16)[Body()]]
            ];
        }
        void Rebuild()
        {
            if(!Director.IsValid()){ChildSlot[SNew(SBox)];return;}
            auto D=Director;Page=D->GetScreenPage();
            if(Page!=TEXT("desktop"))LastApp=Page;
            TSharedRef<SOverlay> Workspace=SNew(SOverlay)
                +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(40,0)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Text(TEXT("TNES"),62,FLinearColor(.12f,.23f,.24f),true)]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,8)[Text(TEXT("LABORATORIO  /  NAPOLI"),13,FLinearColor(.25f,.38f,.37f))]
                ]
                +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(14,18)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[DesktopIcon(TEXT("files"),TEXT("Cyber"),TEXT("cyber"),TEXT("Apri gli incarichi Cyber"))]
                    +SVerticalBox::Slot().AutoHeight()[DesktopIcon(TEXT("terminal"),TEXT("Terminale"),TEXT("terminal"),TEXT("Apri il terminale Linux del portatile"))]
                    +SVerticalBox::Slot().AutoHeight()[DesktopIcon(TEXT("files"),TEXT("File"),TEXT("files"),TEXT("Cartelle, documenti e richieste di lavoro"))]
                    +SVerticalBox::Slot().AutoHeight()[DesktopIcon(TEXT("vision"),TEXT("Network Vision"),D->GetDay()==2&&D->GetSimNodes().Num()>3?TEXT("challenge"):TEXT("vision"),TEXT("Osserva e verifica i collegamenti"))]
                    +SVerticalBox::Slot().AutoHeight()[DesktopIcon(TEXT("gateway"),TEXT("Lab Cisco"),TEXT("gateway"),TEXT("Stato e strumenti del gateway Cisco"))]
                    +SVerticalBox::Slot().AutoHeight()[DesktopIcon(TEXT("notes"),TEXT("Taccuino"),TEXT("notes"),TEXT("Appunti del laboratorio"))]
                ]
                +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(28,18)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[Text(TEXT("PC-LAB"),14,DesktopInk,true)]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,5)[Text(TEXT("/home/tecnico"),13,DesktopMuted)]
                ];
            if(Page!=TEXT("desktop")) Workspace->AddSlot().Padding(WindowMaximized?FMargin(0):FMargin(150,16,18,16))[AppWindow()];
            ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(DesktopWallpaper).Padding(0)[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[TopBar()]
                +SVerticalBox::Slot().FillHeight(1)[Workspace]
                +SVerticalBox::Slot().AutoHeight()[SNew(SBorder)
                    .Visibility_Lambda([D]{return D.IsValid()&&D->GetPage().IsEmpty()&&!D->GetToast().IsEmpty()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.035f,.10f,.11f)).Padding(FMargin(14,9))[
                        Live([D]{return D.IsValid()?D->GetToast():FString();},16,DesktopInk)]]
                +SVerticalBox::Slot().AutoHeight()[Taskbar()]
            ]];
        }

    };
}

TSharedRef<SWidget> MakeClaraStoryHUD(AClaraStoryDirector* Director)
{
    using namespace ClaraStoryUI;TWeakObjectPtr<AClaraStoryDirector>D=Director;
    return SNew(SOverlay).Visibility_Lambda([D]{return D.IsValid()&&AClaraFrontEnd::IsPreparingOffice(D->GetWorld())?EVisibility::Collapsed:EVisibility::SelfHitTestInvisible;})
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(22,0,0,74)[
            SNew(SComboButton)
                .Visibility_Lambda([D]{return D.IsValid()&&D->GetPage().IsEmpty()&&!D->IsScreenOpen()&&!D->IsDeviceInspectionOpen()?EVisibility::Visible:EVisibility::Collapsed;})
                .ToolTipText(FText::FromString(TEXT("Regola voci, effetti e ambiente")))
                .ButtonContent()[Label(TEXT("Audio"),14,Ink)]
                .OnGetMenuContent_Lambda([D]()->TSharedRef<SWidget>{
                    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(Paper).Padding(20)[MakeClaraAudioSettings(D.Get())];
                })
        ]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(22)[SNew(SBox).WidthOverride(340).Visibility_Lambda([D]{return D.IsValid()&&D->GetPage().IsEmpty()&&!D->IsScreenOpen()&&!D->IsDeviceInspectionOpen()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})[
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.045f,.05f,.80f)).Padding(13)[
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Live([D]{return D.IsValid()?D->GetTitle():TEXT("TNES");},12,FLinearColor(.72,.77,.64),true)]
                +SVerticalBox::Slot().AutoHeight()[Live([D]{return D.IsValid()?D->GetObjective():FString();},17,FLinearColor::White,false)]
            ]
        ]]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(22)[SNew(SBox).WidthOverride(340)[
            SNew(SBorder).Visibility_Lambda([D]{return D.IsValid()&&D->GetPage().IsEmpty()&&!D->IsScreenOpen()&&!D->IsDeviceInspectionOpen()&&!D->GetToast().IsEmpty()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Teal).Padding(14)[Live([D]{return D.IsValid()?D->GetToast():FString();},16,FLinearColor::White)]
        ]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,66)[
            SNew(SBorder).Visibility_Lambda([D]{return D.IsValid()&&!D->GetInteractionLabel().IsEmpty()&&D->GetPage().IsEmpty()&&!D->IsScreenOpen()&&!D->IsDeviceInspectionOpen()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Night).Padding(FMargin(18,10))[
                Live([D]{return FString(TEXT("E  ·  "))+(D.IsValid()?D->GetInteractionLabel():FString());},19,FLinearColor::White,true)]
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,22)[
            SNew(SBorder).Visibility_Lambda([D]{return D.IsValid()&&!D->IsDeviceInspectionOpen()&&!D->IsScreenOpen()&&D->GetPage().IsEmpty()?EVisibility::HitTestInvisible:EVisibility::Collapsed;}).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025f,.06f,.075f,.90f)).Padding(FMargin(16,8))[
                SNew(STextBlock).Font(Font(13)).ColorAndOpacity(FLinearColor(.85f,.89f,.86f,1)).Text_Lambda([D]{return FText::FromString(
                    D.IsValid()&&D->GetPhase()<3?TEXT("WASD cammina   ·   tasto destro: guarda intorno   ·   clic o E: interagisci   ·   H indizio")
                    :D.IsValid()&&D->GetPhase()<4?TEXT("Clic o E: interagisci   ·   I borsa   ·   N taccuino   ·   H indizio   ·   M audio")
                    :TEXT("Clic o E: interagisci   ·   T portatile   ·   I borsa   ·   N taccuino   ·   H indizio   ·   M audio"));})]
        ];
}
TSharedRef<SWidget> MakeClaraStoryPanel(AClaraStoryDirector* Director)
{
    return SNew(ClaraStoryUI::SStoryPanel).Director(Director);
}
TSharedRef<SWidget> MakeClaraNotebookPaper(TSharedRef<SWidget> Content)
{
    return SNew(ClaraStoryUI::SNotebookPaper)[Content];
}
TSharedRef<SWidget> MakeClaraStoryScreen(AClaraStoryDirector* Director)
{
    return SNew(SOverlay)
        +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.65))]
        +SOverlay::Slot().Padding(26)[
            SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[
                SNew(SBox).WidthOverride(1440).HeightOverride(900)[SNew(ClaraStoryUI::SStoryScreen).Director(Director)]]];
}
