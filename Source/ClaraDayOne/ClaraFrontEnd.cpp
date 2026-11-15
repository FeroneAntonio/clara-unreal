#include "ClaraFrontEnd.h"
#include "ClaraStoryDirector.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/AssetManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "ClaraAircraftIntro.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

namespace TNESFrontEnd
{
void StartTNESFrontEndTour(AClaraFrontEnd* FrontEnd);
const FLinearColor White(.94f,.96f,.94f),Muted(.61f,.69f,.71f),Gold(.93f,.72f,.39f);
TWeakObjectPtr<UWorld> SessionWorld;
FString SessionName,SessionCareer=TEXT("reti");
bool bArrivalInterview=false,bPreparingWorld=false;
bool IsTour(){FString Dir;return FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTour="),Dir);}
FString ProfilePath(){return FPaths::ProjectSavedDir()/TEXT("ClaraProfile.json");}
FString CleanName(FString Name){Name.TrimStartAndEndInline();for(int32 I=Name.Len()-1;I>=0;--I)if(FChar::IsControl(Name[I]))Name.RemoveAt(I);return Name.Left(40);}
bool SaveProfile(const FString& Name,const FString& Career)
{
    if(IsTour())return true;
    auto Profile=MakeShared<FJsonObject>();Profile->SetStringField(TEXT("name"),CleanName(Name));Profile->SetStringField(TEXT("career"),Career);
    FString Data;FJsonSerializer::Serialize(Profile,TJsonWriterFactory<TCHAR>::Create(&Data));
    IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(),true);const FString Temporary=ProfilePath()+TEXT(".tmp");
    return FFileHelper::SaveStringToFile(Data,*Temporary,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)&&IFileManager::Get().Move(*ProfilePath(),*Temporary,true,true,false,true);
}
bool CanReveal(bool Ready,bool Finished,bool Skip,bool Failed,bool Resume){return Ready&&(Finished||Skip||Failed||Resume);}
TSharedRef<SWidget> Text(const FString& Value,int32 Size=18,FLinearColor Color=White)
{return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true);}
class SMenuAction final:public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMenuAction){} SLATE_ARGUMENT(FString,Label) SLATE_ARGUMENT(bool,Enabled) SLATE_EVENT(FOnClicked,OnClicked) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        ChildSlot[SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsEnabled(Args._Enabled).ContentPadding(FMargin(0,12)).OnClicked(Args._OnClicked)
            [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Args._Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).ColorAndOpacity_Lambda([this](){return IsHovered()?Gold:White;}).AutoWrapText(false)]
             +SVerticalBox::Slot().AutoHeight().Padding(0,9,0,0)[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this](){return IsHovered()?Gold:FLinearColor(.2f,.37f,.39f,.65f);})]]]];
    }
};
TSharedRef<SWidget> Action(const FString& Label,TFunction<FReply()> Callback,bool Enabled=true)
{return SNew(SMenuAction).Label(Label).Enabled(Enabled).OnClicked_Lambda([Callback=MoveTemp(Callback)](){return Callback();});}
}
using namespace TNESFrontEnd;

AClaraFrontEnd::AClaraFrontEnd(){PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("FrontEndRoot"));}
bool AClaraFrontEnd::ShouldShow()
{
    if(IsTour())return true;if(FApp::IsUnattended())return false;
    for(const TCHAR* Flag:{TEXT("ClaraFrontEndCheck"),TEXT("TNESAircraftCheck"),TEXT("ClaraSkipFrontEnd"),TEXT("ClaraGatewayCheck"),TEXT("ClaraGatewayPortCheck"),TEXT("ClaraLayoutCheck"),TEXT("ClaraStoryCheck"),TEXT("ClaraFreshStory"),TEXT("ClaraStoryDay2"),TEXT("ClaraStoryDay3"),TEXT("ClaraCableDemo"),TEXT("ClaraEthernetDemo"),TEXT("ClaraDeviceView"),TEXT("ClaraPlanView"),TEXT("ClaraChairView"),TEXT("ClaraTerminalPreview"),TEXT("ClaraTowerPreview"),TEXT("ClaraDayEndPreview")})if(FParse::Param(FCommandLine::Get(),Flag))return false;
    FString Tour;return !FParse::Value(FCommandLine::Get(),TEXT("ClaraTour="),Tour);
}
void AClaraFrontEnd::LoadProfile()
{
    if(IsTour()){PlayerName.Empty();Career=FParse::Param(FCommandLine::Get(),TEXT("ClaraFrontEndTourCyber"))?TEXT("cyber"):TEXT("reti");return;}
    FString Data;TSharedPtr<FJsonObject> Profile;
    if(IFileManager::Get().FileSize(*ProfilePath())>16384||!FFileHelper::LoadFileToString(Data,*ProfilePath())||!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Data),Profile)||!Profile.IsValid())return;
    Profile->TryGetStringField(TEXT("name"),PlayerName);PlayerName=CleanName(PlayerName);FString SavedCareer;
    if(Profile->TryGetStringField(TEXT("career"),SavedCareer)&&(SavedCareer==TEXT("reti")||SavedCareer==TEXT("cyber")))Career=SavedCareer;
}
bool AClaraFrontEnd::HasCheckpoint() const
{
    if(IsTour()){FString Mode;FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTourMode="),Mode);return Mode==TEXT("continue");}
    const TCHAR* File=Career==TEXT("cyber")?TEXT("ClaraCyberState.txt"):TEXT("ClaraStoryProgress.json");
    return IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/File))||IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/(FString(File)+TEXT(".bak"))));
}
void AClaraFrontEnd::BeginPlay()
{
    Super::BeginPlay();LoadProfile();
    BackgroundTexture=FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/TEXT("Data/UI/TNES_Menu.png"));
    BackgroundBrush=MakeShared<FSlateBrush>();BackgroundBrush->SetResourceObject(BackgroundTexture);BackgroundBrush->ImageSize=FVector2D(1920,1080);
    if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->SetCinematicMode(true,true,true,true,true);PC->bShowMouseCursor=true;FInputModeUIOnly Mode;PC->SetInputMode(Mode);}
    BuildOverlay();ShowMenu(false);
    // GUI tour is installed below; shipping builds have no scripted controls.
    StartTNESFrontEndTour(this);
}
void AClaraFrontEnd::BuildOverlay()
{
    Overlay=SNew(SOverlay)
        +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this](){return FLinearColor(0,0,0,Page==EPage::Aircraft&&!bIntroFailed&&!bContinue?0.f:1.f);})]
        +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[SNew(SImage).Image(BackgroundBrush.Get()).Visibility_Lambda([this](){return Page==EPage::Menu||Page==EPage::Name?EVisibility::Visible:EVisibility::Collapsed;})]]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(76,32))[SNew(SBox).WidthOverride(470)[SAssignNew(Content,SBox)]]
        +SOverlay::Slot().VAlign(VAlign_Bottom)[SNew(SBox).HeightOverride(155).Visibility_Lambda([this](){return Page==EPage::Aircraft?EVisibility::Visible:EVisibility::Collapsed;})[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.82f))]]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(90,0,90,82))[SNew(SBox).WidthOverride(1040).Visibility_Lambda([this](){return Page==EPage::Aircraft?EVisibility::Visible:EVisibility::Collapsed;})[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(bIntroFailed?TEXT("Preparazione dell'ufficio…"):Aircraft?Aircraft->Caption():FString());}).Font(FCoreStyle::GetDefaultFontStyle("Regular",22)).ColorAndOpacity(White).AutoWrapText(true).Justification(ETextJustify::Center)]]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(40,28))[SNew(SBox).Visibility_Lambda([this](){return Page==EPage::Aircraft?EVisibility::Visible:EVisibility::Collapsed;})[Action(TEXT("Salta introduzione"),[this](){bSkip=true;return FReply::Handled();})]];
    Overlay->SetRenderOpacity(0.f);
    if(auto* Viewport=GetWorld()->GetGameViewport())Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(),100);
}
void AClaraFrontEnd::ShowMenu(bool bName)
{
    Page=bName?EPage::Name:EPage::Menu;Elapsed=0;auto Rows=SNew(SVerticalBox);
    auto Add=[&Rows](TSharedRef<SWidget> Widget,float Bottom=20){Rows->AddSlot().AutoHeight().Padding(0,0,0,Bottom)[Widget];};
    Add(Text(TEXT("T N E S"),70),14);Add(Text(TEXT("Ogni connessione ha una storia."),22),34);
    if(!bName)
    {
        Add(Action(TEXT("Storia"),[this](){Notice.Empty();ShowMenu(true);return FReply::Handled();}),4);
        if(HasCheckpoint())Add(Action(TEXT("Continua"),[this](){StartJourney(true);return FReply::Handled();}),4);
        Add(Action(TEXT("Multiplayer · Coming soon"),[](){return FReply::Handled();},false),28);
        Add(Text(TEXT("Reti e sicurezza, da zero.\nUn nuovo lavoro da TNES, a Napoli."),17,Muted));
    }
    else
    {
        Add(Text(TEXT("Come ti chiami?"),22),12);
        Add(SNew(SBox).MinDesiredHeight(48)[SNew(SEditableTextBox).Text(FText::FromString(PlayerName)).HintText(FText::FromString(TEXT("Il tuo nome"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",20)).OnTextChanged_Lambda([this](const FText& Value){PlayerName=CleanName(Value.ToString());}).OnVerifyTextChanged_Lambda([](const FText& Value,FText& Error){if(Value.ToString().Len()>40){Error=FText::FromString(TEXT("Massimo 40 caratteri."));return false;}return true;})],18);
        Add(Text(TEXT("Alessia ti aspetta in ufficio. Conoscerai il team e sceglierete insieme da dove cominciare. Nessuna esperienza richiesta."),18,Muted));
        Add(Action(TEXT("Parti per Napoli"),[this](){StartJourney(false);return FReply::Handled();}),4);
        Add(Action(TEXT("Indietro"),[this](){ShowMenu(false);return FReply::Handled();}),8);
        if(!Notice.IsEmpty())Add(Text(Notice,16,Gold));
        Add(Text(TEXT("Il nome resta sul tuo dispositivo."),16,Muted));
    }
    Content->SetContent(Rows);
}
void AClaraFrontEnd::StartJourney(bool bResume)
{
    if(bPreparing)return;PlayerName=CleanName(PlayerName);
    if(!bResume&&PlayerName.IsEmpty()){Notice=TEXT("Inserisci un nome per continuare.");ShowMenu(true);return;}
    bContinue=bResume;bPreparing=true;bPreparingWorld=true;SessionWorld=GetWorld();SessionName=PlayerName;SessionCareer=Career;bArrivalInterview=!bResume;
    if(!bResume&&!SaveProfile(PlayerName,Career))UE_LOG(LogTemp,Warning,TEXT("TNES: profile not writable; name retained for this session."));
    Page=EPage::Aircraft;Content->SetVisibility(EVisibility::Collapsed);Elapsed=0;
    if(bResume)bIntroFinished=true;
    else
    {
        Aircraft=GetWorld()->SpawnActor<AClaraAircraftIntro>();bIntroFailed=!Aircraft||!Aircraft->IsUsable();
        if(bIntroFailed)UE_LOG(LogTemp,Warning,TEXT("TNES: authored aircraft set unavailable; opaque loading screen retained."));
    }
    PrepareOffice();
}
void AClaraFrontEnd::PrepareOffice()
{
    TArray<FSoftObjectPath> Assets;
    for(const TCHAR* Path:{TEXT("/Game/Materials/M_Office.M_Office"),TEXT("/Game/Materials/M_Glass.M_Glass"),TEXT("/Game/Materials/M_Floor.M_Floor"),TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"),TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"),TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")})Assets.Add(FSoftObjectPath(Path));
    for(const TCHAR* Name:{TEXT("OfficeDesk"),TEXT("OfficeTaskChair"),TEXT("DesktopMonitor"),TEXT("DesktopTower"),TEXT("OfficeArmchair"),TEXT("CoffeeTable"),TEXT("OfficePlant")})Assets.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/OfficeAssets/SM_%s.SM_%s"),Name,Name)));
    if(!FParse::Param(FCommandLine::Get(),TEXT("ClaraMannequins")))
    {
        Assets.Add(FSoftObjectPath(TEXT("/Game/MetaHumans/Alessia/BP_Alessia.BP_Alessia_C")));
        for(const TCHAR* Name:{TEXT("Luca"),TEXT("Jonathan")})Assets.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/OfficeCast/BP_TNES_%sOffice.BP_TNES_%sOffice_C"),Name,Name)));
    }
    TWeakObjectPtr<AClaraFrontEnd> Weak(this);
    Preload=UAssetManager::GetStreamableManager().RequestAsyncLoad(Assets,FStreamableDelegate::CreateLambda([Weak](){if(Weak.IsValid())Weak->bAssetsReady=true;}));
    if(!Preload.IsValid())bAssetsReady=true;
}
void AClaraFrontEnd::Tick(float Delta)
{
    Super::Tick(Delta);Elapsed+=Delta;
    if(Overlay.IsValid())Overlay->SetRenderOpacity(Page==EPage::Reveal?FMath::Clamp(1.f-RevealTime/.65f,0.f,1.f):(Page==EPage::Menu||Page==EPage::Name?FMath::Clamp(Elapsed/.35f,0.f,1.f):1.f));
    if(!bPreparing||bRevealed)return;
    if(Aircraft)bIntroFinished=Aircraft->IsComplete();
    if(bAssetsReady&&!bOfficeRequested){bOfficeRequested=true;if(OnEnter)OnEnter(Career);}
    if(bOfficeRequested){TActorIterator<AClaraStoryDirector> It(GetWorld());bOfficeReady=bool(It);}
    if(Page==EPage::Reveal){RevealTime+=Delta;if(RevealTime>=.65f){bRevealed=true;Destroy();}return;}
    if(CanReveal(bOfficeReady,bIntroFinished,bSkip,bIntroFailed,bContinue)&&Elapsed>=1.f)RevealOffice();
}
void AClaraFrontEnd::RevealOffice()
{
    if(Page==EPage::Reveal)return;Page=EPage::Reveal;bPreparingWorld=false;
    if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->SetCinematicMode(false,true,true,true,true);if(PC->GetPawn())PC->SetViewTarget(PC->GetPawn());PC->bShowMouseCursor=true;FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);PC->SetInputMode(Mode);}
    if(NeedsArrivalInterview(GetWorld())){TActorIterator<AClaraStoryDirector> It(GetWorld());if(It)It->StartArrivalInterview();}
    UE_LOG(LogTemp,Display,TEXT("TNES_FRONTEND_REVEAL: assets prefetched, office ready, introFinished=%d skip=%d failure=%d continue=%d"),bIntroFinished,bSkip,bIntroFailed,bContinue);
}
void AClaraFrontEnd::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Preload.IsValid()&&!Preload->HasLoadCompleted())Preload->CancelHandle();Preload.Reset();
    if(IsValid(Aircraft))Aircraft->Destroy();
    if(Overlay.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset();Content.Reset();BackgroundBrush.Reset();Super::EndPlay(Reason);
}
bool AClaraFrontEnd::IsCyberStory(const UWorld* World){return SessionWorld.Get()==World&&!bArrivalInterview&&SessionCareer==TEXT("cyber");}
bool AClaraFrontEnd::NeedsArrivalInterview(const UWorld* World){return SessionWorld.Get()==World&&bArrivalInterview;}
bool AClaraFrontEnd::IsPreparingOffice(const UWorld* World){return SessionWorld.Get()==World&&bPreparingWorld;}
FString AClaraFrontEnd::GetPlayerName(){return SessionName;}
void AClaraFrontEnd::CommitCareer(UWorld* World,const FString& Career)
{
    if(Career!=TEXT("reti")&&Career!=TEXT("cyber"))return;SessionWorld=World;SessionCareer=Career;bArrivalInterview=false;
    if(!SaveProfile(SessionName,Career))UE_LOG(LogTemp,Warning,TEXT("TNES: career profile retained only for this session."));
}
bool AClaraFrontEnd::RunSelfTest(UWorld* World)
{
    return AClaraAircraftIntro::RunSelfTest()&&CleanName(TEXT("  Èva 李  "))==TEXT("Èva 李")&&CleanName(FString::ChrN(80,TEXT('a'))).Len()==40
        &&!CanReveal(false,false,false,true,false)&&CanReveal(true,false,false,true,false)
        &&!CanReveal(false,false,true,false,false)&&CanReveal(true,false,true,false,false)
        &&!CanReveal(false,false,false,false,true)&&CanReveal(true,false,false,false,true)
        &&!CanReveal(true,false,false,false,false)&&CanReveal(true,true,false,false,false);
}
#if !UE_BUILD_SHIPPING
namespace TNESFrontEnd
{
bool HasTourText(const TSharedRef<SWidget>& Widget,const FString& Label)
{
    if(!Widget->GetVisibility().IsVisible())return false;
    if(Widget->GetType()==FName(TEXT("STextBlock"))&&StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString()==Label)return true;
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)if(HasTourText(Children->GetChildAt(I),Label))return true;return false;
}
void FindTourControl(const TSharedRef<SWidget>& Widget,const FString& Label,bool Input,TArray<TSharedRef<SWidget>>& Found)
{
    if(!Widget->GetVisibility().IsVisible())return;
    if(Input&&Widget->GetType()==FName(TEXT("SEditableTextBox"))&&StaticCastSharedRef<SEditableTextBox>(Widget)->GetHintText().ToString()==Label){Found.Add(Widget);return;}
    if(!Input&&Widget->GetType()==FName(TEXT("SButton"))&&HasTourText(Widget,Label)){Found.Add(Widget);return;}
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)FindTourControl(Children->GetChildAt(I),Label,Input,Found);
}
struct FIntroTourStep{float Wait;FString Action,Value;};
struct FIntroTour
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AClaraFrontEnd> FrontEnd;
    TArray<FIntroTourStep> Steps;TArray<FString> Shots;FString Directory;int32 Index=0,Checks=0;
    float Clock=0;double Started=0,ShotAt=0;bool Failed=false;
};
void StartTNESFrontEndTour(AClaraFrontEnd* FrontEnd)
{
    FString Directory;if(!FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTour="),Directory))return;
    const auto Tour=MakeShared<FIntroTour>();Tour->World=FrontEnd->GetWorld();Tour->FrontEnd=FrontEnd;Tour->Directory=FPaths::ConvertRelativePathToFull(Directory);Tour->Started=FPlatformTime::Seconds();
    auto Fail=[Tour](const FString& Reason){Tour->Failed=true;UE_LOG(LogTemp,Error,TEXT("CLARA_FRONTEND_TOUR_FAIL: %s"),*Reason);FPlatformMisc::RequestExitWithStatus(false,1);};
    if(!FParse::Param(FCommandLine::Get(),TEXT("ClaraLocalLab"))){Fail(TEXT("Use -ClaraLocalLab"));return;}
    IFileManager::Get().MakeDirectory(*Tour->Directory,true);
    if(auto* PC=Tour->World->GetFirstPlayerController()){if(APawn* Pawn=PC->GetPawn())Pawn->DisableInput(PC);PC->DisableInput(PC);}
    auto Add=[Tour](float Delay,const TCHAR* Action,const TCHAR* Value=TEXT("")){Tour->Steps.Add({Delay,Action,Value});};
    FString Mode;FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTourMode="),Mode);
    Add(2,TEXT("disabled"),TEXT("Multiplayer · Coming soon"));Add(.1f,TEXT("shot"),TEXT("01-menu"));
    if(Mode==TEXT("continue"))Add(.5f,TEXT("click"),TEXT("Continua"));
    else
    {
        Add(.5f,TEXT("click"),TEXT("Storia"));Add(.5f,TEXT("shot"),TEXT("02-nome"));
        Add(.3f,TEXT("click"),TEXT("Parti per Napoli"));Add(.3f,TEXT("text"),TEXT("Inserisci un nome per continuare."));
        Add(.3f,TEXT("type"),TEXT("Èva 李"));Add(.2f,TEXT("field"),TEXT("Èva 李"));Add(.5f,TEXT("start"),TEXT("Parti per Napoli"));
        if(Mode==TEXT("skip-film"))Add(.3f,TEXT("click"),TEXT("Salta introduzione"));
    }
    Add(.5f,TEXT("await-office"));Add(.7f,TEXT("shot"),TEXT("04-arrivo-ufficio"));
    if(Mode!=TEXT("continue"))
    {
        Add(.5f,TEXT("click"),TEXT("Continua · Invio"));
        const bool Cyber=FParse::Param(FCommandLine::Get(),TEXT("ClaraFrontEndTourCyber"));
        Add(.5f,TEXT("click"),Cyber?TEXT("Proteggere il lavoro delle persone"):TEXT("Collegare i computer tra loro"));
        Add(.5f,TEXT("click"),Cyber?TEXT("Aiutarlo a tornare in comunicazione"):TEXT("Capire se un accesso è affidabile"));
        Add(.5f,TEXT("click"),Cyber?TEXT("Indagare sui rischi con Jonny"):TEXT("Costruire una piccola rete con Luca"));
        Add(.7f,TEXT("shot"),TEXT("05-colloquio-esito"));
        Add(.5f,TEXT("click"),Cyber?TEXT("Scelgo Cybersecurity"):TEXT("Scelgo Networking"));Add(1,TEXT("shot"),TEXT("06-percorso"));
    }
    Add(.5f,TEXT("done"));
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Tour,Fail](float Delta)
    {
        if(Tour->Failed)return false;
        if(!Tour->World.IsValid()||FPlatformTime::Seconds()-Tour->Started>180){Fail(TEXT("World missing or 180 second bound exceeded"));return false;}
        if(Tour->ShotAt>0&&FScreenshotRequest::IsScreenshotRequested()){if(FPlatformTime::Seconds()-Tour->ShotAt>30){Fail(TEXT("Screenshot timed out"));return false;}return true;}
        Tour->ShotAt=0;Tour->Clock+=Delta;if(!Tour->Steps.IsValidIndex(Tour->Index))return false;
        const auto Step=Tour->Steps[Tour->Index];if(Tour->Clock<Step.Wait)return true;
        if(Step.Action==TEXT("await-office"))
        {
            TActorIterator<AClaraStoryDirector> It(Tour->World.Get());if(Tour->FrontEnd.IsValid()||!It)return true;
            ++Tour->Checks;
        }
        auto Shot=[Tour](const FString& Name){const FString File=Tour->Directory/(Name+TEXT(".png"));Tour->Shots.Add(File);FScreenshotRequest::RequestScreenshot(File,true,false);Tour->ShotAt=FPlatformTime::Seconds();};
        if(Step.Action==TEXT("shot"))Shot(Step.Value);
        else if(Step.Action==TEXT("done"))
        {
            for(const FString& File:Tour->Shots)if(IFileManager::Get().FileSize(*File)<=0){Fail(TEXT("Missing screenshot: ")+File);return false;}
            TActorIterator<AClaraStoryDirector> It(Tour->World.Get());if(!It){Fail(TEXT("Director missing"));return false;}
            if(FParse::Param(FCommandLine::Get(),TEXT("ClaraFrontEndTourCyber"))&&It->GetScreenPage()!=TEXT("cyber")){Fail(TEXT("Cyber not selected"));return false;}
            UE_LOG(LogTemp,Display,TEXT("CLARA_FRONTEND_TOUR_PASS: %d actual GUI checks, %d captures; TNES menu/aircraft/office interview"),Tour->Checks,Tour->Shots.Num());FPlatformMisc::RequestExitWithStatus(false,0);return false;
        }
        else if(Step.Action!=TEXT("await-office"))
        {
            auto* VP=Tour->World->GetGameViewport();const auto Window=VP?VP->GetWindow():nullptr;if(!Window.IsValid()){Fail(TEXT("No real Slate window"));return false;}
            if(Step.Action==TEXT("text")){if(!HasTourText(Window.ToSharedRef(),Step.Value)){Fail(TEXT("Validation text missing"));return false;}++Tour->Checks;}
            else
            {
                const bool Input=Step.Action==TEXT("type")||Step.Action==TEXT("field");TArray<TSharedRef<SWidget>> Found;FindTourControl(Window.ToSharedRef(),Input?TEXT("Il tuo nome"):Step.Value,Input,Found);
                if(Found.Num()!=1){Fail(TEXT("Expected one GUI control: ")+Step.Value);return false;}
                if(Step.Action==TEXT("disabled")){if(Found[0]->IsEnabled()){Fail(TEXT("Multiplayer enabled"));return false;}}
                else if(!Found[0]->IsEnabled()){Fail(TEXT("Control disabled: ")+Step.Value);return false;}
                else if(Step.Action==TEXT("type")){StaticCastSharedRef<SEditableTextBox>(Found[0])->SetText(FText::FromString(Step.Value));FSlateApplication::Get().SetKeyboardFocus(Found[0]);}
                else if(Step.Action==TEXT("field")){if(StaticCastSharedRef<SEditableTextBox>(Found[0])->GetText().ToString()!=Step.Value){Fail(TEXT("Unicode field mismatch"));return false;}}
                else{StaticCastSharedRef<SButton>(Found[0])->SimulateClick();if(Step.Action==TEXT("start"))Shot(TEXT("03-aereo-o-caricamento"));}
                ++Tour->Checks;
            }
        }
        Tour->Clock=0;++Tour->Index;return true;
    }));
}
}
#else
namespace TNESFrontEnd{void StartTNESFrontEndTour(AClaraFrontEnd*){}}
#endif
