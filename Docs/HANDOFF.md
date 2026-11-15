> **Correzione vincolante di Antonio, 15 novembre 2026:** il gioco e l’azienda si chiamano **TNES**, non CLARA. I nomi tecnici storici `Clara*` restano interni. L’intro richiesta è in prima persona, seduti in **aereo**: CV leggibile, sguardo a destra, vero Vesuvio visibile soltanto dal finestrino. Testi introduttivi spiegano il viaggio verso il colloquio in TNES; il percorso si decide al colloquio in ufficio. La versione treno e i precedenti filmati geometrici sono superati. Importazione e resa in Unreal vanno ancora verificate; non dichiararle concluse sulla base di anteprime Blender.

# CLARA — passaggio di consegne completo alla nuova chat

Aggiornato il **14 novembre 2026**, fuso Europe/Rome. Documento operativo: leggere prima di proporre modifiche o ricreare componenti già presenti.

## 1. Istruzione iniziale per la nuova chat

> Riprendi il progetto Unreal esistente. Leggi questo documento, poi AGENTS.md, README.md, TEAM.md e Docs/CML_GATEWAY.md del repository. Verifica lo stato Git e i file reali, avvia la build attuale e prova il percorso del Giorno 1. Segnala i bug con passaggi per riprodurli e prove osservabili; correggi prima i blocchi del giocatore, poi la chiarezza delle interazioni e la qualità grafica. Conserva la piantina e i passaggi liberi. Non presentare simulazione locale, output del gateway e prove IOS come la stessa cosa. Non ricominciare il progetto da zero.

**Non è tutto finito né perfetto.** Esiste una build giocabile con molte funzioni, prove automatiche e collegamento reale al gateway. La qualità visiva e il percorso completo provato manualmente richiedono ancora lavoro. Distinguere sempre implementato, testato automaticamente, osservato a schermo e ancora da verificare.

## 2. Dove si trova tutto

Workspace del Mac:

`/Users/antonioferone/Documents/Codex/2026-11-14/referenced-chatgpt-conversation-this-is-an-2`

Progetto e repository Git locale:

`/Users/antonioferone/Documents/Codex/2026-11-14/referenced-chatgpt-conversation-this-is-an-2/outputs/clara-unreal/ClaraDayOne`

File progetto: `ClaraDayOne.uproject`.

La radice del workspace **non è un repository Git**. Eseguire Git dalla cartella `ClaraDayOne`, non dalla radice. Anche i log `work/...` sono nella radice del workspace, non dentro il progetto: evitare di cercarli dalla directory sbagliata.

Repository pubblico: **https://github.com/FeroneAntonio/clara-unreal**.

- Prima pubblicazione: `94633386e1ce5fc84dc259954575bfb08ccadaa8`.
- Documentazione successiva: `899b0e5aa5ff54c637348f96e5b3d4ed17253a39`.
- Correzioni terminale/Ethernet/router Claude: ramo `fix/ethernet-terminal-router`, commit `d14ffd97cfb05d19a7771aeab40301f14d2fcd28`.
- **PR #1 integrata in main:** https://github.com/FeroneAntonio/clara-unreal/pull/1 . Merge verificato: `cd2fed414049596005eba53f5f6050a57346185c`.
- Il ramo locale può essere ancora `fix/ethernet-terminal-router`; controllare prima di cambiare ramo. Il documento corrente può aggiungere un commit successivo ai riferimenti sopra.
- Nessuna CI Unreal configurata al momento della PR #1: il merge non equivale a un collaudo GitHub Actions.

Documenti utili:

| Percorso | Contenuto |
|---|---|
| `AGENTS.md`, `README.md`, `TEAM.md` nel progetto | Regole, avvio e collaborazione |
| `Docs/CML_GATEWAY.md` | API, mappature, limiti e adattatore IOS |
| `Docs/VERIFICHE.md` | Prove effettuate e limiti |
| `Docs/PROMPT_CLAUDE_ROUTER.md` | Compito separato assegnabile a Claude per asset |
| `SESSIONE-CLARA.md` nella radice workspace | Cronologia di stato precedente; questo handoff è più aggiornato |
| `outputs/storia/PROVA-GIOCABILE.md` | Guida al percorso dei due giorni |
| `outputs/storia/Note-tecniche-copioni.md` | Correzioni tecniche ai copioni |
| `outputs/storia/riferimenti/STORYTELLING_GIORNO_1.md`, `_2.md` | Copie dei testi ricevuti |
| `outputs/grafica-ufficio/` | Ricerca asset, texture, provenienza, anteprime precedenti |
| `outputs/clara-unreal/Checkpoint/` | Vecchia prova storica; conservarla, non modificarla |
| `checkpoints/2026-11-14-sessione-clara/` | Snapshot precedente di sorgenti/configurazione, non un backup totale degli asset |

## 3. Obiettivo del prodotto

Gioco educativo per la challenge **Consorzio CLARA × Cisco Networking Academy / The Big Hack 2026**, ambientato in un vero ufficio informatico a Napoli. Il giocatore entra in un’azienda dopo un colloquio, viene affiancato e impara lavorando. Rami futuri: reti, cyber o entrambi, con progressione verso contenuti CCNA.

Pubblico: persone che possono non conoscere neppure cosa sia un comando. La storia deve far scoprire strumenti, funzione dei cavi e significato dei risultati. Non basta che il tutor detti «fai ping» o che una sequenza di pulsanti Continua sostituisca l’esercizio.

Personaggi stabiliti dall’utente:

- **Alessia**: accoglienza/assunzione, **seduta alla reception**.
- **Luca**: tutor reti, accompagna il giocatore durante le attività.
- **Jonathan / Jonny**: tutor cyber.
- **Gaia**: collega del briefing, assegna il ripristino del Giorno 2.

Attualmente sono **manichini Epic provvisori**, non personaggi realistici definitivi. Antonio ha rifiutato i primi personaggi 3D di bassa qualità. Foto personali e prototipi non devono essere pubblicati.

La modalità singola è il nucleo attuale. Co-op drop-in, scambio di ruoli e Webex sono idee discusse: **non dichiararle implementate**.

## 4. Preferenze dell’utente da rispettare

1. **Realismo alto**: sedie, tavoli, case dei PC, router e switch riconoscibili. Il PC è un case con monitor, non un monitor che fa tutto.
2. **Nessun percorso a ostacoli.** Richiesta ripetuta molte volte. Niente porte aperte nel passaggio, sedie davanti al banco cavi, arredi che chiudono il giocatore. Non riempire spazi vuoti sacrificando la mobilità.
3. **Conservare la piantina attuale.** I copioni non autorizzano a ricostruire le stanze; nuovi oggetti vanno inseriti negli spazi esistenti con criterio.
4. **Interazioni con il mouse**, borsa/inventario, taccuino e cavi da prendere/collegare. Primo piano degli apparati come principio d’interazione Packet Tracer, con rotazione a 360°.
5. **Mouse verticale naturale**, mai invertito accidentalmente.
6. **Desktop del PC come la versione 1 dell’altra sala:** icone Terminale, cartelle, applicazioni in finestre e barra. Il dashboard a grandi schede è stato rifiutato.
7. **Terminale normale e pulito, scuro uniforme.** L’ultima schermata bianca è stata rifiutata esplicitamente. Non ripristinare riquadri bianchi, pulsantoni o suggerimenti che occupano mezza finestra.
8. **Taccuino come un quaderno**, non un pannello amministrativo.
9. Dialoghi naturali e brevi; tutor con indizi progressivi e spiegazioni collegate all’azione.
10. Gli oggetti importanti devono essere riconoscibili/evidenziati per orientare il principiante, senza trasformare l’ufficio in un pannello di etichette.
11. Portatile apribile **ovunque, anche in mano**. Appoggiarlo è facoltativo; deve essere facile riprenderlo.
12. Azione autonoma e concreta, poche domande superflue. Usare subagenti per compiti separati, coordinando gli header e i binari Unreal.

## 5. Ambiente di sviluppo

- Mac con Apple Silicon, circa **24 GB RAM**; evitare molte istanze Unreal/Blender e compilazioni simultanee. In precedenza il Mac si surriscaldava.
- Unreal Engine **5.8.4**, associazione progetto `5.8`.
- Installazione: `/Users/Shared/Epic Games/UE_5.8`.
- Eseguibile: `/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor`.
- Headless: `/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd`.
- Blender già installato; gli script precedenti mostrano come richiamarlo. KeenTools era presente per esperimenti sui volti, non necessario ai manichini attuali.
- `gh` autenticato come **FeroneAntonio**. Non stampare token né leggere/mostrare indiscriminatamente configurazioni private.
- `ClaraDayOne.Build.cs` ora imposta **`bUseUnity = false`**. Mantenerlo: dopo l’inizializzazione Git Unreal ha iniziato a raggruppare vecchi `.cpp` e sono emerse collisioni tra nomi di namespace anonimi. La compilazione separata del modulo risolve quel problema.
- Plugin **AndroidFileServer disabilitato** nel `.uproject`: rigenerava un SecurityToken inutilizzato in `DefaultEngine.ini`. Non pubblicare né riattivare quel token.

### Build sul Mac

Dalla cartella del progetto:

```sh
CLARA_UE_ROOT='/Users/Shared/Epic Games/UE_5.8'
"$CLARA_UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" ClaraDayOneEditor Mac Development -Project="$PWD/ClaraDayOne.uproject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=2
```

Eseguire davvero la build e leggere il risultato finale. Un comando partito non è una build riuscita. Non lanciare il vecchio binario per poi attribuirgli una correzione che non ha compilato.

### Avvio

```sh
"$CLARA_UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/ClaraDayOne.uproject" -game -windowed -ResX=1280 -ResY=800 -nosplash
```

Ci sono anche launcher `.command` nel progetto. Preferire l’eseguibile diretto: `open -a` in passato riutilizzava un’altra istanza Unreal ignorando il progetto desiderato.

Non chiudere progetti estranei: Claude ha lavorato anche a un altro progetto chiamato **ModelloMH**. Se si deve fermare un processo, identificarlo dal percorso completo di `ClaraDayOne.uproject`, non terminare tutte le applicazioni Unreal per nome.

### Flag di sviluppo

| Flag | Uso |
|---|---|
| `-ClaraLocalLab` | Nessuna richiesta al gateway, consigliato per grafica/UI e lavoro parallelo |
| `-ClaraFreshStory` | Ignora il salvataggio delle giornate |
| `-ClaraCableDemo` | Punto di prova dello smistamento dei cavi |
| `-ClaraEthernetDemo` | Punto di prova Ethernet: materiale già disponibile, portatile in mano, fase5 |
| `-ClaraDeviceView` | Primo piano apparati |
| `-ClaraPlanView`, `-ClaraChairView` | Viste di studio precedenti |
| `-ClaraStoryDay2` | Accesso di sviluppo alla seconda giornata |
| `-ClaraStoryCheck` | Test integrato locale di storia e interazioni |
| `-ClaraLayoutCheck` | Test porte, percorsi e apparati |
| `-ClaraGatewayCheck` | Prova HTTP reale di gateway/IOS/verificatore |
| `-ClaraGatewayPortCheck` | Prova reale dell’adattatore porta: solo abilitazione idempotente Ethernet0/1 |

I flag Demo sono accessi di sviluppo, non prove che l’utente abbia completato le attività. Non confondere questi punti di prova con un salvataggio completo della partita.

## 6. Piantina attuale e mobilità

Coordinate Unreal in centimetri, ufficio circa **19 × 14,4 m**:

| Zona | Coordinate indicative |
|---|---|
| Reception | X0–560, Y0–540 |
| Cyber | X560–1900, Y0–540 |
| Corridoio | Y540–760, larghezza circa2,2m |
| Briefing/lounge | X0–560, Y760–1440 |
| Laboratorio reti | X560–1350, Y760–1440 |
| Sala apparati | X1350–1900, Y760–1440 |

Porte scorrevoli a vetro, ante parcheggiate contro la parete. Cinque varchi interni: `(175,540)`, `(690,540)`, `(470,760)`, `(680,760)`, `(1510,760)`.

Spawn normale `(175,80,90)`; capsula del giocatore raggio28cm, semialtezza88cm. Cammino330cm/s, Maiusc480cm/s, FOV85. Le sedie del fronte banchi nel laboratorio reti sono state rimosse per liberare le interazioni.

`ClaraOfficeLayout.cpp` usa un `RoomOffsetY` durante la generazione: reception/cyber +60, lounge/reti/apparati +200. Le coordinate locali nei metodi non sono sempre già coordinate finali. Non duplicare accidentalmente tavoli/PC compensando due volte l’offset.

Banco principale circa `(955,1100)`, switch `(945,1112,77.2)`, router `(1035,1110,77.2)`, posizione iniziale portatile `(1135,1120,77.2)`. PC2 ha un case separato sul banco posteriore sinistro; il banco posteriore destro è per i cavi e le tre scatole.

Alessia usa la sedia reception esistente circa `(305,450)` con posa su skeleton Quinn tramite `UPoseableMeshComponent`. Luca segue a distanza, con controlli di collisione e waypoint; verificare ancora la naturalezza del percorso.

## 7. Architettura attiva: cosa modificare e cosa è legacy

Tutti i file sotto sono in `Source/ClaraDayOne/`.

| File | Responsabilità |
|---|---|
| `ClaraGameMode.*` | Generazione scena, personaggi/director, flag e test |
| `ClaraRoomVisitor.*` | Pawn attivo, movimento, mouse, input e controller |
| `ClaraOfficeLayout.*` | Piantina e arredi |
| `ClaraStoryDirector.*` | Stati delle giornate, dialoghi, borsa, portatile, esercizi e modello locale |
| `ClaraStoryUI.cpp` | Desktop Linux, finestre, terminale, borsa, taccuino e simulazioni |
| `ClaraStoryCast.*` | Manichini, posa Alessia, Luca al seguito, cavi/oggetti |
| `ClaraStoryHighlights.*` | Indicazione discreta degli obiettivi |
| `ClaraNetworkDevice.*` | Mesh, dati delle porte, collegamenti reciproci, cavi spline, anteprima |
| `ClaraDeviceInspector.cpp` | Primo piano, rotazione/zoom, selezione porte e primo/secondo capo |
| `ClaraSwitchCLI.*` | Sottoinsieme IOS locale, modalità, errori, Tab, ?, salvataggio e riavvio |
| `ClaraGateway.*` | Client HTTP asincrono del gateway; code seriali, timeout, provenienza |
| `ClaraStoryGateway.cpp` | Collegamento delle azioni del gioco a CML/AI e adattatore porte |
| `ClaraGatewayCheck.*` | Prove HTTP vere e report locali |
| `ClaraDisplay.*` | Vecchio supporto di interfaccia su monitor fisico; presente, non il desktop attuale |
| `ClaraDesktop.cpp` | Riferimento visivo della versione1, usato per il nuovo desktop |

`ClaraPlayer`, `ClaraOffice`, `ClaraTerminal`, `ClaraCML` e parti delle vecchie interfacce sono **legacy**. In particolare il vecchio bridge loopback `CLARA_CML_BRIDGE` appartiene al vecchio controller, mentre la scena attiva usa il nuovo gateway HTTPS. Non correggere soltanto una classe legacy pensando che il giocatore la usi.

Il GameMode attivo usa `AClaraRoomVisitor` e `AClaraRoomController`, non il vecchio `AClaraPlayer`.

## 8. Storia e condizioni di progresso

### Giorno 1

Fasi interne principali:

| Fase | Attività |
|---|---|
| 0 | Colloquio con Alessia |
| 1 | Ritirare badge |
| 2 | Parlare con Luca |
| 3 | Prendere il portatile |
| 4 | Esaminare e smistare Ethernet, console e fibra |
| 5 | Collegare portatile a SW1 |
| 6 | Scoprire strumenti e provare ping |
| 7 | Ricostruire il tratto mancante verso PC2 |
| 8 | Ping riuscito |
| 9 | Collegare la console |
| 10 | Confrontare MAC del PC e tabella dello switch, identificare la porta |
| 11–12 | Ricostruire i livelli OSI e TCP/IP |
| 13 | Collegare router e distribuzione ottica, con modulo SFP |
| 14–15 | Prova autonoma dopo scollegamento dei due host, nuova lettura MAC |
| 16 | Rapporto/completamento della giornata |

Il ping verifica i collegamenti effettivi del modello locale; una risposta non deve comparire solo perché si è cliccato Continua. Le risposte MAC richiedono lettura dell’indirizzo del PC e della tabella, non soltanto una stringa indovinata.

### Giorno 2

Fasi100–110: saluto con Luca, sequenza ARP, cache cancellata/osservata e ripopolata con ping, esercizio /26, console e configurazione dello switch di prova, lettura startup e ping, incarico di Gaia, ricostruzione rete del piano2, TEST operativo e riavvio.

Esercizio subnet: ultimo ottetto70, /26, rete192.168.1.64, broadcast.127,62host utilizzabili. La challenge include due switch, un indirizzo PC3 errato e più host; è una simulazione locale, non la topologia CML corrente.

La verifica operativa deve poter passare **prima** del salvataggio: il riavvio successivo dimostra perché serve salvare. Non rendere impossibile il caso didattico «funzionava, ma al riavvio ho perso la configurazione».

Correzioni tecniche già concordate: il Cisco1941 non ha una porta SFP; il cavo console non alimenta lo switch; il gateway non serve per un ping tra host della stessa sottorete; MAC non significa identità immutabile; Type7 non è cifratura forte; SSHv2 con RSA adeguata; in config usare `do show ...` o uscire dalla modalità.

## 9. Desktop, portatile e input

- `T`: apre il portatile anche in mano. È un desktop Linux **simulato**, non una VM.
- `Esc`: chiude prima pannello/schermo attivo.
- `I`: borsa a griglia, selezione oggetto e azioni contestuali; da lì riprendere il portatile.
- `N`: taccuino a due pagine, righe e rilegatura.
- `Q`: posa facoltativa se si guarda un piano valido del tavolo.
- Clic o `E`: interazione vicina. `H`: indizio.
- WASD/Maiusc e tasto destro per guardare; movimento bloccato nelle finestre modali.

Il desktop ha icone Terminale, File, Network Vision, Laboratorio Cisco e Taccuino, finestre e barra. Il terminale PC supporta `ip addr`, `ip a`, `ifconfig`, `ip neigh`, ping e ARP; rimangono alias `ipconfig /all` per gli appunti precedenti. Prompt Linux: `tecnico@clara:~$`.

La console locale dello switch e l’app CML sono sessioni distinte. Tab, ?, abbreviazioni, cronologia, errori e conferme vuote di Invio sono implementati nel sottoinsieme locale. La console remota attualmente usa righe HTTP: non dichiarare che Tab locale sia completamento byte-stream remoto.

Ultima modifica visiva: terminale #0e1114 uniforme, testo chiaro monospazio, prompt verde, campo input scuro e comandi compatti. Rimossi il fondo bianco, l’intestazione duplicata, il pulsantone Invio e la fila di suggerimenti grandi. Controllare ancora il risultato a schermo nella build aggiornata: i timeout CUA hanno limitato la verifica manuale.

## 10. Bug Ethernet individuato e corretto

Il blocco del giocatore era **nel client**, separato dall’errore del server:

1. Il portatile trasportato disattivava le collisioni di puntamento per non coprire gli oggetti del mondo.
2. L’anteprima ereditava la stessa disattivazione e le prese Ethernet/console diventavano impossibili da cliccare.
3. Ora l’ignore vale soltanto per `Carried && !bIsPreview`: le prese nell’anteprima restano cliccabili.
4. Riselezionare lo stesso tipo di cavo non cancella più il primo capo.
5. Il primo piano del portatile parte dal lato Ethernet; scegliendo console si orienta sul lato corrispondente.

`SelectDevicePort(Id)` nel controller concentra la logica usata dal clic ed è usato anche nel test del percorso. Restano da provare manualmente comfort del clic, eventuali micro-drag interpretati come rotazione, porte già occupate e feedback per principianti.

Non rimettere il modello in mano con collisioni di puntamento bloccanti: il bug precedente rendeva difficile anche prendere oggetti dietro al portatile.

## 11. Apparati, materiali e nuovo router Claude

Dati degli apparati: `Content/Data/network-devices.json`. La logica usa gli identificatori stabili delle porte, non l’aspetto della mesh.

- Switch: ricostruzione originale ispirata a Cisco2960-X,24porte rame +4SFP. Non CAD ufficiale né livello di qualità definitivo.
- Router precedente: conservato in `/Game/NetworkAssets/SM_Cisco1941`.
- **Router attivo nuovo:** `/Game/NetworkAssets/SM_Cisco1941_Claude.SM_Cisco1941_Claude`.
- Sorgenti pubblicati: `SourceAssets/NetworkAssets/ClaudeCisco1941/`.
- Materiali/texture importati: `Content/NetworkAssets/ClaudeCisco1941/`.
- Originale di lavoro Claude: `outputs/asset-lab/claude-cisco1941/` nella radice workspace, con render e due varianti.

È stata scelta la variante **reale**, non JsonCompat: corregge il lato delle porte. Aggiornare mesh e coordinate insieme, mai una senza l’altra.

Caratteristiche verificate all’importazione:

- Circa36k triangoli,21materiali,4mappe PBR attive (roughness e normali DirectX per due superfici).
- Quote Unreal34,3×29,8×8,9cm; normalizzazione runtime circa1.
- Otto identificatori conservati: Power, Aux, Console, USBConsole, Gi0/0, Gi0/1, USB0, USB1.
- Retro +Y. Porte dati/console sul lato fisicamente corretto; prese con coordinate X positive nella variante attiva.
- Percorsi delle texture relativi al progetto e sorgenti conservati.
- Materiale M_Office salvato con uso spline per evitare il materiale di default sui cavi.

Import script locale: `work/network-assets/import_claude_router.py`. Report e backup locali in `work/network-assets/`; non pubblicare log o backup indiscriminatamente. Il JSON viene aggiornato soltanto dopo salvataggio e controlli dell’asset.

**Non dichiarare fotorealismo raggiunto.** Claude stesso segnala limiti: usura/etichette mancanti, logo sul coperchio non in rilievo, carattere piccolo dell’1 non ideale. Verificare in Unreal illuminazione, testo non specchiato, mappa normale, allineamento delle spine e leggibilità di ogni presa. Un render Blender non sostituisce la prova nel motore.

## 12. Gateway Cisco CML reale

Base URL: `https://api-cml.kappino.me`. Schema: `/openapi.json`; Swagger: `/docs`.

Configurazione client: `CML_GATEWAY_URL`, origine HTTPS senza credenziali nell’URL. Nessuna connessione SSH diretta dal gioco. Nessuna password Cisco nel progetto.

Client asincrono, richieste serializzate, callback sul game thread con riferimenti deboli, limiti JSON e timeout. Modalità locale esplicita `-ClaraLocalLab`. Test automatici ordinari non devono attivare il laboratorio remoto.

Dieci rotte integrate:

| Metodo/rotta | Uso |
|---|---|
| GET `/api/health` | Stato CML/AI |
| POST `/api/cml/bootstrap-demo` | Avvio laboratorio |
| POST `/api/cml/room-sync` | Attivazione stanza e stop altre |
| POST `/api/rack/patch` | Adattatore legacy porte |
| POST `/api/terminal/command` | Riga o sequenza preparata sulla console |
| POST `/api/soc/sinkhole` | Blocco/sblocco dominio nel gateway |
| POST `/api/ai/interact` | Indizi/dialogo AI |
| GET `/api/mission/specs` | Specifiche missioni |
| POST `/api/mission/verify` | Confronto consegna con specifica |
| POST `/api/mission/generate` | Anteprima o deploy esplicito scenario |

Il codice di riferimento del collega è `kappino/cisco-cyber-office`, ramo `feat/fuori-rete-phaser`, commit letto inizialmente `decc6d764917c6f847ac78578c3b86e2be46f7fb`. Copia locale: `work/references/fuori-rete-phaser/`. Il branch contiene API topologia/WebSocket più recenti che sul server verificato davano404: non supporre che il deploy live coincida col sorgente.

### Stanze e nomi reali

| Area del gioco | room_id | Switch predefinito |
|---|---|---|
| Reception/uffici/briefing/reti | office | SW-PIANO1 |
| Corridoio Y540–760 | corridor | SW-ACC |
| Sala apparati X≥1350,Y≥760 | datacenter_vault | SW-DC1 |

Nodi office letti dal console server: SW-PIANO1, RTR-UFFICIO, PC-MARKETING, PC-CONTABILITA, STAMPANTE. **SW-CORE-01 del manuale iniziale non è il nodo office corretto.**

La licenza/lab demo lavora con massimo5nodi attivi; il cambio stanza ferma gli altri laboratori. Il gateway e la telemetria sono condivisi: più giocatori/sviluppatori si possono disturbare. Il `session_id` del client non crea da solo isolamento sul server. Usare modalità locale per lavorare sulla grafica.

### Problema del patch endpoint e soluzione attuale

La risposta legacy osservata era `mode: live_cml`, ma `node: SW-CORE-01`, `link_id: null`. Non è una conferma di cablaggio.

Il nuovo adattatore:

1. Convalida la risposta legacy, nodo corretto e identificatore link.
2. Se non è attendibile, usa una sequenza IOS preparata e limitata a quattro interfacce di SW-PIANO1.
3. Mappa Gi1/0/1→sw-g1→Ethernet0/0, Gi1/0/2→sw-1→Ethernet0/1, Gi1/0/3→sw-2→Ethernet0/2, Gi1/0/4→sw-3→Ethernet0/3.
4. Invia un unico pacchetto di byte: Ctrl-Z, enable, configure terminal, interface consentita, shutdown/no shutdown, end e lettura filtrata. Timeout30s per questa richiesta.
5. Legge nuovamente la stessa interfaccia con una richiesta indipendente e conferma soltanto il risultato coerente.
6. Se trova `consoles>`, effettua un solo tentativo di collegamento al percorso office noto e poi richiede ancora prove IOS. L’ack del trasporto da solo non basta.
7. Accoda e accorpa richieste rapide sulla stessa porta; la console e il cambio stanza di questo client attendono la fine dell’operazione.

La prova è marcata **IOS_ADMIN**, distinta da **CML_LINK**. **Abilita/disabilita realmente l’interfaccia IOS; non ricabla liberamente la topologia CML.** Non spacciare questa soluzione per controllo completo di ogni cavo/porta del modello. Le altre porte e la fibra restano del modello locale.

Le richieste per riga fallivano perché la console condivisa poteva cambiare modalità tra conf t e interface. Il pacchetto unico riduce questa gara; **non è un lock server** contro altri client. Per robustezza definitiva serve correggere/aggiornare il gateway e integrare i suoi endpoint di topologia/console byte-stream.

### Prove e risposte da non confondere

- Health online non prova una console IOS funzionante.
- `source: live_cml` da solo non basta: `consoles>` e `SW-PIANO1# (OK)` senza dati sono rifiutati.
- netAgent verifica la consegna di comandi/azioni, non legge autonomamente la running-config.
- Il backend sostituisce `player_commands: []` con la telemetria globale: il client invia un commento neutro non vuoto quando non ha comandi propri.
- Stato IOS_ADMIN separato da patched_ports con prova di link; non far superare una verifica di cablaggio fisico usando soltanto un no shutdown.
- Sinkhole significa azione DNS del gateway, non prova di una sottoscrizione Cisco Umbrella reale.
- Indizi AI mantengono Luca/Jonny/Alessia come nomi del gioco; ID compatibili backend marco/ciro/rosaria.
- Generazione e deploy hanno comandi separati; un deploy sospende l’autosincronizzazione per non sostituire subito il lab creato.

## 13. Prove realmente eseguite

| Prova | Risultato e limite |
|---|---|
| Build Mac Development | PASS, ultimo log `work/ethernet-terminal-build.log` |
| Cinque test Clara.SwitchCLI | PASS nelle prove precedenti; non equivalgono a IOS completo |
| StoryCheck | PASS: cavi, portatile, MAC, console, fibra, challenge e riavvio |
| Interazione Ethernet | PASS: primo capo switch→secondo capo portatile in mano, stessa selezione cavo, fase5→6 |
| Picking portatile trasportato | PASS con vere tracce ECC_Visibility su Ethernet e ConsoleUSB dell’anteprima |
| Layout dopo nuovo router | PASS:10destinazioni, cinque passaggi, inspector e collegamenti |
| GatewayCheck HTTP da Unreal | PASS: health/bootstrap/office/lettura IOS/specs/verifica negativa |
| Nuovo GatewayPortCheck | **PASS al primo tentativo**: Ethernet0/1, IOS_ADMIN, lettura indipendente; log `work/ethernet-gateway-port-check.log` |
| Desktop Linux a icone | Visto a schermo nella versione precedente |
| Nuovo terminale scuro e router finale | Implementati/importati e compilati; **verifica visiva finale ancora parziale** per timeout CUA |

Report grezzi: `Saved/Logs/GatewayIntegrationReport.json`, `Saved/Logs/GatewayPortRepairReport.json`, esclusi da Git. Unreal può salvarli UTF-16 con BOM, non sempre UTF-8.

L’ultima prova grafica è stata avviata con `-ClaraFreshStory -ClaraEthernetDemo`. All’inizio il renderer mostrava «Sky Light waiting on Shaders, Meshes, Textures for final capture» e scena temporaneamente scura. Attendere il caricamento prima di giudicare il risultato, poi verificare che il problema sparisca. L’ultimo log `work/ethernet-router-play.log` termina con **chiusura normale** (`UGameEngine::HandleExitCommand`), non con crash. Non presumere che il gioco sia ancora aperto.

## 14. Crash dell’importatore: diagnosi già fatta

Il report mostrato dall’utente apparteneva al processo dell’editor che eseguiva uno script di importazione, dopo il salvataggio e durante la chiusura. Stack: notifica Slate di importazione→distruzione tardiva→ICU/ubidi_close_64. Stessa firma in import precedenti. Non era una prova di crash nel gameplay.

Anche l’import Claude ha terminato con asset/report/JSON validi, poi ha ripresentato il crash post-quit. Un drain120SlateTicks+12s **non lo ha risolto**. Non dichiararlo corretto. Verificare report di import e asset salvati, non soltanto exitcode del processo. Nessun report è stato inviato a Epic.

Per futuri import: usare quando possibile una sessione editor persistente, verificare gli errori Python e non caricare script mentre si ricompila la DLL del progetto. Non reiterare import completi senza motivo.

## 15. Bug e miglioramenti ancora da affrontare

### P0 — blocchi o perdita di lavoro

- Riprovare manualmente il percorso completo dopo le ultime correzioni. I test automatici passano, ma non certificano ogni gesto reale del principiante.
- Verificare che non restino blocchi alle porte, sedie/arredi o punti di posa del portatile.
- Controllare il salvataggio: attualmente si conservano soprattutto le giornate completate, **non fase, cavi, inventario e configurazioni a ogni istante**. Un riavvio può far rifare attività. Non dichiarare presente un salvataggio completo.
- Controllare il comportamento dopo completamento Giorno2: il caricamento guarda il flag Giorno1 e può ripartire dal Giorno2 invece di riprendere esattamente la fine.

### P1 — chiarezza e funzionalità

- Prova reale del clic: selezionare ogni presa, cambiare apparato mantenendo il primo capo, errore di cavo, porta occupata, scollegamento e ripetizione.
- Escludere che micro-movimenti del mouse vengano interpretati come drag e annullino un clic.
- Rendere immediato capire quale lato dell’apparato si sta guardando e quali prese sono compatibili.
- Tutor: indizi veramente graduati, meno testo, osservazione dell’errore del giocatore e possibilità chiara di ricominciare uno switch senza azzerare tutta la storia.
- Rivedere il percorso di Luca: niente compenetrazione di arredi, inseguimento macchinoso o passaggi ostruiti.
- Chiarire stati locale/remoto/avvio/errore senza mostrare dettagli di implementazione ovunque.
- Stabilizzare il gateway con il suo gestore della console e introdurre sessioni isolate prima della co-op o di più giocatori.
- Topologia remota e byte-stream: parte ancora da integrare, non un semplice interruttore da attivare.

### P2 — grafica e presentazione

- Verificare il router Claude nel motore da davanti, dietro e lato prese: testi, luci, PBR, normali e profondità connettori.
- Lo switch resta la ricostruzione precedente e può essere il prossimo asset da migliorare/sostituire.
- Nessuna superficie con riflessi triangolari, testo specchiato o verniciatura troppo rumorosa. Evitare di compensare mesh errate con luce eccessiva.
- Terminale scuro pulito, layout ridimensionabile, testo leggibile, niente pulsanti giganti o molte notifiche sovrapposte.
- Verificare che il taccuino sembri un quaderno e che la borsa resti compatta; non riportare il vecchio elenco verticale di schede.
- Regolare illuminazione/esposizione e caricamento degli shader prima di aggiungere altri asset pesanti.
- Personaggi realistici definitivi sono un lavoro futuro. I manichini non sono il risultato finale, e le prime facce procedurali sono state rifiutate.

## 16. Come segnalare i bug nella prossima chat

Per ogni bug produrre:

1. Titolo e priorità P0/P1/P2.
2. Build/commit, punto della storia e modalità locale/remota.
3. Passaggi esatti da un avvio riproducibile.
4. Risultato atteso e osservato.
5. Screenshot/video della build reale o estratto di log pertinente, senza segreti.
6. Causa verificata oppure ipotesi dichiarata come tale.
7. Correzione minima e prova che impedisca la regressione.

Non riscrivere il gioco per un difetto locale. Non presentare un’immagine generata, un render Blender o un mockup come screenshot di gameplay. Non inventare percentuali di completamento o «tutto perfetto».

## 17. Sequenza di lavoro consigliata

1. Controlla Git, commit e documenti prima di toccare file o cambiare ramo.
2. Compila la versione attuale; evita Unity e più istanze pesanti.
3. Avvia in `-ClaraLocalLab -ClaraFreshStory` per provare l’esperienza base senza interrompere il lab condiviso.
4. Fai il Giorno1 da zero come un principiante: colloquio→badge→Luca→portatile→cavi→Ethernet→ping→console→MAC.
5. Prova anche `-ClaraEthernetDemo` per isolare il problema già corretto.
6. Apri il terminale e osserva il tema scuro reale; poi controlla il router Claude in primo piano e l’allineamento di tutte le prese.
7. Registra bug prima di proporre il piano; correggi prima i blocchi, poi UI e grafica un oggetto alla volta.
8. Solo dopo passa ai test CML, coordinando la sessione remota condivisa. Verifica dati IOS, non soltanto HTTP200.
9. Aggiorna Docs/VERIFICHE.md con prove e limiti, poi apri una PR chiara per il team.

## 18. Collaborazione GitHub e materiali pubblicabili

L’utente ha richiesto il repository **pubblico** per permettere al gruppo di lavorare. Tutti possono clonare, forkare e proporre PR. L’invito write a **kappino** è stato inviato e risultava in attesa di accettazione. Per altri tre partecipanti l’utente comunicherà i nomi GitHub più avanti: non affermare che siano già collaboratori write. Non mettere email personali nel repository pubblico.

Conservare esclusioni di `.gitignore`:

- Binaries, Intermediate, Saved, DerivedDataCache, log e configurazioni private.
- Asset Epic Mannequins: non distribuiti pubblicamente, installazione locale tramite `Scripts/setup_epic_mannequins.py --engine-root <UE_5.8>` (25asset, propria installazione autorizzata).
- Ritratti/prototipi personali in Content/Characters/Prototypes.
- Tessuti e derivati Seaprop, SourceAssets/ChairMaterials e Content/OfficeAssets/ChairMaterials.
- Renderpeople e altre risorse non redistribuibili.

I materiali originali CC0 della sedia Innerscene sono il fallback della copia pubblica senza tessuti privati. Non cancellare gli asset esclusi sul Mac presumendo che siano nel backup GitHub.

Provenienza in LICENSES.md e manifesti SourceAssets: ricostruzioni originali, PolyHaven, Innerscene ed Epic hanno condizioni distinte. Non attribuire automaticamente licenza libera a tutto perché il repository è pubblico.

Usare branch per singolo intervento, niente force-push su main, una persona per asset binario, PR con prove concrete. Non modificare gli stessi header in parallelo senza coordinamento. Il prompt per Claude chiede asset in una cartella separata; l’integrazione nel mondo principale è responsabilità del coordinatore.

## 19. Strumenti e comportamento operativo

Per browser usare il Chrome abituale già autenticato, non profili di automazione nuovi. L’account Google di riferimento è info@rowtech.it; GitHub verificato è FeroneAntonio. Preferire connector/CLI pertinenti; per UI del Mac usare gli strumenti CUA disponibili.

Il controllo CUA della finestra Unreal ha timeout intermittenti. Spesso l’albero mostra solo la finestra e non i widget Slate. Talvolta il primo screenshot funziona e il successivo va in timeout: non dichiarare completata una verifica visiva non osservata. Le prove native del progetto e i log sono utili, ma non sostituiscono un collaudo manuale completo.

Non avviare Rizzo Flow per scrivere documenti, dialoghi o fare pianificazione generale; consuma5–6GB e non serve a questi compiti. Seguire le istruzioni globali dell’utente se un futuro compito ne richiede davvero l’uso.

Mantenere aggiornamenti brevi durante il lavoro e consegnare risultati concreti. Una nuova richiesta durante una lavorazione in genere corregge la direzione, non cancella l’obiettivo precedente.

---

**Punto di ripartenza:** il codice delle correzioni è integrato su GitHub, il router di Claude è importato e collegato ai dati delle porte, Ethernet passa il test di interazione e la porta IOS passa il test remoto. La prossima chat deve partire dalla build reale, completare la verifica visiva/manuale, registrare i bug osservati e migliorare grafica e chiarezza senza reintrodurre ostacoli o interfacce ingombranti.
