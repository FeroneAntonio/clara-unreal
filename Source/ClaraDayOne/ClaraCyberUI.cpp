#include "ClaraCyberUI.h"
#include "ClaraCyberWidgets.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "ClaraCyberVision.h"
#include "ClaraCyberSubsystem.h"
#include "ClaraCyberDialogue.h"
#include "ClaraStoryDirector.h"
#include "ClaraAudio.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
namespace{
const FLinearColor Ink(0.075f,0.11f,0.15f);
const FLinearColor Canvas(0.91f,0.94f,0.96f);
const FLinearColor Accent(0.025f,0.34f,0.39f);
TSharedRef<SWidget> Label(const FString& s,int size=16,FName Tag=NAME_None){return SNew(STextBlock).Tag(Tag).Text(FText::FromString(s)).Font(FCoreStyle::GetDefaultFontStyle("Regular",size)).ColorAndOpacity(Ink).AutoWrapText(true);}
class SCyber:public SCompoundWidget{
public:
SLATE_BEGIN_ARGS(SCyber){} SLATE_ARGUMENT(AClaraStoryDirector*,Director) SLATE_END_ARGS()
void Construct(const FArguments& a){Director=a._Director;if(Director.IsValid()&&Director->GetWorld()&&Director->GetWorld()->GetGameInstance())Lab=Director->GetWorld()->GetGameInstance()->GetSubsystem<UClaraCyberSubsystem>();if(Lab.IsValid()&&Lab->GetState().dialogueScene>=0)PlayCurrent();else QueueScene();Build();}
~SCyber(){if(Lab.IsValid())Lab->CancelTerminalCommand();if(Director.IsValid())if(auto* audio=AClaraAudio::Get(Director.Get()))audio->StopVoice();}
bool SupportsKeyboardFocus()const override{return true;}
FReply OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Key)override{
    if(Key.IsControlDown()&&Key.GetKey()==EKeys::C&&Lab.IsValid()&&Lab->IsTerminalBusy()){
        Lab->CancelTerminalCommand();Feedback=TEXT("Prova interrotta: nessun risultato registrato.");FocusTerminal=true;Dirty=true;return FReply::Handled();
    }
    return SCompoundWidget::OnPreviewKeyDown(Geometry,Key);
}
void Tick(const FGeometry& g,double t,float dt)override{
    SCompoundWidget::Tick(g,t,dt);
    if(Lab.IsValid()&&Lab->TickTerminal(dt)){Feedback=Lab->TaskDescription();Dirty=true;}
    bool Speaking=false;if(Director.IsValid())if(auto* Audio=AClaraAudio::Get(Director.Get()))Speaking=Audio->IsSpeaking();
    if(Lab.IsValid()&&!Lab->IsTerminalBusy()&&!Speaking&&FPlatformTime::Seconds()>=NextSceneAt&&Lab->GetState().dialogueScene<0&&QueueScene())Dirty=true;
    if(Dirty){Dirty=false;Build();}
}
private:
TWeakObjectPtr<AClaraStoryDirector> Director;TWeakObjectPtr<UClaraCyberSubsystem> Lab;FString Feedback,Input,Password,Expectation,User=TEXT("admin");bool Dirty=false,FocusTerminal=false;
double LastTutorAt=-100,NextSceneAt=0;
TSharedPtr<SEditableTextBox> TerminalInput;
TSharedPtr<SWrapBox> ActionRow;
void Heading(TSharedRef<SVerticalBox> box,const FString& title,const FString& detail=FString()){
 ActionRow.Reset();
 box->AddSlot().AutoHeight().Padding(0,22,0,6)[SNew(STextBlock).Text(FText::FromString(title)).Font(FCoreStyle::GetDefaultFontStyle("Bold",20)).ColorAndOpacity(Ink)];
 if(!detail.IsEmpty())Text(box,detail);
}
void ShowPage(TSharedRef<SVerticalBox> body){
 ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Canvas).Padding(20)
 [SNew(SScrollBox)+SScrollBox::Slot()[body]]];
}

void SpeakTutor(const FString& Line,bool Throttle=false){
    if(Line.IsEmpty())return;
    const double Now=FPlatformTime::Seconds();
    if(Throttle&&Now-LastTutorAt<12)return;
    LastTutorAt=Now;NextSceneAt=Now+1;
    if(Director.IsValid())if(auto* Audio=AClaraAudio::Get(Director.Get()))Audio->Speak(TEXT("Jonny"),Line);
}
void SubmitCommand(){
    if(!Lab.IsValid())return;
    Feedback=Lab->Command(Input);FocusTerminal=true;
    if(!Lab->LastTutorLine().IsEmpty()){
        Feedback+=TEXT("\nJonny: ")+Lab->LastTutorLine()+TEXT("\n")+Lab->TaskDescription();
        SpeakTutor(Lab->LastTutorLine(),true);
    }
    Input.Empty();if(TerminalInput.IsValid())TerminalInput->SetText(FText::GetEmpty());Dirty=true;
}

void PlayCurrent(){if(!Lab.IsValid())return;const auto& m=Lab->GetState();if(m.dialogueScene<0||m.dialogueScene>=42||m.dialogueOffset<0||m.dialogueOffset>=ClaraCyberScenes[m.dialogueScene].Count)return;int n=ClaraCyberScenes[m.dialogueScene].First+m.dialogueOffset;if(Director.IsValid())if(auto* audio=AClaraAudio::Get(Director.Get()))audio->Speak(ClaraCyberLines[n].Speaker,ClaraCyberLines[n].Text);Dirty=true;}
void Advance(){if(Director.IsValid())if(auto* audio=AClaraAudio::Get(Director.Get()))audio->StopVoice();if(Lab.IsValid()){const auto&m=Lab->GetState();if(m.dialogueScene>=0&&m.dialogueScene%14==0&&m.dialogueOffset==ClaraCyberScenes[m.dialogueScene].Count-2)Lab->SetExpectation(Expectation);Lab->AdvanceScene();}PlayCurrent();Dirty=true;}
bool QueueScene(){if(!Lab.IsValid())return false;const auto& m=Lab->GetState();if(m.day>3||m.dialogueScene>=0)return false;int base=(m.day-1)*14;auto start=[&](int local){if(Lab->StartScene(base+local)){PlayCurrent();return true;}return false;};if(start(0))return true;bool learned=m.day==1?m.inspected.size()>=3:m.day==2?m.notes.count("N2.1")!=0:m.notes.count("N3.1")&&m.notes.count("N3.2");if(learned){if(start(1)||start(2))return true;}char side=m.path[m.day-1];if(side){int branch=side=='A'?3:8;if(start(branch))return true;for(int step=1;step<=4;step++)if((m.rewards[m.day-1]&(1u<<step))&&start(branch+step))return true;}if(m.finished[m.day-1]&&start(13))return true;return false;}
bool Act(const FString& name,int n=0,const FString& v=FString()){if(!Lab.IsValid())return false;bool ok=Lab->Action(name,n,v);Feedback=Lab->LastFeedback();SpeakTutor(Lab->LastTutorLine());Dirty=true;return ok;}
void Add(TSharedRef<SVerticalBox> box,const FString& label,TFunction<void()> f,bool AllowBusy=false){
 if(!ActionRow.IsValid()){
  box->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(ActionRow,SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,8))];
 }
 ActionRow->AddSlot()[SNew(SButton).ContentPadding(FMargin(14,5)).ButtonColorAndOpacity(Accent)
 .IsEnabled_Lambda([this,AllowBusy]{return AllowBusy||!Lab.IsValid()||!Lab->IsTerminalBusy();})
 .OnClicked_Lambda([f]{f();return FReply::Handled();})[ClaraCyberButtonLabel(label)]];
}
void Text(TSharedRef<SVerticalBox> box,const FString& s,FName Tag=NAME_None){ActionRow.Reset();box->AddSlot().AutoHeight().Padding(0,5)[Label(s,16,Tag)];}
void Build(){ActionRow.Reset();TerminalInput.Reset();if(!Lab.IsValid()){ChildSlot[Label(TEXT("Laboratorio Cyber non disponibile."))];return;}auto& m=Lab->GetState();TSharedRef<SVerticalBox> b=SNew(SVerticalBox);TSharedRef<SVerticalBox> PageRoot=b;Heading(b,TEXT("CLARA / Cyber Lab"),FString::Printf(TEXT("GIORNO %d DI 3     •     Blue %d     /     Red %d"),FMath::Min(m.day,3),m.blue,m.red));if(!Lab->PersistenceAvailable())Text(b,TEXT("Salvataggio Cyber non disponibile: conserva la sessione e avvisa il team."));if(m.dialogueScene>=0&&m.dialogueScene<42&&m.dialogueOffset>=0&&m.dialogueOffset<ClaraCyberScenes[m.dialogueScene].Count){const int n=ClaraCyberScenes[m.dialogueScene].First+m.dialogueOffset;Text(b,FString(ClaraCyberLines[n].Speaker)+TEXT(": ")+ClaraCyberLines[n].Text);if(m.dialogueScene%14==0&&m.dialogueOffset==ClaraCyberScenes[m.dialogueScene].Count-2){Text(b,TEXT("La risposta e facoltativa."));b->AddSlot().AutoHeight()[SNew(SEditableTextBox).HintText(FText::FromString(TEXT("La tua idea"))).OnTextChanged_Lambda([this](const FText&t){Expectation=t.ToString();})];}Add(b,TEXT("Avanti"),[this]{Advance();});ShowPage(PageRoot);return;}if(m.day>3){Text(b,TEXT("Grazie per aver giocato alla demo di TNES"));Text(b,TEXT("Nuovi capitoli — Coming soon"));ShowPage(PageRoot);return;}int d=m.day;char path=m.path[d-1];if(m.finished[d-1]){Text(b,TEXT("Rapporto completato. Puoi leggere la prospettiva alternativa prima di passare al prossimo incarico."));if(d==1)Text(b,path=='A'?TEXT("Cinque porte esposte. 21, 23 e 8080 da controllare; 22 per il team tecnico e 443 per le dispense. Raccomandazione: verificare chi usa i servizi."):TEXT("Scansione da 192.168.10.77; chiuse 21, 23 e 8080. Verificate le porte rimanenti e le dispense."));else if(d==2)Text(b,path=='A'?TEXT("Aula e ospiti raggiungono il server. La regola generica in testa prevale. Raccomandazione: riordinarla o rimuoverla."):TEXT("La regola generica consentiva l'accesso. Reception raggiunge il server; aula e ospiti sono bloccati."));else Text(b,path=='A'?TEXT("Accesso con credenziali predefinite e traffico in chiaro. Raccomandazione: cambiare la password e cifrare il canale."):TEXT("Password aggiornata, canale cifrato e accesso del team mantenuto. Verificati vecchia password, team e traffico."));Add(b,TEXT("Leggi l'altra prospettiva"),[this,d]{Act(TEXT("opposite"));if(d==1)Feedback=TEXT("Red: mappa i servizi esposti. Blue: verifica chi li usa e spegne quelli inutili.");if(d==2)Feedback=TEXT("Red: confronta il traffico atteso con quello ottenuto. Blue: corregge e verifica l'ordine delle regole.");if(d==3)Feedback=TEXT("Red: osserva credenziali e traffico. Blue: cambia la password e protegge la connessione.");});Add(b,TEXT("Prossimo incarico"),[this]{Act(TEXT("next"));});}else{
Heading(b,d==1?TEXT("Servizi esposti"):d==2?TEXT("Controllo degli accessi"):TEXT("Sicurezza del server"),Lab->TaskDescription());
TSharedRef<SVerticalBox> page=b;
TSharedRef<SVerticalBox> activity=SNew(SVerticalBox);
TSharedRef<SVerticalBox> network=SNew(SVerticalBox);
page->AddSlot().AutoHeight().Padding(0,12)[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(0.60f).Padding(0,0,12,0)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::White).Padding(18)[activity]]
 +SHorizontalBox::Slot().FillWidth(0.40f)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor::White).Padding(18)[network]]];
network->AddSlot().AutoHeight()[MakeClaraCyberVision(Lab.Get(),[this](const FString& result){Feedback=result;FocusTerminal=Lab.IsValid()&&Lab->IsTerminalBusy();if(Lab.IsValid())SpeakTutor(Lab->LastTutorLine(),true);Dirty=true;})];
if(!Feedback.IsEmpty())Text(network,Feedback,FName(TEXT("cyber.feedback")));
b=activity;ActionRow.Reset();
if(d==1){Text(b,TEXT("Server servizi didattici 192.168.10.20. Seleziona una porta per leggerne la scheda."));const int ports[]={21,22,23,443,8080};const TCHAR* desc[]={TEXT("21 · trasferimento file"),TEXT("22 · amministrazione remota"),TEXT("23 · accesso remoto in chiaro"),TEXT("443 · materiali del corso"),TEXT("8080 · servizio di prova, nessun accesso negli ultimi 90 giorni")};for(int i=0;i<5;i++)Add(b,desc[i],[this,p=ports[i]]{Act(TEXT("inspect"),p);});}
if(!path){if(d==2){Text(b,TEXT("Regole: 1 permette qualsiasi → qualsiasi; 2 permette reception → prenotazioni; 3 blocca gli altri accessi al server. La prima corrispondenza decide."));Add(b,TEXT("Leggi la scheda ACL"),[this]{Act(TEXT("learn"),1);});}if(d==3){Add(b,TEXT("Scheda: credenziali predefinite"),[this]{Act(TEXT("learn"),1);});Add(b,TEXT("Scheda: cifratura dei dati"),[this]{Act(TEXT("learn"),2);});}Text(b,TEXT("Scegli un punto di vista dopo aver esplorato le schede."));Add(b,TEXT("Red · prova autorizzata"),[this]{Act(TEXT("choose"),1);});Add(b,TEXT("Blue · analisi e difesa"),[this]{Act(TEXT("choose"),2);});}
else{if(path=='B'){Text(b,d==1?TEXT("Registro: alle 14:02:11–12 la stessa origine 192.168.10.77 prova cinque porte; gli utenti normali consultano la 443."):d==2?TEXT("Registro firewall: la richiesta dell'aula è consentita dalla regola 1."):TEXT("Registro: cinque tentativi ravvicinati da 192.168.10.77, l'ultimo riuscito con admin."));Add(b,TEXT("La sequenza è anomala: analizza il registro"),[this]{Act(TEXT("log"));});Add(b,TEXT("È normale traffico utente"),[this]{Feedback=UTF8_TO_TCHAR(ClaraCyber::NormalLogFeedback(Lab->Day()));SpeakTutor(Feedback);Dirty=true;});}
if(d==1&&path=='A'){Text(b,TEXT("Classifica ogni porta: necessaria, sospetta o da verificare. Usa il terminale per la scansione."));const int ps[]={21,22,23,443,8080};for(int p:ps){for(int c=1;c<=3;c++)Add(b,FString::Printf(TEXT("%d · %s"),p,c==1?TEXT("necessaria"):c==2?TEXT("sospetta"):TEXT("da verificare")),[this,p,c]{Act(TEXT("classify"),p,FString::FromInt(c));});}Add(b,TEXT("Guida: chi usa la porta 22?"),[this]{if(Act(TEXT("check22")))Feedback=TEXT("La porta 22 serve al team tecnico: va mantenuta e protetta.");});}
if(d==1&&path=='B'){Text(b,TEXT("Interruttori dei servizi: 22 è per il team tecnico; 443 serve alle dispense. Le modifiche si possono correggere."));for(int p:{21,22,23,443,8080})for(int on=0;on<=1;on++)Add(b,FString::Printf(TEXT("Porta %d · %s"),p,on?TEXT("attiva"):TEXT("ferma")),[this,p,on]{Act(TEXT("port"),p,on?TEXT("on"):TEXT("off"));});Add(b,TEXT("Apri le dispense"),[this]{Act(TEXT("materials"));});}
if(d==2){const TCHAR* host[]={TEXT("192.168.10.50 reception"),TEXT("192.168.10.30 aula"),TEXT("192.168.10.99 ospiti")};if(path=='A'){for(int i=0;i<3;i++){Text(b,FString::Printf(TEXT("%s · atteso: %s · osservato: %s"),host[i],m.expected[i]<0?TEXT("da indicare"):m.expected[i]?TEXT("passa"):TEXT("bloccato"),m.observed[i]<0?TEXT("non ancora provato"):m.observed[i]?TEXT("passa"):TEXT("bloccato")));for(int pass=0;pass<2;pass++)Add(b,FString::Printf(TEXT("%s: atteso %s"),host[i],pass?TEXT("passa"):TEXT("bloccato")),[this,i,pass]{Act(TEXT("expect"),i,pass?TEXT("pass"):TEXT("block"));});}for(int n=1;n<=3;n++)Add(b,FString::Printf(TEXT("La causa è la regola %d"),n),[this,n]{Act(TEXT("rule"),n);});}else{Text(b,TEXT("Ordine attuale delle regole:"));for(const auto&r:m.rules)Text(b,FString::Printf(TEXT("Regola %d · %s · %s → %s"),r.id,r.allow?TEXT("permetti"):TEXT("blocca"),UTF8_TO_TCHAR(r.from.c_str()),UTF8_TO_TCHAR(r.to.c_str())));for(const TCHAR* order:{TEXT("123"),TEXT("132"),TEXT("213"),TEXT("231"),TEXT("312"),TEXT("321"),TEXT("23"),TEXT("32")}){FString x(order);Add(b,TEXT("Applica ordine ")+x,[this,x]{Act(TEXT("rules"),0,x);});}}Text(b,TEXT("Terminale: ping -I <postazione> 192.168.20.10"));}
if(d==3){ActionRow.Reset();if(path=='A'){Heading(b,TEXT("Server prenotazioni"),TEXT("Accesso amministratore · ambiente didattico"));Text(b,TEXT("Account di prova: admin / password   •   root / root   •   test / test\nadmin / 1234   •   admin / admin"));Text(b,TEXT("Nome utente"));b->AddSlot().AutoHeight()[SNew(SEditableTextBox).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).Padding(FMargin(10,8)).Tag(FName(TEXT("cyber.user"))).Text(FText::FromString(User)).HintText(FText::FromString(TEXT("Utente"))).OnTextChanged_Lambda([this](const FText& t){User=t.ToString();})];Text(b,TEXT("Password"));b->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SEditableTextBox).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).Padding(FMargin(10,8)).Tag(FName(TEXT("cyber.login-password"))).Text(FText::FromString(Password)).HintText(FText::FromString(TEXT("Password della coppia scelta"))).IsPassword(true).OnTextChanged_Lambda([this](const FText& t){Password=t.ToString();})];Add(b,TEXT("Prova accesso"),[this]{bool ok=Lab.IsValid()&&Lab->Login(User,Password);Feedback=Lab.IsValid()?Lab->LastFeedback():TEXT("Laboratorio non disponibile.");if(Lab.IsValid())SpeakTutor(Lab->LastTutorLine());Password.Empty();Dirty=true;});Add(b,TEXT("Segnala password predefinita e HTTP"),[this]{Act(TEXT("weaknesses"),3);});}else{b->AddSlot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(Ink).AutoWrapText(true).Text_Lambda([this]{auto v=ClaraCyber::Model::passwordCheck(TCHAR_TO_UTF8(*Password));return FText::FromString(FString::Printf(TEXT("Almeno 12 caratteri: %s | non comune: %s | diversa dalla precedente: %s | senza nome utente: %s"),v.length?TEXT("sì"):TEXT("da completare"),v.uncommon?TEXT("sì"):TEXT("da completare"),v.changed?TEXT("sì"):TEXT("da completare"),v.noUsername?TEXT("sì"):TEXT("da completare")));})];b->AddSlot().AutoHeight()[SNew(SEditableTextBox).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).Padding(FMargin(10,8)).Tag(FName(TEXT("cyber.new-password"))).Text(FText::FromString(Password)).HintText(FText::FromString(TEXT("Nuova password: almeno 12 caratteri"))).IsPassword(true).OnTextChanged_Lambda([this](const FText& t){Password=t.ToString();})];Add(b,TEXT("Aggiorna password"),[this]{Act(TEXT("password"),0,Password);Password.Empty();});for(int on=0;on<2;on++){Add(b,on?TEXT("Attiva cifratura"):TEXT("Disattiva cifratura"),[this,on]{Act(TEXT("tls"),on);});Add(b,on?TEXT("Riattiva amministratore"):TEXT("Disattiva amministratore"),[this,on]{Act(TEXT("admin"),on);});}Add(b,TEXT("Verifica vecchia password"),[this]{Act(TEXT("old"));});Add(b,TEXT("Verifica accesso team tecnico"),[this]{Act(TEXT("team"));});Add(b,TEXT("Network Vision: verifica il traffico cifrato"),[this]{Act(TEXT("vision"),1);});}}
b=page;ActionRow.Reset();Heading(b,TEXT("Terminale"),TEXT("Comandi sui dati dell’esercizio · Invio esegue · Ctrl+C interrompe"));
Text(b,Lab->TaskDescription());
b->AddSlot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Mono",14)).ColorAndOpacity(Ink)
    .Text_Lambda([this]{return FText::FromString(Lab.IsValid()?Lab->TerminalTranscript():FString());})];
b->AddSlot().AutoHeight()[SAssignNew(TerminalInput,SEditableTextBox).Tag(FName(TEXT("cyber.command"))).Text(FText::FromString(Input))
    .IsReadOnly_Lambda([this]{return Lab.IsValid()&&Lab->IsTerminalBusy();})
    .HintText_Lambda([this]{return FText::FromString(Lab.IsValid()&&Lab->IsTerminalBusy()?TEXT("Prova in corso · Ctrl+C per interrompere"):TEXT("Scrivi un comando o spiega <comando>"));})
    .OnTextChanged_Lambda([this](const FText& t){Input=t.ToString();})
    .OnTextCommitted_Lambda([this](const FText&,ETextCommit::Type Type){if(Type==ETextCommit::OnEnter)SubmitCommand();})];
Add(b,TEXT("Esegui"),[this]{SubmitCommand();});
if(Lab->IsTerminalBusy())Add(b,TEXT("Interrompi · Ctrl+C"),[this]{Lab->CancelTerminalCommand();Feedback=TEXT("Prova interrotta: nessun risultato registrato.");FocusTerminal=true;Dirty=true;},true);
Add(b,TEXT("Compila il rapporto"),[this]{Act(TEXT("report"));});}}
b=PageRoot;ActionRow.Reset();
Add(b,TEXT("Chiedi un indizio"),[this]{if(Lab.IsValid()){Feedback=Lab->Hint();SpeakTutor(Feedback);}Dirty=true;});Heading(b,TEXT("Appunti della giornata"));for(auto& n:Lab->Notebook()){FString v=n;if(n==TEXT("N1.1"))v+=TEXT(" - Porta e servizio: ogni servizio ascolta su una porta.");else if(n==TEXT("N1.2"))v+=TEXT(" - Superficie esposta: più servizi richiedono più controlli.");else if(n==TEXT("N1.3"))v+=TEXT(" - Una scansione mostra quali porte rispondono.");else if(n==TEXT("N1.4"))v+=TEXT(" - Prima di chiudere, scopri chi usa il servizio.");else if(n==TEXT("N2.1"))v+=TEXT(" - La ACL applica la prima regola corrispondente.");else if(n==TEXT("N2.2"))v+=TEXT(" - Confronta ciò che ti aspetti con ciò che osservi.");else if(n==TEXT("N2.3"))v+=TEXT(" - L'ordine delle regole conta.");else if(n==TEXT("N2.4"))v+=TEXT(" - Consenti solo ciò che serve.");else if(n==TEXT("N3.1"))v+=TEXT(" - Le credenziali predefinite sono prevedibili.");else if(n==TEXT("N3.2"))v+=TEXT(" - La cifratura protegge i dati in transito.");else if(n==TEXT("N3.3"))v+=TEXT(" - I tentativi ravvicinati lasciano tracce nel registro.");else if(n==TEXT("N3.4"))v+=TEXT(" - Cambia la password senza togliere l'accesso necessario.");else if(n==TEXT("N3.5"))v+=TEXT(" - Cambia le credenziali predefinite e cifra il traffico.");Text(b,v);}if(!Feedback.IsEmpty()&&(m.day>3||m.finished[FMath::Clamp(m.day-1,0,2)]))Text(b,Feedback,FName(TEXT("cyber.feedback")));ShowPage(PageRoot);if(FocusTerminal&&TerminalInput.IsValid()){FSlateApplication::Get().SetKeyboardFocus(TerminalInput);FocusTerminal=false;}}
};
}
TSharedRef<SWidget> MakeClaraCyberUI(AClaraStoryDirector* d){return SNew(SCyber).Director(d);}
