#include "ClaraStoryCast.h"
#include "ClaraStoryDirector.h"
#include "Components/PoseableMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    void StoryCollision(UPrimitiveComponent* Component)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Component->SetCollisionObjectType(ECC_WorldDynamic);
        Component->SetCollisionResponseToAllChannels(ECR_Ignore);
        Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Component->SetGenerateOverlapEvents(false);
    }
}

AClaraStoryNPC::AClaraStoryNPC()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .025f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StoryNPCOrigin"));
    Interaction = CreateDefaultSubobject<UCapsuleComponent>(TEXT("StoryInteraction"));
    Interaction->SetupAttachment(RootComponent);
    Interaction->SetCapsuleSize(24, 87.5);
    Interaction->SetRelativeLocation(FVector(0, 0, 87.5));
    StoryCollision(Interaction);
    Mannequin = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TemporaryMannequin"));
    Mannequin->SetupAttachment(RootComponent);
    Mannequin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mannequin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mannequin->bEnableUpdateRateOptimizations = true;
    Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameAndRole"));
    Nameplate->SetupAttachment(RootComponent);
    Nameplate->SetRelativeLocation(FVector(0, 0, 186));
    Nameplate->SetHorizontalAlignment(EHTA_Center);
    Nameplate->SetVerticalAlignment(EVRTA_TextCenter);
    Nameplate->SetWorldSize(3.5);
    Nameplate->SetTextRenderColor(FColor(205, 220, 216));
    Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tags.Add(TEXT("StoryNPC"));
    Tags.Add(TEXT("StoryPlaceholder"));
}

void AClaraStoryNPC::Initialize(const FString& Id, const FString& Name, const FString& RoleName, bool bUseQuinn, FLinearColor Accent)
{
    CharacterId = Id;
    DisplayName = Name;
    RoleDescription = RoleName;
    Tags.AddUnique(FName(*(TEXT("NPC_") + Id)));
    Interaction->ComponentTags.AddUnique(FName(*(TEXT("StoryTarget=") + Id)));
    Nameplate->SetText(FText::FromString(Name + TEXT("\n") + RoleName + TEXT("\nPROVVISORIO")));
    auto* Mesh = LoadObject<USkeletalMesh>(nullptr, bUseQuinn
        ? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")
        : TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (!ensureMsgf(Mesh, TEXT("Epic mannequin asset missing for %s"), *Id)) return;
    Mannequin->SetSkeletalMeshAsset(Mesh);
    const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
    const float Height = Bounds.BoxExtent.Z * 2;
    if (!ensure(Height > 1)) return;
    const float Scale = 175.f / Height;
    Mannequin->SetRelativeScale3D(FVector(Scale));
    Mannequin->SetRelativeLocation(FVector(0, 0, -(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
    IdleAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
    if (IdleAnimation) Mannequin->PlayAnimation(IdleAnimation, true);
    if (Id==TEXT("Luca")) WalkAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
    SetActorTickEnabled(Id==TEXT("Luca"));
    for (int32 Index = 0; Index < Mannequin->GetNumMaterials(); ++Index)
    {
        auto* Parent = Mannequin->GetMaterial(Index);
        if (!Parent) continue;
        TArray<FMaterialParameterInfo> Parameters;
        TArray<FGuid> ParameterIds;
        Parent->GetAllVectorParameterInfo(Parameters, ParameterIds);
        for (const auto& Info : Parameters)
        {
            if (Info.Name == TEXT("Paint Tint"))
            {
                if (auto* Material = Mannequin->CreateDynamicMaterialInstance(Index))
                    Material->SetVectorParameterValueByInfo(Info, Accent);
                break;
            }
        }
    }
    if(Id==TEXT("Alessia")) ConfigureSeatedPose(Mesh,Scale);
    AttachMetaHuman(Id==TEXT("Jonny") ? TEXT("Jonathan") : Id, SeatedPose ? static_cast<USkinnedMeshComponent*>(SeatedPose.Get()) : Mannequin.Get());
#if WITH_EDITOR
    SetActorLabel(Name + (MetaHuman ? TEXT(" - MetaHuman") : TEXT(" - manichino storia provvisorio")));
#endif
}

void AClaraStoryNPC::AttachMetaHuman(const FString& AssetName, USkinnedMeshComponent* Leader)
{
    const FString ClassPath = FString::Printf(TEXT("/Game/MetaHumans/%s/BP_%s.BP_%s_C"), *AssetName, *AssetName, *AssetName);
    UClass* Class = StaticLoadClass(AActor::StaticClass(), nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Class || !Leader) return; // MetaHuman non generato localmente: resta il manichino.
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    MetaHuman = GetWorld()->SpawnActor<AActor>(Class, Leader->GetComponentTransform(), Parameters);
    if (!MetaHuman) return;
    MetaHuman->AttachToComponent(Leader, FAttachmentTransformRules::SnapToTargetIncludingScale);
    TInlineComponentArray<UPrimitiveComponent*> Primitives(MetaHuman.Get());
    for (UPrimitiveComponent* Primitive : Primitives) Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(MetaHuman.Get());
    bool bDriven = false;
    for (USkeletalMeshComponent* Part : Meshes)
        if (Part->GetFName() == TEXT("Body")) { Part->SetLeaderPoseComponent(Leader, true); bDriven = true; }
    // The hidden mannequin keeps animating and drives the MetaHuman skeleton (same UE5 bone names).
    Mannequin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Leader->SetVisibility(false);
    Mannequin->SetVisibility(false);
    Tags.Remove(TEXT("StoryPlaceholder"));
    Nameplate->SetText(FText::FromString(DisplayName + TEXT("\n") + RoleDescription));
    UE_LOG(LogTemp, Display, TEXT("CLARA_METAHUMAN: %s -> %s; pose driven=%s"), *CharacterId, *ClassPath, bDriven ? TEXT("yes") : TEXT("no"));
}

void AClaraStoryNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    if (MetaHuman) MetaHuman->Destroy();
    Super::EndPlay(Reason);
}


void AClaraStoryNPC::ConfigureSeatedPose(USkeletalMesh* Mesh,float Scale)
{
    SeatedPose=NewObject<UPoseableMeshComponent>(this,TEXT("ReceptionSeatedPose"));
    AddInstanceComponent(SeatedPose);
    SeatedPose->SetupAttachment(RootComponent);
    SeatedPose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SeatedPose->SetSkinnedAssetAndUpdate(Mesh);
    SeatedPose->RegisterComponent();
    SeatedPose->SetRelativeScale3D(FVector(Scale));
    for(int32 I=0;I<Mannequin->GetNumMaterials();++I) SeatedPose->SetMaterial(I,Mannequin->GetMaterial(I));
    SeatedPose->RefreshBoneTransforms();
    auto Position=[this](const TCHAR* Name){return SeatedPose->GetBoneLocationByName(Name,EBoneSpaces::ComponentSpace);};
    const float CalfLength=(Position(TEXT("calf_l"))-Position(TEXT("foot_l"))).Size();
    const float FootDrop=Position(TEXT("foot_l")).Z-(Mesh->GetImportedBounds().Origin.Z-Mesh->GetImportedBounds().BoxExtent.Z);
    const float PelvisOffset=Position(TEXT("pelvis")).Z-Position(TEXT("thigh_l")).Z;
    const float PelvisHeight=(CalfLength+FootDrop+PelvisOffset)*Scale;
    const FVector Pelvis=Position(TEXT("pelvis"));
    SeatedPose->SetRelativeLocation(FVector(-Pelvis.X*Scale,-Pelvis.Y*Scale,PelvisHeight-Pelvis.Z*Scale));
    auto Aim=[this,&Position](const TCHAR* Bone,const TCHAR* Child,FVector Direction)
    {
        FTransform T=SeatedPose->GetBoneTransformByName(Bone,EBoneSpaces::ComponentSpace);
        const FVector Current=(Position(Child)-T.GetLocation()).GetSafeNormal();
        T.SetRotation((FQuat::FindBetweenNormals(Current,Direction.GetSafeNormal())*T.GetRotation()).GetNormalized());
        SeatedPose->SetBoneTransformByName(Bone,T,EBoneSpaces::ComponentSpace);
        SeatedPose->RefreshBoneTransforms();
    };
    // The imported Epic skeleton faces +Y; chair and actor face the visitor (-Y in the room).
    for(const TCHAR* Side : {TEXT("l"),TEXT("r")})
    {
        const FString Thigh=FString(TEXT("thigh_"))+Side,Calf=FString(TEXT("calf_"))+Side;
        const FString Foot=FString(TEXT("foot_"))+Side,Ball=FString(TEXT("ball_"))+Side;
        Aim(*Thigh,*Calf,FVector(0,1,0));
        Aim(*Calf,*Foot,FVector(0,0,-1));
        Aim(*Foot,*Ball,FVector(0,1,-.12f));
        const FString Arm=FString(TEXT("upperarm_"))+Side,Forearm=FString(TEXT("lowerarm_"))+Side,Hand=FString(TEXT("hand_"))+Side;
        Aim(*Arm,*Forearm,FVector(0,.25f,-1));
        Aim(*Forearm,*Hand,FVector(0,1,-.12f));
    }
    Mannequin->SetVisibility(false);
    Mannequin->SetComponentTickEnabled(false);
    const float HeadHeight=SeatedPose->GetBoneLocationByName(TEXT("head"),EBoneSpaces::WorldSpace).Z-GetActorLocation().Z;
    Nameplate->SetRelativeLocation(FVector(0,0,HeadHeight+25));
    Nameplate->SetRelativeRotation(FRotator(0,90,0));
    Interaction->SetCapsuleSize(25,67);
    Interaction->SetRelativeLocation(FVector(0,0,67));
    UE_LOG(LogTemp,Display,TEXT("CLARA_ALESSIA_SEATED: reception chair 305,450; pelvis %.1f cm; feet on floor; existing furniture retained."),PelvisHeight);
}

void AClaraStoryNPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(CharacterId!=TEXT("Luca"))return;
    if(!Director.IsValid()){TActorIterator<AClaraStoryDirector> It(GetWorld());if(It)Director=*It;}
    auto* PC=GetWorld()->GetFirstPlayerController();
    APawn* Player=PC?PC->GetPawn():nullptr;
    if(!Player||!Director.IsValid())return;
    const int32 Phase=Director->GetPhase();
    const bool Active=(Phase>=3&&Phase<16)||(Phase>=100&&Phase<108);
    const FVector Here=GetActorLocation(),There=FVector(Player->GetActorLocation().X,Player->GetActorLocation().Y,Here.Z);
    const float Distance=FVector::Dist2D(Here,There);
    bool Moving=false;
    if(Active && Director->GetPage().IsEmpty() && !Director->IsScreenOpen() && Distance>185)
    {
        FCollisionQueryParams Params;Params.AddIgnoredActor(this);Params.AddIgnoredActor(Player);
        const auto Clear=[this,&Params](FVector A,FVector B)
        {
            FHitResult Hit;
            return !GetWorld()->SweepSingleByChannel(Hit,A+FVector(0,0,88),B+FVector(0,0,88),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(22,85),Params);
        };
        if(Clear(Here,There))FollowGoal=There;
        else if(GetWorld()->TimeSeconds>=NextRouteTime)
        {
            NextRouteTime=GetWorld()->TimeSeconds+.65f;
            TArray<FVector> Nodes={Here,There,{650,1100,0},{650,1300,0},{1250,1300,0},{1250,950,0},{955,950,0},{680,850,0},{680,650,0},{175,650,0},{175,450,0},{470,650,0},{470,900,0},{1510,650,0},{1510,900,0}};
            TArray<int32> Previous;Previous.Init(-1,Nodes.Num());Previous[0]=0;
            TArray<int32> Queue;Queue.Add(0);
            for(int32 Head=0;Head<Queue.Num()&&Previous[1]<0;++Head)
            {
                const int32 A=Queue[Head];
                for(int32 B=1;B<Nodes.Num();++B)
                    if(Previous[B]<0&&Clear(Nodes[A],Nodes[B])){Previous[B]=A;Queue.Add(B);}
            }
            FollowGoal=Here;
            if(Previous[1]>=0)
            {
                int32 Next=1;
                while(Previous[Next]>0)Next=Previous[Next];
                FollowGoal=Nodes[Next];
            }
        }
        FVector Direction=FollowGoal-Here;Direction.Z=0;
        const float Length=Direction.Size();
        const FVector Next=Here+Direction.GetSafeNormal()*FMath::Min(Length,DeltaSeconds*160.f);
        if(Length>3&&Clear(Here,Next))
        {
            SetActorLocation(Next);
            const FRotator Facing(0,Direction.Rotation().Yaw-90,0);
            SetActorRotation(FMath::RInterpTo(GetActorRotation(),Facing,DeltaSeconds,6));
            Moving=true;
        }
    }
    else if(Active && Distance<360)
        SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,(There-Here).Rotation().Yaw-90,0),DeltaSeconds,3));
    if(Moving!=bWalking)
    {
        bWalking=Moving;
        if(auto* Animation=Moving?WalkAnimation.Get():IdleAnimation.Get())Mannequin->PlayAnimation(Animation,true);
    }
}

UPrimitiveComponent* AClaraStoryNPC::GetInteractionComponent() const
{
    return Interaction.Get();
}

void AClaraStoryNPC::SetStoryVisible(bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    SetActorEnableCollision(bVisible);
    if (MetaHuman) MetaHuman->SetActorHiddenInGame(!bVisible);
}

AClaraStoryProp::AClaraStoryProp()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StoryPropOrigin"));
    Interaction = CreateDefaultSubobject<UBoxComponent>(TEXT("StoryPropInteraction"));
    Interaction->SetupAttachment(RootComponent);
    Interaction->SetBoxExtent(FVector(3.2, 5, 1.2));
    StoryCollision(Interaction);
    Card = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BadgeCard"));
    Card->SetupAttachment(RootComponent);
    Card->SetRelativeScale3D(FVector(.054, .086, .0014));
    Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Stripe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BadgeStripe"));
    Stripe->SetupAttachment(RootComponent);
    Stripe->SetRelativeLocation(FVector(0, 2.2, .08));
    Stripe->SetRelativeScale3D(FVector(.054, .012, .0002));
    Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Clip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BadgeClip"));
    Clip->SetupAttachment(RootComponent);
    Clip->SetRelativeLocation(FVector(0, 4.4, .15));
    Clip->SetRelativeScale3D(FVector(.018, .008, .003));
    Clip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Lettering = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BadgePrint"));
    Lettering->SetupAttachment(RootComponent);
    Lettering->SetRelativeLocation(FVector(0, -.8, .09));
    Lettering->SetRelativeRotation(FRotator(90, 0, 0));
    Lettering->SetHorizontalAlignment(EHTA_Center);
    Lettering->SetVerticalAlignment(EVRTA_TextCenter);
    Lettering->SetWorldSize(.9);
    Lettering->SetTextRenderColor(FColor(28, 54, 54));
    Lettering->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tags.Add(TEXT("StoryProp"));
}

void AClaraStoryProp::Initialize(const FString& Id, const FString& Label)
{
    PropId = Id;
    DisplayName = Label;
    Tags.AddUnique(FName(*(TEXT("Prop_") + Id)));
    Interaction->ComponentTags.AddUnique(FName(*(TEXT("StoryProp=") + Id)));
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Office.M_Office"));
    if (!Parent) Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    UStaticMeshComponent* Parts[] = {Card.Get(), Stripe.Get(), Clip.Get()};
    const FLinearColor Colors[] = {FLinearColor(.88, .89, .84), FLinearColor(.035, .28, .27), FLinearColor(.32, .35, .36)};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Parts[Index]->SetStaticMesh(Cube);
        if (Parent)
        {
            auto* Material = UMaterialInstanceDynamic::Create(Parent, this);
            Material->SetVectorParameterValue(TEXT("BaseColor"), Colors[Index]);
            Material->SetVectorParameterValue(TEXT("Color"), Colors[Index]);
            Parts[Index]->SetMaterial(0, Material);
        }
    }
    Lettering->SetText(FText::FromString(TEXT("CLARA\nSTAFF")));

    if (Id == TEXT("Badge")) return;

    Card->SetVisibility(false);
    Stripe->SetVisibility(false);
    Clip->SetVisibility(false);
    Lettering->SetText(FText::FromString(Label));
    Lettering->SetWorldSize(1.6);
    Lettering->SetRelativeLocation(FVector(0, -15, .15));

    auto* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto Material = [this, Parent](FLinearColor Color)
    {
        auto* Result = UMaterialInstanceDynamic::Create(Parent, this);
        Result->SetVectorParameterValue(TEXT("Color"), Color);
        Result->SetVectorParameterValue(TEXT("BaseColor"), Color);
        return Result;
    };
    auto Part = [this](UStaticMesh* Mesh, FVector Position, FVector Size, UMaterialInterface* Mat, FRotator Rotation = FRotator::ZeroRotator)
    {
        auto* Component = NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Component);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetupAttachment(RootComponent);
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetMaterial(0, Mat);
        Component->RegisterComponent();
        Component->SetRelativeLocation(Position);
        Component->SetRelativeRotation(Rotation);
        Component->SetRelativeScale3D(Size / 100.f);
        return Component;
    };
    auto Tube = [&Part, Cylinder](FVector A, FVector B, float Diameter, UMaterialInterface* Mat)
    {
        const FVector Delta = B - A;
        Part(Cylinder, (A+B)*.5f, FVector(Diameter, Diameter, Delta.Size()), Mat,
            FRotationMatrix::MakeFromZ(Delta).Rotator());
    };
    auto* Dark = Material(FLinearColor(.018, .022, .024));
    auto* Silver = Material(FLinearColor(.42, .45, .47));
    if (Id.StartsWith(TEXT("Cable_")))
    {
        const bool Fiber = Id.EndsWith(TEXT("fiber"));
        const bool Console = Id.EndsWith(TEXT("console"));
        auto* Sheath = Material(Fiber ? FLinearColor(.93,.72,.025) : Console ? FLinearColor(.05,.4,.68) : FLinearColor(.78,.79,.74));
        const float Diameter = Fiber ? .24f : .56f;
        constexpr int32 Segments = 112;
        FVector Previous;
        for (int32 I=0; I<=Segments; ++I)
        {
            const float T = static_cast<float>(I)/Segments;
            const float Angle = T*PI*5.f;
            const float Radius = 8.f+T*2.f;
            const FVector Current(FMath::Cos(Angle)*Radius, FMath::Sin(Angle)*Radius, .5f+T*.8f);
            if (I>0) Tube(Previous,Current,Diameter,Sheath);
            Previous = Current;
        }
        const FVector Ends[] = {FVector(8,0,.5f),Previous};
        for (int32 Index=0; Index<2; ++Index)
        {
            const FVector P = Ends[Index] + FVector(0,-3,0);
            Tube(Ends[Index],P,Diameter,Sheath);
            if (Fiber)
            {
                auto* Connector = Material(FLinearColor(.12,.44,.36));
                Part(Cube,P+FVector(0,-1.2f,.2f),FVector(1.5f,2.4f,1),Connector);
                for(float X : {-.38f,.38f})
                    Part(Cube,P+FVector(X,-3,.2f),FVector(.3f,1.2f,.3f),Silver);
            }
            else if (Console && Index==0)
            {
                Part(Cube,P+FVector(0,-1.3f,.25f),FVector(1.35f,2.6f,.7f),Sheath);
                Part(Cube,P+FVector(0,-3.1f,.25f),FVector(1.2f,1.2f,.45f),Silver);
            }
            else
            {
                Part(Cube,P+FVector(0,-1.2f,.25f),FVector(1.15f,2.4f,.9f),Silver);
                Part(Cube,P+FVector(0,-.6f,.8f),FVector(.4f,1.8f,.18f),Silver);
                auto* Gold = Material(FLinearColor(.65,.43,.1));
                for(int32 Pin=0;Pin<8;++Pin)
                    Part(Cube,P+FVector(-.42f+Pin*.12f,-2.2f,.72f),FVector(.055f,.35f,.06f),Gold);
            }
        }
        Interaction->SetBoxExtent(FVector(19,20,9));
        Interaction->SetRelativeLocation(FVector(0,0,6));
    }
    else if (Id.StartsWith(TEXT("Box_")))
    {
        Part(Cube,FVector(0,0,-.1f),FVector(44,26,.8f),Material(FLinearColor(.18,.21,.21)));
        Interaction->SetBoxExtent(FVector(22,13,5));
        Interaction->SetRelativeLocation(FVector(0,0,3));
        Lettering->SetRelativeLocation(FVector(0,-9,1));
        Lettering->SetWorldSize(2.1);
        Lettering->SetTextRenderColor(FColor(220,230,225));
    }
    else if (Id == TEXT("SFP") || Id == TEXT("ModuleSFP"))
    {
        Part(Cube,FVector(0,0,.55f),FVector(1.35f,5.6f,1.1f),Silver);
        Part(Cube,FVector(0,-2.85f,.55f),FVector(1.3f,.3f,.9f),Dark);
        Part(Cube,FVector(0,-3.3f,.55f),FVector(1.4f,.6f,.18f),Material(FLinearColor(.16,.38,.42)));
        for(float X : {-.3f,.3f}) Part(Cube,FVector(X,-3,.55f),FVector(.4f,.35f,.45f),Dark);
        Part(Cube,FVector(0,.7f,1.12f),FVector(1.15f,2.4f,.04f),Material(FLinearColor(.8,.81,.76)));
        Interaction->SetBoxExtent(FVector(3,5,2));
        Lettering->SetRelativeLocation(FVector(0,-6,.15f));
        Lettering->SetWorldSize(1.1);
    }
    else if (Id == TEXT("Coffee"))
    {
        auto* Ceramic = Material(FLinearColor(.78,.8,.77));
        Part(Cylinder,FVector(0,0,4),FVector(7,7,8),Ceramic);
        Part(Cylinder,FVector(0,0,8.05f),FVector(6.1f,6.1f,.12f),Material(FLinearColor(.045,.025,.013)));
        FVector Previous(4,0,4);
        for (int32 I=1;I<=24;++I)
        {
            const float Angle = static_cast<float>(I)/24.f*PI*2.f;
            FVector Current(4+FMath::Sin(Angle)*2,0,4+FMath::Cos(Angle)*2.4f);
            if(I>1) Tube(Previous,Current,.75f,Ceramic);
            Previous=Current;
        }
        Interaction->SetBoxExtent(FVector(7,4,5));
        Interaction->SetRelativeLocation(FVector(0,0,4));
        Lettering->SetText(FText::GetEmpty());
    }

}

UPrimitiveComponent* AClaraStoryProp::GetInteractionComponent() const
{
    return Interaction.Get();
}

AClaraStoryCast::AClaraStoryCast()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CastOrigin"));
}

void AClaraStoryCast::BeginPlay()
{
    Super::BeginPlay();
    struct FColleague
    {
        const TCHAR* Id;
        const TCHAR* Role;
        FVector Position;
        float Yaw;
        bool bQuinn;
        FLinearColor Accent;
    };
    const FColleague People[] = {
        {TEXT("Alessia"), TEXT("Accoglienza"), FVector(305, 450, 0), 180, true, FLinearColor(.55, .50, .40)},
        {TEXT("Luca"), TEXT("Tutor reti"), FVector(675, 1100, 0), -90, false, FLinearColor(.08, .24, .38)},
        {TEXT("Jonny"), TEXT("Tutor cyber"), FVector(1220, 450, 0), -90, false, FLinearColor(.10, .13, .15)},
        {TEXT("Gaia"), TEXT("Briefing"), FVector(250, 880, 0), -90, true, FLinearColor(.08, .30, .28)}
    };
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FColleague& Person : People)
    {
        auto* NPC = GetWorld()->SpawnActor<AClaraStoryNPC>(Person.Position, FRotator(0, Person.Yaw, 0), Parameters);
        if (!NPC) continue;
        NPC->Initialize(Person.Id, Person.Id, Person.Role, Person.bQuinn, Person.Accent);
        Colleagues.Add(NPC);
    }
    Badge = GetWorld()->SpawnActor<AClaraStoryProp>(FVector(245, 330, 82), FRotator::ZeroRotator, Parameters);
    if (Badge) Badge->Initialize(TEXT("Badge"), TEXT("Badge del nuovo collega"));
    int32 MetaHumans = 0;
    for (const auto& NPC : Colleagues) MetaHumans += NPC->HasMetaHuman() ? 1 : 0;
    UE_LOG(LogTemp, Display, TEXT("CLARA_STORY_CAST_READY: %d colleagues, %d MetaHuman; badge=%s; floor plan unchanged."),
        Colleagues.Num(), MetaHumans, Badge ? TEXT("yes") : TEXT("no"));
}
