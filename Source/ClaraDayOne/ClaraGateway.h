#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "ClaraGateway.generated.h"

struct FClaraGatewayResponse
{
    bool Success=false;
    bool LiveCML=false;
    bool Fallback=false;
    bool Superseded=false;
    int32 StatusCode=0;
    FString Source,Mode,Error;
    TSharedPtr<FJsonObject> Data;
};
using FClaraGatewayCallback=TFunction<void(const FClaraGatewayResponse&)>;

/** Serial, explicit HTTP connection to the deployed CLARA gateway. No simulated fallback. */
UCLASS()
class CLARADAYONE_API UClaraGateway : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(bool AllowNetwork);
    void Shutdown();
    void CancelAll();
    bool Enabled() const {return bEnabled;}
    bool IsLive() const {return bEnabled&&bHealthLive&&bLatestLive&&!bDegraded;}
    FString GetStatusLabel() const;
    FString GetBaseURL() const {return BaseURL;}
    void Request(const FString& Method,const FString& Path,const TSharedPtr<FJsonObject>& Body,FClaraGatewayCallback Callback,float Timeout=10.f);
    void Health(FClaraGatewayCallback Callback);
    void Bootstrap(FClaraGatewayCallback Callback);
    void SyncRoom(const FString& RoomId,FClaraGatewayCallback Callback);
    void Patch(const FString& PortId,const FString& CableId,bool Connected,FClaraGatewayCallback Callback);
    void Terminal(const FString& Node,const FString& Command,FClaraGatewayCallback Callback);
    void Specs(FClaraGatewayCallback Callback);
    void Verify(const FString& MissionId,const TArray<FString>& Commands,const TSharedPtr<FJsonObject>& State,FClaraGatewayCallback Callback);
    void AI(const FString& Mode,const FString& NpcId,const FString& Prompt,const FString& Minigame,const FString& Context,FClaraGatewayCallback Callback);
    void Sinkhole(const FString& Domain,bool Block,FClaraGatewayCallback Callback);
    void Generate(const FString& Prompt,bool DeployToCML,FClaraGatewayCallback Callback);
    static bool ValidateBaseURL(const FString& URL);
    static FClaraGatewayResponse ParseResponse(int32 HTTPStatus,const FString& Content,bool TransportOK);
    static bool RunSelfTest();
protected:
    virtual void BeginDestroy() override;
private:
    struct FPending
    {
        FString Method,Path,Payload;
        float Timeout=10.f;
        uint64 RoomRevision=0;
        FClaraGatewayCallback Callback;
    };
    FString BaseURL=TEXT("https://api-cml.kappino.me"),LastError;
    bool bEnabled=false,bHealthLive=false,bLatestLive=false,bDegraded=false;
    uint64 Generation=0,RoomRevision=0;
    TArray<TSharedPtr<FPending>> Queue;
    TSharedPtr<FPending> Active;
    FHttpRequestPtr ActiveHTTP;
    void Pump();
    void Enqueue(const TSharedPtr<FPending>& Pending);
    void Finish(const TSharedPtr<FPending>& Pending,FClaraGatewayResponse Response,uint64 RequestGeneration);
    void Observe(const FString& Path,const FClaraGatewayResponse& Response);
};
