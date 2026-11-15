#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ClaraGameMode.generated.h"

class AClaraOfficeLayout;
class AClaraStoryCast;

UCLASS()
class CLARADAYONE_API AClaraGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AClaraGameMode();
    virtual void BeginPlay() override;
private:
    bool bOfficeStarted=false;
    void StartOfficeStory();
    void WaitForOfficeReady(AClaraOfficeLayout* Layout);
    void CompleteOfficeStory(AClaraOfficeLayout* Layout);
    TWeakObjectPtr<AClaraStoryCast> PreparedCast;
};
