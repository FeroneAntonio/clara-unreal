#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ClaraHUD.generated.h"

UCLASS()
class CLARADAYONE_API AClaraHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    float Scale = 1;
    void Text(const FString& Value, float X, float Y, FLinearColor Color, float Size = 1);
    float Wrapped(const FString& Value, float X, float Y, float Width, FLinearColor Color, float Size = 1);
};
