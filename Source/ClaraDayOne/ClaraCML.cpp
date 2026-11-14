#include "ClaraPlayer.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include <cstdlib>

namespace
{
bool IsClaraBridgeURL(const FString& URL)
{
    FString Address = URL;
    if (!Address.RemoveFromStart(TEXT("http://"), ESearchCase::CaseSensitive)) return false;
    FString Host, Port;
    if (!Address.Split(TEXT(":"), &Host, &Port)) return false;
    if (Host != TEXT("127.0.0.1") && Host != TEXT("localhost")) return false;
    if (Port.IsEmpty() || Port.Len() > 5) return false;
    for (TCHAR Char : Port) if (Char < TEXT('0') || Char > TEXT('9')) return false;
    const int32 Number = FCString::Atoi(*Port);
    return Number >= 1 && Number <= 65535;
}

FString CMLInputFromCurrentLine(const FString& Line)
{
    // The bridge normally already removes the prompt. Preserve the real trailing completion space.
    for (int32 Index = 0; Index < Line.Len(); ++Index)
    {
        if (FChar::IsWhitespace(Line[Index])) return Line;
        if (Line[Index] == TEXT('#') || Line[Index] == TEXT('>'))
        {
            FString Result = Line.Mid(Index + 1);
            if (Result.StartsWith(TEXT(" "))) Result.RightChopInline(1);
            return Result;
        }
    }
    return Line;
}

FString JSONText(const TSharedPtr<FJsonObject>& Object)
{
    FString Body;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Body));
    return Body;
}

TSharedPtr<FJsonObject> ReadJSON(FHttpResponsePtr Response)
{
    TSharedPtr<FJsonObject> Object;
    if (Response.IsValid() && Response->GetContentLength() <= 1024 * 1024)
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Object);
    return Object;
}

void CloseRemoteConsole(const FString& BaseURL, const FString& ConsoleId)
{
    if (!IsClaraBridgeURL(BaseURL) || ConsoleId.IsEmpty()) return;
    const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BaseURL + TEXT("/console/close"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetTimeout(5.f);
    const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("console_id"), ConsoleId);
    Request->SetContentAsString(JSONText(Body));
    Request->ProcessRequest();
}
}

void AClaraPlayerController::ConfigureCMLBridge()
{
    // Presence, including an invalid empty value, opts in. A broken bridge never selects the local CLI.
    bCMLMode = std::getenv("CLARA_CML_BRIDGE") != nullptr;
    CMLBaseURL = FPlatformMisc::GetEnvironmentVariable(TEXT("CLARA_CML_BRIDGE")).TrimStartAndEnd();
    while (CMLBaseURL.EndsWith(TEXT("/"))) CMLBaseURL.LeftChopInline(1);
}

void AClaraPlayerController::BeginCMLConsole()
{
    CloseCMLConsole();
    bCMLReady = false;
    bCMLConnected = false;
    TerminalOutput = TEXT("[CML] Connessione al bridge locale in corso.\nNessun comando e' stato eseguito. Non viene usato il modello IOS locale.");
    if (!IsClaraBridgeURL(CMLBaseURL))
    {
        CMLFailure(TEXT("CLARA_CML_BRIDGE deve indicare http://127.0.0.1:PORTA o http://localhost:PORTA. Chiudi la console e correggi l'avvio; il modello locale resta disattivato."));
        return;
    }
    if (!GetClaraPlayer() || !GetClaraPlayer()->IsConsoleConnected())
    {
        CMLFailure(TEXT("Collega entrambi i capi del cavo console prima di aprire la sessione reale."));
        return;
    }
    GetWorldTimerManager().SetTimer(CMLPollTimer, this, &AClaraPlayerController::PollCMLConsole, 1.f, true);
    RequestCML(TEXT("GET"), TEXT("/status"), nullptr, ECMLAction::Status);
}

void AClaraPlayerController::CloseCMLConsole()
{
    ++CMLGeneration;
    GetWorldTimerManager().ClearTimer(CMLPollTimer);
    // Let an in-flight open finish so its callback can close an otherwise orphaned remote session.
    CMLRequest.Reset();
    const FString ClosingId = CMLConsoleId;
    CMLConsoleId.Empty();
    bCMLReady = false;
    bCMLConnected = false;
    bCMLRequestInFlight = false;
    bCMLInputPending = false;
    bCMLAwaitingCompletion = false;
    CMLQueuedAction = ECMLAction::None;
    CMLQueuedText.Empty();
    CloseRemoteConsole(CMLBaseURL, ClosingId);
}

bool AClaraPlayerController::CanUseCMLInput() const
{
    const AClaraPlayer* Player = GetClaraPlayer();
    return bCMLMode && bCMLReady && bCMLConnected && !bCMLInputPending
        && !CMLConsoleId.IsEmpty() && Player && Player->IsConsoleConnected();
}

void AClaraPlayerController::PollCMLConsole()
{
    if (!TerminalRoot.IsValid() || !bSwitchTerminal || !bCMLMode || CMLConsoleId.IsEmpty()) return;
    if (!GetClaraPlayer() || !GetClaraPlayer()->IsConsoleConnected())
    {
        CloseCMLConsole();
        CMLFailure(TEXT("Cavo console scollegato: sessione chiusa. Ricollega i capi e riapri la console; nessun nuovo comando viene inoltrato."));
        return;
    }
    if (!bCMLReady || bCMLRequestInFlight || bCMLInputPending) return;
    RequestCML(TEXT("GET"), TEXT("/console/output?console_id=") + FGenericPlatformHttp::UrlEncode(CMLConsoleId) + TEXT("&offset=0"), nullptr, ECMLAction::Poll);
}

void AClaraPlayerController::CMLFailure(const FString& Message)
{
    bCMLReady = false;
    bCMLConnected = false;
    bCMLInputPending = false;
    CMLQueuedAction = ECMLAction::None;
    bCMLAwaitingCompletion = false;
    TerminalOutput = TEXT("[CML · errore di collegamento]\n") + Message
        + TEXT("\nChiudi e riapri la console per riprovare. Nessun risultato locale viene sostituito a quello del laboratorio.");
    if (TerminalScroll.IsValid()) TerminalScroll->ScrollToEnd();
}

void AClaraPlayerController::SendCMLConsoleInput(const FString& Prefix, ECMLAction Action)
{
    if (!CanUseCMLInput())
    {
        if (!GetClaraPlayer() || !GetClaraPlayer()->IsConsoleConnected())
        {
            CloseCMLConsole();
            CMLFailure(TEXT("Cavo console scollegato. Il testo non e' stato inoltrato."));
        }
        return;
    }
    if (Prefix.Len() > 1024)
    {
        TerminalOutput = TEXT("[CML] Riga troppo lunga: nessun testo e' stato inoltrato. Limite dell'interfaccia: 1024 caratteri.");
        return;
    }
    CMLQueuedText = FString(1, TEXT("\x15")) + Prefix;
    if (Action == ECMLAction::Completion) CMLQueuedText += TEXT("\t");
    else if (Action == ECMLAction::Help) CMLQueuedText += TEXT("?");
    else CMLQueuedText += TEXT("\r");
    CMLQueuedAction = Action;
    bCMLInputPending = true;
    if (!bCMLRequestInFlight) FlushCMLInput();
}

void AClaraPlayerController::FlushCMLInput()
{
    if (bCMLRequestInFlight || CMLQueuedAction == ECMLAction::None) return;
    if (!GetClaraPlayer() || !GetClaraPlayer()->IsConsoleConnected())
    {
        CloseCMLConsole();
        CMLFailure(TEXT("Cavo console scollegato prima dell'invio. Nessun nuovo testo inoltrato."));
        return;
    }
    const TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("console_id"), CMLConsoleId);
    Body->SetStringField(TEXT("text"), CMLQueuedText);
    const ECMLAction Action = CMLQueuedAction;
    CMLQueuedAction = ECMLAction::None;
    CMLQueuedText.Empty();
    bCMLAwaitingCompletion = Action == ECMLAction::Completion;
    RequestCML(TEXT("POST"), TEXT("/console/input"), Body, Action);
}

void AClaraPlayerController::RequestCML(const FString& Verb, const FString& Route, const TSharedPtr<FJsonObject>& Body, ECMLAction Action)
{
    if (bCMLRequestInFlight || !TerminalRoot.IsValid() || bEndingPlay) return;
    bCMLRequestInFlight = true;
    const uint32 Generation = CMLGeneration;
    const FString BaseURL = CMLBaseURL;
    const TWeakObjectPtr<AClaraPlayerController> WeakThis(this);
    CMLRequest = FHttpModule::Get().CreateRequest();
    CMLRequest->SetURL(BaseURL + Route);
    CMLRequest->SetVerb(Verb);
    CMLRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
    CMLRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    CMLRequest->SetTimeout(15.f);
    if (Body.IsValid())
    {
        CMLRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        CMLRequest->SetContentAsString(JSONText(Body));
    }
    CMLRequest->OnProcessRequestComplete().BindLambda([WeakThis, Generation, BaseURL, Action](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        const TSharedPtr<FJsonObject> Object = ReadJSON(Response);
        if (!WeakThis.IsValid() || WeakThis->bEndingPlay || WeakThis->CMLGeneration != Generation || !WeakThis->TerminalRoot.IsValid())
        {
            FString StaleId;
            if (Action == ECMLAction::Open && Object.IsValid() && Object->TryGetStringField(TEXT("console_id"), StaleId)) CloseRemoteConsole(BaseURL, StaleId);
            return;
        }
        AClaraPlayerController* Self = WeakThis.Get();
        Self->CMLRequest.Reset();
        Self->bCMLRequestInFlight = false;
        bool bOK = false;
        if (!bSucceeded || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode())
            || !Object.IsValid() || !Object->TryGetBoolField(TEXT("ok"), bOK) || !bOK)
        {
            Self->CMLFailure(TEXT("Il bridge non ha completato la richiesta. Verifica servizio locale, accesso CML e stato del nodo. Le credenziali restano nel bridge."));
            return;
        }
        if (Action == ECMLAction::Status)
        {
            bool bConfigured = false, bConnected = false;
            Object->TryGetBoolField(TEXT("configured"), bConfigured);
            Object->TryGetBoolField(TEXT("connected"), bConnected);
            if (!bConfigured || !bConnected)
            {
                Self->CMLFailure(TEXT("Il bridge e' raggiungibile ma il controller CML non risulta configurato e connesso."));
                return;
            }
            const TSharedPtr<FJsonObject> Open = MakeShared<FJsonObject>();
            Open->SetStringField(TEXT("target"), TEXT("switch"));
            Self->RequestCML(TEXT("POST"), TEXT("/console/open"), Open, ECMLAction::Open);
            return;
        }
        if (Action == ECMLAction::Open)
        {
            if (!Object->TryGetStringField(TEXT("console_id"), Self->CMLConsoleId) || Self->CMLConsoleId.IsEmpty())
            {
                Self->CMLFailure(TEXT("Risposta console priva dell'identificativo di sessione. Nessun testo inoltrato."));
                return;
            }
        }
        if (Action == ECMLAction::Submit || Action == ECMLAction::Completion || Action == ECMLAction::Help) Self->bCMLInputPending = false;
        Self->ApplyCMLSnapshot(Object, Action);
        if (Self->bCMLReady && Self->CMLQueuedAction != ECMLAction::None) Self->FlushCMLInput();
    });
    if (!CMLRequest->ProcessRequest())
    {
        CMLRequest.Reset();
        bCMLRequestInFlight = false;
        CMLFailure(TEXT("Impossibile avviare la richiesta al bridge locale."));
    }
}

void AClaraPlayerController::ApplyCMLSnapshot(const TSharedPtr<FJsonObject>& Snapshot, ECMLAction Action)
{
    FString Screen;
    bool bConnected = false, bClosed = false;
    Snapshot->TryGetBoolField(TEXT("connected"), bConnected);
    Snapshot->TryGetBoolField(TEXT("closed"), bClosed);
    if (!Snapshot->TryGetStringField(TEXT("screen"), Screen) || Screen.Len() > 65536)
    {
        CMLFailure(TEXT("Il bridge non ha restituito una schermata console valida."));
        return;
    }
    // The transcript is always the complete screen rendered from the remote byte stream.
    TerminalOutput = Screen;
    bCMLConnected = bConnected && !bClosed;
    bCMLReady = bCMLConnected;
    if (!bCMLReady)
    {
        TerminalOutput += TEXT("\n\n[CML] Canale remoto chiuso. Riapri la console; il modello locale resta disattivato.");
        bCMLInputPending = false;
        return;
    }
    FString CurrentLine;
    if (bCMLAwaitingCompletion && Snapshot->TryGetStringField(TEXT("current_line"), CurrentLine))
    {
        if (TerminalInput.IsValid())
        {
            bUpdatingTerminalText = true;
            TerminalInput->SetText(FText::FromString(CMLInputFromCurrentLine(CurrentLine)));
            TerminalInput->GoTo(ETextLocation::EndOfDocument);
            bUpdatingTerminalText = false;
        }
        bCMLAwaitingCompletion = false;
    }
    if (TerminalScroll.IsValid()) TerminalScroll->ScrollToEnd();
    if (TerminalInput.IsValid() && Action != ECMLAction::Poll) FSlateApplication::Get().SetKeyboardFocus(TerminalInput);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraCMLLineTest, "Clara.CML.RemoteCompletionLine", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraCMLLineTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Bridge input preserved including completion space"), CMLInputFromCurrentLine(TEXT("configure ")), FString(TEXT("configure ")));
    TestEqual(TEXT("IOS config prompt stripped"), CMLInputFromCurrentLine(TEXT("AULA-01(config)#interface vlan ")), FString(TEXT("interface vlan ")));
    TestEqual(TEXT("User prompt stripped"), CMLInputFromCurrentLine(TEXT("Switch>enable")), FString(TEXT("enable")));
    TestEqual(TEXT("Prompt symbols inside a command are not interpreted"), CMLInputFromCurrentLine(TEXT("show run | include #")), FString(TEXT("show run | include #")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraCMLURLTest, "Clara.CML.LoopbackBridgeOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FClaraCMLURLTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Expected local bridge"), IsClaraBridgeURL(TEXT("http://127.0.0.1:8766")));
    TestTrue(TEXT("Explicit localhost port"), IsClaraBridgeURL(TEXT("http://localhost:8766")));
    for (const TCHAR* Invalid : {TEXT("http://10.208.66.184:8443"), TEXT("http://u:p@localhost:8766"), TEXT("http://localhost:8766/path"), TEXT("http://localhost:0"), TEXT("http://localhost:65536"), TEXT("http://localhost:8766?token=secret"), TEXT("")})
        TestFalse(Invalid, IsClaraBridgeURL(Invalid));
    return true;
}
#endif
