#include "ClaraCyberSubsystem.h"
#include "ClaraCyberDialogue.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace{FString CyberPath(){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ClaraCyberState.txt"));}std::string Narrow(const FString&s){return TCHAR_TO_UTF8(*s);}bool ValidScene(const ClaraCyber::Model& m){return m.dialogueScene<0||(m.day<=3&&m.dialogueScene<42&&m.dialogueScene/14==m.day-1&&m.dialogueOffset<ClaraCyberScenes[m.dialogueScene].Count);}}
void UClaraCyberSubsystem::Initialize(FSubsystemCollectionBase&Collection){Super::Initialize(Collection);
    FString FrontEndTourDirectory;
    const bool FrontEndTour=FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTour="),FrontEndTourDirectory);
    Isolated=FrontEndTour||FApp::IsUnattended()||FParse::Param(FCommandLine::Get(),TEXT("ClaraTour"))||
        FParse::Param(FCommandLine::Get(),TEXT("ClaraFreshStory"))||FParse::Param(FCommandLine::Get(),TEXT("ClaraFreshCyber"))||
        FParse::Param(FCommandLine::Get(),TEXT("FreshCyber"))||FCString::Strifind(FCommandLine::Get(),TEXT("-ClaraTour="))!=nullptr;
    if(Isolated)return;
    FString data;const FString path=CyberPath();if(FPlatformFileManager::Get().GetPlatformFile().FileExists(*path)||FPlatformFileManager::Get().GetPlatformFile().FileExists(*(path+TEXT(".bak")))){if(!FFileHelper::LoadFileToString(data,*path)||(!State.load(Narrow(data))||!ValidScene(State))){FString backup;if(!FFileHelper::LoadFileToString(backup,*(path+TEXT(".bak")))||(!State.load(Narrow(backup))||!ValidScene(State)))Writable=false;else Recovered=true;}}}
void UClaraCyberSubsystem::Save(){if(Isolated||!Writable)return;const FString path=CyberPath(),temp=path+TEXT(".tmp");const std::string data=State.save();FString check;ClaraCyber::Model parsed;auto& files=IFileManager::Get();if(!FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(data.c_str()),*temp)||!FFileHelper::LoadFileToString(check,*temp)||!parsed.load(Narrow(check))){Writable=false;return;}if(!Recovered&&files.FileExists(*path)&&files.Copy(*(path+TEXT(".bak")),*path,true,true)!=COPY_OK){Writable=false;return;}if(!files.Move(*path,*temp,true,true,false,true))Writable=false;else Recovered=false;}
FString UClaraCyberSubsystem::Command(const FString& Input) {
    if(State.dialogueScene>=0)return TEXT("Termina prima il dialogo.");
    const std::string before=State.save();
    const std::string answer=Terminal.command(State,Narrow(Input));
    Feedback=UTF8_TO_TCHAR(answer.c_str());
    TutorLine=UTF8_TO_TCHAR(ClaraCyber::CommandFeedback(State,Narrow(Input),answer));
    if(before!=State.save())Save();
    return Feedback;
}
bool UClaraCyberSubsystem::TickTerminal(float DeltaSeconds) {
    const bool WasBusy=Terminal.busy();
    const std::string before=State.save();
    Terminal.tick(State,DeltaSeconds);
    if(before!=State.save())Save();
    return WasBusy&&!Terminal.busy();
}
bool UClaraCyberSubsystem::CancelTerminalCommand(){return Terminal.cancel();}
FString UClaraCyberSubsystem::TerminalTranscript()const{return UTF8_TO_TCHAR(Terminal.transcript().c_str());}
FString UClaraCyberSubsystem::TaskDescription()const{return UTF8_TO_TCHAR(ClaraCyber::CurrentTask(State));}
bool UClaraCyberSubsystem::ResetForTour(){
    if(!Isolated)return false;
    Terminal=ClaraCyber::Terminal();State=ClaraCyber::Model();Feedback.Empty();TutorLine.Empty();
    return true;
}
FString UClaraCyberSubsystem::CaptureForTourCheckpoint()const{
    return Isolated?UTF8_TO_TCHAR(State.save().c_str()):FString();
}
bool UClaraCyberSubsystem::RestoreForTourCheckpoint(const FString& Checkpoint){
    if(!Isolated)return false;
    ClaraCyber::Model Loaded;
    if(!Loaded.load(Narrow(Checkpoint))||!ValidScene(Loaded))return false;
    Terminal=ClaraCyber::Terminal();State=Loaded;Feedback.Empty();TutorLine.Empty();return true;
}
bool UClaraCyberSubsystem::Action(const FString&Name,int32 Number,const FString&Value){
    if(Terminal.busy()||State.dialogueScene>=0){Feedback=Terminal.busy()?TEXT("Attendi la fine della prova oppure interrompila con Ctrl+C."):TEXT("Termina prima il dialogo.");TutorLine.Empty();return false;}
    const std::string before=State.save(),v=Narrow(Value);bool ok=false;if(Name==TEXT("choose"))ok=State.choose(Number==1?'A':Number==2?'B':0);else if(Name==TEXT("inspect"))ok=State.inspectPort(Number);else if(Name==TEXT("learn"))ok=State.learn(Number);else if(Name==TEXT("log"))ok=State.recognizeLog();else if(Name==TEXT("classify"))ok=State.classify(Number,FCString::Atoi(*Value));else if(Name==TEXT("check22"))ok=State.check22();else if(Name==TEXT("port"))ok=State.setPort(Number,Value==TEXT("on"));else if(Name==TEXT("materials"))ok=State.openMaterials();else if(Name==TEXT("expect"))ok=State.expect(Number,Value==TEXT("pass"));else if(Name==TEXT("rules")){std::vector<int> ids;for(TCHAR c:Value)if(c>='1'&&c<='3')ids.push_back(c-'0');ok=State.reorder(ids);}else if(Name==TEXT("rule"))ok=State.identifyRule(Number);else if(Name==TEXT("login"))ok=State.login("admin",v);else if(Name==TEXT("vision"))ok=(State.day!=3||State.path[2]!='B'||Number!=0)&&State.networkVision(Number!=0);else if(Name==TEXT("password"))ok=State.changePassword(v);else if(Name==TEXT("tls"))ok=State.setTls(Number!=0);else if(Name==TEXT("admin"))ok=State.setAdmin(Number!=0);else if(Name==TEXT("old"))ok=State.verifyOld();else if(Name==TEXT("team"))ok=State.verifyTeam();else if(Name==TEXT("weaknesses"))ok=State.recognizeWeaknesses((Number&1)!=0,(Number&2)!=0);else if(Name==TEXT("report"))ok=State.report();else if(Name==TEXT("opposite"))ok=State.opposite();else if(Name==TEXT("next"))ok=State.next();TutorLine=UTF8_TO_TCHAR(ClaraCyber::ActionFeedback(State,Narrow(Name),Number,v,ok));
    Feedback=TutorLine.IsEmpty()?(ok?TEXT("Operazione registrata."):TaskDescription()):TutorLine;
    if(before!=State.save())Save();return ok;}
TArray<FString> UClaraCyberSubsystem::Notebook()const{TArray<FString> out;for(const auto&x:State.notes)out.Add(UTF8_TO_TCHAR(x.c_str()));return out;}
FString UClaraCyberSubsystem::Hint(){
    const std::string before=State.save();
    std::string text=State.hint();
    if(State.day<=3&&State.path[State.day-1]&&(State.rewards[State.day-1]&2))text=ClaraCyber::CurrentTask(State);
    Feedback=UTF8_TO_TCHAR(text.c_str());TutorLine=Feedback;
    if(before!=State.save())Save();
    return Feedback;
}
bool UClaraCyberSubsystem::Login(const FString&User,const FString&Password){
    if(Terminal.busy()||State.dialogueScene>=0)return false;
    const std::string before=State.save();const bool result=State.login(Narrow(User),Narrow(Password));
    Feedback=UTF8_TO_TCHAR(ClaraCyber::ActionFeedback(State,"login",0,"",result));TutorLine=Feedback;
    if(before!=State.save())Save();return result;
}
bool UClaraCyberSubsystem::StartScene(int32 Scene){if(Terminal.busy()||State.day>3||Scene<0||Scene>=42||Scene/14!=State.day-1||State.dialogueScene>=0||ClaraCyberScenes[Scene].Count==0||(State.sceneSeen[State.day-1]&(1ULL<<Scene)))return false;State.dialogueScene=Scene;State.dialogueOffset=0;State.sceneSeen[State.day-1]|=1ULL<<Scene;Save();return true;}
void UClaraCyberSubsystem::AdvanceScene(){const int32 Scene=State.dialogueScene;if(Scene<0||Scene>=42)return;if(++State.dialogueOffset>=ClaraCyberScenes[Scene].Count){State.dialogueScene=-1;State.dialogueOffset=0;}Save();}
void UClaraCyberSubsystem::SetExpectation(const FString&Text){if(State.day<1||State.day>3)return;std::string value=Narrow(Text.Left(100));if(value.size()>500)return;State.expectations[State.day-1]=value;Save();}
