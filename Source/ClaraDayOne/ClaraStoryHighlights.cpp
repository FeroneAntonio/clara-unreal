#include "ClaraStoryHighlights.h"
#include "ClaraStoryDirector.h"
#include "ClaraRoomVisitor.h"
#include "ClaraStoryCast.h"
#include "ClaraNetworkDevice.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
class SClaraObjectiveMarkers : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraObjectiveMarkers){} SLATE_END_ARGS()
    void Construct(const FArguments&,AClaraStoryHighlights* InOwner){Owner=InOwner;}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
        FSlateWindowElementList& Draw,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
    {
        if(!Owner.IsValid()||!Owner->ShouldShowMarkers())return Layer;
        auto* PC=Owner->GetWorld()->GetFirstPlayerController();
        if(!PC||!PC->PlayerCameraManager)return Layer;
        const FVector Camera=PC->PlayerCameraManager->GetCameraLocation();
        const FRotator View=PC->PlayerCameraManager->GetCameraRotation();
        const FVector2D Size=Geometry.GetLocalSize();
        int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
        if(Width<1||Height<1)return Layer;
        for(auto* Target:Owner->GetTargets())
        {
            const FVector Position=Owner->GetMarkerLocation(Target);
            FVector2D Screen;
            const bool Projected=PC->ProjectWorldLocationToScreen(Position,Screen);
            const FVector Local=View.UnrotateVector(Position-Camera);
            bool Edge=!Projected||Local.X<=0;
            if(!Edge){Screen.X*=Size.X/Width;Screen.Y*=Size.Y/Height;Edge=Screen.X<100||Screen.X>Size.X-210||Screen.Y<95||Screen.Y>Size.Y-115;}
            if(Edge)
            {
                FVector2D Direction(Local.Y,-Local.Z);
                if(Local.X<0)Direction.X=Local.Y>=0?FMath::Max(1.,FMath::Abs(Direction.X)):-FMath::Max(1.,FMath::Abs(Direction.X));
                if(Direction.IsNearlyZero())Direction={0,-1};
                Direction.Normalize();
                const double Radius=FMath::Min((Size.X*.5-125)/FMath::Max(.001,FMath::Abs(Direction.X)),(Size.Y*.5-110)/FMath::Max(.001,FMath::Abs(Direction.Y)));
                Screen=Size*.5+Direction*Radius;
            }
            Screen.X=FMath::Clamp(Screen.X,80.,FMath::Max(81.,Size.X-210));
            Screen.Y=FMath::Clamp(Screen.Y,80.,FMath::Max(81.,Size.Y-100));
            const float Meters=FVector::Distance(Camera,Target->GetActorLocation())/100.f;
            const FString Label=FString::Printf(TEXT("%s  ·  %.1f m"),*Owner->GetTargetName(Target),Meters);
            const FLinearColor Accent(.68f,.83f,.72f,.9f);
            const FSlateFontInfo Font=FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),13);
            FSlateDrawElement::MakeText(Draw,Layer+1,Geometry.ToPaintGeometry(FVector2D(220,24),FSlateLayoutTransform(Screen+FVector2D(15,1))),FText::FromString(Label),Font,ESlateDrawEffect::None,FLinearColor(0,0,0,.75));
            FSlateDrawElement::MakeText(Draw,Layer+2,Geometry.ToPaintGeometry(FVector2D(220,24),FSlateLayoutTransform(Screen+FVector2D(14,0))),FText::FromString(Label),Font,ESlateDrawEffect::None,Accent);
            TArray<FVector2D> Diamond={Screen+FVector2D(0,-4),Screen+FVector2D(5,1),Screen+FVector2D(0,6),Screen+FVector2D(-5,1),Screen+FVector2D(0,-4)};
            FSlateDrawElement::MakeLines(Draw,Layer+2,Geometry.ToPaintGeometry(),Diamond,ESlateDrawEffect::None,Accent,true,1.2f);
        }
        return Layer+2;
    }
private:
    TWeakObjectPtr<AClaraStoryHighlights> Owner;
};
}
AClaraStoryHighlights::AClaraStoryHighlights()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("HighlightsRoot"));
    SetActorEnableCollision(false);
}
void AClaraStoryHighlights::BeginPlay()
{
    Super::BeginPlay();
    Overlay=SNew(SClaraObjectiveMarkers,this).Visibility(EVisibility::HitTestInvisible);
    if(GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->AddViewportWidgetContent(Overlay.ToSharedRef(),12);
}
FVector AClaraStoryHighlights::GetMarkerLocation(AActor* Target) const
{
    if(!Target)return FVector::ZeroVector;
    FVector Center,Extent;Target->GetActorBounds(false,Center,Extent);
    return Center+FVector(0,0,Extent.Z+14);
}
FString AClaraStoryHighlights::GetTargetName(AActor* Target) const
{
    if(auto* NPC=Cast<AClaraStoryNPC>(Target))return NPC->GetDisplayName();
    if(auto* Prop=Cast<AClaraStoryProp>(Target))return Prop->GetDisplayName();
    if(auto* Device=Cast<AClaraNetworkDevice>(Target))return Device->GetDisplayName();
    return TEXT("Obiettivo");
}
bool AClaraStoryHighlights::ShouldShowMarkers() const
{
    const auto* Controller=Cast<AClaraRoomController>(GetWorld()->GetFirstPlayerController());
    return Director.IsValid()&&Director->GetPage().IsEmpty()&&!Director->IsScreenOpen()&&(!Controller||!Controller->IsDeviceOpen());
}
TArray<AActor*> AClaraStoryHighlights::GetTargets() const
{
    TArray<AActor*> Result;
    for(const auto& Pair:Markers)if(Pair.Key.IsValid())Result.Add(Pair.Key.Get());
    return Result;
}
void AClaraStoryHighlights::RefreshTargets()
{
    if(!Director.IsValid()){TActorIterator<AClaraStoryDirector> It(GetWorld());if(It)Director=*It;}
    if(!Director.IsValid())return;
    TSet<TWeakObjectPtr<AActor>> Desired;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        AActor* Target=*It;
        if(Target->IsHidden()||!Director->IsMissionTarget(Target))continue;
        Desired.Add(Target);
        if(Markers.Contains(Target))continue;
        FMarker Marker;
        auto* Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(Light);
        Light->SetupAttachment(RootComponent);Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightColor(FLinearColor(.7f,.86f,.75f));Light->SetIntensity(75);Light->SetAttenuationRadius(75);
        Light->SetCastShadows(false);Light->RegisterComponent();Marker.Glow=Light;
        auto* Diamond=NewObject<UTextRenderComponent>(this);AddInstanceComponent(Diamond);
        Diamond->SetupAttachment(RootComponent);Diamond->SetMobility(EComponentMobility::Movable);
        Diamond->SetText(FText::FromString(TEXT("◇")));Diamond->SetTextRenderColor(FColor(171,211,184));
        Diamond->SetHorizontalAlignment(EHTA_Center);Diamond->SetWorldSize(6);Diamond->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Diamond->RegisterComponent();Marker.Diamond=Diamond;Markers.Add(Target,Marker);
    }
    for(auto It=Markers.CreateIterator();It;++It)if(!It.Key().IsValid()||!Desired.Contains(It.Key()))
    {
        if(It.Value().Glow.IsValid())It.Value().Glow->DestroyComponent();
        if(It.Value().Diamond.IsValid())It.Value().Diamond->DestroyComponent();
        It.RemoveCurrent();
    }
}
void AClaraStoryHighlights::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Time=GetWorld()->GetTimeSeconds();
    if(Time>=NextScan){RefreshTargets();NextScan=Time+.35f;}
    const bool Visible=ShouldShowMarkers();
    const auto* PC=GetWorld()->GetFirstPlayerController();
    const FVector Camera=PC&&PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraLocation():FVector::ZeroVector;
    for(auto& Pair:Markers)
    {
        if(!Pair.Key.IsValid())continue;
        const FVector Point=GetMarkerLocation(Pair.Key.Get());
        if(Pair.Value.Glow.IsValid()){Pair.Value.Glow->SetVisibility(Visible);Pair.Value.Glow->SetWorldLocation(Point-FVector(0,0,10));Pair.Value.Glow->SetIntensity(65+25*FMath::Sin(Time*2.4f));}
        if(Pair.Value.Diamond.IsValid()){Pair.Value.Diamond->SetVisibility(Visible);Pair.Value.Diamond->SetWorldLocation(Point+FVector(0,0,2*FMath::Sin(Time*2.4f)));Pair.Value.Diamond->SetWorldRotation((Camera-Point).Rotation());}
    }
}
void AClaraStoryHighlights::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Overlay.IsValid()&&GetWorld()&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset();Markers.Empty();Super::EndPlay(Reason);
}
