#include "ClaraOfficeLayout.h"
#include "ClaraFrontEnd.h"
#include "TimerManager.h"
#include "ClaraNetworkDevice.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace OfficePalette
{
const FLinearColor Wall(.76,.75,.69), Oak(.35,.21,.11), Teal(.025,.22,.21), Ink(.025,.035,.04);
const FLinearColor Metal(.34,.37,.38), Paper(.82,.82,.76), Blue(.025,.26,.48), GlassTint(.65,.75,.76);
}
using namespace OfficePalette;

AClaraOfficeLayout::AClaraOfficeLayout()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("FloorPlanRoot"));
}
UMaterialInstanceDynamic* AClaraOfficeLayout::Mat(FLinearColor C)
{
    const FString Key = C.ToString();
    if (auto* Existing = Materials.Find(Key)) return Existing->Get();
    auto* M = UMaterialInstanceDynamic::Create(Wood && C.Equals(Oak) ? Wood.Get() : Base.Get(), this);
    M->SetVectorParameterValue(TEXT("Color"), C);
    M->SetVectorParameterValue(TEXT("BaseColor"), C);
    M->SetScalarParameterValue(TEXT("Roughness"), C.Equals(Metal) ? .32f : .66f);
    Materials.Add(Key, M);
    return M;
}
UStaticMeshComponent* AClaraOfficeLayout::Shape(const TCHAR* Name, FVector P, FVector Size, FLinearColor C, bool bSolid, FRotator R, UStaticMesh* Mesh)
{
    P.Y += RoomOffsetY;
    auto* Part = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this,UStaticMeshComponent::StaticClass(),FName(Name)));
    Part->SetupAttachment(RootComponent);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetStaticMesh(Mesh ? Mesh : Cube.Get());
    Part->SetMaterial(0, Mat(C));
    Part->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    if(FString(Name).StartsWith(TEXT("SlidingDoor")))Part->ComponentTags.Add(TEXT("SlidingDoor"));
    Part->RegisterComponent();
    Part->SetWorldLocationAndRotation(P,R);
    Part->SetWorldScale3D(Size/100.f);
    return Part;
}
void AClaraOfficeLayout::Text(const FString& Value, FVector P, float Size, FRotator R, FColor C)
{
    P.Y += RoomOffsetY;
    auto* T = NewObject<UTextRenderComponent>(this);
    T->SetupAttachment(RootComponent);
    T->SetText(FText::FromString(Value));
    T->SetWorldSize(Size);
    T->SetTextRenderColor(C);
    T->SetHorizontalAlignment(EHTA_Center);
    T->SetVerticalAlignment(EVRTA_TextCenter);
    T->RegisterComponent();
    T->SetWorldLocationAndRotation(P,R);
}
void AClaraOfficeLayout::BeginPlay()
{
    Super::BeginPlay();
    Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Office.M_Office"));
    Wood=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Wood.M_Wood"));
    Glass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Glass.M_Glass"));
    if (!ensure(Cube && Cylinder && Sphere && Base)) return;
    if(AClaraFrontEnd::IsPreparingOffice(GetWorld()))
    {
        TWeakObjectPtr<AClaraOfficeLayout> Weak(this);
        GetWorldTimerManager().SetTimerForNextTick([Weak](){if(Weak.IsValid())Weak->BuildNextStage();});
    }
    else while(!bBuildReady)BuildNextStage();
}
void AClaraOfficeLayout::BuildNextStage()
{
    switch(BuildStage++)
    {
        case 0:Architecture();break;
        case 1:RoomOffsetY=60;Reception();break;
        case 2:Cyber();break;
        case 3:RoomOffsetY=200;Lounge();break;
        case 4:Network();break;
        case 5:Apparatus();break;
        case 6:RoomOffsetY=0;Details();break;
        default:bBuildReady=true;return;
    }
    if(BuildStage==7){bBuildReady=true;UE_LOG(LogTemp,Display,TEXT("CLARA_LAYOUT_READY: five zones built; staged=%d"),AClaraFrontEnd::IsPreparingOffice(GetWorld()));return;}
    if(AClaraFrontEnd::IsPreparingOffice(GetWorld()))
    {
        TWeakObjectPtr<AClaraOfficeLayout> Weak(this);
        GetWorldTimerManager().SetTimerForNextTick([Weak](){if(Weak.IsValid())Weak->BuildNextStage();});
    }
}
void AClaraOfficeLayout::ShowPlan(bool bPlan)
{
    for (auto C : Ceiling) if (C) C->SetVisibility(!bPlan);
}
void AClaraOfficeLayout::GlassWallX(float X,float Y,float Length)
{
    Shape(TEXT("PlinthX"),{X,Y,7},{8,Length,14},Paper);
    const int32 Count=FMath::Max(1,FMath::RoundToInt(Length/145));
    const float Bay=Length/Count;
    for(int32 I=0;I<=Count;++I)
        Shape(TEXT("GlassMullionX"),{X,Y-Length/2+I*Bay,163},{5,5,326},Metal);
    Shape(TEXT("GlassHeaderX"),{X,Y,327},{7,Length,6},Metal);
    auto* Pane=Shape(TEXT("GlassPanelX"),{X,Y,169},{1,Length-4,310},GlassTint);
    if(Glass) Pane->SetMaterial(0,Glass);
}
void AClaraOfficeLayout::GlassWallY(float Y,float X,float Length)
{
    Shape(TEXT("PlinthY"),{X,Y,7},{Length,8,14},Paper);
    const int32 Count=FMath::Max(1,FMath::RoundToInt(Length/145));
    const float Bay=Length/Count;
    for(int32 I=0;I<=Count;++I)
        Shape(TEXT("GlassMullionY"),{X-Length/2+I*Bay,Y,163},{5,5,326},Metal);
    Shape(TEXT("GlassHeaderY"),{X,Y,327},{Length,7,6},Metal);
    auto* Pane=Shape(TEXT("GlassPanelY"),{X,Y,169},{Length-4,1,310},GlassTint);
    if(Glass) Pane->SetMaterial(0,Glass);
}
void AClaraOfficeLayout::Portal(float X,float Y,const FString& Name)
{
    Shape(TEXT("DoorFrameLeft"),{X-61,Y,125},{5,12,250},Metal);
    Shape(TEXT("DoorFrameRight"),{X+61,Y,125},{5,12,250},Metal);
    Shape(TEXT("DoorLintel"),{X,Y,248},{126,12,6},Metal);
    Shape(TEXT("AboveDoorWall"),{X,Y,293},{126,10,86},Wall);
    // A sliding leaf is parked against the partition, outside the clear opening.
    const float Slide=(FMath::IsNearlyEqual(X,175.f)||FMath::IsNearlyEqual(X,470.f))?-1.f:1.f;
    const float Face=FMath::IsNearlyEqual(Y,540.f)?-1.f:1.f;
    const FVector P(X+Slide*122,Y+Face*12,121);
    Shape(TEXT("SlidingDoorTrack"),{X+Slide*60,Y+Face*11,253},{246,5,5},Metal,false);
    for(float Edge : {-53.f,53.f})
        Shape(TEXT("SlidingDoorStile"),P+FVector(Edge,0,0),{4,3,238},Metal,false);
    Shape(TEXT("SlidingDoorTop"),P+FVector(0,0,117),{110,3,4},Metal,false);
    Shape(TEXT("SlidingDoorBottom"),P+FVector(0,0,-116),{110,3,6},Metal,false);
    auto* Pane=Shape(TEXT("SlidingDoorGlass"),P,{102,.8,226},GlassTint,false);
    if(Glass)Pane->SetMaterial(0,Glass);
    Shape(TEXT("SlidingDoorHandle"),P+FVector(-Slide*43,-Face*4,-13),{2,3,28},Metal,false);
    Text(Name,{X,Y-9,270},8);
}
bool AClaraOfficeLayout::Asset(const TCHAR* Id,FVector P,FVector TargetSize,float Yaw)
{
    const FString Path=FString::Printf(TEXT("/Game/OfficeAssets/SM_%s.SM_%s"),Id,Id);
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
    if(!Mesh) return false;
    const FBox Bounds=Mesh->GetBoundingBox();
    const FVector Size=Bounds.GetSize();
    if(Size.GetMin()<.01f)return false;
    const FVector Scale=TargetSize/Size;
    auto* C=NewObject<UStaticMeshComponent>(this);
    C->SetupAttachment(RootComponent); C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(Mesh); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    C->RegisterComponent(); C->SetWorldScale3D(Scale); C->SetWorldRotation({0,Yaw,0});
    const FVector Offset(Bounds.GetCenter().X*Scale.X,Bounds.GetCenter().Y*Scale.Y,Bounds.Min.Z*Scale.Z);
    C->SetWorldLocation(P+FVector(0,RoomOffsetY,0)-FRotator(0,Yaw,0).RotateVector(Offset));
    C->ComponentTags.Add(FName(Id));
    if(FString(Id)==TEXT("OfficeTaskChair"))
    {
        C->ComponentTags.Add(TEXT("Inspectable")); C->ComponentTags.Add(TEXT("Chair"));
        for(int32 Slot:{3,4})if(!C->GetMaterial(Slot)||C->GetMaterial(Slot)->GetName().Contains(TEXT("DefaultMaterial")))
        {
            const FString Fallback=FString::Printf(TEXT("/Game/OfficeAssets/M_OfficeTaskChair_%02d.M_OfficeTaskChair_%02d"),Slot,Slot);
            if(auto* Material=LoadObject<UMaterialInterface>(nullptr,*Fallback))C->SetMaterial(Slot,Material);
        }
    }
    if(FString(Id)==TEXT("DesktopTower"))
    {
        C->ComponentTags.Add(TEXT("Inspectable")); C->ComponentTags.Add(TEXT("PC"));
    }
    if(FString(Id)==TEXT("OfficePlant"))
    {
        C->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
        auto* Pot=Shape(TEXT("PlantPotCollision"),P+FVector(0,0,18),{27,27,36},Teal);
        Pot->SetVisibility(false);
    }
    return true;
}
void AClaraOfficeLayout::Architecture()
{
    Shape(TEXT("Foundation"),{950,720,-10},{1940,1480,20},Wall);
    auto* F=Shape(TEXT("OakFloor"),{950,720,.2},{1900,1440,.4},Oak,false);
    if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Floor.M_Floor"))) F->SetMaterial(0,M);
    Shape(TEXT("WestWall"),{-10,720,170},{20,1480,340},Wall);
    Shape(TEXT("EastWall"),{1910,720,170},{20,1480,340},Wall);
    Shape(TEXT("SouthWallLeft"),{55,-10,170},{110,20,340},Wall);
    Shape(TEXT("SouthWallRight"),{1070,-10,170},{1660,20,340},Wall);
    Portal(175,0,TEXT("INGRESSO"));
    Shape(TEXT("NorthSill"),{950,1450,40},{1940,20,80},Paper);
    Shape(TEXT("NorthHeader"),{950,1450,320},{1940,20,40},Wall);
    for(int32 I=0;I<=10;++I)
        Shape(TEXT("NorthWindowFrame"),{I*190.f,1445,190},{7,14,220},Ink);
    auto* Window=Shape(TEXT("NorthWindowGlass"),{950,1449,190},{1890,1,215},GlassTint);
    if(Glass) Window->SetMaterial(0,Glass);
    Shape(TEXT("WindowCrossbar"),{950,1444,175},{1900,9,4},Ink);
    for(float X : {280.f,760.f,1110.f})
    {
        Shape(TEXT("Radiator"),{X,1415,45},{110,13,60},Paper);
        for(int32 Fin=0;Fin<12;++Fin) Shape(TEXT("RadiatorFin"),{X-49+Fin*9,1407,45},{3,4,54},Metal,false);
    }
    // Reception/cyber split; both rooms have their own doorway from the common corridor.
    Shape(TEXT("ReceptionSideWall"),{560,270,170},{12,540,340},Wall);
    GlassWallY(540,55,110); GlassWallY(540,395,330); Portal(175,540,TEXT("ACCOGLIENZA"));
    GlassWallY(540,595,70); Portal(690,540,TEXT("LAB CYBER")); GlassWallY(540,1325,1150);
    GlassWallX(560,1100,680);
    GlassWallY(760,205,410); Portal(470,760,TEXT("BRIEFING")); GlassWallY(760,545,30);
    GlassWallY(760,590,60); Portal(680,760,TEXT("LAB RETI")); GlassWallY(760,1045,610);
    GlassWallX(1350,1100,680);
    GlassWallY(760,1400,100); Portal(1510,760,TEXT("SALA APPARATI")); GlassWallY(760,1735,330);
    for(float X : {6.f,1894.f}) Shape(TEXT("SkirtingSide"),{X,720,6},{2,1440,12},Paper,false);
    auto* Roof=Shape(TEXT("Ceiling"),{950,720,345},{1940,1480,12},Paper);
    Ceiling.Add(Roof);
    for(float X : {200.f,650.f,1100.f,1550.f})
        for(float Y : {290.f,1090.f})
        {
            Ceiling.Add(Shape(TEXT("LEDFrame"),{X,Y,331},{115,54,4},Ink,false));
            Ceiling.Add(Shape(TEXT("LEDDiffuser"),{X,Y,328},{108,48,1},Paper,false));
            auto* L=GetWorld()->SpawnActor<ARectLight>(FVector(X,Y,325),FRotator(-90,0,0));
            L->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Cast<URectLightComponent>(L->GetLightComponent())->SetIntensity(2600);
            Cast<URectLightComponent>(L->GetLightComponent())->SetSourceWidth(108);
            Cast<URectLightComponent>(L->GetLightComponent())->SetSourceHeight(48);
            Cast<URectLightComponent>(L->GetLightComponent())->SetLightColor(FLinearColor(1,.94,.84));
            Cast<URectLightComponent>(L->GetLightComponent())->SetCastShadows(false);
        }
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(950,1800,1800),FRotator(-38,-75,0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(2.8);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.93,.84));
    Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->SetAtmosphereSunLight(true);
    auto* Atmosphere=NewObject<USkyAtmosphereComponent>(this);
    Atmosphere->SetupAttachment(RootComponent); Atmosphere->RegisterComponent();
    auto* Sky=GetWorld()->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.15); Sky->GetLightComponent()->RecaptureSky();
    Shape(TEXT("BayWater"),{950,7000,-75},{26000,11000,15},FLinearColor(.07,.25,.31),false);
    for(int32 I=0;I<18;++I)
    {
        const float H=180+(I*37)%160;
        Shape(TEXT("DistantNaplesBuildings"),{-2200+I*360.f,3100+(I%3)*230.f,H/2-70},{220,200,H},FLinearColor(.50,.48,.39),false);
    }
}
void AClaraOfficeLayout::Table(FVector P,FVector2D Size,float Height)
{
    if(Asset(TEXT("OfficeDesk"),P,{Size.X,Size.Y,Height+2},180)) return;
    Shape(TEXT("DeskTop"),P+FVector(0,0,Height),{Size.X,Size.Y,4},Oak);
    for(float X : {-Size.X/2+10,Size.X/2-10})
        for(float Y : {-Size.Y/2+10,Size.Y/2-10})
            Shape(TEXT("DeskLeg"),P+FVector(X,Y,Height/2),{4,4,Height-3},Metal);
    Shape(TEXT("DeskCableTray"),P+FVector(0,Size.Y/2-12,Height-12),{Size.X-35,12,8},Ink,false);
}
void AClaraOfficeLayout::Chair(FVector P,float Yaw)
{
    // Preserve the source proportions: 66.8 x 69.2 x 110.1 cm, including the casters.
    if(!Asset(TEXT("OfficeTaskChair"),P,{66.8,69.2,110.1},Yaw))
        UE_LOG(LogTemp,Error,TEXT("CLARA_CHAIR_MISSING: imported chair unavailable."));
}
void AClaraOfficeLayout::Monitor(FVector P,bool bRed)
{
    if(Asset(TEXT("DesktopMonitor"),P,{54.09,18,41.64},180)) return;
    Shape(TEXT("MonitorBase"),P+FVector(0,0,1),{27,17,2},Ink,false);
    Shape(TEXT("MonitorStem"),P+FVector(0,5,14),{4,4,26},Metal,false);
    Shape(TEXT("MonitorFrame"),P+FVector(0,6,30),{55,3,34},Ink);
    Shape(TEXT("MonitorDisplay"),P+FVector(0,4.3,30),{51,0.3,29},FLinearColor(.015,.028,.032),false);
    for(int32 Row=0;Row<6;++Row)
        Shape(TEXT("ScreenLine"),P+FVector(-9+(Row%2)*3,4,39-Row*3.5),{28.f+(Row%3)*5,.15,0.4},bRed?FLinearColor(.32,.11,.07):FLinearColor(.10,.31,.29),false);
    Shape(TEXT("MonitorLED"),P+FVector(21,4.2,14.5),{.6,.3,.5},FLinearColor(.2,.6,.25),false);
}
void AClaraOfficeLayout::Tower(FVector P)
{
    if(Asset(TEXT("DesktopTower"),P,{20.5,41.65,47.4},180)) return;
    auto* C=Shape(TEXT("DesktopCase"),P+FVector(0,0,23),{21,43,46},Ink);
    C->ComponentTags.Add(TEXT("Inspectable")); C->ComponentTags.Add(TEXT("PC"));
    Shape(TEXT("CaseFront"),P+FVector(0,-21.7,23),{19,.6,44},Metal,false);
    for(int32 I=0;I<10;++I) Shape(TEXT("CaseVent"),P+FVector(0,-22.1,12+I*1.4),{15,.3,.5},Ink,false);
    Shape(TEXT("PowerButton"),P+FVector(6,-22.3,41),{1.5,.3,1.5},Blue,false,FRotator::ZeroRotator,Sphere);
    for(float X : {-5.f,0.f}) Shape(TEXT("USBFront"),P+FVector(X,-22.3,39),{2.4,.4,.8},Ink,false);
    Shape(TEXT("RearIO"),P+FVector(0,21.8,26),{17,.5,33},Metal,false);
    auto* Port=Shape(TEXT("PC_Ethernet"),P+FVector(5,22.2,30),{1.7,.4,1.5},Ink,false);
    Port->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Port->ComponentTags.Add(TEXT("Port"));
    Text(TEXT("LAN"),P+FVector(5,22.6,33),1.2,{0,90,0},FColor::White);
}
void AClaraOfficeLayout::Computer(FVector P,bool bDual,bool bRed,bool bTower)
{
    if(bDual){Monitor(P+FVector(-28,18,78),bRed); Monitor(P+FVector(29,18,78),bRed);} else Monitor(P+FVector(-12,18,78),bRed);
    if(bTower) Tower(P+FVector(61,12,77));
    Shape(TEXT("Keyboard"),P+FVector(-12,-15,78),{44,16,1.8},Ink,false);
    for(int32 R=0;R<4;++R) for(int32 K=0;K<12;++K)
        Shape(TEXT("KeyCap"),P+FVector(-31+K*3.4,-20+R*3.5,79.2),{2.8,2.6,.4},Metal,false);
    Shape(TEXT("MouseMat"),P+FVector(31,-14,77.4),{24,24,.2},Teal,false);
    Shape(TEXT("Mouse"),P+FVector(31,-15,79),{6,10,3.8},Ink,false,FRotator::ZeroRotator,Sphere);
}
void AClaraOfficeLayout::Switch(FVector P,int32 Ports)
{
    auto* Device=GetWorld()->SpawnActor<AClaraNetworkDevice>(P+FVector(0,RoomOffsetY,-2.8),FRotator::ZeroRotator);
    Device->Initialize(TEXT("Cisco2960X"));
    if(P.Z<100)Device->Tags.Add(TEXT("BenchDevice"));
}
void AClaraOfficeLayout::Router(FVector P)
{
    auto* Device=GetWorld()->SpawnActor<AClaraNetworkDevice>(P+FVector(0,RoomOffsetY,-2.8),FRotator::ZeroRotator);
    Device->Initialize(TEXT("Cisco1941")); Device->Tags.Add(TEXT("BenchDevice"));
}
void AClaraOfficeLayout::Laptop(FVector P)
{
    auto* C=Shape(TEXT("ConsoleLaptop"),P+FVector(0,0,1),{33,23,1.5},Metal);
    C->ComponentTags.Add(TEXT("Inspectable")); C->ComponentTags.Add(TEXT("Laptop"));
    Shape(TEXT("LaptopKeys"),P+FVector(0,1,2),{28,13,.4},Ink,false);
    for(int32 R=0;R<4;++R) for(int32 K=0;K<10;++K)
        Shape(TEXT("LaptopKey"),P+FVector(-12+K*2.6,-4+R*2.6,2.4),{2,2,.3},Paper,false);
    Shape(TEXT("LaptopLid"),P+FVector(0,11,13),{33,1,22},Ink,false);
    Shape(TEXT("LaptopDisplay"),P+FVector(0,10.3,13),{30,.2,19},FLinearColor(.015,.028,.03),false);
    Text(TEXT("CONSOLE"),P+FVector(0,10,14),2,FRotator(0,-90,0),FColor(130,180,170));
    auto* Port=Shape(TEXT("Laptop_USB"),P+FVector(-16.8,0,1),{.5,2,1},Ink,false);
    Port->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Port->ComponentTags.Add(TEXT("Port"));
}
void AClaraOfficeLayout::Cable(FVector P,float Radius,FLinearColor C)
{
    for(int32 I=0;I<36;++I)
    {
        const float A=I*2*PI/36;
        Shape(TEXT("CableCoil"),P+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0),{.55,.55,Radius*.18f},C,false,FRotationMatrix::MakeFromZ(FVector(-FMath::Sin(A),FMath::Cos(A),0)).Rotator(),Cylinder);
    }
    auto* Plug=Shape(TEXT("Cavo_RJ45"),P+FVector(Radius+3,0,.4),{3,1.3,1.1},C,false);
    Plug->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Plug->ComponentTags.Add(TEXT("Cable"));
    Shape(TEXT("RJ45Clip"),P+FVector(Radius+3,0,1.1),{1.5,.5,.2},Paper,false);
}
void AClaraOfficeLayout::Reception()
{
    Shape(TEXT("ReceptionRug"),{295,205,.6},{410,300,.5},FLinearColor(.24,.25,.22),false);
    // One working surface; the front panel belongs to this same reception desk.
    Table({340,300,0},{250,85}); Computer({305,300,0}); Chair({305,390,0},180);
    Shape(TEXT("ReceptionModestyPanel"),{340,258,39},{240,3,60},Teal);
    Text(TEXT("TNES"),{340,256,43},12);
    for(float X : {270.f,425.f}) Chair({X,130,0});
    Shape(TEXT("Printer"),{440,300,90},{40,33,20},Paper,false);
    Shape(TEXT("PrinterOutput"),{440,282,89},{29,1.5,4},Ink,false);
    Shape(TEXT("BadgeTray"),{245,270,80},{40,25,2},Metal,false);
    for(int32 I=0;I<4;++I) Shape(TEXT("VisitorBadge"),{232+I*8.f,270,81.3},{5,9,.3},Paper,false);
    Shape(TEXT("PhoneBase"),{354,270,82},{18,22,4},Ink,false);
    Shape(TEXT("PhoneHandset"),{354,269,86},{20,6,5},Ink,false,FRotator::ZeroRotator,Sphere);
    Text(TEXT("TNES"),{12,295,225},32,{0,0,0});
    Text(TEXT("NAPOLI / ACCOGLIENZA"),{12,295,190},8,{0,0,0});
    Plant({505,80,0},120); Plant({505,350,0},105);
}
void AClaraOfficeLayout::Lounge()
{
    Table({265,1000,0},{255,110});
    for(float X : {180.f,265.f,350.f}){Chair({X,910,0}); Chair({X,1090,0},180);}
    Shape(TEXT("WhiteboardFrame"),{10,980,170},{4,220,120},Metal);
    Shape(TEXT("Whiteboard"),{13,980,170},{1,213,113},Paper,false);
    Text(TEXT("BRIEFING"),{16,980,185},10,{0,0,0});
    Shape(TEXT("CoffeeCabinet"),{490,1000,43},{85,240,86},Paper);
    Shape(TEXT("CoffeeCounter"),{490,1000,88},{90,245,4},Oak);
    Shape(TEXT("EspressoMachine"),{490,1070,112},{48,40,44},Ink);
    Shape(TEXT("EspressoFront"),{490,1049,113},{43,1,30},Metal,false);
    for(float X : {480.f,500.f})
    {
        Shape(TEXT("CoffeeCup"),{X,1036,95},{7,7,8},Paper,false,FRotator::ZeroRotator,Cylinder);
        Shape(TEXT("CoffeeNozzle"),{X,1038,110},{2,7,2},Metal,false);
    }
    for(int32 I=0;I<4;++I) Shape(TEXT("CupStack"),{477+I*10.f,950,95},{7,7,10},Paper,false,FRotator::ZeroRotator,Cylinder);
    Shape(TEXT("LoungeRug"),{225,740,.6},{320,180,.4},FLinearColor(.27,.27,.22),false);
    for(float X : {120.f,325.f})
    {
        if(Asset(TEXT("OfficeArmchair"),{X,735,0},{78,88,95},180)) continue;
        Shape(TEXT("ArmchairSeat"),{X,735,33},{78,72,27},Teal,true,FRotator::ZeroRotator,Sphere);
        Shape(TEXT("ArmchairBack"),{X,764,60},{82,23,72},Teal,true,FRotator::ZeroRotator,Sphere);
        for(float S : {-40.f,40.f}) Shape(TEXT("ArmchairArm"),{X+S,735,48},{18,72,42},Teal,true,FRotator::ZeroRotator,Sphere);
    }
    if(!Asset(TEXT("CoffeeTable"),{225,720,0},{90,52,39}))
    {
        Shape(TEXT("RoundCoffeeTable"),{225,720,34},{58,58,5},Oak,true,FRotator::ZeroRotator,Cylinder);
        Shape(TEXT("RoundTableStem"),{225,720,16},{9,9,30},Metal,false,FRotator::ZeroRotator,Cylinder);
    }
    Plant({70,1140,0},130); Plant({475,1160,0},120);
}
void AClaraOfficeLayout::Network()
{
    Table({955,900,0},{470,100});
    Monitor({785,918,78}); Tower({838,920,77});
    Switch({945,912,80}); Router({1035,910,80});
    Shape(TEXT("CableTester"),{803,864,80},{8,15,4},FLinearColor(.5,.36,.08),false);
    Shape(TEXT("CrimpTool"),{990,865,79},{16,5,3},Metal,false,{0,35,0});
    for(float X : {760.f,1120.f})
    {
        Table({X,1100,0},{165,70});
        if(X==760.f){Computer({X,1100,0},false,false,false);}
    }

    Plant({1260,1130,0},125);
}
void AClaraOfficeLayout::WallScreen(FVector P,bool bLogs)
{
    Shape(TEXT("WallScreenFrame"),P,{145,5,83},Ink);
    Shape(TEXT("WallScreenGlass"),P+FVector(0,-2.8,0),{138,.4,76},FLinearColor(.015,.035,.045),false);
    if(bLogs)
        for(int32 I=0;I<12;++I) Shape(TEXT("LogRow"),P+FVector(-18+(I%3)*5,-3.1,28-I*4.8),{80.f+(I%3)*8,.2,.8},FLinearColor(.12,.36,.30),false);
    else
    {
        for(int32 I=0;I<6;++I)
        {
            const float X=-48+(I%3)*48,Y=I<3?-18:18;
            Shape(TEXT("NetworkNode"),P+FVector(X,-3.2,Y),{12,.4,9},Teal,false);
            if(I%3<2)Shape(TEXT("NetworkLink"),P+FVector(X+24,-3.1,Y),{36,.3,.6},Blue,false);
            if(I<3)Shape(TEXT("NetworkLinkVertical"),P+FVector(X,-3.1,0),{.6,.3,27},Blue,false);
        }
    }
    Text(bLogs?TEXT("LOG / MONITORAGGIO"):TEXT("NETWORK VISION"),P+FVector(0,-3.5,49),6,FRotator(0,-90,0),FColor(145,185,180));
}
void AClaraOfficeLayout::Cyber()
{
    for(float X : {735.f,960.f,1450.f,1700.f})
    {
        Table({X,235,0},{185,85}); Computer({X,235,0},true,X>1200); Chair({X,140,0});
    }
    WallScreen({1010,470,180}); WallScreen({1630,470,180},true);
    Shape(TEXT("SOCSign"),{840,481,279},{135,3,22},Paper,false);
    Text(TEXT("BLUE TEAM"),{840,479,279},8);
    Shape(TEXT("LabSign"),{1575,481,279},{160,3,22},Paper,false);
    Text(TEXT("RED TEAM / LAB"),{1575,479,279},8);
    Shape(TEXT("CyberStorage"),{1855,190,85},{65,300,170},Oak);
    for(int32 Shelf=0;Shelf<4;++Shelf)
    {
        Shape(TEXT("ShelfOpening"),{1819,190,32+Shelf*38.f},{2,280,30},Ink,false);
        for(int32 Book=0;Book<8;++Book) Shape(TEXT("CyberBook"),{1816,74+Book*31.f,32+Shelf*38.f},{18,16,26},Book%2?Teal:Paper,false);
    }
    Plant({1850,370,0},115);
    Shape(TEXT("PrinterCabinet"),{605,100,40},{70,65,80},Paper);
    Shape(TEXT("SharedPrinter"),{605,100,92},{45,38,24},Paper);
}
void AClaraOfficeLayout::Rack(FVector P)
{
    Shape(TEXT("RackBase"),P+FVector(0,0,8),{65,90,12},Ink);
    Shape(TEXT("RackRoof"),P+FVector(0,0,208),{65,90,7},Ink);
    for(float X : {-29.f,29.f})for(float Y : {-42.f,42.f})
        Shape(TEXT("RackUpright"),P+FVector(X,Y,108),{5,5,196},Metal);
    for(int32 U=0;U<4;++U)
    {
        const float Z=35+U*31;
        Shape(TEXT("RackServer"),P+FVector(0,0,Z),{55,75,15},Metal);
        Shape(TEXT("ServerFace"),P+FVector(0,-38,Z),{54,2,14},Ink,false);
        for(int32 Disk=0;Disk<6;++Disk) Shape(TEXT("DiskCaddy"),P+FVector(-21+Disk*8.f,-39.3,Z),{6.8,.5,10},Metal,false);
        Shape(TEXT("ServerLED"),P+FVector(25,-39.6,Z+3),{1,.3,1},FLinearColor(.05,.55,.18),false);
    }
    Switch(P+FVector(0,-24,165)); Switch(P+FVector(0,-24,185));
    Shape(TEXT("UPS"),P+FVector(0,0,18),{53,74,13},Ink);
    Text(TEXT("UPS"),P+FVector(0,-39,18),3,FRotator(0,-90,0),FColor::White);
}
void AClaraOfficeLayout::Apparatus()
{
    auto* Floor=Shape(TEXT("ServerRaisedFloor"),{1625,900,.8},{540,580,1},FLinearColor(.23,.25,.25),false);
    (void)Floor;
    for(int32 I=0;I<6;++I)Shape(TEXT("FloorTileJoint"),{1360+I*90.f,900,1.5},{.6,580,.1},Metal,false);
    for(int32 I=0;I<6;++I)Shape(TEXT("FloorTileJoint"),{1625,630+I*90.f,1.5},{540,.6,.1},Metal,false);
    Rack({1505,1000,0}); Rack({1700,1000,0});
    WallScreen({1800,1230,155});
    for(float X : {1460.f,1760.f})
    {
        Shape(TEXT("CableLadderRail"),{X,1050,290},{4,220,5},Metal,false);
        for(int32 I=0;I<8;++I) Shape(TEXT("CableLadderRung"),{1610,950+I*28.f,290},{300,3,3},Metal,false);
    }
    for(int32 I=0;I<8;++I)Shape(TEXT("OverheadBlueCable"),{1510+I*5.f,1100,294},{2,200,2},Blue,false);
    Shape(TEXT("AirConditioning"),{1880,950,270},{25,180,45},Paper);
    for(int32 I=0;I<9;++I)Shape(TEXT("ACVent"),{1866,891+I*15.f,264},{1,9,6},Ink,false);

}
void AClaraOfficeLayout::Plant(FVector P,float Height)
{
    if(Asset(TEXT("OfficePlant"),P,{Height*.65f,Height*.65f,Height}))return;
    Shape(TEXT("PlantPot"),P+FVector(0,0,18),{33,33,36},Teal,true,FRotator::ZeroRotator,Cylinder);
    Shape(TEXT("PotSoil"),P+FVector(0,0,36.2),{29,29,1},FLinearColor(.055,.035,.02),false,FRotator::ZeroRotator,Cylinder);
    Shape(TEXT("PlantStem"),P+FVector(0,0,Height/2+18),{2,2,Height-30},FLinearColor(.13,.17,.04),false,FRotator::ZeroRotator,Cylinder);
    for(int32 I=0;I<12;++I)
    {
        const float A=I*137.5f;
        Shape(TEXT("PlantLeaf"),P+FVector(FMath::Cos(FMath::DegreesToRadians(A))*14,FMath::Sin(FMath::DegreesToRadians(A))*14,43+I*(Height-43)/12),{40,14,3},FLinearColor(.07,.18,.045),false,{float(I%3)*15,A,15},Sphere);
    }
}
void AClaraOfficeLayout::Details()
{
    for(FVector P : {FVector(35,650,0),FVector(1840,650,0)}) Plant(P,115);
    for(float X : {200.f,800.f,1250.f,1750.f})
    {
        Shape(TEXT("WallPowerSocket"),{X,9,30},{12,1,8},Paper,false);
        for(float S : {-3.f,3.f}) Shape(TEXT("SocketHole"),{X+S,8.3,30},{.7,.5,2},Ink,false);
    }
    Shape(TEXT("Extinguisher"),{75,735,53},{18,18,60},FLinearColor(.4,.025,.015),true,FRotator::ZeroRotator,Cylinder);
    Shape(TEXT("ExtinguisherHandle"),{75,735,86},{17,8,4},Ink,false);
    Text(TEXT("LABORATORI / NAPOLI"),{1896,650,210},11,{0,180,0});
    Text(TEXT("RETI"),{950,810,5},18,{90,-90,0});
    Text(TEXT("CYBER"),{1185,410,5},18,{90,-90,0});
}
