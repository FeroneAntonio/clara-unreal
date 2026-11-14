#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClaraStoryHighlights.generated.h"
class UPointLightComponent;
class UTextRenderComponent;
class SWidget;
class AClaraStoryDirector;

/** Quiet objective markers and a soft pulse on the objects needed now. */
UCLASS()
class CLARADAYONE_API AClaraStoryHighlights : public AActor
{
    GENERATED_BODY()
public:
    AClaraStoryHighlights();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    TArray<AActor*> GetTargets() const;
    FString GetTargetName(AActor* Target) const;
    FVector GetMarkerLocation(AActor* Target) const;
    bool ShouldShowMarkers() const;
private:
    struct FMarker
    {
        TWeakObjectPtr<UPointLightComponent> Glow;
        TWeakObjectPtr<UTextRenderComponent> Diamond;
    };
    TMap<TWeakObjectPtr<AActor>,FMarker> Markers;
    TWeakObjectPtr<AClaraStoryDirector> Director;
    TSharedPtr<SWidget> Overlay;
    float NextScan=0;
    void RefreshTargets();
};
