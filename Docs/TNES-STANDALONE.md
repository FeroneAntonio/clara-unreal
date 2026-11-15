# TNES — distribuzione standalone

Il gioco e l’azienda si chiamano TNES. Il progetto tecnico conserva il nome ClaraDayOne.

## Asset locali

Installare City Sample Crowds dalla libreria Fab/Epic e usare Scripts/setup_office_cast.py in Unreal per generare i due preset locali già vestiti. Content/CitySampleCrowd e Content/Characters/OfficeCast restano esclusi dal repository pubblico. Non sostituire le animazioni native con la posa di Manny.

La cabina originale è rigenerabile con Blender da SourceAssets/AircraftIntro/make_aircraft_cabin.py e importabile dall’editor con Scripts/import_aircraft_intro.py. Non eseguire l’importatore FBX in un commandlet senza Slate.

## Modalità tecnica

La demo standalone parte nel laboratorio locale, indicato come tale nelle interfacce. Il gateway CML reale richiede avvio esplicito con -ClaraLiveCML e un servizio disponibile; simulazione locale e risultati remoti non sono equivalenti.

## Stato della consegna

Mac Apple Silicon, configurazione Shipping: bundle completo verificato con dati di gioco inclusi, firma locale ad-hoc valida, avvio senza editor e uscita regolare. I controlli integrati di storia, piantina e ingresso passano direttamente nell’app esportata.

La prova nativa di Luca conferma animazione e movimento di entrambi i piedi sia sulla root sia sul corpo visibile, oltre alla traslazione dell’attore; verificato anche il ritorno all’idle. I due NPC usano abiti e scarpe Epic originali, senza posa di Manny sul corpo vestito.

Il doppiaggio viene integrato progressivamente solo dopo validazione; le battute mancanti restano leggibili. Nella prima app verificata sono presenti 309 delle 332 battute previste. Il saluto che contiene il nome libero del giocatore resta testuale.

Commit di consegna e versione Windows seguono la verifica Mac, nell’ordine richiesto. Windows richiede una compilazione nativa: il compilatore e lo SDK sono presenti sul PC, mentre l’engine va ancora installato.

## Riprodurre il pacchetto Mac

Dopo l’installazione degli asset locali, eseguire `Scripts/packaging/package_mac.command`. `TNES_UE_ROOT` permette di indicare l’installazione UE 5.8; `TNES_ARCHIVE_DIR` imposta la destinazione. Le due animazioni native vengono duplicate da `Scripts/setup_office_cast.py` dentro OfficeCast, per evitare di includere l’intera libreria di locomozione.

Il bundle archiviato può essere nominato e firmato localmente con `Scripts/packaging/finalize_mac.py`, passando app sorgente, nuova destinazione `.app`, cartella `SourceAssets/UI` e `LICENSES.md`. La firma ad-hoc locale non equivale alla notarizzazione per una pubblicazione pubblica.
