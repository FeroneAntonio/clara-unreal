#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ClaraGameMode.generated.h"

UCLASS()
class CLARADAYONE_API AClaraGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AClaraGameMode();
    virtual void BeginPlay() override;
};
