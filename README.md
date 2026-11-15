# TNES — demo di reti e sicurezza

Avventura educativa ambientata a Napoli nell’azienda TNES: tre giornate di networking e tre giornate Cyber, con prospettive Red e Blue, dialoghi guidati, taccuino e checkpoint. Il laboratorio locale è distinto dal gateway CML reale.

L’app Mac esportata si avvia senza Unreal Editor. Unreal serve agli sviluppatori per compilare il progetto, il cui nome tecnico resta `ClaraDayOne`. Stato della distribuzione e dipendenze locali: [TNES standalone](Docs/TNES-STANDALONE.md). Il repository contiene sorgenti e risorse originali o con licenza documentata in LICENSES.md; i contenuti Epic e le foto personali restano esclusi.

## Avvio rapido per sviluppatori

1. Installa Unreal Engine **5.8** e gli strumenti C++ richiesti dall’engine per il tuo sistema. La versione verificata sul Mac di sviluppo è **5.8.4**.
2. Clona il repository: `git clone https://github.com/FeroneAntonio/clara-unreal.git`.
3. Installa i manichini dai template della tua copia autorizzata di Unreal: `python3 Scripts/setup_epic_mannequins.py --engine-root "/percorso/UE_5.8"`. Su Windows puoi usare `py -3` al posto di `python3`. Serve il contenuto Templates/Feature Packs dell’installazione Epic. Lo script copia solo i file richiesti, senza scaricare o redistribuire asset Epic.
4. Apri `ClaraDayOne.uproject`; consenti la ricompilazione del modulo C++ se richiesta. Per Windows genera prima i file del progetto dal menu contestuale del `.uproject` o dagli strumenti Unreal.
5. Apri la mappa `Content/Maps/DayOne` e premi Play. La mappa e il GameMode sono già configurati.

Su macOS `Apri-editor.command` apre l’editor e `Avvia-gioco.command` avvia la scena in una finestra separata, dopo la compilazione. I launcher cercano Unreal nella cartella standard `/Users/Shared/Epic Games/UE_5.8`: modifica quel percorso se l’installazione è diversa. Su Windows usa l’editor o il comando sotto.

Non serve Git LFS. Serve la propria installazione di Unreal, con template e licenza Epic applicabile. `Binaries`, `Intermediate`, `Saved` e cache vengono ricreati; il primo avvio può richiedere tempo per compilazione e shader.

## Comandi

| Azione | Comando |
|---|---|
| Cammina / corsa | WASD / Maiusc |
| Guarda intorno | Tieni premuto il tasto destro e muovi il mouse |
| Interagisci | Clic sinistro oppure E |
| Borsa | I |
| Taccuino | N |
| Indizio | H |
| Apri il portatile, anche in mano | T |
| Posa il portatile sul piano mirato | Q |
| Chiudi finestra o interazione | Esc |

Per i cavi si sceglie l’apparato, lo si osserva in primo piano e si seleziona la porta. Nel terminale sono disponibili Invio, Tab, `?` e cronologia. Il terminale Linux del PC e la console IOS dello switch sono sessioni distinte.

## Laboratorio locale e Cisco CML

La storia include un modello locale di rete e un sottoinsieme di comandi IOS. **Non è Packet Tracer completo e non è una VM Linux.** Le risposte locali verificano stato dei collegamenti, indirizzi e configurazioni previste dalle missioni.

L’app **Laboratorio Cisco** nel desktop del portatile comunica con il gateway HTTPS. La base predefinita è `https://api-cml.kappino.me`; si può sostituire con la variabile d’ambiente `CML_GATEWAY_URL`, impostata prima di avviare Unreal. Non inserire account Cisco, password CML o token nel repository: le credenziali dei servizi rimangono sul gateway.

Un gateway raggiungibile non dimostra una console CML attiva. L’interfaccia distingue stato del gateway, risposta di ripiego e conferma live. Le verifiche remote dipendono dal laboratorio disponibile e dal relativo backend, che non viene distribuito da questo repository.

Per lavorare senza richieste al gateway avvia il gioco con **`-ClaraLocalLab`**. L’argomento **`-ClaraFreshStory`** ignora il salvataggio della progressione all’avvio. I salvataggi ordinari sono locali e non vengono condivisi con Git.

### Compilazione e avvio macOS

Da terminale nella cartella del repository:

```sh
CLARA_UE_ROOT='/Users/Shared/Epic Games/UE_5.8'
"$CLARA_UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" ClaraDayOneEditor Mac Development -Project="$PWD/ClaraDayOne.uproject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=2
"$CLARA_UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/ClaraDayOne.uproject" -game -windowed -ResX=1280 -ResY=800 -ClaraLocalLab
```

### Compilazione e avvio Windows

In PowerShell, adattando il percorso dell’engine:

```powershell
$ClaraEngine = 'C:\Program Files\Epic Games\UE_5.8'
$ClaraProject = Join-Path (Get-Location) 'ClaraDayOne.uproject'
& "$ClaraEngine\Engine\Build\BatchFiles\Build.bat" ClaraDayOneEditor Win64 Development "-Project=$ClaraProject" -WaitMutex
& "$ClaraEngine\Engine\Binaries\Win64\UnrealEditor.exe" $ClaraProject -game -windowed -ResX=1280 -ResY=800 -ClaraLocalLab
```

Le istruzioni Windows sono un percorso di sviluppo da verificare su quel sistema: la validazione corrente è su macOS. Non distribuire cartelle `Binaries` costruite su una piattaforma diversa.

## Dove lavorare

| Cartella / file | Contenuto |
|---|---|
| `Source/ClaraDayOne/ClaraStoryDirector.*` | Progressione e attività dei Giorni 1 e 2 |
| `Source/ClaraDayOne/ClaraStoryUI.cpp` | Desktop Linux, finestre, dialoghi, borsa e taccuino |
| `Source/ClaraDayOne/ClaraStoryCast.*` | Manichini temporanei, pose e comportamento del tutor |
| `Source/ClaraDayOne/ClaraOfficeLayout.*` | Piantina e arredi |
| `Source/ClaraDayOne/ClaraRoomVisitor.*` | Movimento e interazioni |
| `Source/ClaraDayOne/ClaraNetworkDevice.*`, `ClaraDeviceInspector.cpp` | Apparati, porte, cavi e primo piano |
| `Source/ClaraDayOne/ClaraSwitchCLI.*` | Modello locale della console IOS |
| `Source/ClaraDayOne/ClaraGateway.*`, `ClaraStoryGateway.cpp` | Client HTTPS e collegamento della storia al gateway |
| `Content/Data` | Metadati dei dispositivi e delle porte |
| `Content/OfficeAssets`, `Content/NetworkAssets` | Asset originali o CC0 importati della scena |
| `SourceAssets` | Sorgenti modificabili e registri di provenienza degli asset inclusi |

Il vecchio `ClaraPlayer` e `ClaraOffice` restano come riferimento storico nel codice; il GameMode corrente usa `ClaraRoomVisitor`, `ClaraOfficeLayout` e `ClaraStoryDirector`. I ritratti prototipali, i download Renderpeople e i materiali Seaprop sono esclusi. Sulle copie pubbliche la sedia usa i materiali originali CC0 disponibili; nessuna foto privata è necessaria.

## Verifiche prima di una PR

Compila `ClaraDayOneEditor`, avvia la scena e prova il percorso modificato. Per i controlli automatici della storia e degli spazi sono disponibili `-ClaraStoryCheck` e `-ClaraLayoutCheck`, da eseguire con `-game -nullrhi -unattended -ClaraLocalLab`. Cercare nel log il risultato esplicito PASS/FAIL, senza presumere che il solo avvio sia sufficiente. La console locale ha test Unreal Automation con prefisso `Clara.SwitchCLI`.

Non avviare verifiche remote contro un laboratorio condiviso senza coordinarti: bootstrap, collegamenti, blocchi di dominio e scenari possono cambiare lo stato visibile agli altri.

Leggi [TEAM.md](TEAM.md) per il lavoro in parallelo e [LICENSES.md](LICENSES.md) per i vincoli degli asset. Questa guida è il riferimento corrente. Il repository è visibile pubblicamente; la scrittura resta riservata ai collaboratori autorizzati.
