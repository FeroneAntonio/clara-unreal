#include "ClaraGatewayCheck.h"
#include "ClaraGateway.h"
#include "ClaraStoryDirector.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
class FClaraGatewayCheck final : public TSharedFromThis<FClaraGatewayCheck>
{
public:
    TWeakObjectPtr<UWorld> World;
    UClaraGateway* Gateway=nullptr;
    FTSTicker::FDelegateHandle Deadline,IOSRetry;
    TArray<TSharedPtr<FJsonValue>> Steps;
    double Started=0;
    int32 IOSAttempts=0;
    static constexpr int32 MaxIOSAttempts=16;
    static constexpr double TimeoutSeconds=110.;
    bool Done=false,StaticPass=false,VerifySent=false;

    void Start(UWorld* InWorld)
    {
        World=InWorld;Started=FPlatformTime::Seconds();
        Gateway=NewObject<UClaraGateway>();Gateway->AddToRoot();Gateway->Initialize(true);
        StaticPass=UClaraGateway::RunSelfTest();
        const auto Self=AsShared();
        Deadline=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
        {
            if(Self->Done)return false;
            if(FPlatformTime::Seconds()-Self->Started>=TimeoutSeconds){Self->Finish(false,TEXT("Timeout globale: verifica non terminata entro 110 secondi"));return false;}
            if(!Self->World.IsValid()){Self->Finish(false,TEXT("Il mondo Unreal non e' piu' disponibile"));return false;}
            return true;
        }),.25f);
        if(!StaticPass||!Gateway->Enabled()){Finish(false,TEXT("Controlli statici falliti oppure gateway disabilitato"));return;}
        Gateway->Health([Self](const FClaraGatewayResponse& R)
        {
            if(Self->Done)return;
            if(!Self->Record(TEXT("health"),R,R.Success&&R.LiveCML&&!R.Fallback))return;
            Self->Bootstrap();
        });
    }
    bool Record(const FString& Name,const FClaraGatewayResponse& R,bool Pass,bool FinalFailure=true)
    {
        if(Done)return false;
        auto Step=MakeShared<FJsonObject>();Step->SetStringField(TEXT("step"),Name);Step->SetBoolField(TEXT("passed"),Pass);Step->SetBoolField(TEXT("transient_attempt"),!Pass&&!FinalFailure);
        Step->SetNumberField(TEXT("http_status"),R.StatusCode);Step->SetBoolField(TEXT("success"),R.Success);Step->SetBoolField(TEXT("live_cml"),R.LiveCML);Step->SetBoolField(TEXT("fallback"),R.Fallback);
        Step->SetStringField(TEXT("source"),R.Source);Step->SetStringField(TEXT("mode"),R.Mode);Step->SetStringField(TEXT("error"),R.Error);
        if(R.Data.IsValid())Step->SetObjectField(TEXT("response"),R.Data);
        Steps.Add(MakeShared<FJsonValueObject>(Step));
        UE_LOG(LogTemp,Display,TEXT("CLARA_GATEWAY_STEP_%s: %s HTTP=%d source=%s mode=%s"),Pass?TEXT("PASS"):FinalFailure?TEXT("FAIL"):TEXT("WAIT"),*Name,R.StatusCode,*R.Source,*R.Mode);
        if(!Pass&&FinalFailure)Finish(false,Name+TEXT(": la risposta non soddisfa la prova richiesta"));
        return Pass;
    }
    void Bootstrap()
    {
        const auto Self=AsShared();Gateway->Bootstrap([Self](const FClaraGatewayResponse& R)
        {
            if(Self->Done)return;
            if(!Self->Record(TEXT("bootstrap"),R,R.Success&&R.LiveCML&&!R.Fallback))return;
            Self->SyncOffice();
        });
    }
    void SyncOffice()
    {
        const auto Self=AsShared();Gateway->SyncRoom(TEXT("office"),[Self](const FClaraGatewayResponse& R)
        {
            if(Self->Done)return;
            FString Room;if(R.Data.IsValid())R.Data->TryGetStringField(TEXT("room_id"),Room);
            if(!Self->Record(TEXT("room_sync_office"),R,R.Success&&R.LiveCML&&!R.Fallback&&Room==TEXT("office")))return;
            Self->ReadIOS();
        });
    }
    void ReadIOS()
    {
        if(Done)return;
        ++IOSAttempts;
        const auto Self=AsShared();Gateway->Terminal(TEXT("SW-PIANO1"),TEXT("show ip interface brief"),[Self](const FClaraGatewayResponse& R)
        {
            if(Self->Done)return;
            FString Output,Node;if(R.Data.IsValid()){R.Data->TryGetStringField(TEXT("output"),Output);R.Data->TryGetStringField(TEXT("node"),Node);}
            const bool IOS=Output.Contains(TEXT("Interface"))&&Output.Contains(TEXT("Protocol"))&&Output.Contains(TEXT("Vlan99"))&&Output.Contains(TEXT("10.10.99.2"))&&(Output.Contains(TEXT("SW-PIANO1>"))||Output.Contains(TEXT("SW-PIANO1#")))&&!Output.Contains(TEXT("consoles>"))&&!Output.Contains(TEXT("[SIM]"));
            const bool Pass=R.Success&&R.LiveCML&&!R.Fallback&&R.Source==TEXT("live_cml")&&Node==TEXT("SW-PIANO1")&&IOS;
            const FString AttemptName=FString::Printf(TEXT("terminal_SW-PIANO1_show_ip_interface_brief_attempt_%d"),Self->IOSAttempts);
            Self->Record(AttemptName,R,Pass,false);
            if(Pass){Self->ReadSpecs();return;}
            if(Self->IOSAttempts>=MaxIOSAttempts){Self->Finish(false,TEXT("16 tentativi di lettura IOS senza tabella Interface/Protocol, Vlan99 e IP attesi"));return;}
            // Room activation may return before IOS boot completes. Retry only this read, never bootstrap or sync again.
            Self->IOSRetry=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
            {if(!Self->Done)Self->ReadIOS();return false;}),5.f);
        });
    }
    void ReadSpecs()
    {
        const auto Self=AsShared();Gateway->Specs([Self](const FClaraGatewayResponse& R)
        {
            if(Self->Done)return;
            bool TerminalFound=false;const TArray<TSharedPtr<FJsonValue>>* Missions=nullptr;
            if(R.Data.IsValid()&&R.Data->TryGetArrayField(TEXT("missions"),Missions)&&Missions)
                for(const auto& V:*Missions)
                {
                    const TSharedPtr<FJsonObject>* Mission=nullptr;
                    if(V.IsValid()&&V->TryGetObject(Mission)&&Mission&&Mission->IsValid())
                    {FString Id;(*Mission)->TryGetStringField(TEXT("mission_id"),Id);TerminalFound|=Id==TEXT("terminal");}
                }
            if(!Self->Record(TEXT("mission_specs"),R,R.Success&&!R.Fallback&&TerminalFound))return;
            Self->VerifyEmptySubmission();
        });
    }
    void VerifyEmptySubmission()
    {
        const auto Self=AsShared();// The deployed endpoint replaces [] with global telemetry. A read-only command is an explicit zero-configuration submission.
        const TArray<FString> EmptyCommands={TEXT("show ip interface brief")};
        auto State=MakeShared<FJsonObject>();State->SetStringField(TEXT("session_id"),TEXT("unreal-gateway-integration-check"));State->SetStringField(TEXT("room_id"),TEXT("office"));State->SetStringField(TEXT("node"),TEXT("SW-PIANO1"));
        State->SetObjectField(TEXT("patched_ports"),MakeShared<FJsonObject>());State->SetArrayField(TEXT("blocked_domains"),TArray<TSharedPtr<FJsonValue>>());
        VerifySent=true;
        Gateway->Verify(TEXT("terminal"),EmptyCommands,State,[Self](const FClaraGatewayResponse& R)
        {
            if(Self->Done)return;
            bool Passed=true;const bool HasPassed=R.Data.IsValid()&&R.Data->TryGetBoolField(TEXT("passed"),Passed);
            const bool NetAgent=R.Source.StartsWith(TEXT("netagent"));
            if(!Self->Record(TEXT("verify_terminal_zero_configuration_submission_must_fail"),R,R.Success&&!R.Fallback&&HasPassed&&!Passed&&NetAgent))return;
            Self->Finish(true,TEXT("HTTP Unreal: CML live, console IOS reale e verifica netAgent negativa su consegna senza configurazione"));
        });
    }
    void Finish(bool Pass,const FString& Reason)
    {
        if(Done)return;Done=true;FTSTicker::RemoveTicker(Deadline);FTSTicker::RemoveTicker(IOSRetry);
        auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("check"),TEXT("ClaraGatewayCheck"));Report->SetBoolField(TEXT("passed"),Pass);Report->SetBoolField(TEXT("static_checks_passed"),StaticPass);
        Report->SetStringField(TEXT("reason"),Reason);Report->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());Report->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);
        Report->SetNumberField(TEXT("ios_attempt_count"),IOSAttempts);Report->SetNumberField(TEXT("ios_max_attempts"),MaxIOSAttempts);Report->SetNumberField(TEXT("ios_retry_interval_seconds"),5);Report->SetNumberField(TEXT("global_timeout_seconds"),TimeoutSeconds);
        Report->SetStringField(TEXT("base_url"),Gateway?Gateway->GetBaseURL():FString());Report->SetArrayField(TEXT("steps"),Steps);
        if(VerifySent)
        {
            auto Submitted=MakeShared<FJsonObject>();Submitted->SetStringField(TEXT("mission_id"),TEXT("terminal"));
            TArray<TSharedPtr<FJsonValue>> Commands;Commands.Add(MakeShared<FJsonValueString>(TEXT("show ip interface brief")));Submitted->SetArrayField(TEXT("player_commands"),Commands);
            Submitted->SetObjectField(TEXT("patched_ports"),MakeShared<FJsonObject>());Submitted->SetArrayField(TEXT("blocked_domains"),TArray<TSharedPtr<FJsonValue>>());Report->SetObjectField(TEXT("verification_submission"),Submitted);
        }
        Report->SetStringField(TEXT("scope"),TEXT("Only health, bootstrap, office room sync, up to 16 attempts of the same read-only IOS command (5-second asynchronous retry), mission list and explicit zero-configuration submission verification. No patch, device configuration, sinkhole, AI or mission deployment."));
        Report->SetStringField(TEXT("verification_limit"),TEXT("The terminal response proves a live IOS read. The verifier receives only this session's read-only show command, explicit empty patched_ports and blocked_domains; it must reject this zero-configuration submission. A literally empty command list would trigger the deployed backend's global-telemetry fallback. Verification is not an independent running-config audit."));
        FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));
        const FString Path=FPaths::ProjectSavedDir()/TEXT("Logs/GatewayIntegrationReport.json");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
        const bool Written=FFileHelper::SaveStringToFile(JSON,*Path);
        if(Gateway){Gateway->Shutdown();Gateway->RemoveFromRoot();Gateway=nullptr;}
        UE_LOG(LogTemp,Display,TEXT("CLARA_GATEWAY_LIVE_%s: %s; report=%s"),(Pass&&Written)?TEXT("PASS"):TEXT("FAIL"),*Reason,*Path);
        FPlatformMisc::RequestExitWithStatus(false,Pass&&Written?0:1);
    }
};
}

void RunClaraGatewayCheck(UWorld* World)
{
    if(!World){UE_LOG(LogTemp,Error,TEXT("CLARA_GATEWAY_LIVE_FAIL: no Unreal world"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
    MakeShared<FClaraGatewayCheck>()->Start(World);
}

void RunClaraGatewayPortCheck(UWorld* World,AClaraStoryDirector* Story)
{
    struct FState {double Start=0,Retry=0;int32 Attempts=0;bool Waiting=false;};
    auto State=MakeShared<FState>();State->Start=FPlatformTime::Seconds();
    TWeakObjectPtr<AClaraStoryDirector> Weak(Story);
    const bool StaticPass=AClaraStoryDirector::RunGatewayPortRepairSelfTest();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,Weak,StaticPass](float)
    {
        const double Now=FPlatformTime::Seconds();
        bool Done=false,Pass=false;
        if(!StaticPass||!Weak.IsValid()||Now-State->Start>130)Done=true;
        else
        {
            auto* D=Weak.Get();
            if(State->Waiting&&!D->IsGatewayPortRepairPending())
            {
                if(D->IsGatewayPortConfirmed(TEXT("sw-1"),true))Done=Pass=true;
                else if(State->Attempts>=5)Done=true;
                else {State->Waiting=false;State->Retry=Now+5;D->GatewayRefresh();}
            }
            if(!Done&&!State->Waiting&&Now>=State->Retry&&D->IsGatewayLive())
            {++State->Attempts;State->Waiting=true;D->GatewayPatchPort(TEXT("sw-1"),true);}
        }
        if(!Done)return true;
        auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Pass);Report->SetNumberField(TEXT("attempts"),State->Attempts);
        Report->SetStringField(TEXT("scope"),TEXT("Idempotent enable of office Ethernet0/1 via game gateway adapter; no shutdown or topology rewiring."));
        if(Weak.IsValid()){Report->SetStringField(TEXT("proof_method"),Weak->GetGatewayPortProofMethod(TEXT("sw-1")));Report->SetStringField(TEXT("evidence"),Weak->GetGatewayResult());Weak->GatewayShutdown();}
        FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));
        FFileHelper::SaveStringToFile(JSON,*(FPaths::ProjectSavedDir()/TEXT("Logs/GatewayPortRepairReport.json")));
        UE_LOG(LogTemp,Display,TEXT("CLARA_GATEWAY_PORT_LIVE_%s: real office Ethernet0/1; attempts=%d"),Pass?TEXT("PASS"):TEXT("FAIL"),State->Attempts);
        FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);return false;
    }),.25f);
}
