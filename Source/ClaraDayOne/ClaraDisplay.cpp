#include "ClaraDisplay.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/SWidget.h"

UClaraDisplay::UClaraDisplay()
{
    bReceiveHardwareInput = true;
    SetAutoActivate(false);
    SetMobility(EComponentMobility::Movable);
    SetWidgetSpace(EWidgetSpace::World);
    SetDrawSize(FVector2D(1280, 720));
    SetPivot(FVector2D(.5, .5));
    SetTwoSided(true);
    SetWindowFocusable(true);
    SetBackgroundColor(FLinearColor::Black);
    SetBlendMode(EWidgetBlendMode::Opaque);
    SetManuallyRedraw(false);
    SetTickWhenOffscreen(false);
    SetCastShadow(false);
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetVisibility(false);
}

void UClaraDisplay::ActivateDisplay(const TSharedPtr<SWidget>& Content)
{
    if (!Content.IsValid())
    {
        DeactivateDisplay();
        return;
    }
    SetActive(true);
    SetSlateWidget(Content);
    // WidgetComponent only assigns this parent automatically for UUserWidget, not raw Slate.
    // Pointer capture/focus must still resolve back to the real game viewport window.
    if (GetWorld() && GetWorld()->GetGameViewport())
    {
        const TSharedPtr<SViewport> Viewport = GetWorld()->GetGameViewport()->GetGameViewportWidget();
        if (const TSharedPtr<SWindow> Window = GetVirtualWindow(); Window.IsValid() && Viewport.IsValid())
            Window->AssignParentWidget(Viewport);
    }
    SetWindowFocusable(true);
    SetVisibility(true);
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RequestRenderUpdate();
}

void UClaraDisplay::DeactivateDisplay()
{
    SetActive(false);
    SetWindowFocusable(false);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetVisibility(false);
    SetSlateWidget(TSharedPtr<SWidget>());
    RequestRenderUpdate();
}
