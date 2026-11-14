#include "ClaraLayoutNavigation.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"

bool CheckClaraWalkableRoutes(UWorld* World,APawn* Visitor)
{
    if(!World || !Visitor)return false;
    constexpr int32 NX=76,NY=58;
    constexpr float Step=25.f;
    FCollisionQueryParams Params;Params.AddIgnoredActor(Visitor);
    const FCollisionShape Capsule=FCollisionShape::MakeCapsule(28,88);
    auto Point=[](int32 I){return FVector((I%NX+.5f)*Step,(I/NX+.5f)*Step,90);};
    auto Cell=[](FVector P){return FMath::Clamp(FMath::FloorToInt(P.Y/Step),0,NY-1)*NX+FMath::Clamp(FMath::FloorToInt(P.X/Step),0,NX-1);};
    TArray<uint8> Free;Free.SetNumZeroed(NX*NY);
    for(int32 I=0;I<Free.Num();++I)Free[I]=!World->OverlapBlockingTestByChannel(Point(I),FQuat::Identity,ECC_Pawn,Capsule,Params);
    const int32 Start=Cell({175,80,90});
    if(!Free[Start]){UE_LOG(LogTemp,Error,TEXT("CLARA_ROUTES_FAIL: entry spawn is blocked"));return false;}
    TArray<uint8> Reached;Reached.SetNumZeroed(NX*NY);Reached[Start]=1;
    TArray<int32> Queue;Queue.Add(Start);
    for(int32 Head=0;Head<Queue.Num();++Head)
    {
        const int32 Current=Queue[Head],X=Current%NX,Y=Current/NX;
        for(FIntPoint D : {FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
        {
            const int32 XX=X+D.X,YY=Y+D.Y;
            if(XX<0 || XX>=NX || YY<0 || YY>=NY)continue;
            const int32 Next=YY*NX+XX;
            if(!Free[Next] || Reached[Next])continue;
            FHitResult Hit;
            if(World->SweepSingleByChannel(Hit,Point(Current),Point(Next),FQuat::Identity,ECC_Pawn,Capsule,Params))continue;
            Reached[Next]=1;Queue.Add(Next);
        }
    }
    const FVector Destinations[]={{175,100,90},{175,650,90},{470,1000,90},{650,960,90},{945,935,90},{630,1300,90},{1600,900,90},{1600,1020,90},{1180,160,90},{1180,450,90}};
    const TCHAR* Labels[]={TEXT("Reception"),TEXT("Corridoio"),TEXT("Lounge"),TEXT("Ingresso reti"),TEXT("Banco reti"),TEXT("Postazioni reti"),TEXT("Ingresso apparati"),TEXT("Rack"),TEXT("Cyber"),TEXT("Postazioni cyber")};
    bool Good=true;
    for(int32 I=0;I<UE_ARRAY_COUNT(Destinations);++I)
    {
        const bool OK=Reached[Cell(Destinations[I])]!=0;
        UE_LOG(LogTemp,Display,TEXT("CLARA_ROUTE_%s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),Labels[I]);
        Good &= OK;
    }
    UE_LOG(LogTemp,Display,TEXT("CLARA_ROUTES_%s: %d reachable 25cm cells, capsule diameter 56cm, 10 destinations."),Good?TEXT("PASS"):TEXT("FAIL"),Queue.Num());
    return Good;
}
