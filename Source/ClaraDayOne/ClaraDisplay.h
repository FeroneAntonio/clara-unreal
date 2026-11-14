#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ClaraDisplay.generated.h"

class SWidget;

/** The PC's interface is rendered on its physical screen, never as a viewport overlay. */
UCLASS(ClassGroup = (Clara), meta = (BlueprintSpawnableComponent))
class CLARADAYONE_API UClaraDisplay : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UClaraDisplay();

    void ActivateDisplay(const TSharedPtr<SWidget>& Content);
    UFUNCTION(BlueprintCallable) void DeactivateDisplay();
    // IsActive() is inherited from UActorComponent and tracks the display's activation.
};
