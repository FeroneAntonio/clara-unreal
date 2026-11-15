#include "ClaraAircraftIntro.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/RectLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
namespace TNESAircraft
{
const FVector SetOrigin(-40000,0,1000);
float HeadTurn(float Time){const float T=FMath::Clamp((Time-9.f)/5.f,0.f,1.f);return T*T*T*(T*(T*6.f-15.f)+10.f);}
FQuat Look(float Time,const FVector& Eye,const FVector& Paper,const FVector& Window)
{return FQuat::Slerp((Paper-Eye).Rotation().Quaternion(),(Window-Eye).Rotation().Quaternion(),HeadTurn(Time)).GetNormalized();}
}
using namespace TNESAircraft;
AClaraAircraftIntro::AClaraAircraftIntro()
{
    PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("AircraftSet"));
    Cabin=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AuthoredAircraftCabin"));Cabin->SetupAttachment(RootComponent);Cabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    View=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AerialPhotographOutsideWindow"));View->SetupAttachment(RootComponent);View->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AClaraAircraftIntro::BeginPlay()
{
    Super::BeginPlay();SetActorLocation(SetOrigin);
    Cabin->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/AircraftIntro/SM_TNES_AircraftCabin.SM_TNES_AircraftCabin")));
    View->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/AircraftIntro/SM_TNES_AircraftView.SM_TNES_AircraftView")));
    AerialPhoto=FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/TEXT("Data/Intro/Vesuvius_Aerial.jpg"));
    if(AerialPhoto){AerialPhoto->AddressX=TA_Clamp;AerialPhoto->AddressY=TA_Clamp;AerialPhoto->UpdateResource();}
    auto* ViewMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/AircraftIntro/M_TNES_Aerial.M_TNES_Aerial"));
    if(!Cabin->GetStaticMesh()||!View->GetStaticMesh()||!AerialPhoto||!ViewMaterial){UE_LOG(LogTemp,Error,TEXT("TNES_AIRCRAFT_MISSING_ASSET: loading screen retained; no geometric fallback."));return;}
    for(const FName Socket:{FName(TEXT("Eye")),FName(TEXT("CV")),FName(TEXT("RightWindow")),FName(TEXT("Ahead"))})if(!Cabin->DoesSocketExist(Socket)){UE_LOG(LogTemp,Error,TEXT("TNES_AIRCRAFT_SOCKET_MISSING: %s"),*Socket.ToString());return;}
    FVector Forward=Cabin->GetSocketLocation(TEXT("Ahead"))-Cabin->GetSocketLocation(TEXT("Eye"));
    FVector Right=Cabin->GetSocketLocation(TEXT("RightWindow"))-Cabin->GetSocketLocation(TEXT("Eye"));
    if(FVector::CrossProduct(Forward,Right).Z<=0){UE_LOG(LogTemp,Error,TEXT("TNES_AIRCRAFT_HANDEDNESS_FAIL: window must be camera-right; never mirror printed text."));return;}
    FVector PhotoScale(1,1,1);
    const FVector PhotoBounds=View->GetStaticMesh()->GetBoundingBox().GetSize();
    const float Aspect=float(AerialPhoto->GetSizeX())/FMath::Max(1,AerialPhoto->GetSizeY());
    if(PhotoBounds.X>PhotoBounds.Y)PhotoScale.X*=Aspect*3.f;else PhotoScale.Y*=Aspect*3.f;
    PhotoScale.Z=3.f;
    View->SetRelativeScale3D(PhotoScale);View->SetRelativeLocation({0,0,-359});
    auto* Material=UMaterialInstanceDynamic::Create(ViewMaterial,this);Material->SetTextureParameterValue(TEXT("AerialPhoto"),AerialPhoto);View->SetMaterial(0,Material);
    Eye=Cabin->GetSocketLocation(TEXT("Eye"));Paper=Cabin->GetSocketLocation(TEXT("CV"));Window=Cabin->GetSocketLocation(TEXT("RightWindow"));
    const float Scale=Cabin->GetStaticMesh()->GetBoundingBox().GetSize().Z/250.f;
    if(Scale<.75f||Scale>1.5f){UE_LOG(LogTemp,Error,TEXT("TNES_AIRCRAFT_SCALE_FAIL: expected approximately 250 cm cabin, factor=%f"),Scale);return;}
    Forward=Cabin->GetSocketLocation(TEXT("Ahead"))-Eye;Right=Window-Eye;
    if(FVector::CrossProduct(Forward,Right).Z<=0){UE_LOG(LogTemp,Error,TEXT("TNES_AIRCRAFT_RIGHT_FAIL"));return;}
    Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,Look(0,Eye,Paper,Window).Rotator());Camera->GetCameraComponent()->SetFieldOfView(36);
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->GetCameraComponent()->PostProcessSettings.AutoExposureBias=.35f;
    const FVector ForwardUnit=FVector(Forward.X,Forward.Y,0).GetSafeNormal();
    const FVector CameraRight=FVector::CrossProduct(FVector::UpVector,ForwardUnit);
    for(const FVector Offset:{FVector(40,-40,93),FVector(-35,25,78)})
    {
        const FVector Position=Eye+ForwardUnit*Offset.X+CameraRight*Offset.Y+FVector::UpVector*Offset.Z;
        auto* Light=GetWorld()->SpawnActor<ARectLight>(Position,(Paper-Position).Rotation());Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        auto* Component=Cast<URectLightComponent>(Light->GetLightComponent());Component->SetIntensity(1500);Component->SetSourceWidth(90);Component->SetSourceHeight(45);Component->SetCastShadows(true);Component->SetLightColor(FLinearColor(1,.97f,.92f));Lights.Add(Light);
    }
    FString Data;TSharedPtr<FJsonObject> Narrative;
    if(FFileHelper::LoadFileToString(Data,*(FPaths::ProjectContentDir()/TEXT("Data/Intro/narrative-it.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Data),Narrative)&&Narrative.IsValid())
    {
        double Seconds=27;Narrative->TryGetNumberField(TEXT("duration_seconds"),Seconds);Duration=FMath::Clamp(float(Seconds),27.f,60.f);
        const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;if(Narrative->TryGetArrayField(TEXT("captions"),Items))for(const auto& Item:*Items)
        {
            const TSharedPtr<FJsonObject>* Row=nullptr;double Start=0,End=0;FString Text;
            if(Item->TryGetObject(Row)&&(*Row)->TryGetNumberField(TEXT("start"),Start)&&(*Row)->TryGetNumberField(TEXT("end"),End)&&(*Row)->TryGetStringField(TEXT("text"),Text)&&Start>=0&&End>Start&&End<=Duration)Captions.Add({float(Start),float(End),Text});
        }
    }
    if(Captions.Num()!=3)
    {
        Captions={{1,9,TEXT("Stai andando a Napoli per un colloquio in TNES, un’azienda che costruisce e protegge reti.")},{9.5f,17,TEXT("Non serve sapere già tutto. Cerchi un’occasione per imparare, partendo da zero.")},{17.5f,27,TEXT("Al colloquio parlerai dei tuoi interessi. Da lì comincerà il tuo percorso tra reti e sicurezza informatica.")}};
    }
    if(FParse::Value(FCommandLine::Get(),TEXT("TNESAircraftTour="),ShotDirectory))IFileManager::Get().MakeDirectory(*ShotDirectory,true);
    if(auto* PC=GetWorld()->GetFirstPlayerController())PC->SetViewTarget(Camera);
    bUsable=true;UE_LOG(LogTemp,Display,TEXT("TNES_AIRCRAFT_READY: authored cabin, printed CV, genuine aerial photograph; seated camera, right turn; duration=%.1f"),Duration);
}
void AClaraAircraftIntro::Tick(float Delta)
{
    Super::Tick(Delta);if(!bUsable)return;Time+=Delta;
    Camera->SetActorLocationAndRotation(Eye,Look(Time,Eye,Paper,Window).Rotator());Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(36.f,82.f,HeadTurn(Time)));
#if !UE_BUILD_SHIPPING
    const float At[]={5,11.5f,21};const TCHAR* Names[]={TEXT("01-cv"),TEXT("02-right-turn"),TEXT("03-window")};
    if(!ShotDirectory.IsEmpty()&&NextShot<3&&Time>=At[NextShot]&&!FScreenshotRequest::IsScreenshotRequested())
    {
        FScreenshotRequest::RequestScreenshot(ShotDirectory/(FString(Names[NextShot])+TEXT(".png")),true,false);
        UE_LOG(LogTemp,Display,TEXT("TNES_AIRCRAFT_TOUR_SHOT: %s, actualTime=%.2f, yaw=%f, pitch=%f"),Names[NextShot],Time,Camera->GetActorRotation().Yaw,Camera->GetActorRotation().Pitch);++NextShot;
    }
#endif
}
FString AClaraAircraftIntro::Caption() const
{for(const FCaption& Item:Captions)if(Time>=Item.Start&&Time<Item.End)return Item.Text;return FString();}
void AClaraAircraftIntro::EndPlay(const EEndPlayReason::Type Reason)
{
    if(IsValid(Camera))Camera->Destroy();for(AActor* Light:Lights)if(IsValid(Light))Light->Destroy();Lights.Empty();Super::EndPlay(Reason);
}
bool AClaraAircraftIntro::RunSelfTest()
{
    const FVector E(0,72,122),CV(49,72,79),W(0,130,136);
    float LastYaw=-1;for(float T:{0.f,5.f,9.f,10.f,11.5f,13.f,14.f,21.f,27.f})
    {
        const FRotator R=Look(T,E,CV,W).Rotator();if(R.Yaw<LastYaw-.01f||R.Yaw<-.01f||R.Yaw>90.01f||R.ContainsNaN())return false;LastYaw=R.Yaw;
    }
    return FMath::IsNearlyEqual(HeadTurn(0),0)&&FMath::IsNearlyEqual(HeadTurn(9),0)&&FMath::IsNearlyEqual(HeadTurn(14),1)
        &&Look(0,E,CV,W).Rotator().Pitch<0&&FMath::IsNearlyEqual(Look(27,E,CV,W).Rotator().Yaw,90.f,.01f)&&E.Y<125&&E.Z>40&&E.Z<240;
}
