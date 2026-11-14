#include "ClaraNetworkDevice.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FVector ReadVector(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,FVector Default)
{
    const TArray<TSharedPtr<FJsonValue>>* Values;
    if(!Object->TryGetArrayField(Key,Values)||Values->Num()!=3)return Default;
    double X,Y,Z;
    if(!(*Values)[0]->TryGetNumber(X)||!(*Values)[1]->TryGetNumber(Y)||!(*Values)[2]->TryGetNumber(Z))return Default;
    return FVector(X,Y,Z);
}
FLinearColor CableColor(const FString& Kind)
{
    if(Kind==TEXT("console"))return FLinearColor(.12,.48,.82);
    if(Kind==TEXT("fiber"))return FLinearColor(.95,.70,.03);
    return FLinearColor(.88,.88,.82);
}
UMaterialInstanceDynamic* WireMaterial(UObject* Outer,const FString& Kind=TEXT("ethernet"))
{
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_Office.M_Office"));
    auto* Material=UMaterialInstanceDynamic::Create(Base,Outer);
    if(Material)Material->SetVectorParameterValue(TEXT("Color"),CableColor(Kind));
    return Material;
}
}
AClaraNetworkDevice::AClaraNetworkDevice()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("DeviceRoot"));
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CiscoChassis"));
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetupAttachment(RootComponent);
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Body->ComponentTags.Add(TEXT("Inspectable"));
}
void AClaraNetworkDevice::Initialize(const FString& Id,bool bPreview)
{
    ModelId=Id; bIsPreview=bPreview;
    FString Data; TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* Models=nullptr; const TSharedPtr<FJsonObject>* Model=nullptr;
    if(!FFileHelper::LoadFileToString(Data,*(FPaths::ProjectContentDir()/TEXT("Data/network-devices.json")))
        ||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Root)||!Root.IsValid()
        ||!Root->TryGetObjectField(TEXT("models"),Models)||!(*Models)->TryGetObjectField(Id,Model))
    {UE_LOG(LogTemp,Error,TEXT("Cisco device metadata missing for %s"),*Id);return;}
    (*Model)->TryGetStringField(TEXT("displayName"),DisplayName);
    DeviceSize=ReadVector(*Model,TEXT("size"),Id==TEXT("Cisco1941")?FVector(34.3,29.2,8.9):FVector(44.5,27.94,4.45));
    FString MeshPath;
    if(!(*Model)->TryGetStringField(TEXT("mesh"),MeshPath))MeshPath=TEXT("/Game/NetworkAssets/SM_")+Id+TEXT(".SM_")+Id;
    if(!MeshPath.Contains(TEXT(".")))MeshPath+=TEXT(".")+FPaths::GetCleanFilename(MeshPath);
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,*MeshPath);
    Body->SetStaticMesh(Mesh);
    if(Mesh)
    {
        const FBox Bounds=Mesh->GetBoundingBox();const FVector Extent=Bounds.GetSize();
        if(Extent.X>0&&Extent.Y>0&&Extent.Z>0)
        {
            const FVector S=DeviceSize/Extent;
            Body->SetRelativeScale3D(S);
            Body->SetRelativeLocation(FVector(-Bounds.GetCenter().X*S.X,-Bounds.GetCenter().Y*S.Y,-Bounds.Min.Z*S.Z));
        }
    }
    else UE_LOG(LogTemp,Error,TEXT("Cisco device mesh could not be loaded for %s"),*Id);
    const TArray<TSharedPtr<FJsonValue>>* JsonPorts;
    if((*Model)->TryGetArrayField(TEXT("ports"),JsonPorts))
    {
        for(const auto& Value:*JsonPorts)
        {
            const TSharedPtr<FJsonObject> Port=Value->AsObject();if(!Port.IsValid())continue;
            FClaraDevicePort P;
            if(!Port->TryGetStringField(TEXT("id"),P.Id)||P.Id.IsEmpty())continue;
            Port->TryGetStringField(TEXT("label"),P.Label);Port->TryGetStringField(TEXT("kind"),P.Kind);
            P.Position=ReadVector(Port,TEXT("position"),FVector::ZeroVector);
            P.Normal=ReadVector(Port,TEXT("normal"),FVector(0,-1,0)).GetSafeNormal();
            if(P.Normal.IsNearlyZero())P.Normal=FVector(0,-1,0);
            if(P.Label.IsEmpty())P.Label=P.Id;
            if(!FindPort(P.Id))Ports.Add(P);
        }
    }
    if(ModelId==TEXT("Laptop"))LaptopOpenSize=DeviceSize;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    for(const auto& P:Ports)
    {
        float Neighbor=4.f;
        for(const auto& Other:Ports)
            if(Other.Id!=P.Id&&FVector::DotProduct(P.Normal,Other.Normal)>.8f)Neighbor=FMath::Min(Neighbor,static_cast<float>(FVector::Dist(P.Position,Other.Position)));
        // Small enough to leave a gap between adjacent targets, even on a dense RJ45 bank.
        const float Half=FMath::Clamp(Neighbor*.42f,.16f,.70f);
        auto* Hit=NewObject<UBoxComponent>(this);
        AddInstanceComponent(Hit);Hit->SetupAttachment(RootComponent);
        Hit->SetRelativeLocation(P.Position+P.Normal*.6f);
        Hit->SetRelativeRotation(FRotationMatrix::MakeFromX(P.Normal).Rotator());
        Hit->SetBoxExtent(FVector(.85f,Half,Half));Hit->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Hit->SetCollisionResponseToAllChannels(ECR_Ignore);Hit->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Hit->RegisterComponent();PortHits.Add(Hit);
        auto* Plug=NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Plug);Plug->SetMobility(EComponentMobility::Movable);Plug->SetupAttachment(RootComponent);Plug->SetStaticMesh(Cube);
        auto* Connector=(P.Kind==TEXT("ethernet")||P.Kind==TEXT("console"))?LoadObject<UStaticMesh>(nullptr,TEXT("/Game/NetworkAssets/SM_RJ45Plug.SM_RJ45Plug")):nullptr;
        if(Connector)Plug->SetStaticMesh(Connector);else Plug->SetMaterial(0,WireMaterial(this));
        Plug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Plug->SetRelativeLocation(P.Position+P.Normal*.9f);Plug->SetRelativeRotation(FRotationMatrix::MakeFromX(P.Normal).Rotator());
        if(Connector)
        {
            const auto Bounds=Connector->GetBoundingBox();const FVector Scale=FVector(3.6,1.16,1.1)/Bounds.GetSize();
            Plug->SetRelativeScale3D(Scale);
            Plug->SetRelativeLocation(P.Position+P.Normal*.9f-Plug->GetRelativeRotation().RotateVector(Bounds.GetCenter()*Scale));
        }
        else Plug->SetRelativeScale3D(FVector(.028,.011,.0085));Plug->SetVisibility(false);Plug->RegisterComponent();Plugs.Add(Plug);
    }
    if(bPreview)
    {
        for(const FVector Offset:{FVector(-90,-100,150),FVector(80,90,100),FVector(0,0,200)})
        {
            auto* Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(Light);Light->SetupAttachment(RootComponent);
            Light->SetRelativeLocation(Offset);Light->SetIntensity(2400);Light->SetAttenuationRadius(700);Light->SetCastShadows(false);Light->RegisterComponent();
        }
    }
}
void AClaraNetworkDevice::SetPreviewSource(AClaraNetworkDevice* Source)
{PreviewSource=Source?Source->GetOriginalDevice():nullptr;if(ModelId==TEXT("Laptop")&&Source)SetLaptopCarried(Source->IsLaptopCarried());}
AClaraNetworkDevice* AClaraNetworkDevice::GetOriginalDevice() const{return PreviewSource.IsValid()?PreviewSource.Get():const_cast<AClaraNetworkDevice*>(this);}
const FClaraDevicePort* AClaraNetworkDevice::FindPort(const FString& Id)const{return Ports.FindByPredicate([&](const auto& P){return P.Id==Id;});}
FString AClaraNetworkDevice::PortIdForComponent(const UPrimitiveComponent* Component)const
{for(int32 I=0;I<PortHits.Num();++I)if(PortHits[I]==Component&&Ports.IsValidIndex(I))return Ports[I].Id;return FString();}
FVector AClaraNetworkDevice::GetPortWorldLocation(const FString& Id)const{const auto* P=FindPort(Id);return P?GetActorTransform().TransformPosition(P->Position):GetActorLocation();}
FVector AClaraNetworkDevice::GetPortWorldNormal(const FString& Id)const{const auto* P=FindPort(Id);return P?GetActorTransform().TransformVectorNoScale(P->Normal).GetSafeNormal():GetActorForwardVector();}
bool AClaraNetworkDevice::IsPortOccupied(const FString& Id)const
{const auto* Source=GetOriginalDevice();const auto* Link=Source->Connections.Find(Id);return Link&&Link->Peer.IsValid();}
FString AClaraNetworkDevice::GetPortStatus(const FString& Id)const
{
    const auto* P=FindPort(Id);if(!P)return TEXT("Seleziona una porta");
    const auto* Link=GetOriginalDevice()->Connections.Find(Id);
    return P->Label+TEXT(" · ")+GetEffectivePortKind(Id)+(HasOpticalModule(Id)?TEXT(" (SFP inserito)"):TEXT(""))+(Link&&Link->Peer.IsValid()?TEXT(" · collegata a ")+Link->Peer->GetDisplayName()+TEXT(" / ")+Link->PeerPort:TEXT(" · libera"));
}
AClaraNetworkDevice* AClaraNetworkDevice::GetPeerForPort(const FString& Id,FString& PeerPort)const
{
    PeerPort.Empty();const auto* Link=GetOriginalDevice()->Connections.Find(Id);
    if(!Link||!Link->Peer.IsValid())return nullptr;PeerPort=Link->PeerPort;return Link->Peer.Get();
}
FString AClaraNetworkDevice::GetEffectivePortKind(const FString& Id)const
{
    const auto* Source=GetOriginalDevice();const auto* Port=Source->FindPort(Id);
    if(!Port)return FString();return Port->Kind==TEXT("sfp")&&Source->OpticalModules.Contains(Id)?TEXT("fiber"):Port->Kind;
}
bool AClaraNetworkDevice::InstallOpticalModule(const FString& Id)
{
    auto* Source=GetOriginalDevice();const auto* Port=Source->FindPort(Id);
    if(!Port||Port->Kind!=TEXT("sfp")||Source->IsPortOccupied(Id))return false;
    Source->OpticalModules.Add(Id);return true;
}
bool AClaraNetworkDevice::HasOpticalModule(const FString& Id)const{return GetOriginalDevice()->OpticalModules.Contains(Id);}
void AClaraNetworkDevice::SetLaptopCarried(bool Carried)
{
    if(ModelId!=TEXT("Laptop"))return;
    bLaptopCarried=Carried;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,Carried?TEXT("/Game/NetworkAssets/SM_LaptopClosed.SM_LaptopClosed"):TEXT("/Game/NetworkAssets/SM_Laptop.SM_Laptop"));
    if(!Mesh)return;
    if(LaptopOpenSize.IsNearlyZero())LaptopOpenSize=DeviceSize;
    DeviceSize=Carried?FVector(33,24,2):LaptopOpenSize;
    Body->SetStaticMesh(Mesh);const FBox Bounds=Mesh->GetBoundingBox();const FVector Size=Bounds.GetSize();
    if(Size.X>0&&Size.Y>0&&Size.Z>0)
    {
        const FVector S=DeviceSize/Size;Body->SetRelativeScale3D(S);
        Body->SetRelativeLocation(FVector(-Bounds.GetCenter().X*S.X,-Bounds.GetCenter().Y*S.Y,-Bounds.Min.Z*S.Z));
    }
    Body->SetCollisionEnabled(Carried?ECollisionEnabled::QueryOnly:ECollisionEnabled::QueryAndPhysics);
    Body->SetCollisionResponseToChannel(ECC_Visibility,Carried?ECR_Ignore:ECR_Block);
    TArray<UBoxComponent*> PortBoxes;GetComponents(PortBoxes);
    for(auto* Box:PortBoxes)Box->SetCollisionResponseToChannel(ECC_Visibility,Carried?ECR_Ignore:ECR_Block);
}
bool AClaraNetworkDevice::IsCableCompatible(const FString& CableKind,const FString& KindA,const FString& KindB)
{
    if(CableKind==TEXT("ethernet"))return KindA==TEXT("ethernet")&&KindB==TEXT("ethernet");
    if(CableKind==TEXT("console"))return (KindA==TEXT("console")&&KindB==TEXT("console_host"))||(KindB==TEXT("console")&&KindA==TEXT("console_host"));
    if(CableKind==TEXT("fiber"))return KindA==TEXT("fiber")&&KindB==TEXT("fiber");
    return false;
}
bool AClaraNetworkDevice::ConnectPorts(AClaraNetworkDevice* A,const FString& PortA,AClaraNetworkDevice* B,const FString& PortB,const FString& CableKind,FString& Error)
{
    Error.Empty();if(!A||!B){Error=TEXT("Manca un'estremita'.");return false;}A=A->GetOriginalDevice();B=B->GetOriginalDevice();
    const auto* PA=A->FindPort(PortA);const auto* PB=B->FindPort(PortB);
    if(!PA||!PB){Error=TEXT("La porta richiesta non esiste.");return false;}
    if(A==B){Error=TEXT("Per questa prova scegli un altro apparato come seconda estremita'.");return false;}
    if(!IsCableCompatible(CableKind,A->GetEffectivePortKind(PortA),B->GetEffectivePortKind(PortB)))
    {
        if(CableKind==TEXT("console"))Error=TEXT("Il cavo console collega la USB console dedicata del portatile alla porta CONSOLE. Una USB di memoria non e' una console.");
        else if(CableKind==TEXT("fiber"))Error=TEXT("La fibra richiede porte ottiche: negli slot SFP devi prima inserire il modulo.");
        else Error=TEXT("Ethernet collega due porte dati Ethernet. Console, USB, AUX, alimentazione e slot SFP hanno funzioni diverse.");
        return false;
    }
    if(A->IsPortOccupied(PortA)||B->IsPortOccupied(PortB)){Error=TEXT("Una porta e' gia' occupata: prima scollega il connettore esistente.");return false;}
    A->Connections.Add(PortA,{B,PortB,CableKind});B->Connections.Add(PortB,{A,PortA,CableKind});A->AddCable(PortA,B,PortB,CableKind);return true;
}
bool AClaraNetworkDevice::DisconnectPort(const FString& Id,FString& Error)
{
    AClaraNetworkDevice* Source=GetOriginalDevice();if(Source!=this)return Source->DisconnectPort(Id,Error);
    Error.Empty();auto* Link=Connections.Find(Id);if(!Link){Error=TEXT("La porta e' gia' libera.");return false;}
    if(Link->Peer.IsValid())Link->Peer->Connections.Remove(Link->PeerPort);
    Connections.Remove(Id);RefreshCables();return true;
}
void AClaraNetworkDevice::AddCable(const FString& Port,AClaraNetworkDevice* Peer,const FString& OtherPort,const FString& CableKind)
{
    if(bIsPreview)return;
    FCable Cable;Cable.Port=Port;Cable.OtherPort=OtherPort;Cable.Peer=Peer;Cable.Kind=CableKind;
    Cable.Spline=NewObject<USplineComponent>(this);AddInstanceComponent(Cable.Spline);Cable.Spline->SetupAttachment(RootComponent);Cable.Spline->RegisterComponent();
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));auto* Material=WireMaterial(this,CableKind);
    for(int32 I=0;I<6;++I)
    {
        auto* Segment=NewObject<USplineMeshComponent>(this);AddInstanceComponent(Segment);Segment->SetMobility(EComponentMobility::Movable);Segment->SetupAttachment(RootComponent);
        Segment->SetStaticMesh(Cylinder);Segment->SetForwardAxis(ESplineMeshAxis::Z,false);Segment->SetMaterial(0,Material);
        Segment->SetStartScale(FVector2D(.006));Segment->SetEndScale(FVector2D(.006));Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);Segment->RegisterComponent();Cable.Segments.Add(Segment);
    }
    Cables.Add(MoveTemp(Cable));RefreshCables();
}
void AClaraNetworkDevice::RefreshCables()
{
    for(int32 I=Cables.Num()-1;I>=0;--I)
    {
        auto& Cable=Cables[I];const auto* Link=Connections.Find(Cable.Port);
        if(!Cable.Peer.IsValid()||!Link||Link->Peer!=Cable.Peer||Link->PeerPort!=Cable.OtherPort)
        {for(USplineMeshComponent* Part:Cable.Segments)if(Part)Part->DestroyComponent();if(Cable.Spline)Cable.Spline->DestroyComponent();Cables.RemoveAt(I);continue;}
        const FVector A=GetPortWorldLocation(Cable.Port)+GetPortWorldNormal(Cable.Port)*2;
        const FVector B=Cable.Peer->GetPortWorldLocation(Cable.OtherPort)+Cable.Peer->GetPortWorldNormal(Cable.OtherPort)*2;
        FVector Middle=(A+B)*.5;
        Middle.Z=ActorHasTag(TEXT("BenchDevice"))&&Cable.Peer->ActorHasTag(TEXT("BenchDevice"))
            ? FMath::Max(77.8,FMath::Min(A.Z,B.Z)+.8)
            : FMath::Min(A.Z,B.Z)-FMath::Min(30.,FVector::Distance(A,B)*.18);
        Cable.Spline->SetSplinePoints({A,A+GetPortWorldNormal(Cable.Port)*8,Middle,B+Cable.Peer->GetPortWorldNormal(Cable.OtherPort)*8,B},ESplineCoordinateSpace::World,true);
        const float Length=Cable.Spline->GetSplineLength();
        for(int32 J=0;J<Cable.Segments.Num();++J)
        {
            const float Start=Length*J/Cable.Segments.Num(),End=Length*(J+1)/Cable.Segments.Num();
            Cable.Segments[J]->SetStartAndEnd(Cable.Spline->GetLocationAtDistanceAlongSpline(Start,ESplineCoordinateSpace::Local),Cable.Spline->GetDirectionAtDistanceAlongSpline(Start,ESplineCoordinateSpace::Local)*(End-Start),
                Cable.Spline->GetLocationAtDistanceAlongSpline(End,ESplineCoordinateSpace::Local),Cable.Spline->GetDirectionAtDistanceAlongSpline(End,ESplineCoordinateSpace::Local)*(End-Start));
        }
    }
}
void AClaraNetworkDevice::Tick(float Delta)
{
    Super::Tick(Delta);RefreshCables();
    if(ModelId==TEXT("Laptop")&&PreviewSource.IsValid()&&bLaptopCarried!=PreviewSource->IsLaptopCarried())SetLaptopCarried(PreviewSource->IsLaptopCarried());
    for(int32 I=0;I<Plugs.Num();++I)if(Ports.IsValidIndex(I))
    {
        const FString& Id=Ports[I].Id;Plugs[I]->SetVisibility(IsPortOccupied(Id)||PendingPort==Id||HasOpticalModule(Id));
        const auto* Link=GetOriginalDevice()->Connections.Find(Id);
        if(auto* Material=Cast<UMaterialInstanceDynamic>(Plugs[I]->GetMaterial(0)))
            Material->SetVectorParameterValue(TEXT("Color"),Link?CableColor(Link->CableKind):(HasOpticalModule(Id)?FLinearColor(.45,.46,.47):CableColor(PendingPort==Id?PendingKind:TEXT("ethernet"))));
    }
}
void AClaraNetworkDevice::EndPlay(const EEndPlayReason::Type Reason)
{
    if(!bIsPreview){TArray<FString> Ids;Connections.GetKeys(Ids);for(const auto& Id:Ids){FString Error;DisconnectPort(Id,Error);}}
    Super::EndPlay(Reason);
}

bool RunClaraNetworkDeviceSelfTest(UWorld* World)
{
    if(!World)return false;
    auto* A=World->SpawnActor<AClaraNetworkDevice>(FVector(0,0,50000),FRotator::ZeroRotator);
    auto* B=World->SpawnActor<AClaraNetworkDevice>(FVector(100,0,50000),FRotator::ZeroRotator);
    auto* Preview=World->SpawnActor<AClaraNetworkDevice>(FVector(200,0,50000),FRotator::ZeroRotator);
    if(!A||!B||!Preview)return false;
    A->Initialize(TEXT("Cisco2960X"));B->Initialize(TEXT("Cisco1941"));Preview->Initialize(TEXT("Cisco2960X"),true);Preview->SetPreviewSource(A);
    FString AP,BP,Console;for(const auto& P:A->GetPorts()){if(P.Kind==TEXT("ethernet")&&AP.IsEmpty())AP=P.Id;if(P.Kind==TEXT("console"))Console=P.Id;}
    for(const auto& P:B->GetPorts())if(P.Kind==TEXT("ethernet")&&BP.IsEmpty())BP=P.Id;
    bool Passed=!AP.IsEmpty()&&!BP.IsEmpty()&&!Console.IsEmpty();FString Error;
    Passed=Passed&&IsValid(A)&&AClaraNetworkDevice::IsCableCompatible(TEXT("console"),TEXT("console_host"),TEXT("console"))
        &&!AClaraNetworkDevice::IsCableCompatible(TEXT("console"),TEXT("usb"),TEXT("console"))&&AClaraNetworkDevice::IsCableCompatible(TEXT("fiber"),TEXT("fiber"),TEXT("fiber"))
        &&!AClaraNetworkDevice::IsCableCompatible(TEXT("fiber"),TEXT("sfp"),TEXT("fiber"));
    for(const auto& Port:A->GetPorts())if(Port.Kind==TEXT("sfp"))
    {
        Passed=Passed&&A->GetEffectivePortKind(Port.Id)==TEXT("sfp")&&Preview->InstallOpticalModule(Port.Id)
            &&A->HasOpticalModule(Port.Id)&&Preview->GetEffectivePortKind(Port.Id)==TEXT("fiber");
        break;
    }
    Passed=Passed&&AClaraNetworkDevice::ConnectPorts(A,AP,B,BP,TEXT("ethernet"),Error);
    Passed=Passed&&A->IsPortOccupied(AP)&&B->IsPortOccupied(BP)&&Preview->IsPortOccupied(AP);
    TArray<USplineMeshComponent*> CableMeshes;A->GetComponents(CableMeshes);
    Passed=Passed&&CableMeshes.Num()==6;
    for(auto* Part:CableMeshes)Passed=Passed&&Part->IsRegistered()&&Part->GetAttachParent()==A->GetRootComponent();
    Passed=Passed&&A->GetComponentsBoundingBox().GetSize().GetMax()<100;
    Passed=Passed&&!AClaraNetworkDevice::ConnectPorts(A,AP,B,BP,TEXT("ethernet"),Error);
    Passed=Passed&&Preview->DisconnectPort(AP,Error)&&!A->IsPortOccupied(AP)&&!B->IsPortOccupied(BP);
    Passed=Passed&&!AClaraNetworkDevice::ConnectPorts(A,Console,B,BP,TEXT("ethernet"),Error);
    Passed=Passed&&!AClaraNetworkDevice::ConnectPorts(A,TEXT("missing-port"),B,BP,TEXT("ethernet"),Error);
    Passed=Passed&&!AClaraNetworkDevice::ConnectPorts(A,AP,A,AP,TEXT("ethernet"),Error);
    A->Destroy();B->Destroy();Preview->Destroy();
    UE_LOG(LogTemp,Display,TEXT("CLARA_NETWORK_DEVICE_SELFTEST_%s: reciprocal ports, preview state, occupancy and compatibility."),Passed?TEXT("PASS"):TEXT("FAIL"));
    return Passed;
}
