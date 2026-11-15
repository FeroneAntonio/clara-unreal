#include "ClaraStoryDirector.h"
#include "ClaraAudio.h"
#include "ClaraNetworkDevice.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"

void AClaraStoryDirector::AppendTerminalOutput(const FString& Session,const FString& Text)
{
    FString& Output=Session==TerminalTarget?Transcript:TerminalTranscripts.FindOrAdd(Session);
    Output+=Text;if(Output.Len()>30000)Output.RightChopInline(Output.Len()-24000);
}
void AClaraStoryDirector::OfferTerminalTutor(bool Unknown)
{
    const float Now=GetWorld()->GetTimeSeconds();if(Now<TerminalTutorAt+12.f||Page==TEXT("dialogue"))return;TerminalTutorAt=Now;
    const FString Line=Unknown?TEXT("Questo comando non è disponibile nel laboratorio. Guarda il manuale e l'obiettivo."):
        TEXT("Questo comando ora non ci serve. Torniamo alla prova della rete.");
    Notify(Line+TEXT("\nObiettivo: ")+GetObjective());
    if(!SelfTesting)if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(ScreenPage==TEXT("cyber")?TEXT("Jonny"):TEXT("Luca"),Line);
}
FString AClaraStoryDirector::RunCommand(const FString& Input)
{
    if(TerminalTarget==TEXT("CML"))return RunGatewayCommand(Input);
    if(IsTerminalBusy()){Notify(TEXT("Ping in corso. Ctrl+C interrompe questa sessione."));return FString();}
    const bool Secret=IsTerminalPasswordInput(),Awaiting=IsTerminalAwaitingInput();const FString Command=Input.TrimStartAndEnd();
    if(!Secret&&!Awaiting&&!Command.IsEmpty())
    {
        auto& History=TerminalHistories.FindOrAdd(TerminalTarget);if(History.IsEmpty()||History.Last()!=Command)History.Add(Command);
        if(History.Num()>200)History.RemoveAt(0,History.Num()-200);
    }
    FString Preview;
    if(!Secret&&!Awaiting&&TryPredictTerminalPing(Command,Preview))
    {
        TArray<FString> Words;Command.ParseIntoArrayWS(Words);FPendingPing Job;Job.Command=Command;Job.Address=Words.Last();Job.StartedPhase=Phase;
        Job.Reachable=Preview.Contains(TEXT("Reply from"))||Preview.Contains(TEXT("100 percent"));Job.Interval=Job.Reachable?1.f:4.f;Job.FailureReply=Preview.Contains(TEXT("Destination host unreachable"))?TEXT("Destination host unreachable."):TEXT("Request timed out.");
        PendingPings.Add(TerminalTarget,Job);
        const FString Opening=GetTerminalPrompt()+TEXT(" ")+Command+TEXT("\nPING ")+Job.Address+TEXT(" · 4 richieste · Ctrl+C per interrompere\n");
        AppendTerminalOutput(TerminalTarget,Opening);AClaraAudio::Cue(this,TEXT("enter"),.6f);return Opening;
    }
    const FString Output=RunImmediate(Input);
    if(!Secret&&!Awaiting)
    {
        const FString Lower=Command.ToLower();
        if(Lower==TEXT("ls")||Lower==TEXT("ls -l")||Lower==TEXT("ls -la")||Lower==TEXT("pwd")||Lower==TEXT("whoami"))OfferTerminalTutor(false);
        else if(Output.StartsWith(TEXT("Comando non riconosciuto"))||Output.Contains(TEXT("Invalid input detected")))OfferTerminalTutor(true);
    }
    return Output;
}
bool AClaraStoryDirector::CancelTerminalCommand()
{
    if(!PendingPings.Remove(TerminalTarget))return false;
    AppendTerminalOutput(TerminalTarget,TEXT("^C\nPing interrotto. La prova non è stata completata.\n\n"));return true;
}
void AClaraStoryDirector::CancelAllTerminalCommands()
{
    for(const auto& Entry:PendingPings)AppendTerminalOutput(Entry.Key,TEXT("\nPing interrotto dal cambio di scenario.\n"));
    PendingPings.Empty();
}
void AClaraStoryDirector::TickTerminal(float Delta)
{
    TArray<FString> Sessions;PendingPings.GetKeys(Sessions);
    for(const FString& Session:Sessions)
    {
        FPendingPing* Job=PendingPings.Find(Session);if(!Job)continue;
        Job->Elapsed+=FMath::Max(Delta,0.f);
        while(Job->Sent<4&&Job->Elapsed+.0001f>=(Job->Sent+1)*Job->Interval)
        {
            ++Job->Sent;
            AppendTerminalOutput(Session,Job->Reachable?FString::Printf(TEXT("Reply from %s: bytes=32 time<1ms TTL=128 · seq=%d\n"),*Job->Address,Job->Sent):
                FString::Printf(TEXT("%s · seq=%d\n"),*Job->FailureReply,Job->Sent));
        }
        if(Job->Sent<4)continue;
        const FPendingPing Finished=*Job;PendingPings.Remove(Session);
        AppendTerminalOutput(Session,FString::Printf(TEXT("\nPacchetti: inviati = 4, ricevuti = %d, persi = %d\n\n"),Finished.Reachable?4:0,Finished.Reachable?0:4));
        if(Phase!=Finished.StartedPhase)continue;
        // Commit only after the last response, in the originating session, if the network still matches.
        const FString VisibleSession=TerminalTarget;
        if(Session!=VisibleSession){TerminalTranscripts.Add(VisibleSession,Transcript);TerminalTarget=Session;Transcript=TerminalTranscripts.FindRef(Session);}
        FString CurrentPreview;
        const bool StillValid=TryPredictTerminalPing(Finished.Command,CurrentPreview)&&
            (CurrentPreview.Contains(TEXT("Reply from"))||CurrentPreview.Contains(TEXT("100 percent")))==Finished.Reachable&&
            (Finished.Reachable||CurrentPreview.Contains(TEXT("Destination host unreachable"))==Finished.FailureReply.StartsWith(TEXT("Destination host unreachable")));
        if(StillValid)RunImmediate(Finished.Command,false);
        else AppendTerminalOutput(Session,TEXT("La rete è cambiata durante la prova: ripeti il ping.\n\n"));
        if(TerminalTarget!=VisibleSession){TerminalTranscripts.Add(TerminalTarget,Transcript);TerminalTarget=VisibleSession;Transcript=TerminalTranscripts.FindRef(VisibleSession);}
    }
}

bool AClaraStoryDirector::RunTerminalSelfTest()
{
    const auto Original=CaptureProgress();bool OK=true;StartDay2();Phase=103;ArpCache.Empty();Transcript.Empty();TerminalTarget=TEXT("LAPTOP");
    RunCommand(TEXT("ping 192.168.1.11"));OK&=IsTerminalBusy()&&ArpCache.IsEmpty()&&!Transcript.Contains(TEXT("Reply from"));
    TickTerminal(.9f);OK&=IsTerminalBusy()&&!Transcript.Contains(TEXT("Reply from"));
    TickTerminal(.2f);OK&=IsTerminalBusy()&&Transcript.Contains(TEXT("seq=1"))&&!Transcript.Contains(TEXT("seq=2"))&&ArpCache.IsEmpty();
    CancelTerminalCommand();TickTerminal(20);OK&=!IsTerminalBusy()&&Phase==103&&ArpCache.IsEmpty();
    Transcript.Empty();RunCommand(TEXT("ping 192.168.1.11"));SwitchTerminalSession(TEXT("SW-PROVA"));Transcript.Empty();TickTerminal(4);
    OK&=Transcript.IsEmpty()&&TerminalTranscripts.FindRef(TEXT("LAPTOP")).Contains(TEXT("ricevuti = 4"))&&ArpCache.Contains(PC2IP);
    SwitchTerminalSession(TEXT("LAPTOP"));FString Error;Devices[TEXT("PC2")]->DisconnectPort(TEXT("Ethernet"),Error);Phase=6;Transcript.Empty();
    RunCommand(TEXT("ping 192.168.1.11"));TickTerminal(3);OK&=IsTerminalBusy()&&!Transcript.Contains(TEXT("Request timed out"))&&Phase==6;
    TickTerminal(1);OK&=IsTerminalBusy()&&Transcript.Contains(TEXT("seq=1"))&&Phase==6;TickTerminal(12);OK&=!IsTerminalBusy()&&Phase==6;
    Phase=15;Say(TEXT("Luca"),{TEXT("Controllo pannelli durante dialogo")},16);OpenNotebook();OpenInventory();OK&=Page==TEXT("dialogue")&&AfterDialogue==16;FinishDialogue();OK&=Phase==16;
    CancelAllTerminalCommands();OK&=RestoreProgress(Original);
    UE_LOG(LogTemp,Display,TEXT("CLARA_TERMINAL_ASYNC_%s: timed output, timeout, cancel, origin session and dialogue guards."),OK?TEXT("PASS"):TEXT("FAIL"));return OK;
}
