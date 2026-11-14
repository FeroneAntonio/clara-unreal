#include "ClaraGateway.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "Async/Async.h"
#include "Containers/StringConv.h"

namespace
{
constexpr int32 MaxJSONBytes=1024*1024;
TSharedPtr<FJsonObject> Object(){return MakeShared<FJsonObject>();}
FClaraGatewayResponse Failure(const FString& Message){FClaraGatewayResponse R;R.Error=Message;return R;}
void Provenance(const TSharedPtr<FJsonObject>& Data,bool& Fallback,int32 Depth=0)
{
    if(!Data.IsValid()||Depth>8)return;
    for(const auto& Pair:Data->Values)
    {
        if(Pair.Value->Type==EJson::Object)Provenance(Pair.Value->AsObject(),Fallback,Depth+1);
        else if(Pair.Value->Type==EJson::Array)
        {
            for(const auto& Item:Pair.Value->AsArray())if(Item->Type==EJson::Object)Provenance(Item->AsObject(),Fallback,Depth+1);
        }
        else if(Pair.Value->Type==EJson::String&&(Pair.Key==TEXT("mode")||Pair.Key==TEXT("source")))
        {
            const FString Value=Pair.Value->AsString().ToLower();
            Fallback|=Value.Contains(TEXT("fallback"))||Value.Contains(TEXT("mock"))||Value.Contains(TEXT("simulat"));
        }
        if((Pair.Key==TEXT("fallback")||Pair.Key==TEXT("simulated")||Pair.Key==TEXT("is_mock"))&&Pair.Value->Type==EJson::Boolean)
            Fallback|=Pair.Value->AsBool();
    }
}
}
bool UClaraGateway::ValidateBaseURL(const FString& URL)
{
    if(!URL.StartsWith(TEXT("https://"),ESearchCase::IgnoreCase)||URL.Len()>2048)return false;
    FString Host=URL.Mid(8);if(Host.EndsWith(TEXT("/")))Host.LeftChopInline(1);
    if(Host.IsEmpty()||Host.Contains(TEXT("/"))||Host.Contains(TEXT("@"))||Host.Contains(TEXT("?"))||Host.Contains(TEXT("#")))return false;
    FString Port;
    if(Host.Split(TEXT(":"),&Host,&Port,ESearchCase::CaseSensitive,ESearchDir::FromEnd))
    {
        if(Port.IsEmpty()||!Port.IsNumeric()||FCString::Atoi(*Port)<1||FCString::Atoi(*Port)>65535)return false;
    }
    if(Host.IsEmpty()||Host.StartsWith(TEXT("."))||Host.EndsWith(TEXT("."))||Host.Contains(TEXT("..")))return false;
    for(TCHAR C:Host)if(!FChar::IsAlnum(C)&&C!=TEXT('-')&&C!=TEXT('.'))return false;
    return true;
}
void UClaraGateway::Initialize(bool AllowNetwork)
{
    Shutdown();
    FString Config=FPlatformMisc::GetEnvironmentVariable(TEXT("CML_GATEWAY_URL"));Config.TrimStartAndEndInline();
    BaseURL=Config.IsEmpty()?TEXT("https://api-cml.kappino.me"):Config;
    while(BaseURL.EndsWith(TEXT("/")))BaseURL.LeftChopInline(1);
    const bool Local=FParse::Param(FCommandLine::Get(),TEXT("ClaraLocalLab"));
    const bool Check=FParse::Param(FCommandLine::Get(),TEXT("ClaraGatewayCheck"))||FParse::Param(FCommandLine::Get(),TEXT("ClaraGatewayPortCheck"));
    bEnabled=AllowNetwork&&!Local&&(!FApp::IsUnattended()||Check)&&ValidateBaseURL(BaseURL);
    LastError=!ValidateBaseURL(BaseURL)?TEXT("Indirizzo gateway HTTPS non valido"):FString();
}
FString UClaraGateway::GetStatusLabel() const
{
    if(!bEnabled)return LastError.IsEmpty()?TEXT("Laboratorio locale · gateway disattivato"):LastError;
    if(bDegraded)return TEXT("Gateway degradato · ")+(LastError.IsEmpty()?TEXT("risposta di ripiego dichiarata"):LastError);
    if(IsLive())return TEXT("CML live · confermato dal gateway");
    return TEXT("Gateway attivo · CML da verificare");
}
FClaraGatewayResponse UClaraGateway::ParseResponse(int32 HTTPStatus,const FString& Content,bool TransportOK)
{
    FClaraGatewayResponse R;R.StatusCode=HTTPStatus;
    if(!TransportOK){R.Error=TEXT("Connessione gateway non riuscita");return R;}
    if(HTTPStatus<200||HTTPStatus>=300){R.Error=FString::Printf(TEXT("Gateway HTTP %d"),HTTPStatus);return R;}
    if(FTCHARToUTF8(*Content).Length()>MaxJSONBytes){R.Error=TEXT("Risposta gateway troppo grande");return R;}
    const auto Reader=TJsonReaderFactory<>::Create(Content);
    if(!FJsonSerializer::Deserialize(Reader,R.Data)||!R.Data.IsValid()){R.Error=TEXT("Risposta gateway JSON non valida");return R;}
    R.Data->TryGetStringField(TEXT("source"),R.Source);R.Data->TryGetStringField(TEXT("mode"),R.Mode);
    R.Success=true;
    bool Flag;
    if((R.Data->TryGetBoolField(TEXT("ok"),Flag)&&!Flag)||(R.Data->TryGetBoolField(TEXT("success"),Flag)&&!Flag))
    {R.Success=false;R.Error=TEXT("Il gateway ha segnalato un errore");}
    FString Error;if(R.Data->TryGetStringField(TEXT("error"),Error)&&!Error.IsEmpty()){R.Success=false;R.Error=Error.Left(512);}
    Provenance(R.Data,R.Fallback);
    R.LiveCML=R.Source==TEXT("live_cml")||R.Mode==TEXT("live_cml");
    const TSharedPtr<FJsonObject>* CML=nullptr;
    if(R.Data->TryGetObjectField(TEXT("cml"),CML)&&CML&&CML->IsValid())
    {
        FString Mode,Status;(*CML)->TryGetStringField(TEXT("mode"),Mode);(*CML)->TryGetStringField(TEXT("status"),Status);
        bool Found=true;(*CML)->TryGetBoolField(TEXT("lab_found"),Found);
        R.LiveCML=Mode==TEXT("live_cml")&&Status==TEXT("online")&&Found;
        if(R.Mode.IsEmpty())R.Mode=Mode;
    }
    FString Output;
    if(R.Data->TryGetStringField(TEXT("output"),Output)&&(Output.Contains(TEXT("consoles>"),ESearchCase::IgnoreCase)||Output.Contains(TEXT("consoles#"),ESearchCase::IgnoreCase)))
    {
        R.Success=false;
        R.Error=TEXT("Console TermServ non agganciata: il gateway ha restituito il prompt consoles, non la console IOS del nodo");
    }
    if(!Output.IsEmpty()&&Output.TrimStartAndEnd().EndsWith(TEXT("# (OK)")))
    {R.Success=false;R.Error=TEXT("Il gateway non ha restituito output IOS: il segnaposto (OK) non prova l'esecuzione del comando. Il nodo potrebbe essere ancora in avvio.");}
    R.LiveCML&=R.Success&&!R.Fallback;
    return R;
}
void UClaraGateway::Request(const FString& Method,const FString& Path,const TSharedPtr<FJsonObject>& Body,FClaraGatewayCallback Callback,float Timeout)
{
    if(!ensure(IsInGameThread()))return;
    if(!bEnabled){if(Callback)Callback(Failure(GetStatusLabel()));return;}
    if((Method!=TEXT("GET")&&Method!=TEXT("POST"))||!Path.StartsWith(TEXT("/api/"))||Path.Contains(TEXT(".."))||Path.Contains(TEXT("?"))||Path.Contains(TEXT("#"))||Path.Contains(TEXT("\\"))||Path.Contains(TEXT("\r"))||Path.Contains(TEXT("\n")))
    {if(Callback)Callback(Failure(TEXT("Richiesta gateway non valida")));return;}
    if(Queue.Num()>=64){if(Callback)Callback(Failure(TEXT("Troppe richieste gateway in attesa")));return;}
    auto Pending=MakeShared<FPending>();Pending->Method=Method;Pending->Path=Path;Pending->Timeout=FMath::Clamp(Timeout,1.f,90.f);Pending->Callback=MoveTemp(Callback);
    if(Body.IsValid()){auto Writer=TJsonWriterFactory<>::Create(&Pending->Payload);FJsonSerializer::Serialize(Body.ToSharedRef(),Writer);}
    if(FTCHARToUTF8(*Pending->Payload).Length()>MaxJSONBytes){if(Pending->Callback)Pending->Callback(Failure(TEXT("Richiesta gateway troppo grande")));return;}
    if(Path==TEXT("/api/cml/room-sync"))
    {
        Pending->RoomRevision=++RoomRevision;
        TArray<TSharedPtr<FPending>> Superseded;
        for(int32 I=Queue.Num()-1;I>=0;--I)if(Queue[I]->RoomRevision){Superseded.Add(Queue[I]);Queue.RemoveAt(I);}
        // An active sync finishes before the newest one; cancelling HTTP cannot undo a server mutation.
        Queue.Add(Pending);
        for(const auto& Old:Superseded)if(Old->Callback){auto R=Failure(TEXT("Cambio stanza superato da una richiesta piu' recente"));R.Superseded=true;Old->Callback(R);}
        Pump();return;
    }
    Enqueue(Pending);
}
void UClaraGateway::Enqueue(const TSharedPtr<FPending>& Pending){Queue.Add(Pending);Pump();}
void UClaraGateway::Pump()
{
    if(!bEnabled||Active.IsValid()||Queue.IsEmpty())return;
    Active=Queue[0];Queue.RemoveAt(0);
    auto Pending=Active;
    auto HTTP=FHttpModule::Get().CreateRequest();ActiveHTTP=HTTP;
    HTTP->SetURL(BaseURL+Pending->Path);HTTP->SetVerb(Pending->Method);HTTP->SetTimeout(Pending->Timeout);
    HTTP->SetHeader(TEXT("Accept"),TEXT("application/json"));
    HTTP->SetHeader(TEXT("User-Agent"),TEXT("ClaraDayOne/1.0"));
    if(Pending->Method==TEXT("POST")){HTTP->SetHeader(TEXT("Content-Type"),TEXT("application/json"));HTTP->SetContentAsString(Pending->Payload.IsEmpty()?TEXT("{}"):Pending->Payload);}
    const uint64 Epoch=Generation;TWeakObjectPtr<UClaraGateway> Weak(this);
    HTTP->OnProcessRequestComplete().BindLambda([Weak,Pending,Epoch](FHttpRequestPtr,FHttpResponsePtr Response,bool OK)
    {
        FClaraGatewayResponse Result;
        if(Response.IsValid()&&Response->GetContent().Num()>MaxJSONBytes)Result=Failure(TEXT("Risposta gateway troppo grande"));
        else Result=ParseResponse(Response.IsValid()?Response->GetResponseCode():0,Response.IsValid()?Response->GetContentAsString():FString(),OK);
        AsyncTask(ENamedThreads::GameThread,[Weak,Pending,Epoch,Result]()mutable{if(Weak.IsValid())Weak->Finish(Pending,MoveTemp(Result),Epoch);});
    });
    if(!HTTP->ProcessRequest())Finish(Pending,Failure(TEXT("Impossibile avviare la richiesta gateway")),Epoch);
}
void UClaraGateway::Observe(const FString& Path,const FClaraGatewayResponse& R)
{
    if(Path==TEXT("/api/health"))bHealthLive=R.LiveCML;
    const bool CMLRequest=Path==TEXT("/api/health")||Path.StartsWith(TEXT("/api/cml/"))||Path==TEXT("/api/rack/patch")||Path==TEXT("/api/terminal/command");
    if(CMLRequest)bLatestLive=R.LiveCML;
    if(!R.Success||R.Fallback){bDegraded=true;LastError=R.Error.IsEmpty()?TEXT("risposta di ripiego dichiarata dal gateway"):R.Error;}
    else if(R.LiveCML){bDegraded=false;LastError.Empty();}
}
void UClaraGateway::Finish(const TSharedPtr<FPending>& Pending,FClaraGatewayResponse R,uint64 Epoch)
{
    if(Epoch!=Generation||Active!=Pending)return;
    Active.Reset();ActiveHTTP.Reset();
    if(Pending->RoomRevision&&Pending->RoomRevision!=RoomRevision){R.Superseded=true;R.Success=false;R.LiveCML=false;R.Error=TEXT("Risposta stanza superata");}
    if(!R.Superseded)Observe(Pending->Path,R);
    if(Pending->Callback)Pending->Callback(R);
    Pump();
}
void UClaraGateway::CancelAll()
{
    ++Generation;
    TArray<TSharedPtr<FPending>> Cancelled=MoveTemp(Queue);Queue.Empty();
    if(Active.IsValid())Cancelled.Insert(Active,0);Active.Reset();
    if(ActiveHTTP.IsValid()){ActiveHTTP->OnProcessRequestComplete().Unbind();ActiveHTTP->CancelRequest();ActiveHTTP.Reset();}
    for(const auto& Pending:Cancelled)if(Pending->Callback)Pending->Callback(Failure(TEXT("Richiesta gateway annullata")));
}
void UClaraGateway::Shutdown(){bEnabled=false;bHealthLive=false;bLatestLive=false;bDegraded=false;CancelAll();}
void UClaraGateway::BeginDestroy(){Shutdown();Super::BeginDestroy();}
void UClaraGateway::Health(FClaraGatewayCallback CB){Request(TEXT("GET"),TEXT("/api/health"),nullptr,MoveTemp(CB));}
void UClaraGateway::Bootstrap(FClaraGatewayCallback CB){Request(TEXT("POST"),TEXT("/api/cml/bootstrap-demo"),nullptr,MoveTemp(CB),30);}
void UClaraGateway::SyncRoom(const FString& Id,FClaraGatewayCallback CB){auto B=Object();B->SetStringField(TEXT("room_id"),Id);Request(TEXT("POST"),TEXT("/api/cml/room-sync"),B,MoveTemp(CB),30);}
void UClaraGateway::Patch(const FString& Port,const FString& Cable,bool Connected,FClaraGatewayCallback CB){auto B=Object();B->SetStringField(TEXT("port_id"),Port);if(Cable.IsEmpty())B->SetField(TEXT("cable_id"),MakeShared<FJsonValueNull>());else B->SetStringField(TEXT("cable_id"),Cable);B->SetBoolField(TEXT("connected"),Connected);Request(TEXT("POST"),TEXT("/api/rack/patch"),B,MoveTemp(CB));}
void UClaraGateway::Terminal(const FString& Node,const FString& Command,FClaraGatewayCallback CB){auto B=Object();B->SetStringField(TEXT("node"),Node);B->SetStringField(TEXT("command"),Command);Request(TEXT("POST"),TEXT("/api/terminal/command"),B,MoveTemp(CB));}
void UClaraGateway::Specs(FClaraGatewayCallback CB){Request(TEXT("GET"),TEXT("/api/mission/specs"),nullptr,MoveTemp(CB));}
void UClaraGateway::Verify(const FString& Id,const TArray<FString>& Commands,const TSharedPtr<FJsonObject>& State,FClaraGatewayCallback CB)
{
    auto B=Object();B->SetStringField(TEXT("mission_id"),Id);TArray<TSharedPtr<FJsonValue>> Lines;for(const auto& C:Commands)Lines.Add(MakeShared<FJsonValueString>(C));if(Lines.IsEmpty())Lines.Add(MakeShared<FJsonValueString>(TEXT("! no commands submitted by this game session")));B->SetArrayField(TEXT("player_commands"),Lines);B->SetObjectField(TEXT("player_state"),State.IsValid()?State:Object());Request(TEXT("POST"),TEXT("/api/mission/verify"),B,MoveTemp(CB));
}
void UClaraGateway::AI(const FString& Mode,const FString& NPC,const FString& Prompt,const FString& Minigame,const FString& Context,FClaraGatewayCallback CB)
{
    auto B=Object();B->SetStringField(TEXT("mode"),Mode);B->SetStringField(TEXT("npc_id"),NPC);B->SetStringField(TEXT("prompt"),Prompt);B->SetStringField(TEXT("minigame"),Minigame);B->SetStringField(TEXT("context"),Context);Request(TEXT("POST"),TEXT("/api/ai/interact"),B,MoveTemp(CB),30);
}
void UClaraGateway::Sinkhole(const FString& Domain,bool Block,FClaraGatewayCallback CB){auto B=Object();B->SetStringField(TEXT("domain"),Domain);B->SetStringField(TEXT("action"),Block?TEXT("block"):TEXT("unblock"));Request(TEXT("POST"),TEXT("/api/soc/sinkhole"),B,MoveTemp(CB));}
void UClaraGateway::Generate(const FString& Prompt,bool Deploy,FClaraGatewayCallback CB){auto B=Object();B->SetStringField(TEXT("prompt"),Prompt);B->SetBoolField(TEXT("deploy_to_cml"),Deploy);Request(TEXT("POST"),TEXT("/api/mission/generate"),B,MoveTemp(CB),90);}
bool UClaraGateway::RunSelfTest()
{
    bool OK=ValidateBaseURL(TEXT("https://api-cml.kappino.me"))&&ValidateBaseURL(TEXT("https://example.test:8443/"));
    for(const FString& URL:{TEXT("http://example.test"),TEXT("https://user:pass@example.test"),TEXT("https://example.test/api"),TEXT("https://example.test?key=x"),TEXT("https://example.test:99999"),TEXT("https://example.test\n")})OK&=!ValidateBaseURL(URL);
    const auto Live=ParseResponse(200,TEXT("{\"source\":\"live_cml\",\"output\":\"Switch>\"}"),true);
    const auto Health=ParseResponse(200,TEXT("{\"ok\":true,\"cml\":{\"mode\":\"live_cml\",\"status\":\"online\",\"lab_found\":true}}"),true);
    const auto Fallback=ParseResponse(200,TEXT("{\"source\":\"live_cml\",\"fallback\":true}"),true);
    const auto Mock=ParseResponse(200,TEXT("{\"source\":\"mock_fallback\"}"),true);
    OK&=Live.Success&&Live.LiveCML&&Health.LiveCML&&Fallback.Fallback&&!Fallback.LiveCML&&Mock.Fallback&&!Mock.LiveCML;
    const auto TermServ=ParseResponse(200,TEXT("{\"source\":\"live_cml\",\"output\":\"unknown command, type 'help' for help\\nconsoles>\"}"),true);
    OK&=!ParseResponse(200,TEXT("{\"source\":\"live_cml\",\"output\":\"SW-PIANO1# (OK)\"}"),true).LiveCML;
    OK&=!TermServ.Success&&!TermServ.LiveCML&&!TermServ.Fallback&&TermServ.Error.Contains(TEXT("TermServ"));
    OK&=!ParseResponse(200,TEXT("not json"),true).Success&&!ParseResponse(503,TEXT("{}"),true).Success&&!ParseResponse(200,TEXT("{}"),false).Success;
    OK&=!ParseResponse(200,TEXT("{\"ok\":false,\"mode\":\"live_cml\"}"),true).LiveCML;
    UE_LOG(LogTemp,Display,TEXT("CLARA_GATEWAY_SELFTEST_%s: HTTPS URLs, JSON/HTTP failures, live provenance and fallback rejection"),OK?TEXT("PASS"):TEXT("FAIL"));return OK;
}
