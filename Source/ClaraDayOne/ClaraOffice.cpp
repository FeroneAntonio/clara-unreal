#include "ClaraOffice.h"
#include "ClaraDisplay.h"
#include "ClaraPlayer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    const FLinearColor Plaster(.73f, .73f, .68f);
    const FLinearColor Ink(.025f, .04f, .055f);
    const FLinearColor Aluminium(.31f, .34f, .35f);
    const FLinearColor Oak(.35f, .21f, .11f);
    const FLinearColor Blue(.025f, .21f, .55f);
    const FLinearColor Paper(.88f, .88f, .81f);
    const FLinearColor Teal(.025f, .30f, .29f);
}

AClaraOffice::AClaraOffice()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("OfficeRoot"));
    CableSpline = CreateDefaultSubobject<USplineComponent>(TEXT("EthernetCablePath"));
    CableSpline->SetupAttachment(RootComponent);
    ConsoleSpline = CreateDefaultSubobject<USplineComponent>(TEXT("ConsoleCablePath"));
    ConsoleSpline->SetupAttachment(RootComponent);
}

UMaterialInstanceDynamic* AClaraOffice::Material(FLinearColor Color, float Roughness)
{
    const FString Key = Color.ToString() + FString::SanitizeFloat(Roughness);
    if (const auto* Existing = Materials.Find(Key)) return Existing->Get();
    auto* ParentMaterial = WoodMaterial && Color.Equals(Oak) ? WoodMaterial.Get() : BaseMaterial.Get();
    auto* Result = UMaterialInstanceDynamic::Create(ParentMaterial, this);
    Result->SetVectorParameterValue(TEXT("Color"), Color);
    Result->SetVectorParameterValue(TEXT("BaseColor"), Color);
    Result->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    Materials.Add(Key, Result);
    return Result;
}

AStaticMeshActor* AClaraOffice::Box(const FString& Name, FVector Position, FVector Size,
    FLinearColor Color, FName Interaction, bool bCollision, FRotator Rotation)
{
    auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, Rotation);
    Actor->SetOwner(this);
#if WITH_EDITOR
    Actor->SetActorLabel(Name);
#endif
    Actor->Tags.Add(TEXT("Clara.Office"));
    if (!Interaction.IsNone()) Actor->Tags.Add(Interaction);
    auto* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(CubeMesh);
    Mesh->SetMaterial(0, Material(Color));
    Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Actor->SetActorScale3D(Size / 100.f);
    return Actor;
}

AStaticMeshActor* AClaraOffice::Cylinder(const FString& Name, FVector Position, FVector Size,
    FLinearColor Color, bool bCollision, FRotator Rotation)
{
    auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, Rotation);
    Actor->SetOwner(this);
#if WITH_EDITOR
    Actor->SetActorLabel(Name);
#endif
    Actor->Tags.Add(TEXT("Clara.Office"));
    auto* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(CylinderMesh);
    Mesh->SetMaterial(0, Material(Color));
    Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Actor->SetActorScale3D(Size / 100.f);
    return Actor;
}

void AClaraOffice::Label(const FString& Text, FVector Position, float Size, FColor Color, FRotator Rotation)
{
    auto* Component = NewObject<UTextRenderComponent>(this);
    Component->SetupAttachment(RootComponent);
    Component->SetText(FText::FromString(Text));
    Component->SetWorldSize(Size);
    Component->SetTextRenderColor(Color);
    Component->SetHorizontalAlignment(EHTA_Center);
    Component->SetVerticalAlignment(EVRTA_TextCenter);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->RegisterComponent();
    Component->SetWorldLocationAndRotation(Position, Rotation);
}

void AClaraOffice::BeginPlay()
{
    Super::BeginPlay();
    CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Office.M_Office"));
    WoodMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Wood.M_Wood"));
    if (!BaseMaterial) BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!ensure(CubeMesh && CylinderMesh && BaseMaterial)) return;
    LucaMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Prototypes/SM_Luca.SM_Luca"));
    AlessiaMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Prototypes/SM_Alessia.SM_Alessia"));
    JonnyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Prototypes/SM_Jonny.SM_Jonny"));
    // Keep the neutral bind-pose sample out of the scene until a natural pose is validated.
    if (JonnyMesh && JonnyMesh->GetBounds().BoxExtent.X > 55.f) JonnyMesh = nullptr;
    Room();
    Desk(FVector(400, 50, 0), true);
    Desk(FVector(500, -340, 0), false);
    Rack();
    Details();
    People();

    for (int32 Index = 0; Index < 12; ++Index)
    {
        auto* Segment = NewObject<USplineMeshComponent>(this);
        Segment->SetupAttachment(RootComponent);
        Segment->SetStaticMesh(CylinderMesh);
        Segment->SetForwardAxis(ESplineMeshAxis::Z, false);
        Segment->SetStartScale(FVector2D(.007f));
        Segment->SetEndScale(FVector2D(.007f));
        Segment->SetMaterial(0, Material(Blue));
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Segment->RegisterComponent();
        CableSegments.Add(Segment);
    }
    FreeConnector = Box(TEXT("RJ45 Ethernet - impugnatura blu"), FreeCableRest, FVector(1.6, 1.2, 1.1), Blue, TEXT("Clara.Cable"));
    FreeConnector->Tags.Add(TEXT("Clara.CableRoot"));
    auto AddConnectorPart = [this](const TCHAR* Name, FVector Offset, FVector Size, UMaterialInterface* PartMaterial,
        bool bTarget = false, FRotator Rotation = FRotator::ZeroRotator)
    {
        auto* Part = NewObject<UStaticMeshComponent>(FreeConnector, Name);
        Part->SetupAttachment(FreeConnector->GetRootComponent());
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetStaticMesh(CubeMesh);
        Part->SetMaterial(0, PartMaterial);
        Part->SetCollisionEnabled(bTarget ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        Part->SetCollisionResponseToAllChannels(ECR_Ignore);
        if (bTarget) Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Part->RegisterComponent();
        Part->SetWorldLocationAndRotation(FreeCableRest + Offset, Rotation);
        Part->SetWorldScale3D(Size / 100.f);
        if (bTarget) Part->SetVisibility(false);
    };
    auto* ClearPlastic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass"));
    if (!ClearPlastic) ClearPlastic = Material(FLinearColor(.65, .75, .78), .12f);
    AddConnectorPart(TEXT("Corpo RJ45 policarbonato"), FVector(1.2, 0, .02), FVector(2.2, 1.15, 1.05), ClearPlastic);
    AddConnectorPart(TEXT("Linguetta di aggancio"), FVector(.8, 0, .68), FVector(1.5, .42, .13), ClearPlastic, false, FRotator(-12, 0, 0));
    for (int32 Pin = 0; Pin < 8; ++Pin)
        AddConnectorPart(*FString::Printf(TEXT("Contatto ottone %d"), Pin + 1), FVector(1.6, -.45 + Pin * .128, .5),
            FVector(.9, .065, .065), Material(FLinearColor(.68, .52, .16), .24f));
    AddConnectorPart(TEXT("Presa assistita connettore"), FVector(.5, 0, 0), FVector(5, 4, 3), Material(Blue), true);
    for (int32 Layer = 0; Layer < 2; ++Layer)
        for (int32 Part = 0; Part < 24; ++Part)
        {
            const float Angle = Part * 2 * PI / 24;
            const FVector Position(355 + FMath::Cos(Angle) * 9, 178 + FMath::Sin(Angle) * 9, 63.3 + Layer * .65);
            auto* Coil = Cylinder(TEXT("Ethernet ripiegato sul ripiano"), Position, FVector(.65, .65, 2.4), Blue,
                false, FRotationMatrix::MakeFromZ(FVector(-FMath::Sin(Angle), FMath::Cos(Angle), 0)).Rotator());
            CableRestCoilSegments.Add(Coil);
        }
    // The cable socket and jacket are represented separately, independent of network state.
    UpdateCable();
    ConsoleSpline->SetSplinePoints({FVector(629.7, 264.4, 124), FVector(623, 234, 111),
        FVector(606, 211, 87), FVector(583, 166, 72), FVector(581, 126, 75), FVector(592.7, 110, 79.2)}, ESplineCoordinateSpace::World, true);
    const float ConsoleLength = ConsoleSpline->GetSplineLength();
    for (int32 Index = 0; Index < 12; ++Index)
    {
        auto* Segment = NewObject<USplineMeshComponent>(this);
        Segment->SetupAttachment(RootComponent);
        Segment->SetStaticMesh(CylinderMesh);
        Segment->SetForwardAxis(ESplineMeshAxis::Z, false);
        Segment->SetStartScale(FVector2D(.006f));
        Segment->SetEndScale(FVector2D(.006f));
        Segment->SetMaterial(0, Material(FLinearColor(.22, .54, .67)));
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Segment->RegisterComponent();
        const float Start = ConsoleLength * Index / 12.f;
        const float End = ConsoleLength * (Index + 1) / 12.f;
        Segment->SetStartAndEnd(ConsoleSpline->GetLocationAtDistanceAlongSpline(Start, ESplineCoordinateSpace::Local),
            ConsoleSpline->GetDirectionAtDistanceAlongSpline(Start, ESplineCoordinateSpace::Local) * (End - Start),
            ConsoleSpline->GetLocationAtDistanceAlongSpline(End, ESplineCoordinateSpace::Local),
            ConsoleSpline->GetDirectionAtDistanceAlongSpline(End, ESplineCoordinateSpace::Local) * (End - Start));
        ConsoleSegments.Add(Segment);
    }
    ConsoleSwitchPlug = Box(TEXT("RJ45 console collegato"), FVector(629.7, 264.4, 124), FVector(1.1, 3.4, 1), FLinearColor(.22, .54, .67), NAME_None, false);
    ConsoleLaptopPlug = Box(TEXT("USB console collegato"), FVector(592.7, 110, 79.2), FVector(4, 1.7, .9), Aluminium, NAME_None, false);
    UpdateConsoleCable(false, false, false);
    SetVision(false);
}

void AClaraOffice::Room()
{
    Box(TEXT("Solaio"), FVector(100, 0, -12), FVector(1900, 1240, 24), Plaster);
    auto* Floor = Box(TEXT("Pavimento PBR"), FVector(100, 0, .15), FVector(1900, 1240, .3),
        FLinearColor(.32f, .34f, .34f), NAME_None, false);
    if (auto* FloorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Floor.M_Floor")))
        Floor->GetStaticMeshComponent()->SetMaterial(0, FloorMaterial);
    Box(TEXT("Parete sud"), FVector(100, -620, 170), FVector(1900, 20, 340), Plaster);
    Box(TEXT("Parete est"), FVector(1040, 0, 170), FVector(20, 1240, 340), Plaster);
    Box(TEXT("Parete ingresso"), FVector(-840, 0, 170), FVector(20, 1240, 340), Plaster);
    Box(TEXT("Davanzale nord"), FVector(100, 620, 45), FVector(1900, 24, 90), Plaster);
    Box(TEXT("Architrave nord"), FVector(100, 620, 310), FVector(1900, 24, 60), Plaster);
    for (int32 Index = 0; Index <= 6; ++Index)
    {
        const float X = -820 + Index * 310;
        Box(TEXT("Montante finestra"), FVector(X, 617, 190), FVector(8, 14, 200), Ink);
        Box(TEXT("Radiatore"), FVector(X + 100, 597, 52), FVector(120, 13, 56), Paper);
        for (int32 Fin = 0; Fin < 12; ++Fin)
            Box(TEXT("Elemento radiatore"), FVector(X + 45 + Fin * 10, 588, 52), FVector(2.2, 3, 55), Aluminium, NAME_None, false);
    }
    Box(TEXT("Infisso traverso"), FVector(100, 617, 190), FVector(1900, 12, 6), Ink);
    for (const float Y : {-608.f, 608.f})
        Box(TEXT("Battiscopa"), FVector(100, Y, 6), FVector(1860, 2, 12), Ink, NAME_None, false);
    Box(TEXT("Controsoffitto"), FVector(100, 0, 350), FVector(1900, 1240, 16), FLinearColor(.72, .72, .69));
    for (int32 X = 0; X < 6; ++X)
    {
        Box(TEXT("Giunto soffitto"), FVector(-650 + X * 300, 0, 340), FVector(1.2, 1200, 1), Aluminium, NAME_None, false);
        for (const float Y : {-320.f, 250.f})
        {
            const FVector P(-650 + X * 300, Y, 336);
            Box(TEXT("Cornice pannello LED"), P, FVector(120, 60, 3), Aluminium, NAME_None, false);
            Box(TEXT("Diffusore"), P - FVector(0, 0, 2), FVector(115, 55, 1), Paper, NAME_None, false);
            auto* Light = GetWorld()->SpawnActor<ARectLight>(P - FVector(0, 0, 8), FRotator(-90, 0, 0));
            auto* Component = Cast<URectLightComponent>(Light->GetLightComponent());
            Component->SetMobility(EComponentMobility::Movable);
            Component->SetIntensity(4400);
            Component->SetSourceWidth(115);
            Component->SetSourceHeight(55);
            Component->SetLightColor(FLinearColor(1, .91, .79));
            Component->SetCastShadows(false);
        }
    }
    // Low, open cabinetry separates reception without blocking the walk to the laboratory.
    Box(TEXT("Zoccolo divisorio reception"), FVector(-120, 380, 38), FVector(12, 450, 76), Plaster);
    Box(TEXT("Corrimano divisorio"), FVector(-120, 380, 78), FVector(16, 450, 4), Oak);
    for (const float Y : {156.f, 305.f, 455.f, 605.f})
        Box(TEXT("Telaio vetrata reception"), FVector(-120, Y, 191), FVector(7, 5, 226), Aluminium);
    Box(TEXT("Traverso superiore vetrata"), FVector(-120, 380, 305), FVector(7, 450, 5), Aluminium);
    if (auto* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Glass.M_Glass")))
    {
        for (int32 Pane = 0; Pane < 3; ++Pane)
        {
            auto* Window = Box(TEXT("Lastra vetro reception"), FVector(-120, 231 + Pane * 150, 191), FVector(1, 144, 220), Paper);
            Window->GetStaticMeshComponent()->SetMaterial(0, Glass);
        }
    }
    Box(TEXT("Bancone reception"), FVector(-380, 180, 50), FVector(80, 230, 100), Teal);
    Box(TEXT("Piano bancone"), FVector(-380, 180, 102), FVector(95, 240, 4), Oak);
    if (!AlessiaMesh)
        Box(TEXT("Telefono reception provvisorio"), FVector(-430, 145, 108), FVector(15, 22, 8), Ink, TEXT("Clara.Reception"));
    Label(TEXT("ALESSIA | ACCOGLIENZA"), FVector(-432, 220, 115), 5, FColor(220, 232, 226));
    Box(TEXT("Badge primo giorno"), FVector(-432, 105, 106), FVector(9, 6, .4), Paper,
        AlessiaMesh ? NAME_None : FName(TEXT("Clara.Reception")));
    Label(TEXT("TNES"), FVector(-828, 160, 237), 34, FColor(36, 104, 105), FRotator(0, 0, 0));
    Label(TEXT("LABORATORI  /  NAPOLI"), FVector(-828, 160, 198), 11, FColor(52, 67, 67), FRotator(0, 0, 0));
    Label(TEXT("RETI  +  SICUREZZA"), FVector(-131, 380, 158), 13, FColor(47, 86, 86));
    Label(TEXT("LABORATORIO 01   >"), FVector(-100, -607, 208), 15, FColor(35, 80, 79), FRotator(0, 90, 0));

    auto* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 500), FRotator(-32, -55, 0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(2.3f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1, .88, .71));
    Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->SetAtmosphereSunLight(true);
    auto* Sky = GetWorld()->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(.8f);
    auto* Atmosphere = NewObject<USkyAtmosphereComponent>(this);
    Atmosphere->SetupAttachment(RootComponent);
    Atmosphere->RegisterComponent();
    Sky->GetLightComponent()->RecaptureSky();
    // A distant impression of the bay, not a claim to reproduce an actual CLARA premises.
    Box(TEXT("Mare distante"), FVector(0, 6500, -170), FVector(26000, 11000, 20), FLinearColor(.08, .28, .37), NAME_None, false);
    for (int32 I = 0; I < 19; ++I)
    {
        const float Height = 210 + ((I * 37) % 230);
        Box(TEXT("Edificio urbano distante"), FVector(-3500 + I * 390, 2400 + (I % 3) * 350, Height / 2 - 150),
            FVector(230, 220, Height), FLinearColor(.54 + (I % 4) * .04, .49, .39), NAME_None, false);
    }
}

void AClaraOffice::Desk(FVector P, bool bMainDesk)
{
    Box(TEXT("Piano scrivania"), P + FVector(0, 0, 75), FVector(80, 160, 4), Oak);
    for (const float Y : {-65.f, 65.f})
    {
        Box(TEXT("Gamba scrivania"), P + FVector(25, Y, 37), FVector(5, 5, 74), Aluminium);
        Box(TEXT("Gamba scrivania"), P + FVector(-25, Y, 37), FVector(5, 5, 74), Aluminium);
        Box(TEXT("Piede scrivania"), P + FVector(0, Y, 3), FVector(70, 6, 5), Ink);
    }
    Box(TEXT("Pannello privacy"), P + FVector(32, 0, 52), FVector(2, 135, 35), FLinearColor(.24, .28, .28));
    Box(TEXT("Base monitor"), P + FVector(12, 0, 80), FVector(23, 36, 3), Ink);
    Box(TEXT("Supporto monitor"), P + FVector(25, 0, 91), FVector(4, 5, 22), Aluminium);
    auto* Monitor = Box(TEXT("Cornice monitor"), P + FVector(21, 0, 112), FVector(4, 62, 38), Ink,
        bMainDesk ? FName(TEXT("Clara.Desktop")) : FName(TEXT("Clara.Transfer")));
    if (bMainDesk) Monitor->Tags.Add(TEXT("Clara.Service"));
    auto* Screen = Box(TEXT("Vetro monitor"), P + FVector(18.8, 0, 112), FVector(.2, 58, 34), FLinearColor(.035, .10, .13),
        bMainDesk ? FName(TEXT("Clara.Desktop")) : FName(TEXT("Clara.Transfer")));
    if (bMainDesk) Screen->Tags.Add(TEXT("Clara.Service"));
    Label(bMainDesk ? TEXT("PC DOCENTE") : TEXT("PRENOTAZIONI"), P + FVector(18.4, 0, 124), 3.8, FColor(173, 222, 218));
    if (bMainDesk)
    {
        PCDisplay = NewObject<UClaraDisplay>(this, TEXT("PCScreenDisplay"));
        PCDisplay->SetupAttachment(RootComponent);
        PCDisplay->RegisterComponent();
        PCDisplay->SetWorldLocationAndRotation(FVector(418, 50, 112), FRotator(0, 180, 0));
        PCDisplay->SetWorldScale3D(FVector(.0453, .0453, .0472));
        const float IconY[] = {-18, 0, 18};
        const TCHAR* Names[] = {TEXT("TERMINALE"), TEXT("DISPENSE"), TEXT("GUIDA")};
        for (int32 I = 0; I < 3; ++I)
        {
            Box(TEXT("Icona desktop"), P + FVector(18.4, IconY[I], 113), FVector(.15, 9, 7),
                I == 1 ? FLinearColor(.55, .38, .11) : Teal, TEXT("Clara.Desktop"), false);
            Label(Names[I], P + FVector(18.1, IconY[I], 105), 1.7, FColor(218, 228, 220));
        }
        Label(TEXT("Apri la postazione"), P + FVector(18.4, 0, 98), 2.3, FColor(174, 196, 185));
    }
    else Label(TEXT("192.168.10.30"), P + FVector(18.4, 0, 108), 3.3, FColor(215, 210, 181));
    Box(TEXT("Tastiera"), P + FVector(-16, -7, 79), FVector(17, 46, 2), Ink,
        bMainDesk ? FName(TEXT("Clara.Ticket")) : FName(TEXT("Clara.Transfer")));
    for (int32 Row = 0; Row < 4; ++Row)
        for (int32 Col = 0; Col < 13; ++Col)
            Box(TEXT("Tasto tastiera"), P + FVector(-22 + Row * 3.6, -27 + Col * 3.3, 80.3), FVector(2.6, 2.6, .6), Aluminium, NAME_None, false);
    Box(TEXT("Mousepad"), P + FVector(-15, 42, 77.3), FVector(30, 25, .3), Teal, NAME_None, false);
    Box(TEXT("Mouse"), P + FVector(-13, 42, 80), FVector(10, 6, 4), Ink, NAME_None, false);
    if (bMainDesk)
    {
        const FVector Cabinet = P + FVector(-25, 150, 30);
        Box(TEXT("Mobile laterale PC"), Cabinet, FVector(80, 75, 60), Paper);
        Box(TEXT("Ripiano laterale legno"), Cabinet + FVector(0, 0, 31.5), FVector(84, 79, 3), Oak);
        for (int32 Drawer = 0; Drawer < 3; ++Drawer)
        {
            Box(TEXT("Frontale cassetto laterale"), Cabinet + FVector(-40.7, 0, -19 + Drawer * 19), FVector(1, 67, 17), Aluminium);
            Box(TEXT("Maniglia cassetto laterale"), Cabinet + FVector(-42, 0, -19 + Drawer * 19), FVector(2, 22, 2), Ink, NAME_None, false);
        }
        const FVector Tower = P + FVector(-25, 150, 90);
        Box(TEXT("Torre PC docente - pannello accessibile"), Tower, FVector(44, 22, 54), Ink, TEXT("Clara.InspectPC"));
        Box(TEXT("Pannello I/O PC"), FVector(352.7, 200, 96), FVector(.6, 18, 38), Aluminium, TEXT("Clara.InspectPC"));
        for (int32 Row = 0; Row < 5; ++Row)
            for (int32 Slot = 0; Slot < 8; ++Slot)
                Box(TEXT("Griglia ventilazione PC"), FVector(352.2, 193.5 + Slot * 1.8, 110 + Row * 1.2),
                    FVector(.25, .8, .45), Ink, NAME_None, false);
        for (const float Y : {196.5f, 203.5f})
        {
            Box(TEXT("Cornice USB PC"), FVector(352, Y, 88), FVector(.5, 2.8, 1.2), Ink, TEXT("Clara.InspectPC"));
            Box(TEXT("Linguetta USB PC"), FVector(351.7, Y, 87.9), FVector(.2, 2.1, .35), Blue, NAME_None, false);
        }
        for (int32 Jack = 0; Jack < 3; ++Jack)
        {
            const float Y = 195 + Jack * 4.6;
            Cylinder(TEXT("Cornice jack audio"), FVector(351.9, Y, 81), FVector(.9, .9, .5),
                Jack == 0 ? FLinearColor(.18, .55, .23) : (Jack == 1 ? FLinearColor(.6, .20, .32) : Blue),
                false, FRotator(0, 90, 0));
            Cylinder(TEXT("Foro jack audio"), FVector(351.55, Y, 81), FVector(.5, .5, .1), Ink, false, FRotator(0, 90, 0));
        }
        Box(TEXT("Cornice metallica Ethernet PC"), PCSocket, FVector(.7, 1.7, 1.5), Aluminium, NAME_None, false);
        Box(TEXT("Cavità Ethernet PC"), PCSocket - FVector(.42, 0, .05), FVector(.15, 1.4, 1.15), Ink, NAME_None, false);
        Box(TEXT("Scasso linguetta Ethernet PC"), PCSocket + FVector(-.45, 0, -.75), FVector(.1, .55, .35), Ink, NAME_None, false);
        for (int32 Pin = 0; Pin < 8; ++Pin)
            Box(TEXT("Contatto Ethernet PC"), PCSocket + FVector(-.52, -.46 + Pin * .13, .32),
                FVector(.08, .06, .45), FLinearColor(.68, .52, .16), NAME_None, false);
        PCLinkLED = Box(TEXT("LED link Ethernet PC"), PCSocket + FVector(-.42, -1.2, -.2),
            FVector(.15, .25, .45), FLinearColor(.06, .07, .05), NAME_None, false);
        Box(TEXT("LED attività Ethernet PC"), PCSocket + FVector(-.42, 1.2, -.2),
            FVector(.15, .25, .45), FLinearColor(.07, .065, .035), NAME_None, false);
        auto* SocketTarget = Box(TEXT("Presa Ethernet - area di precisione assistita"), PCSocket,
            FVector(3, 8, 6), Ink, TEXT("Clara.Socket"));
        SocketTarget->GetStaticMeshComponent()->SetVisibility(false);
        SocketTarget->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SocketTarget->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        SocketTarget->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Label(TEXT("ETHERNET / RETE"), PCSocket + FVector(-.65, 0, 6), 3.5, FColor(28, 39, 41));
        Label(TEXT("AULA-01"), FVector(351.9, 200, 73), 2.1, FColor(216, 223, 213));
        auto* Inspection = GetWorld()->SpawnActor<ATargetPoint>(FVector(260, 200, 117),
            (PCSocket - FVector(260, 200, 117)).Rotation());
        Inspection->Tags.Add(TEXT("Clara.PCInspectCamera"));
        Inspection->SetOwner(this);
        Box(TEXT("Ticket docente"), P + FVector(-15, -59, 78), FVector(25, 20, .6), Paper, TEXT("Clara.Ticket"));
        Label(TEXT("TICKET #001"), P + FVector(-15, -59, 78.6), 2.7, FColor(40, 65, 65), FRotator(90, 0, 0));
        Box(TEXT("Taccuino tecnico"), P + FVector(-10, 61, 79), FVector(23, 15, 2), Teal, TEXT("Clara.Guide"));
        Label(TEXT("GUIDA"), P + FVector(-10, 61, 80.3), 3, FColor::White, FRotator(90, 0, 0));
    }
    else
    {
        Box(TEXT("Torre PC prenotazioni"), P + FVector(20, 70, 30), FVector(44, 22, 60), Ink);
        for (int32 I = 0; I < 12; ++I)
            Box(TEXT("Feritoia PC"), P + FVector(-2.1, 70, 13 + I * 1.5), FVector(.2, 15, .6), Aluminium, NAME_None, false);
        Label(TEXT("RECEPTION-02"), P + FVector(-2.4, 70, 56), 2.3, FColor(210, 214, 205));
    }
    const FVector Chair = P + FVector(-95, 0, 0);
    Cylinder(TEXT("Pistone sedia"), Chair + FVector(0, 0, 26), FVector(6, 6, 40), Aluminium);
    Box(TEXT("Seduta imbottita"), Chair + FVector(0, 0, 48), FVector(43, 45, 8), Teal);
    Box(TEXT("Schienale"), Chair + FVector(-20, 0, 75), FVector(6, 43, 50), Teal);
    for (int32 I = 0; I < 5; ++I)
    {
        const float A = I * 2 * PI / 5;
        const FVector R(FMath::Cos(A) * 23, FMath::Sin(A) * 23, 7);
        Cylinder(TEXT("Ruota sedia"), Chair + R, FVector(7, 7, 5), Ink);
        Box(TEXT("Raggio base sedia"), Chair + R * .5, FVector(25, 3, 3), Aluminium, NAME_None, false, FRotator(0, FMath::RadiansToDegrees(A), 0));
    }
}

void AClaraOffice::Rack()
{
    const FVector P(650, 300, 0);
    Box(TEXT("Rack base"), P + FVector(0, 0, 8), FVector(65, 80, 16), Ink);
    Box(TEXT("Rack tetto"), P + FVector(0, 0, 204), FVector(65, 80, 7), Ink);
    Box(TEXT("Rack retro"), P + FVector(0, 39, 106), FVector(64, 2, 190), Ink);
    for (const float X : {-30.f, 30.f})
    {
        Box(TEXT("Montante rack"), P + FVector(X, -37, 105), FVector(3, 3, 190), Aluminium);
        Box(TEXT("Montante rack posteriore"), P + FVector(X, 37, 105), FVector(3, 3, 190), Aluminium);
        for (int32 U = 0; U < 40; ++U)
            Box(TEXT("Asola montante"), P + FVector(X, -38.6, 17 + U * 4.5), FVector(.7, .2, 1.4), Ink, NAME_None, false);
    }
    for (const float Z : {30.f, 46.f, 150.f, 166.f})
    {
        Box(TEXT("Server rack"), P + FVector(0, 0, Z), FVector(54, 65, 12), Aluminium);
        Box(TEXT("Frontale server"), P + FVector(0, -34, Z), FVector(54, 2, 12), Ink);
        for (int32 I = 0; I < 8; ++I)
            Box(TEXT("Cassetto disco"), P + FVector(-22 + I * 6, -35.4, Z), FVector(5.2, .6, 7), Aluminium, NAME_None, false);
        Box(TEXT("LED server"), P + FVector(24, -35.5, Z + 3), FVector(.7, .3, .7), FLinearColor(.1, .8, .2), NAME_None, false);
    }
    Box(TEXT("Ripiano switch"), P + FVector(0, -4, 120), FVector(57, 64, 2), Ink);
    Box(TEXT("Chassis switch 24 porte"), P + FVector(0, -16.5, 124), FVector(44.5, 32.9, 4.4), Aluminium, TEXT("Clara.Switch"));
    Box(TEXT("Frontale switch"), P + FVector(0, -33.1, 124), FVector(44.5, .5, 4.4), FLinearColor(.11, .18, .22), TEXT("Clara.Switch"));
    Label(TEXT("TNES LAB  |  24 x 1G + 4 SFP"), P + FVector(0, -33.5, 128.8), 1.35, FColor(188, 205, 209), FRotator(0, -90, 0));
    for (int32 I = 0; I < 24; ++I)
    {
        const float X = -17.3 + (I / 2) * 2.45;
        const float Z = 123 + (I % 2) * 2.0;
        Box(TEXT("Cornice porta RJ45"), P + FVector(X, -33.5, Z), FVector(1.65, .6, 1.55), Aluminium, TEXT("Clara.Switch"));
        Box(TEXT("Interno porta RJ45"), P + FVector(X, -33.86, Z), FVector(1.35, .15, 1.15), Ink, TEXT("Clara.Switch"));
        for (int32 Pin = 0; Pin < 8; ++Pin)
            Box(TEXT("Contatto RJ45"), P + FVector(X - .48 + Pin * .135, -33.97, Z + .27), FVector(.065, .08, .36), FLinearColor(.68, .52, .16), NAME_None, false);
        Label(FString::FromInt(I + 1), P + FVector(X, -34.02, Z - 1), .55, FColor(220, 222, 213), FRotator(0, -90, 0));
    }
    for (int32 I = 0; I < 4; ++I)
    {
        const float X = 13.3 + I * 2.0;
        Box(TEXT("Gabbia SFP"), P + FVector(X, -33.5, 124), FVector(1.6, .6, 2.1), Aluminium, TEXT("Clara.Switch"));
        Box(TEXT("SFP vuoto"), P + FVector(X, -33.9, 124), FVector(1.3, .2, 1.7), Ink, TEXT("Clara.Switch"));
    }
    for (const float X : {-21.f, 21.f})
        for (const float Z : {122.5f, 125.5f})
            Cylinder(TEXT("Vite frontale"), P + FVector(X, -33.7, Z), FVector(.45, .45, .2), Aluminium, false, FRotator(90, 0, 0));
    const FVector SwitchConsole = P + FVector(-20.3, -33.7, 124);
    Box(TEXT("Cornice blu porta console"), SwitchConsole, FVector(1.55, .4, 1.4), Blue, NAME_None, false);
    Box(TEXT("Cavità porta console RJ45"), SwitchConsole + FVector(0, -.27, 0), FVector(1.2, .15, 1), Ink, NAME_None, false);
    Box(TEXT("Scasso linguetta console"), SwitchConsole + FVector(0, -.36, -.6), FVector(.5, .1, .3), Ink, NAME_None, false);
    for (int32 Pin = 0; Pin < 8; ++Pin)
        Box(TEXT("Contatto porta console"), SwitchConsole + FVector(-.43 + Pin * .123, -.37, .23),
            FVector(.055, .08, .32), FLinearColor(.68, .52, .16), NAME_None, false);
    auto* SwitchConsoleTarget = Box(TEXT("Porta console - precisione assistita"), SwitchConsole,
        FVector(3.2, 5, 4), Ink, TEXT("Clara.ConsolePort"));
    SwitchConsoleTarget->GetStaticMeshComponent()->SetVisibility(false);
    SwitchConsoleTarget->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SwitchConsoleTarget->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    SwitchConsoleTarget->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box(TEXT("Targhetta console"), P + FVector(-20.3, -34, 116.3), FVector(12, .4, 4.5), Ink, NAME_None, false);
    Label(TEXT("CONSOLE"), P + FVector(-20.3, -34.3, 116.3), 2, FColor(181, 213, 224), FRotator(0, -90, 0));
    LinkLED = Box(TEXT("LED collegamento locale"), P + FVector(-18.9, -34.05, 125), FVector(.35, .2, .35), FLinearColor(.08, .09, .07), NAME_None, false);
    Box(TEXT("Connettore Ethernet collegato allo switch"), FixedCableEnd, FVector(1.2, 3.4, 1.1), Blue, NAME_None, false);
    Label(TEXT("SW-LAB-01"), P + FVector(0, -41, 195), 5, FColor(219, 225, 215), FRotator(0, -90, 0));
    Label(TEXT("LAB LOCALE / CML NON COLLEGATO"), P + FVector(0, -41, 183), 2.1, FColor(245, 193, 99), FRotator(0, -90, 0));
    if (!LucaMesh)
    {
        Box(TEXT("Pannello Luca provvisorio"), FVector(583, 272, 140), FVector(4, 30, 42), Teal, TEXT("Clara.Mentor"));
        Label(TEXT("LUCA"), FVector(580.8, 272, 149), 5, FColor::White);
        Label(TEXT("TUTOR RETI"), FVector(580.8, 272, 139), 2.7, FColor(195, 219, 213));
        Label(TEXT("CONTATTA"), FVector(580.8, 272, 128), 2.5, FColor(229, 216, 162));
    }
    ConsoleCoil = Cylinder(TEXT("Cavo console arrotolato"), FVector(585, 105, 79), FVector(1), FLinearColor(.22, .54, .67));
    ConsoleCoil->GetStaticMeshComponent()->SetStaticMesh(nullptr);
    ConsoleCoil->SetActorScale3D(FVector::OneVector);
    ConsoleCoil->Tags.Add(TEXT("Clara.ConsoleCable"));
    for (int32 Layer = 0; Layer < 2; ++Layer)
    {
        for (int32 Segment = 0; Segment < 24; ++Segment)
        {
            const float A = Segment * 2 * PI / 24;
            auto* Sheath = NewObject<UStaticMeshComponent>(ConsoleCoil);
            Sheath->SetupAttachment(ConsoleCoil->GetRootComponent());
            Sheath->SetStaticMesh(CylinderMesh);
            Sheath->SetMaterial(0, Material(FLinearColor(.22, .54, .67)));
            Sheath->SetRelativeLocation(FVector(FMath::Cos(A) * 6, FMath::Sin(A) * 6, Layer * .65));
            Sheath->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector(-FMath::Sin(A), FMath::Cos(A), 0)).Rotator());
            Sheath->SetRelativeScale3D(FVector(.0065, .0065, .0165));
            Sheath->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Sheath->SetCollisionResponseToAllChannels(ECR_Ignore);
            Sheath->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            Sheath->RegisterComponent();
        }
    }
    ConsoleConnector = Box(TEXT("Connettore RJ45 console"), FVector(576, 105, 79), FVector(3, 1.3, 1.2), FLinearColor(.22, .54, .67), TEXT("Clara.ConsoleCable"));
    ConsoleConnectorClip = Box(TEXT("Clip RJ45 console"), FVector(576, 105, 79.8), FVector(1.5, .6, .3), Aluminium, NAME_None, false);
    ConsoleConnectorClip->AttachToActor(ConsoleConnector, FAttachmentTransformRules::KeepWorldTransform);
    Label(TEXT("CONSOLE / GESTIONE"), FVector(581, 91, 79), 2.1, FColor(220, 231, 224), FRotator(90, 0, 0));
    // A separate laptop keeps device-console access distinct from the learner's Linux PC.
    Box(TEXT("Base laptop configurazione"), FVector(610, 112, 79.2), FVector(30, 23, 1.3), Aluminium, TEXT("Clara.SwitchTerminal"));
    const FVector LaptopUSB(594.7, 110, 79.2);
    Box(TEXT("Porta USB console laptop"), LaptopUSB, FVector(.4, 3, 1), Ink, NAME_None, false);
    Box(TEXT("Linguetta porta USB console"), LaptopUSB + FVector(-.23, 0, -.1), FVector(.08, 2.3, .3), Blue, NAME_None, false);
    auto* LaptopUSBTarget = Box(TEXT("Porta USB laptop - precisione assistita"), LaptopUSB,
        FVector(5, 5, 4), Ink, TEXT("Clara.ConsoleLaptopPort"));
    LaptopUSBTarget->GetStaticMeshComponent()->SetVisibility(false);
    LaptopUSBTarget->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    LaptopUSBTarget->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    LaptopUSBTarget->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Label(TEXT("USB"), FVector(593.9, 110, 81.8), 1.5, FColor(216, 226, 219));
    Box(TEXT("Tastiera laptop"), FVector(608, 112, 80), FVector(20, 18, .4), Ink, TEXT("Clara.SwitchTerminal"));
    for (int32 Row = 0; Row < 4; ++Row)
        for (int32 Key = 0; Key < 10; ++Key)
            Box(TEXT("Tasto laptop"), FVector(602 + Row * 3.4, 104 + Key * 1.7, 80.4), FVector(2.6, 1.3, .25), Aluminium, NAME_None, false);
    Box(TEXT("Schermo laptop"), FVector(626, 112, 91), FVector(1.8, 23, 22), Ink, TEXT("Clara.SwitchTerminal"), true, FRotator(0, 0, -6));
    Box(TEXT("Display laptop console"), FVector(624.9, 112, 91), FVector(.2, 21, 19), FLinearColor(.025, .06, .065), TEXT("Clara.SwitchTerminal"));
    SwitchDisplay = NewObject<UClaraDisplay>(this, TEXT("SwitchConsoleScreenDisplay"));
    SwitchDisplay->SetupAttachment(RootComponent);
    SwitchDisplay->SetDrawSize(FVector2D(800, 600));
    SwitchDisplay->RegisterComponent();
    SwitchDisplay->SetWorldLocationAndRotation(FVector(624.4, 112, 91), FRotator(0, 180, 0));
    SwitchDisplay->SetWorldScale3D(FVector(1, .02625, .03166));
    Label(TEXT("CONSOLE"), FVector(624.6, 112, 95), 2.5, FColor(184, 220, 205));
    Label(TEXT("SW-LAB-01"), FVector(624.6, 112, 88), 1.7, FColor(172, 192, 182));
    for (int32 I = 0; I < 8; ++I)
    {
        auto* M = Box(TEXT("Network Vision"), FVector(413 + I * 32, 160 + I * 12, 65), FVector(4, 4, 4), FLinearColor(.02, .8, .7), NAME_None, false);
        VisionMarkers.Add(M);
    }
}

void AClaraOffice::Details()
{
    // Everyday objects make the office inhabited without substituting primitives for people.
    Box(TEXT("Libreria"), FVector(965, -380, 110), FVector(60, 290, 220), Oak);
    for (int32 Shelf = 0; Shelf < 4; ++Shelf)
    {
        const float Z = 30 + Shelf * 52;
        Box(TEXT("Ripiano libreria"), FVector(934, -380, Z), FVector(62, 290, 3), Paper);
        for (int32 Book = 0; Book < 12; ++Book)
        {
            const float H = 24 + (Book * 7 % 12);
            Box(TEXT("Raccoglitore"), FVector(922, -500 + Book * 20, Z + H / 2 + 2), FVector(30, 14, H),
                Book % 3 == 0 ? Teal : (Book % 3 == 1 ? Paper : Ink), NAME_None, false);
        }
    }
    Box(TEXT("Mobile caffè"), FVector(-620, -480, 45), FVector(160, 65, 90), Paper);
    Box(TEXT("Top mobile caffè"), FVector(-620, -480, 92), FVector(166, 72, 4), Oak);
    Box(TEXT("Macchina espresso"), FVector(-640, -480, 110), FVector(37, 34, 32), Aluminium);
    Box(TEXT("Frontale espresso"), FVector(-640, -499, 108), FVector(30, 2, 24), Ink);
    Box(TEXT("Vaschetta raccogligocce"), FVector(-640, -506, 96), FVector(32, 17, 2.4), Aluminium);
    for (int32 I = 0; I < 10; ++I)
        Box(TEXT("Fessura vaschetta espresso"), FVector(-653 + I * 2.8, -506, 97.3), FVector(1.1, 13, .2), Ink, NAME_None, false);
    Cylinder(TEXT("Gruppo erogazione"), FVector(-640, -501, 110), FVector(9, 9, 5), Aluminium, false, FRotator(90, 0, 0));
    Cylinder(TEXT("Portafiltro"), FVector(-640, -506, 108), FVector(3, 3, 12), Ink, false, FRotator(90, 0, 0));
    Cylinder(TEXT("Manometro"), FVector(-648, -501, 119), FVector(5.5, 5.5, .7), Paper, false, FRotator(90, 0, 0));
    Cylinder(TEXT("Manopola vapore"), FVector(-624, -500, 116), FVector(4, 4, 3), Ink, false, FRotator(90, 0, 0));
    Cylinder(TEXT("Lancia vapore"), FVector(-622, -504, 106), FVector(.8, .8, 17), Aluminium);
    Box(TEXT("Pulsante espresso singolo"), FVector(-637, -501.4, 120), FVector(3.5, 1, 2.2), Paper, NAME_None, false);
    Box(TEXT("Pulsante espresso doppio"), FVector(-631, -501.4, 120), FVector(3.5, 1, 2.2), Paper, NAME_None, false);
    Cylinder(TEXT("Tazzina"), FVector(-580, -480, 100), FVector(7, 7, 8), Paper);
    Cylinder(TEXT("Piattino"), FVector(-580, -480, 95), FVector(12, 12, 1), Paper);
    Cylinder(TEXT("Caffè nella tazzina"), FVector(-580, -480, 104.05), FVector(5.7, 5.7, .15), FLinearColor(.08, .035, .012));
    Box(TEXT("Manico tazzina"), FVector(-575.7, -480, 100), FVector(3.1, 1.6, 4), Paper, NAME_None, false);
    Box(TEXT("Portazucchero"), FVector(-596, -463, 99), FVector(13, 10, 8), Teal, NAME_None, false);
    Cylinder(TEXT("Tavolino attesa"), FVector(-360, -425, 48), FVector(78, 78, 4), Oak);
    Cylinder(TEXT("Piantana tavolino"), FVector(-360, -425, 24), FVector(7, 7, 44), Aluminium);
    Cylinder(TEXT("Base tavolino"), FVector(-360, -425, 3), FVector(45, 45, 4), Ink);
    Box(TEXT("Rivista formazione"), FVector(-354, -425, 51), FVector(28, 21, 1), Paper, NAME_None, false, FRotator(0, 12, 0));
    Box(TEXT("Seduta lounge"), FVector(-515, -300, 42), FVector(135, 62, 16), Teal);
    Box(TEXT("Schienale lounge"), FVector(-515, -273, 71), FVector(135, 12, 62), Teal);
    for (const float X : {-588.f, -442.f})
        Box(TEXT("Bracciolo lounge"), FVector(X, -302, 59), FVector(13, 65, 34), Teal);
    for (const float X : {-563.f, -467.f})
        for (const float Y : {-322.f, -284.f})
            Cylinder(TEXT("Piedino lounge"), FVector(X, Y, 16), FVector(4, 4, 30), Aluminium);
    Box(TEXT("Bacheca interventi"), FVector(200, -606, 195), FVector(160, 3, 110), Oak, NAME_None, false);
    for (int32 I = 0; I < 6; ++I)
        Box(TEXT("Avviso bacheca"), FVector(155 + (I % 3) * 42, -603.8, 175 + (I / 3) * 43), FVector(33, .3, 34), I % 2 ? Paper : FLinearColor(.77, .67, .4), NAME_None, false);
    Label(TEXT("INTERVENTI / FORMAZIONE"), FVector(200, -603, 244), 7, FColor(33, 53, 51), FRotator(0, 90, 0));
    Label(TEXT("NAPOLI   |   GIORNO 01"), FVector(-620, -603, 185), 11, FColor(44, 76, 76), FRotator(0, 90, 0));
    Box(TEXT("Canalina a muro"), FVector(100, -600, 29), FVector(1850, 7, 9), Paper, NAME_None, false);
    for (int32 I = 0; I < 6; ++I)
    {
        Box(TEXT("Presa a muro"), FVector(-650 + I * 300, -595, 29), FVector(16, 3, 11), Aluminium, NAME_None, false);
        Cylinder(TEXT("Vaso"), FVector(-660 + I * 270, 540, 26), FVector(28, 28, 52), Teal, false);
        for (int32 Stem = 0; Stem < 4; ++Stem)
        {
            const FVector F(-660 + I * 270 + Stem * 3, 540 + Stem * 4, 55 + Stem * 8);
            Cylinder(TEXT("Stelo pianta"), F, FVector(1, 1, 55), FLinearColor(.13, .20, .075));
            Box(TEXT("Foglia"), F + FVector(7, 0, 17), FVector(19, 7, .7), FLinearColor(.12, .23, .08), NAME_None, false, FRotator(25, Stem * 42, 0));
        }
    }
    Box(TEXT("Tavolo strumenti"), FVector(600, 100, 75), FVector(90, 80, 4), Oak);
    Box(TEXT("Cassettiera strumenti"), FVector(600, 100, 37), FVector(70, 65, 74), Aluminium);
    for (int32 I = 0; I < 4; ++I)
    {
        Box(TEXT("Fronte cassetto"), FVector(563, 100, 15 + I * 17), FVector(2, 61, 14), Ink);
        Box(TEXT("Maniglia cassetto"), FVector(560, 100, 15 + I * 17), FVector(3, 20, 2), Aluminium, NAME_None, false);
    }
}

void AClaraOffice::People()
{
    struct FColleague
    {
        const TCHAR* Name;
        const TCHAR* SpawnTag;
        const TCHAR* Interaction;
        UStaticMesh* Mesh;
        FVector Position;
    };
    const FColleague Colleagues[] = {
        {TEXT("Alessia"), TEXT("Clara.AlessiaSpawn"), TEXT("Clara.Reception"), AlessiaMesh, FVector(-290, 180, 0)},
        {TEXT("Luca"), TEXT("Clara.LucaSpawn"), TEXT("Clara.Mentor"), LucaMesh, FVector(570, 350, 0)},
        {TEXT("Jonny"), TEXT("Clara.JonnySpawn"), TEXT("Clara.CyberMentor"), JonnyMesh, FVector(610, -340, 0)}
    };
    for (const FColleague& Colleague : Colleagues)
    {
        auto* Marker = GetWorld()->SpawnActor<ATargetPoint>(Colleague.Position, FRotator(0, 180, 0));
        Marker->Tags.Add(Colleague.SpawnTag);
        Marker->SetOwner(this);
        if (!Colleague.Mesh) continue;
        const FBox Bounds = Colleague.Mesh->GetBoundingBox();
        const float Height = Bounds.GetSize().Z;
        if (!ensure(Height > .1f)) continue;
        const float Scale = 180.f / Height;
        FVector Origin = Colleague.Position;
        const FVector Center = Bounds.GetCenter();
        Origin += FRotator(0, 180, 0).RotateVector(FVector(-Center.X * Scale, -Center.Y * Scale, -Bounds.Min.Z * Scale));
        auto* Person = GetWorld()->SpawnActor<AStaticMeshActor>(Origin, FRotator(0, 180, 0));
        Person->SetOwner(this);
        Person->Tags.Add(Colleague.Interaction);
        Person->Tags.Add(TEXT("Clara.ScannedPrototype"));
#if WITH_EDITOR
        Person->SetActorLabel(FString(Colleague.Name) + TEXT(" - riferimento scansionato"));
#endif
        Person->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Person->GetStaticMeshComponent()->SetStaticMesh(Colleague.Mesh);
        Person->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Person->SetActorScale3D(FVector(Scale));
        auto* HitTarget = Box(FString(Colleague.Name) + TEXT(" interazione"), Colleague.Position + FVector(0, 0, 106),
            FVector(45, 50, 126), Paper, Colleague.Interaction);
        HitTarget->GetStaticMeshComponent()->SetVisibility(false);
        HitTarget->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        HitTarget->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        HitTarget->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        HitTarget->GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    }
}

void AClaraOffice::SetCableConnected(bool bConnected)
{
    if (bCableConnected == bConnected) return;
    bCableConnected = bConnected;
    if (bConnected) bCableHeld = false;
    if (LinkLED) LinkLED->GetStaticMeshComponent()->SetMaterial(0,
        Material(bConnected ? FLinearColor(.12, .9, .17) : FLinearColor(.08, .09, .07)));
    if (PCLinkLED) PCLinkLED->GetStaticMeshComponent()->SetMaterial(0,
        Material(bConnected ? FLinearColor(.12, .9, .17) : FLinearColor(.06, .07, .05)));
    UpdateCable();
}

void AClaraOffice::SetCableHeld(bool bHeld)
{
    if (bCableHeld == bHeld) return;
    bCableHeld = bHeld;
    if (bHeld) SetCableConnected(false);
    UpdateCable();
}

void AClaraOffice::SetVision(bool bEnabled)
{
    bVision = bEnabled;
    for (const auto& Marker : VisionMarkers) if (Marker) Marker->SetActorHiddenInGame(!bEnabled);
}

void AClaraOffice::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (const auto* Player = Cast<AClaraPlayer>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        SetCableConnected(Player->IsCableConnected());
        SetCableHeld(Player->IsHoldingCable() || Player->IsCableStored());
        if (bVision != Player->IsVisionEnabled()) SetVision(Player->IsVisionEnabled());
        UpdateConsoleCable(Player->IsConsoleConnected(), Player->IsHoldingConsole(), Player->HasConsoleCable());
    }
    if (bCableHeld) UpdateCable();
}

void AClaraOffice::UpdateConsoleCable(bool bConnected, bool bHeld, bool bCarried)
{
    for (const auto& Segment : ConsoleSegments) if (Segment) Segment->SetVisibility(bConnected);
    if (ConsoleSwitchPlug) ConsoleSwitchPlug->SetActorHiddenInGame(!bConnected);
    if (ConsoleLaptopPlug) ConsoleLaptopPlug->SetActorHiddenInGame(!bConnected);
    const bool bCoilAttached = ConsoleCoil && ConsoleCoil->GetAttachParentActor() != nullptr;
    const bool bConnectorAttached = ConsoleConnector && ConsoleConnector->GetAttachParentActor() != nullptr;
    if (ConsoleCoil)
    {
        ConsoleCoil->SetActorHiddenInGame(bConnected || (bCarried && (!bHeld || !bCoilAttached)));
        ConsoleCoil->SetActorEnableCollision(!bCarried && !bConnected);
    }
    if (ConsoleConnector)
    {
        const bool bHidden = bConnected || (bCarried && (!bHeld || bCoilAttached));
        ConsoleConnector->SetActorHiddenInGame(bHidden);
        ConsoleConnector->SetActorEnableCollision(!bCarried && !bConnected);
        if (ConsoleConnectorClip) ConsoleConnectorClip->SetActorHiddenInGame(bHidden);
        if (bHeld && !bConnected && !bCoilAttached && !bConnectorAttached)
        {
            if (const auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
                ConsoleConnector->SetActorLocation(Camera->GetCameraLocation() + Camera->GetCameraRotation().Vector() * 55 + FVector(0, 12, -22));
        }
        else if (!bCarried && !bConnected && !bConnectorAttached)
        {
            ConsoleConnector->SetActorLocation(FVector(576, 105, 79));
        }
    }
}

void AClaraOffice::UpdateCable()
{
    if (!CubeMesh || CableSegments.IsEmpty()) return;
    FVector End = FreeCableRest;
    if (bCableConnected) End = PCSocket - FVector(2.4, 0, 0);
    else if (bCableHeld)
    {
        if (FreeConnector) End = FreeConnector->GetActorLocation();
    }
    TArray<FVector> Points;
    Points.Add(FixedCableEnd);
    Points.Add(FVector(650, 240, 60));
    Points.Add(FVector(610, 215, 8));
    Points.Add(FVector(485, 240, 8));
    Points.Add(FVector(424, 245, 15));
    Points.Add(FVector(418, 240, 58));
    Points.Add(FVector(378, 238, 64));
    if (!bCableHeld && !bCableConnected) Points.Add(FVector(358, 181, 64));
    Points.Add(End);
    CableSpline->SetSplinePoints(Points, ESplineCoordinateSpace::World, true);
    const float Length = CableSpline->GetSplineLength();
    for (int32 I = 0; I < CableSegments.Num(); ++I)
    {
        const float A = Length * I / CableSegments.Num();
        const float B = Length * (I + 1) / CableSegments.Num();
        FVector Start = CableSpline->GetLocationAtDistanceAlongSpline(A, ESplineCoordinateSpace::Local);
        FVector Finish = CableSpline->GetLocationAtDistanceAlongSpline(B, ESplineCoordinateSpace::Local);
        FVector T0 = CableSpline->GetDirectionAtDistanceAlongSpline(A, ESplineCoordinateSpace::Local) * (B - A);
        FVector T1 = CableSpline->GetDirectionAtDistanceAlongSpline(B, ESplineCoordinateSpace::Local) * (B - A);
        CableSegments[I]->SetStartAndEnd(Start, T0, Finish, T1);
    }
    if (FreeConnector && !bCableHeld)
    {
        FreeConnector->SetActorLocation(End);
        FreeConnector->SetActorRotation(bCableConnected ? FRotator(0, 0, 0) : FRotator(0, 30, 0));
    }
    for (const auto& Coil : CableRestCoilSegments)
        if (Coil) Coil->SetActorHiddenInGame(bCableHeld || bCableConnected);
}
