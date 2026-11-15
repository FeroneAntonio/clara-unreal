#include "ClaraStoryDirector.h"
#include "ClaraAudio.h"
#include "Dom/JsonObject.h"

bool AClaraStoryDirector::IsTowerPuzzle()const
{
    return (Phase==11&&LayerTower.Kind==TEXT("osi"))||(Phase==12&&LayerTower.Kind==TEXT("tcp"));
}

void AClaraStoryDirector::TickLayerTower(float DeltaSeconds)
{
    if(IsTowerPuzzle())LayerTower.Tick(DeltaSeconds,Page.IsEmpty()&&ScreenOpen&&ScreenPage==TEXT("puzzle"));
}

void AClaraStoryDirector::SelectTowerPeg(int32 Peg)
{
    if(!IsTowerPuzzle()||!Page.IsEmpty())return;
    if(LayerTower.SelectPeg(Peg))AClaraAudio::Cue(this,TEXT("click"),.55f);
}

void AClaraStoryDirector::PreviewLayerTower()
{
    if(!IsTowerPuzzle()||!Page.IsEmpty())return;
    ScreenPage=TEXT("puzzle");OpenLaptopScreen();LayerTower.Preview(5.f);
    if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(TEXT("Luca"),TEXT("Riguarda l’ordine per cinque secondi. La base va in fondo, il servizio in cima."));
}

FString AClaraStoryDirector::GetTowerMnemonic()const
{
    return LayerTower.Kind==TEXT("osi")
        ?TEXT("Dall’alto: Anna Porta Sempre Torte Nella Dispensa Piccola. Application, Presentation, Session, Transport, Network, Data Link, Physical. Physical è il pavimento della torre.")
        :TEXT("Dall’alto: Alessia Trasporta Informazioni Nuove. Application, Transport, Internet, Network Access. Network Access è la base.");
}

void AClaraStoryDirector::ShowTowerMnemonic()
{
    if(!IsTowerPuzzle()||!Page.IsEmpty())return;
    TowerMnemonicVisible=!TowerMnemonicVisible;
    if(TowerMnemonicVisible)if(AClaraAudio* Audio=AClaraAudio::Get(this))Audio->Speak(TEXT("Luca"),GetTowerMnemonic());
}

bool AClaraStoryDirector::CheckLayerTower()
{
    if(!IsTowerPuzzle()||!Page.IsEmpty()||LayerTower.IsPreview())return false;
    if(!LayerTower.Check())
    {
        AClaraAudio::Cue(this,TEXT("error"),.5f);
        Notify(LayerTower.OffersHint()?TEXT("Luca ti offre un indizio: rivedi la torre per 5 secondi oppure apri la frase mnemonica. Nessuna penalità.")
            :TEXT("La torre non è ancora nell’ordine giusto sul piolo destro. Sposta un blocco alla volta: puoi riprovare senza penalità."));
        return false;
    }
    AClaraAudio::Cue(this,TEXT("success"),.65f);
    if(Phase==11)
    {
        Say(TEXT("Luca"),{
            TEXT("Torre ricostruita. Application è il servizio che usi, come il web o la posta."),
            TEXT("Presentation riguarda formato dei dati, compressione e cifratura."),
            TEXT("Session apre, gestisce e chiude il dialogo tra due host."),
            TEXT("Transport divide i dati in segmenti e gestisce la comunicazione tra le applicazioni; le garanzie dipendono dal protocollo."),
            TEXT("Network si occupa degli indirizzi IP e del percorso tra reti."),
            TEXT("Data Link gestisce indirizzi MAC e frame nella rete locale."),
            TEXT("Physical sono i segnali sul mezzo di trasmissione: rame, fibra o radio.")},12);
    }
    else
    {
        Say(TEXT("Luca"),{
            TEXT("Torre TCP/IP ricostruita. Application riunisce Application, Presentation e Session di OSI."),
            TEXT("Transport corrisponde al livello 4; Internet al Network di OSI."),
            TEXT("Network Access comprende Data Link e Physical. I colori ti aiutano a ritrovare la corrispondenza.")},13);
    }
    return true;
}

bool RunClaraLayerTowerFlowSelfTest()
{
    FClaraLayerTower Tower;Tower.Tick(30.f,false);
    bool Passed=Tower.PreviewPending&&Tower.PreviewRemaining==10.f&&!Tower.SelectPeg(0);
    Tower.Tick(0.f,true);Tower.Tick(10.f,true);
    const int32 Top=Tower.Pegs[0].Last();Tower.SelectPeg(0);Tower.SelectPeg(1);
    Passed&=Tower.Pegs[1].Num()==1&&Tower.Pegs[1][0]==Top;
    const auto Before=Tower.Pegs[0];Tower.Check();Tower.Check();Tower.Check();
    Passed&=Tower.OffersHint()&&Tower.Errors==3&&Tower.Pegs[0]==Before;
    Tower.Preview();Tower.Tick(20.f,false);Passed&=Tower.PreviewRemaining==5.f;
    FClaraLayerTower Restored;Passed&=Restored.ImportState(Tower.ExportState())&&Restored.Pegs[0]==Tower.Pegs[0]&&Restored.PreviewRemaining==5.f;
    auto Invalid=Tower.ExportState();Invalid->SetNumberField(TEXT("previewRemaining"),-1);
    Passed&=!Restored.ImportState(Invalid)&&Restored.PreviewRemaining==5.f;
    Tower.SolveForTest();Passed&=Tower.Check();
    Tower.Reset(TEXT("tcpip"));Tower.SolveForTest();Passed&=Tower.Kind==TEXT("tcp")&&Tower.Pegs[2].Num()==4&&Tower.Check();
    UE_LOG(LogTemp,Display,TEXT("CLARA_LAYER_TOWER_%s: dialogue timer, top-only moves, hint offer, saved board, invalid checkpoint, OSI/TCP solutions"),Passed?TEXT("PASS"):TEXT("FAIL"));
    return Passed;
}
