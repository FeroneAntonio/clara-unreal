# Lavorare insieme su CLARA

Il repository è pubblico; solo i collaboratori autorizzati possono scrivere. I collaboratori autorizzati lavorano con permesso **Write**, senza bisogno di amministrare il repository. Gli inviti vengono gestiti dal proprietario tramite gli account GitHub effettivi dei membri del team.

## Un cambiamento per ramo

1. Parti da `main` aggiornata: `git switch main` e `git pull --ff-only`.
2. Crea un ramo breve: `git switch -c feat/desktop-linux` oppure `fix/cavi-puntamento`.
3. Prima di iniziare, dichiara nel team quali file o asset toccherai. Evita che due persone modifichino lo stesso `.uasset` o `.umap` contemporaneamente.
4. Compila e prova il percorso modificato; descrivi nella PR cosa cambia e cosa hai verificato.
5. Apri una pull request verso `main`. Il proprietario o un compagno revisiona, prova e integra.

Non usare `push --force` su `main`. Non caricare build, cache, salvataggi, password o configurazioni personali. Non cambiare versione dell’engine, piantina o convenzioni dei dati dentro una PR dedicata a un’altra funzione.

## Suddivisione pratica

| Area | File principali | Coordinamento necessario |
|---|---|---|
| Storia e didattica | `ClaraStoryDirector.*` | Concordare fasi, obiettivi e condizioni reali di riuscita |
| Interfaccia e desktop | `ClaraStoryUI.cpp` | Concordare prima le API pubbliche del Director |
| Stanza e grafica | `ClaraOfficeLayout.*`, `Content/OfficeAssets`, `SourceAssets` | Conservare passaggi liberi e proporzioni; annunciare ogni asset modificato |
| Apparati e cavi | `ClaraNetworkDevice.*`, `ClaraDeviceInspector.cpp`, `Content/Data` | Aggiornare insieme porte, coordinate e verifiche |
| Personaggi | `ClaraStoryCast.*`, manichini Epic installati localmente | I manichini sono provvisori e non vengono pubblicati; non importare ritratti privati o asset Epic nel repository |
| Rete e integrazioni | `ClaraGateway.*`, `ClaraStoryGateway.cpp`, `ClaraSwitchCLI.*` | Separare risposte locali e sessioni live; non distribuire segreti |

La tabella indica responsabilità possibili, non assegna persone né blocca file. Per una modifica che attraversa più aree concordare chi modifica gli header condivisi, poi adeguare i chiamanti.

## Asset binari

Git conserva direttamente gli asset inclusi, senza Git LFS obbligatorio. Gli asset Unreal e i sorgenti Blender non si fondono come testo: una sola persona per asset alla volta. Le ricostruzioni Cisco hanno `.blend` e `.fbx` in `SourceAssets/NetworkAssets`; aggiorna il sorgente insieme all’importato e ai metadati delle porte.

Non aggiungere singoli file da 100 MB o più. Per nuove librerie pesanti decidere prima un archivio separato o Git LFS; non convertire la cronologia condivisa senza accordo. Controllare provenienza e licenza prima di importare nuovi modelli o texture.

## Una PR facile da provare

Scrivi: problema concreto, comportamento finale, file/asset coinvolti, prova eseguita e limite ancora aperto. Per modifiche visive aggiungi una schermata della scena reale. Per rete e console includi i comandi utilizzati e il risultato atteso, eliminando indirizzi sensibili, password e token dai log.

La disponibilità del gateway non garantisce la disponibilità di CML. Quando lavori solo sulla grafica usa `-ClaraLocalLab`; per modificare il laboratorio remoto accordati con il team prima di avviare bootstrap, scenari o cambi di policy.
