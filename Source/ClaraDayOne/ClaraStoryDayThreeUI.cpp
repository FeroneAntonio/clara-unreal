#include "ClaraStoryDirector.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Ink(.07f,.14f,.16f),Teal(.03f,.34f,.35f),Paper(.95f,.93f,.87f),Quiet(.34f,.41f,.42f);
TSharedRef<SWidget> Text3(const FString& Value,int32 Size=18,FLinearColor Color=Ink,bool Bold=false)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true);
}
TSharedRef<SWidget> Live3(TFunction<FString()> Value,int32 Size=18,FLinearColor Color=Ink)
{
    return SNew(STextBlock).Text_Lambda([Value=MoveTemp(Value)]{return FText::FromString(Value());}).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true);
}
TSharedRef<SWidget> Button3(const FString& Label,TFunction<void()> Action,TAttribute<bool> Enabled=true)
{
    static const FButtonStyle Style=FButtonStyle().SetNormal(FSlateColorBrush(FLinearColor::White)).SetHovered(FSlateColorBrush(FLinearColor(.8f,.9f,.86f))).SetPressed(FSlateColorBrush(FLinearColor(.65f,.8f,.76f)));
    return SNew(SButton).ButtonStyle(&Style).ButtonColorAndOpacity(Teal).ContentPadding(FMargin(16,11)).IsEnabled(Enabled)
        .OnClicked_Lambda([Action=MoveTemp(Action)]{Action();return FReply::Handled();})[SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Bold",17)).ColorAndOpacity(FLinearColor::White).AutoWrapText(false)];
}
}

FString AClaraStoryDirector::GetDayThreeGuide()const
{
    if(Phase<203)return TEXT("RACK NUOVO\nPrendi due Cat 6 bianchi dalla scatola. Collega R1 Gi0/1 a R2 Gi0/0 e R2 Gi0/1 a una porta dati di SW2. Guarda tutti i LED, poi lascia il primo piano.\n\nIl cavo azzurro è console: USB console del portatile → CONSOLE del dispositivo. Non trasporta traffico di rete.");
    if(Phase==203)return TEXT("R1 · LINK FRA ROUTER\nPortatile in mano, console su R1. Scrivi un comando per riga:\n\nenable\nconfigure terminal\ninterface gigabitethernet 0/1\nip address 10.0.0.1 255.255.255.252\nno shutdown\nend\ncopy running-config startup-config\n\nPremi Invio per confermare il nome startup-config. Oppure usa write memory dal prompt privilegiato.");
    if(Phase==204)return TEXT("R2 · DUE INTERFACCE\nSposta fisicamente la console su R2.\n\nenable\nconfigure terminal\nhostname R2\nenable secret Cisco123\nline console 0\npassword Console123\nlogin\nexit\ninterface gigabitethernet 0/0\nip address 10.0.0.2 255.255.255.252\nno shutdown\nexit\ninterface gigabitethernet 0/1\nip address 192.168.2.1 255.255.255.0\nno shutdown\nend\ncopy running-config startup-config\n\nConferma con Invio. show ip route mostra le reti direttamente collegate.");
    if(Phase==205)return TEXT("SW2 · GESTIONE\nSposta fisicamente la console su SW2.\n\nenable\nconfigure terminal\nhostname SW2\nenable secret Cisco123\nline console 0\npassword Console123\nlogin\nexit\ninterface vlan 1\nip address 192.168.2.2 255.255.255.0\nno shutdown\nexit\nip default-gateway 192.168.2.1\nend\ncopy running-config startup-config\n\nConferma con Invio. Gateway e indirizzo di gestione appartengono alla stessa subnet.");
    if(Phase==206)return TEXT("PRIMA OSSERVA\nVai alla postazione PC2 e aprila con E. Scrivi ping 192.168.2.2. Un cavo collegato e un indirizzo corretto non aggiungono automaticamente le rotte tra reti diverse.");
    if(Phase==207)return TEXT("ROTTA DI ANDATA · R1\nConsole su R1. Dal prompt privilegiato leggi show ip route, poi:\n\nconfigure terminal\nip route 192.168.2.0 255.255.255.0 10.0.0.2\nend\ncopy running-config startup-config\n\nConferma con Invio, rileggi show ip route, poi torna a PC2 e ripeti ping 192.168.2.2. Osserva anche il percorso della risposta.");
    if(Phase==208)return TEXT("ROTTA DI RITORNO · R2\nConsole su R2, dal prompt privilegiato:\n\nconfigure terminal\nip route 0.0.0.0 0.0.0.0 10.0.0.1\nend\ncopy running-config startup-config\n\nConferma con Invio. Salva anche R1. Da PC2 ripeti ping 192.168.2.2. show ip route su R2 deve includere S*.");
    if(Phase==209)return TEXT("TRACEROUTE · PC2\nAlla postazione PC2: traceroute 192.168.2.2\n\n1  192.168.1.1   R1\n2  10.0.0.2      R2\n3  192.168.2.2   SW2\n\nTTL crescente, risposte ICMP: PC2 è la sorgente e non è uno dei salti. Un timeout da solo non identifica un guasto: verifica anche tabella di routing e interfacce.");
    if(Phase==210)return TEXT("COLLAUDO\nParla con Gaia per iniziare. Le impostazioni alterate saranno presenti anche nella configurazione salvata: un semplice riavvio non le ripara.");
    if(Phase==211)return TEXT("COLLAUDO AUTONOMO\nNessun limite di tempo e nessuna penalità. Usa PC2 e le console per diagnosticare.\n\nshow running-config\nshow startup-config\nshow ip interface brief\nshow ip route\nping <IP>\ntraceroute <IP>\n\nModifica le configurazioni nel modo corretto del prompt. no ip route <rete> <mask> <next-hop> rimuove una rotta errata. Salva le riparazioni prima di TEST.\n\nGli indizi di Luca si aprono soltanto su richiesta. TEST elenca ciò che non torna; RIAVVIA è disponibile solo dopo tutti i controlli superati.");
    return TEXT("Rete verificata anche dopo il riavvio. Torna alla reception per la conclusione con Alessia.");
}

TSharedRef<SWidget> MakeClaraDayThreeLab(AClaraStoryDirector* Director)
{
    TWeakObjectPtr<AClaraStoryDirector> D=Director;
    auto Actions=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding({10,10});
    Actions->AddSlot()[Button3(TEXT("Apri terminale"),[D]{if(D.IsValid())D->SetScreenPage(TEXT("terminal"));})];
    Actions->AddSlot()[Button3(TEXT("Chiedi a Luca"),[D]{if(D.IsValid())D->RequestHint();})];
    Actions->AddSlot()[Button3(TEXT("TEST · Gaia"),[D]{if(D.IsValid())D->RunDayThreeTest();},TAttribute<bool>::CreateLambda([D]{return D.IsValid()&&D->GetPhase()==211;}))];
    Actions->AddSlot()[Button3(TEXT("RIAVVIA e verifica"),[D]{if(D.IsValid())D->RebootDayThree();},TAttribute<bool>::CreateLambda([D]{return D.IsValid()&&D->GetPhase()==211&&D->CanRebootChallenge();}))];
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Paper).Padding(24)[
        SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Text3(TEXT("TNES / RETI / GIORNO 3"),13,Teal,true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,14)[Text3(TEXT("Routing e collaudo"),30,Ink,true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Live3([D]{return D.IsValid()?D->GetDayThreeObjective():FString();},20,Teal)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Text3(TEXT("PC2 — SW1 — R1 — R2 — SW2\n192.168.1.0/24   |   10.0.0.0/30   |   192.168.2.0/24\nPortatile: soltanto console. Per ping e traceroute raggiungi PC2."),17,Quiet)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Live3([D]{return D.IsValid()?D->GetDayThreeCableStatus():FString();},16)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[Actions]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)[Live3([D]{return D.IsValid()?D->GetDayThreeGuide():FString();},18)]
            +SVerticalBox::Slot().AutoHeight()[Live3([D]{return D.IsValid()?D->GetDayThreeTestResult():FString();},18,Teal)]
        ]
    ];
}

TSharedRef<SWidget> MakeClaraDayReport(AClaraStoryDirector* Director);
TSharedRef<SWidget> MakeClaraDayThreeReport(AClaraStoryDirector* Director)
{
    return MakeClaraDayReport(Director);
}
