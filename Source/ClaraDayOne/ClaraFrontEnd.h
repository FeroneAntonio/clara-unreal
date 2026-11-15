#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraFrontEnd.generated.h"
class AClaraAircraftIntro;
class UTexture2D;
class SWidget;
class SBox;
struct FSlateBrush;
struct FStreamableHandle;

UCLASS()
class CLARADAYONE_API AClaraFrontEnd final : public AActor
{
    GENERATED_BODY()
public:
    AClaraFrontEnd();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    static bool ShouldShow();
    static bool RunSelfTest(UWorld* World);
    static bool IsCyberStory(const UWorld* World);
    static bool NeedsArrivalInterview(const UWorld* World);
    static bool IsPreparingOffice(const UWorld* World);
    static FString GetPlayerName();
    static void CommitCareer(UWorld* World,const FString& Career);
    void SetOnEnter(TFunction<void(const FString&)> Callback){OnEnter=MoveTemp(Callback);}
private:
    enum class EPage:uint8 {Menu,Name,Aircraft,Reveal};
    EPage Page=EPage::Menu;
    FString PlayerName,Career=TEXT("reti"),Notice;
    bool bContinue=false,bPreparing=false,bAssetsReady=false,bOfficeRequested=false,bOfficeReady=false;
    bool bIntroFinished=false,bIntroFailed=false,bSkip=false,bRevealed=false;
    float Elapsed=0,RevealTime=0;
    TSharedPtr<SWidget> Overlay;
    TSharedPtr<SBox> Content;
    TSharedPtr<FSlateBrush> BackgroundBrush;
    TSharedPtr<FStreamableHandle> Preload;
    TFunction<void(const FString&)> OnEnter;
    UPROPERTY() TObjectPtr<AClaraAircraftIntro> Aircraft;
    UPROPERTY() TObjectPtr<UTexture2D> BackgroundTexture;
    void BuildOverlay();
    void ShowMenu(bool bName);
    void StartJourney(bool bResume);
    void PrepareOffice();
    void RevealOffice();
    void LoadProfile();
    bool HasCheckpoint() const;
};
