#include "ClaraStoryDirector.h"
#include "ClaraFrontEnd.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void AClaraStoryDirector::StartArrivalInterview()
{
    Persist=false;ArrivalInterviewStep=0;ArrivalNetworkingAnswers=0;
    Say(TEXT("Alessia"),{FString::Printf(TEXT("Benvenuto in TNES, %s. Prima di iniziare, conosciamoci. Non è un esame: non servono conoscenze tecniche."),*AClaraFrontEnd::GetPlayerName()),TEXT("Nel tuo primo giorno, cosa ti incuriosisce di più?")},-1,{TEXT("Collegare i computer tra loro"),TEXT("Proteggere il lavoro delle persone")});
}
bool AClaraStoryDirector::ChooseArrivalInterview(int32 Choice)
{
    if(ArrivalInterviewStep<0)return false;
    if(Choice<0||Choice>1)return true;
    if(ArrivalInterviewStep<3)
    {
        if(Choice==0)++ArrivalNetworkingAnswers;++ArrivalInterviewStep;
        if(ArrivalInterviewStep==1)Say(TEXT("Alessia"),{TEXT("Un collega ha un problema. Da dove vorresti partire?")},-1,{TEXT("Aiutarlo a tornare in comunicazione"),TEXT("Capire se un accesso è affidabile")});
        else if(ArrivalInterviewStep==2)Say(TEXT("Alessia"),{TEXT("Quale attività proveresti per prima con un tutor?")},-1,{TEXT("Costruire una piccola rete con Luca"),TEXT("Indagare sui rischi con Jonny")});
        else Say(TEXT("Alessia"),{FString::Printf(TEXT("Hai scelto %d attività sulle connessioni e %d sulla protezione. Ti proporrei %s, ma la scelta resta tua. I tutor ti accompagnano e gli errori servono a imparare, senza penalità."),ArrivalNetworkingAnswers,3-ArrivalNetworkingAnswers,ArrivalNetworkingAnswers>=2?TEXT("Networking con Luca"):TEXT("Cybersecurity con Jonny"))},-1,{TEXT("Scelgo Networking"),TEXT("Scelgo Cybersecurity")});
        return true;
    }
    ArrivalInterviewStep=-1;ClosePage();const bool Cyber=Choice==1;AClaraFrontEnd::CommitCareer(GetWorld(),Cyber?TEXT("cyber"):TEXT("reti"));
    if(Cyber){Persist=false;Owned.Add(TEXT("badge"));Owned.Add(TEXT("laptop"));SetScreenPage(TEXT("cyber"));return true;}
    FString TourDirectory;const bool Isolated=FApp::IsUnattended()||FParse::Value(FCommandLine::Get(),TEXT("ClaraFrontEndTour="),TourDirectory);
    // Resume existing Reti progress only after selection. Never overwrite it with interview state.
    bool Found=false,Restored=false,Writable=true;
    if(!Isolated)for(const FString& Suffix:{FString(),FString(TEXT(".bak"))})
    {
        const FString File=FPaths::ProjectSavedDir()/TEXT("ClaraStoryProgress.json")+Suffix;
        if(!IFileManager::Get().FileExists(*File))continue;Found=true;FString Data;TSharedPtr<FJsonObject> Save;
        if(IFileManager::Get().FileSize(*File)<=2*1024*1024&&FFileHelper::LoadFileToString(Data,*File)&&FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Data),Save)&&RestoreProgress(Save))
        {
            Restored=true;
            if(!Suffix.IsEmpty())Writable=IFileManager::Get().Copy(*(FPaths::ProjectSavedDir()/TEXT("ClaraStoryProgress.json")),*File,true,true)==COPY_OK;
            break;
        }
    }
    Persist=!Isolated&&(!Found||Restored)&&Writable;
    if(!Writable)Notify(TEXT("Partita Reti recuperata dal backup; scrittura disattivata per conservare i file originali."));
    if(Found&&!Restored){Notify(TEXT("Salvataggio Reti non leggibile: il file originale è conservato. Questa sessione non lo sovrascrive."));return true;}
    if(!Restored)Say(TEXT("Alessia"),{TEXT("Perfetto. Il tuo badge è qui sul bancone: prendilo, poi ti presento Luca. Parti dalle basi e chiedi ogni volta che qualcosa non è chiaro.")},1);
    return true;
}
