#include "ClaraStoryCast.h"
#include "ClaraFrontEnd.h"
#include "TimerManager.h"
#include "ClaraStoryDirector.h"
#include "Components/PoseableMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GroomAsset.h"
#include "GroomComponent.h"

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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
    Nameplate->SetText(FText::FromString(Name + TEXT("\n") + RoleName));
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
    SetActorTickEnabled(true);
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
    AttachMetaHuman(Id==TEXT("Jonny") ? TEXT("Jonathan") : Id==TEXT("Gaia") ? TEXT("Alessia") : Id, SeatedPose ? static_cast<USkinnedMeshComponent*>(SeatedPose.Get()) : Mannequin.Get());
#if WITH_EDITOR
    SetActorLabel(Name + (MetaHuman ? TEXT(" - MetaHuman") : TEXT(" - manichino storia provvisorio")));
#endif
    if (Id == TEXT("Gaia")) SetStoryVisible(false);
}

bool AClaraStoryNPC::AttachOfficeCharacter(const FString& AssetName)
{
    const FString Path = FString::Printf(TEXT("/Game/Characters/OfficeCast/BP_TNES_%sOffice.BP_TNES_%sOffice_C"), *AssetName, *AssetName);
    UClass* Class = StaticLoadClass(AActor::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Class) return false;
    auto* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/OfficeCast/Animations/MTN_N_Idle.MTN_N_Idle"));
    auto* Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/OfficeCast/Animations/MTN_N_Walk_InPlace.MTN_N_Walk_InPlace"));
    if (!Idle || !Walk)
    {
        UE_LOG(LogTemp, Error, TEXT("TNES_OFFICE_CAST: missing native crowd animations for %s"), *AssetName);
        return false;
    }
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    MetaHuman = GetWorld()->SpawnActor<AActor>(Class, GetActorTransform(), Parameters);
    if (!MetaHuman) return false;
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(MetaHuman.Get());
    for (USkeletalMeshComponent* Part : Meshes)
        if (const USkeletalMesh* Mesh = Part->GetSkeletalMeshAsset(); Mesh && Mesh->GetName() == TEXT("m_tal_nrw_base"))
            OfficeAnimationRoot = Part;
    if (!OfficeAnimationRoot)
    {
        UE_LOG(LogTemp, Error, TEXT("TNES_OFFICE_CAST: %s has no native animation root"), *AssetName);
        MetaHuman->Destroy();
        MetaHuman = nullptr;
        return false;
    }
    // Preserve Epic's native body/clothes/face bindings and corrective pose graph.
    MetaHuman->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    MetaHuman->SetActorRelativeScale3D(FVector::OneVector);
    TInlineComponentArray<UPrimitiveComponent*> Primitives(MetaHuman.Get());
    for (UPrimitiveComponent* Part : Primitives) Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    IdleAnimation = Idle;
    WalkAnimation = Walk;
    OfficeAnimationRoot->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    OfficeAnimationRoot->bEnableUpdateRateOptimizations = false;
    OfficeAnimationRoot->SetComponentTickEnabled(true);
    OfficeAnimationRoot->PlayAnimation(IdleAnimation, true);
    Mannequin->SetVisibility(false);
    Mannequin->SetComponentTickEnabled(false);
    Tags.Remove(TEXT("StoryPlaceholder"));
    UE_LOG(LogTemp, Display, TEXT("TNES_OFFICE_CAST: %s -> %s; native assembly, clothes, shoes and animations"), *AssetName, *Path);
    return true;
}

void AClaraStoryNPC::AttachMetaHuman(const FString& AssetName, USkinnedMeshComponent* Leader)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("ClaraMannequins"))) return;
    if ((AssetName == TEXT("Luca") || AssetName == TEXT("Jonathan")) && AttachOfficeCharacter(AssetName)) return;
    const FString ClassPath = FString::Printf(TEXT("/Game/MetaHumans/%s/BP_%s.BP_%s_C"), *AssetName, *AssetName, *AssetName);
    UClass* Class = StaticLoadClass(AActor::StaticClass(), nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Class || !Leader)
    {
        if (CharacterId != TEXT("Gaia"))
            UE_LOG(LogTemp, Error, TEXT("CLARA_METAHUMAN: required character %s could not load %s"), *CharacterId, *ClassPath);
        return;
    }
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    MetaHuman = GetWorld()->SpawnActor<AActor>(Class, Leader->GetComponentTransform(), Parameters);
    if (!MetaHuman) return;
    MetaHuman->AttachToComponent(Leader, FAttachmentTransformRules::SnapToTargetIncludingScale);
    TInlineComponentArray<UPrimitiveComponent*> Primitives(MetaHuman.Get());
    for (UPrimitiveComponent* Primitive : Primitives) Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(MetaHuman.Get());
    USkeletalMeshComponent* Body = nullptr;
    USkeletalMeshComponent* Face = nullptr;
    for (USkeletalMeshComponent* Part : Meshes)
    {
        const USkeletalMesh* Mesh = Part->GetSkeletalMeshAsset();
        if (!Mesh) continue;
        if (Part->GetFName() == TEXT("Body") || Mesh->GetName().EndsWith(TEXT("_BodyMesh"))) Body = Part;
        if (Part->GetFName() == TEXT("Face") || Mesh->GetName().EndsWith(TEXT("_FaceMesh"))) Face = Part;
    }
    if (!Body || !Face)
    {
        UE_LOG(LogTemp, Error, TEXT("CLARA_METAHUMAN: %s has no valid Body/Face mesh"), *AssetName);
        MetaHuman->Destroy();
        MetaHuman = nullptr;
        return;
    }
    Body->SetLeaderPoseComponent(Leader, true);
    // CopyPoseFromMesh only follows skeletal leaders, not the seated poseable mesh.
    // A direct face leader keeps Alessia's neutral face aligned with her seated head.
    if (SeatedPose) Face->SetLeaderPoseComponent(Leader, true);
    const FLinearColor Shirt = CharacterId == TEXT("Alessia") ? FLinearColor(.80f,.78f,.74f)
        : CharacterId == TEXT("Luca") ? FLinearColor(.30f,.45f,.62f) : FLinearColor(.035f,.035f,.04f);
    const FLinearColor Trousers = CharacterId == TEXT("Alessia") ? FLinearColor(.02f,.025f,.04f)
        : CharacterId == TEXT("Luca") ? FLinearColor(.05f,.05f,.055f) : FLinearColor(.05f,.08f,.15f);
    int32 TintedMaterials = 0;
    for (USkeletalMeshComponent* Part : Meshes)
    {
        for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
        {
            UMaterialInterface* Material = Part->GetMaterial(Slot);
            if (!Material || !Material->GetName().Contains(TEXT("DefaultGarment"))) continue;
            TArray<FMaterialParameterInfo> ParametersInfo;
            TArray<FGuid> ParameterIds;
            Material->GetAllVectorParameterInfo(ParametersInfo, ParameterIds);
            UMaterialInstanceDynamic* Instance = nullptr;
            for (const FMaterialParameterInfo& Info : ParametersInfo)
            {
                if (!Info.Name.ToString().StartsWith(TEXT("diffuse_color"))) continue;
                if (!Instance) Instance = Part->CreateDynamicMaterialInstance(Slot);
                if (Instance) Instance->SetVectorParameterValueByInfo(Info,
                    Material->GetName().Contains(TEXT("Shirt")) ? Shirt : Trousers);
            }
            if (Instance) ++TintedMaterials;
        }
    }
    // The hidden driver still updates every frame for the visible body and face.
    Mannequin->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mannequin->bEnableUpdateRateOptimizations = false;
    Leader->SetVisibility(false);
    Mannequin->SetVisibility(false);
    Tags.Remove(TEXT("StoryPlaceholder"));
    Nameplate->SetText(FText::FromString(DisplayName + TEXT("\n") + RoleDescription));
    // Personal likeness is still being fitted; the assembled preset is the playable default.
    if (CharacterId == TEXT("Luca") && FParse::Param(FCommandLine::Get(), TEXT("ClaraPersonalLuca")))
        AttachCustomLucaHead(Leader, Face);
    UE_LOG(LogTemp, Display, TEXT("CLARA_METAHUMAN: %s -> %s; body=%s face=%s leader=%s; tinted materials=%d"),
        *CharacterId, *ClassPath, *Body->GetName(), *Face->GetName(), *Leader->GetName(), TintedMaterials);
}

void AClaraStoryNPC::AttachCustomLucaHead(USkinnedMeshComponent* Leader, USkeletalMeshComponent* Face)
{
    auto* HeadMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Luca/SM_Luca_Testa.SM_Luca_Testa"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    auto* HairAsset = LoadObject<UGroomAsset>(nullptr, TEXT("/Game/Luca/GR_Luca_Capelli.GR_Luca_Capelli"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    const FName NoseBone(TEXT("FACIAL_C_NoseTip"));
    if (!HeadMesh || !HairAsset || !Face || Face->GetBoneIndex(NoseBone) == INDEX_NONE || Face->GetBoneIndex(TEXT("head")) == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("CLARA_CUSTOM_LUCA: personal head, groom or facial anchor missing"));
        return;
    }
    Mannequin->TickAnimation(0.f, false);
    Mannequin->RefreshBoneTransforms();
    Face->TickAnimation(0.f, false);
    Face->RefreshBoneTransforms();
    // Head and groom share the imported frame: centimetres, forward +Y, measured nose tip.
    const FVector SourceNose(-.1f, 12.9f, .3f);
    const FVector Scale = Leader->GetComponentScale();
    const FQuat Rotation = Leader->GetComponentQuat();
    const FVector Nose = Face->GetBoneLocation(NoseBone);
    FTransform Placement(Rotation, Nose - Rotation.RotateVector(SourceNose * Scale), Scale);
    // Centre the measured open neck ring on the body's neck, preserving the vertical overlap.
    const FVector SourceNeck(-.094f, -2.525f, -14.947f);
    const FVector Neck = Leader->GetBoneLocation(TEXT("neck_01"));
    const FVector Ring = Placement.TransformPosition(SourceNeck);
    Placement.AddToTranslation(FVector(Neck.X - Ring.X, Neck.Y - Ring.Y, 0));
    auto* Head = NewObject<UStaticMeshComponent>(this, TEXT("LucaPersonalHead"));
    AddInstanceComponent(Head);
    Head->SetStaticMesh(HeadMesh);
    if (auto* Skin = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Luca/M_Luca_Pelle.M_Luca_Pelle"))) Head->SetMaterial(0, Skin);
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Head->SetupAttachment(Leader, TEXT("head"));
    Head->RegisterComponent();
    Head->SetWorldTransform(Placement);
    auto* Hair = NewObject<UGroomComponent>(this, TEXT("LucaPersonalHair"));
    AddInstanceComponent(Hair);
    Hair->SetupAttachment(Head);
    Hair->SetGroomAsset(HairAsset);
    if (auto* HairMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Luca/M_Luca_Capelli.M_Luca_Capelli"))) Hair->SetMaterial(0, HairMaterial);
    Hair->SimulationSettings.bOverrideSettings = true;
    Hair->SimulationSettings.SolverSettings.bEnableSimulation = false;
    Hair->SetEnableSimulation(false);
    Hair->SetForcedLOD(0);
    Hair->SetHairWidth(.03f);
    Hair->SetHairRootScale(1.15f);
    Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hair->RegisterComponent();
    // The Face asset also owns the neck and upper chest; retain those below the personal head.
    Face->SetVisibility(true);
    Face->SetHiddenInGame(false);
    Face->HideBoneByName(TEXT("head"), PBO_None);
    TInlineComponentArray<UGroomComponent*> PresetGrooms(MetaHuman.Get());
    for (UGroomComponent* PresetGroom : PresetGrooms)
    {
        PresetGroom->SetVisibility(false, true);
        PresetGroom->SetHiddenInGame(true, true);
        PresetGroom->SetComponentTickEnabled(false);
        PresetGroom->SetCastShadow(false);
    }
    Head->SetVisibility(true, true);
    Head->SetHiddenInGame(false, true);
    Head->PrestreamTextures(30.f, true);
    bCustomLucaHead = true;
    TInlineComponentArray<USkeletalMeshComponent*> VisualParts(MetaHuman.Get());
    for (USkeletalMeshComponent* Part : VisualParts)
    {
        const USkeletalMesh* Mesh = Part->GetSkeletalMeshAsset();
        const FBoxSphereBounds Bounds = Mesh ? Mesh->GetImportedBounds() : FBoxSphereBounds(EForceInit::ForceInit);
        UE_LOG(LogTemp, Display, TEXT("CLARA_CUSTOM_LUCA_PART: component=%s mesh=%s visible=%d hidden=%d ref center=%s extent=%s"),
            *Part->GetName(), *GetNameSafe(Mesh), Part->IsVisible(), Part->bHiddenInGame,
            *Bounds.Origin.ToCompactString(), *Bounds.BoxExtent.ToCompactString());
        if (Part->GetFName() == TEXT("Body"))
            UE_LOG(LogTemp, Display, TEXT("CLARA_CUSTOM_LUCA_NECK: body neck01=%s head open-ring=%s"),
                *Part->GetBoneLocation(TEXT("neck_01")).ToCompactString(),
                *Placement.TransformPosition(SourceNeck).ToCompactString());
    }
    UE_LOG(LogTemp, Display, TEXT("CLARA_CUSTOM_LUCA: personal head + original groom attached; nose=%s; head center=%s extent=%s scale=%s; neck visible=%d preset head hidden=%d; rigid neutral face"),
        *Nose.ToCompactString(), *Head->Bounds.Origin.ToCompactString(), *Head->Bounds.BoxExtent.ToCompactString(),
        *Head->GetComponentScale().ToCompactString(), Face->IsVisible(), Face->IsBoneHiddenByName(TEXT("head")));
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
    if(!Director.IsValid()){TActorIterator<AClaraStoryDirector> It(GetWorld());if(It)Director=*It;}
    if (CharacterId == TEXT("Gaia") && Director.IsValid())
    {
        const bool bVisible = Director->GetDay() >= 2;
        if (IsHidden() == bVisible) SetStoryVisible(bVisible);
        if (!bVisible) return;
    }
    auto* PC=GetWorld()->GetFirstPlayerController();
    APawn* Player=PC?PC->GetPawn():nullptr;
    if(!Player||!Director.IsValid())return;
    FVector Eye; FRotator View;
    PC->GetPlayerViewPoint(Eye, View);
    Nameplate->SetWorldRotation((Eye-Nameplate->GetComponentLocation()).Rotation());
    Nameplate->SetVisibility(FVector::DistSquared(Eye, GetActorLocation()) < FMath::Square(420.f)
        && Director->GetPage().IsEmpty() && !Director->IsScreenOpen());
    const FVector Here=GetActorLocation(),There=FVector(Player->GetActorLocation().X,Player->GetActorLocation().Y,Here.Z);
    const float Distance=FVector::Dist2D(Here,There);
    const bool bFacePlayer = !SeatedPose && Distance < 300 && !Director->IsScreenOpen()
        && (Director->GetPage().IsEmpty() || Director->GetSpeaker() == DisplayName);
    const FRotator PlayerFacing(0, (There-Here).Rotation().Yaw-90, 0);
    if(CharacterId!=TEXT("Luca"))
    {
        if (bFacePlayer) SetActorRotation(FMath::RInterpTo(GetActorRotation(),PlayerFacing,DeltaSeconds,3));
        return;
    }
    const int32 Phase=Director->GetPhase();
    const bool Active=(Phase>=3&&Phase<16)||(Phase>=100&&Phase<108)||(Phase>=202&&Phase<=211);
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
    else if(bFacePlayer)
        SetActorRotation(FMath::RInterpTo(GetActorRotation(),PlayerFacing,DeltaSeconds,3));
    if(Moving!=bWalking)
    {
        bWalking=Moving;
        if (auto* Animation = Moving ? WalkAnimation.Get() : IdleAnimation.Get())
            (OfficeAnimationRoot ? OfficeAnimationRoot.Get() : Mannequin.Get())->PlayAnimation(Animation, true);
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
    Lettering->SetRelativeRotation(FRotator(90, -90, 0));
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
    Lettering->SetText(FText::FromString(TEXT("TNES\nSTAFF")));

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
        // A label on the front lip stays readable while standing at the cable table.
        Part(Cube,FVector(0,-13.3f,2.6f),FVector(31,.5f,5),Dark);
        Lettering->SetRelativeLocation(FVector(0,-13.6f,2.6f));
        Lettering->SetRelativeRotation(FRotator(0,-90,0));
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
    Super::BeginPlay();BuildNextColleague();
}
void AClaraStoryCast::BuildNextColleague()
{
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
        {TEXT("Gaia"), TEXT("Briefing"), FVector(265, 1335, 0), -90, true, FLinearColor(.08, .30, .28)}
    };
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const bool Staged=AClaraFrontEnd::IsPreparingOffice(GetWorld());
    const int32 End=Staged?FMath::Min(NextColleague+1,4):4;
    for (int32 Index=NextColleague;Index<End;++Index)
    {
        const FColleague& Person=People[Index];
        auto* NPC = GetWorld()->SpawnActor<AClaraStoryNPC>(Person.Position, FRotator(0, Person.Yaw, 0), Parameters);
        if (!NPC) continue;
        NPC->Initialize(Person.Id, Person.Id, Person.Role, Person.bQuinn, Person.Accent);
        Colleagues.Add(NPC);
    }
    NextColleague=End;
    if(NextColleague<4)
    {
        TWeakObjectPtr<AClaraStoryCast> Weak(this);
        GetWorldTimerManager().SetTimerForNextTick([Weak](){if(Weak.IsValid())Weak->BuildNextColleague();});return;
    }
    bBuildReady=true;
    Badge = GetWorld()->SpawnActor<AClaraStoryProp>(FVector(245, 330, 82), FRotator::ZeroRotator, Parameters);
    if (Badge) Badge->Initialize(TEXT("Badge"), TEXT("Badge del nuovo collega"));
    int32 MetaHumans = 0;
    for (const auto& NPC : Colleagues) MetaHumans += NPC->HasMetaHuman() ? 1 : 0;
    UE_LOG(LogTemp, Display, TEXT("CLARA_STORY_CAST_READY: %d colleagues, %d MetaHuman; badge=%s; floor plan unchanged."),
        Colleagues.Num(), MetaHumans, Badge ? TEXT("yes") : TEXT("no"));
}
