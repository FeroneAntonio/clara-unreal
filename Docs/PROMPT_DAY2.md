# Prompt per la sessione «Giorno 2» (secondo PC)

Copia tutto il blocco qui sotto nella chat dell'assistente sul secondo computer. Il lavoro sul Giorno 1, sull'input, sul salvataggio e sulla grafica procede in parallelo sul primo PC: le regole sui file servono a non creare conflitti.

---

Lavori sul progetto Unreal **CLARA** (`https://github.com/FeroneAntonio/clara-unreal`), gioco educativo di reti per principianti, Unreal Engine 5.8, C++ con interfaccia Slate. Il tuo compito è **solo il Giorno 2** della storia (fasi interne 100–110). Un'altra sessione, su un altro computer, sta lavorando nello stesso momento su Giorno 1, input, primo piano degli apparati, salvataggio e grafica. Non ricominciare il progetto e non riscrivere ciò che non è tuo.

## 1. Prima di toccare qualcosa

1. Clona il repository, oppure aggiorna `main` con `git pull --ff-only`. Se su GitHub esiste il ramo `feat/day1-flow-save-hints` non ancora integrato, parti da quello (`git switch -c feat/day2-story origin/feat/day1-flow-save-hints`); altrimenti parti da `main`. Il tuo ramo è `feat/day2-story`.
2. Leggi nell'ordine: `Docs/HANDOFF.md`, `AGENTS.md`, `README.md`, `TEAM.md`, `Docs/VERIFICHE.md`. Poi leggi per intero `Source/ClaraDayOne/ClaraStoryDirector.h/.cpp`, `ClaraSwitchCLI.h/.cpp` e le parti di `ClaraStoryUI.cpp` che disegnano puzzle, foglio subnet, terminale e challenge.
3. Installa i manichini come da README, compila (`ClaraDayOneEditor`, Development; il modulo ha `bUseUnity = false`, non cambiarlo) e leggi l'esito finale della build. Un comando partito non è una build riuscita.
4. Avvia con `-game -windowed -ResX=1280 -ResY=800 -ClaraLocalLab -ClaraStoryDay2`: entri direttamente nel Giorno 2 senza toccare il laboratorio Cisco remoto condiviso. Gioca tutte le fasi 100→110 come un principiante e annota i difetti **prima** di proporre modifiche.
5. Esegui il test integrato con `-ClaraStoryCheck -unattended -nullrhi` e controlla nel log `CLARA_STORY_SELFTEST_PASS`.

## 2. Che cos'è il Giorno 2 oggi

| Fase | Attività |
|---|---|
| 100 | Saluto con Luca |
| 101 | Riordinare le tre carte ARP |
| 102–103 | Svuotare e rileggere la cache ARP, ripopolarla con un ping |
| 104 | Foglio subnet: 192.168.1.70 /26 in sei passaggi |
| 105 | Collegare la console allo switch |
| 106–107 | Configurare SW-PROVA, salvare, rileggere startup-config, ping |
| 108 | Incarico di Gaia nell'area briefing |
| 109 | Challenge: ricostruire la rete del piano 2 nel simulatore del portatile, TEST e riavvio |
| 110 | Rapporto finale |

Vincoli didattici già concordati, da non rompere: la verifica operativa deve poter passare **prima** del salvataggio, perché è il riavvio a dimostrare a che cosa serve salvare; il gateway non serve per un ping nella stessa sottorete; Type 7 non è cifratura forte; SSHv2 con RSA di almeno 2048 bit; in modalità configurazione si usa `do show ...`; il Cisco 1941 non ha porte SFP; un MAC non è un'identità immutabile.

## 3. Difetti già individuati leggendo il codice (verificali giocando, poi correggili)

Blocchi e fluidità:

1. **Il portatile si chiude prima che il giocatore legga il risultato.** In `ExecutePC`, alla fase 103 `arp -a` chiama subito `CloseLaptopScreen()` e passa alla 104; alla 107 il ping riuscito chiude lo schermo allo stesso modo. Il giocatore non vede l'uscita del comando che ha appena scritto. Lascia il terminale aperto e fai parlare Luca sopra lo schermo (il pannello del dialogo sta già sopra lo schermo del portatile).
2. **Sintassi Windows su un desktop Linux.** La fase 102 chiede `arp -d *` e `arp -a`. Accetta come forma principale `ip neigh` e `sudo ip neigh flush all` (o `ip neigh flush dev eth0`), tieni le forme vecchie come alias, aggiorna obiettivo, `help` e completamento con Tab.
3. **Ripresa della partita.** Il salvataggio ora scrive `Saved/ClaraStoryProgress.json` con il campo `phase` (vedi §5), ma per le fasi ≥100 la ripresa riparte dalla 100. Implementa la ripresa reale del Giorno 2: passaggio del foglio subnet, configurazione di SW-PROVA (running e startup), stato della challenge (collegamenti, IP degli host, console, SW-A e SW-B).
4. **Nessun modo di ricominciare un solo esercizio.** Esiste `ResetTrialSwitch()` per SW-PROVA; controlla che sia raggiungibile dall'interfaccia in modo chiaro e aggiungi l'equivalente per la challenge, senza azzerare tutta la storia.

Chiarezza:

5. **Indizi non graduati.** `RequestHint()` per 106/107 mostra direttamente il primo elemento mancante di `ValidateDay2` e per 109 un elenco generico. Segui il modello di `Day1Hint` nello stesso file: tre livelli (dove guardare, che cosa fare, solo alla fine il comando). Parlare con Luca fuori fase deve dare l'indizio corrente, come nel Giorno 1.
6. **La configurazione di SW-PROVA è lunga e non mostra l'avanzamento.** Aggiungi nel terminale o nel taccuino una scheda di lavoro che segna le voci completate (nome, accessi, IP di gestione, SSH, salvataggio) leggendo lo stato reale del modello, non i comandi scritti.
7. **Foglio subnet rigido.** Accetta forme equivalenti (`sì`, `si`, `no`; spazi; `/26`), spiega l'errore con il passaggio sbagliato, non solo «Riprova».
8. **Taccuino.** `GetNotebookText()` ora si riempie per gradi nel Giorno 1 e aggiunge in blocco il testo del Giorno 2 quando `Phase>=100`. Rendi graduale anche la tua parte. Nella pagina sinistra del taccuino (`ClaraStoryUI.cpp`, funzione `Notebook()`) il titolo è fisso su «Il primo messaggio»: usa il titolo del giorno.
9. **Messaggio SSH della challenge**: «La sessione reale richiede il backend Cisco» è un dettaglio di implementazione. Scrivi una risposta credibile per il giocatore e tieni la distinzione locale/remoto dove serve davvero.

Storia:

10. Il Giorno 1 ora anticipa il guasto del piano 2 (Alessia lo nomina, Luca dice che il banco ne è una copia in piccolo, a fine giornata rimanda a Gaia). Riprendi quel filo: Gaia deve raccontare che cosa è successo e perché i due switch sono nuovi; la challenge è la riparazione vera a cui il giocatore si è preparato.
11. Jonny (tutor cyber) oggi dice una sola battuta. Dagli un momento concreto e breve legato a ciò che si configura: perché SSH e non Telnet, che cosa si vede in chiaro. Deve essere un'azione o un'osservazione del giocatore, non una lezione di cinque schermate.
12. Dialoghi brevi, naturali, in italiano con accenti veri (`è`, `più`, `perché`): il vecchio testo usa `e'` e va corretto **nelle righe che tocchi**.

## 4. File: che cosa puoi toccare

Tuoi:

- In `ClaraStoryDirector.cpp` solo le parti del Giorno 2: i `case 100…110` di `GetObjective`, i rami `N>=100` di `SetPhase`, le battute di Luca fase 100 e di Gaia in `HandleInteraction`, `SetupPuzzle` ramo ARP, `SubmitAnswer` ramo 104, i rami 102/103/109 di `ExecutePC`, i rami 106/107 di `RunCommand`, `ResetTrialSwitch`, `StartDay2`, tutte le funzioni `Sim*` e della challenge, i rami ≥100 di `RequestHint` e di `IsMissionTarget`.
- **Codice nuovo in un file nuovo**: `Source/ClaraDayOne/ClaraStoryDay2.cpp`, con le nuove funzioni membro del Director. In `ClaraStoryDirector.h` aggiungi le tue dichiarazioni in un unico blocco in fondo alla sezione pubblica e uno in fondo alla privata, marcati `// --- Giorno 2 ---`. Non riordinare né riformattare le righe esistenti: le righe sono molto lunghe e ogni ritocco estetico diventa un conflitto.
- `ClaraSwitchCLI.*` (è quasi tutto Giorno 2) e i suoi test di automazione.
- In `ClaraStoryUI.cpp` solo le pagine `puzzle` (ARP), `subnet`, `challenge` e gli elementi del terminale usati da SW-PROVA, SW-A e SW-B.

Non tuoi (li modifica l'altra sessione): `ClaraRoomVisitor.*`, `ClaraDeviceInspector.cpp`, `ClaraNetworkDevice.*`, `ClaraOfficeLayout.*`, `ClaraStoryCast.*`, `ClaraStoryHighlights.*`, `ClaraGameMode.*`, `ClaraGateway*`, `ClaraStoryGateway.cpp`, tutto `Content/` e `SourceAssets/`, `Config/`, il `.uproject`, le fasi 0–16 e le funzioni `SaveProgress`, `RestoreProgress`, `RestartStory`, `FinishDialogue`, `Notify`, `Day1Hint`. Se ti serve una modifica lì, scrivila nella descrizione della PR come richiesta invece di farla.

Niente asset binari, niente cambi alla piantina, niente arredi nuovi. Non usare il laboratorio Cisco remoto: lavora sempre con `-ClaraLocalLab`.

## 5. Contratti introdotti dal lavoro sul Giorno 1 (ramo `feat/day1-flow-save-hints`)

- **Salvataggio**: `Saved/ClaraStoryProgress.json` contiene `day1Complete`, `day2Complete`, `phase`, `owned`, `sorted`, `completed`, `links` (`a`,`pa`,`b`,`pb`,`kind`), `sfp`, `macs`, `laptop`, `player`. Si scrive da solo ogni 4 secondi se cambia. `RestoreProgress()` per le fasi ≥100 chiama `StartDay2()` e, se `day2Complete`, mostra il rapporto finale. Per la ripresa del Giorno 2 aggiungi **un solo oggetto** `"day2": {...}` nel JSON e due funzioni tue (`SaveDay2(FJsonObject&)`, `RestoreDay2(const FJsonObject&)`) in `ClaraStoryDay2.cpp`; indica nella PR le due righe da cui vanno chiamate. Il salvataggio è disattivato con `-ClaraStoryCheck`, `-ClaraFreshStory`, `-ClaraStoryDay2` e nei test senza finestra: per provare la ripresa avvia senza quei flag.
- **Tabella MAC**: `show mac address-table` ora scrive i MAC in notazione Cisco (`02aa.0000.0010`). Il confronto con `ip addr` (`02:aa:00:00:00:10`) è parte della lezione.
- **Ping del PC**: uscita in stile Linux (`64 bytes from … icmp_seq=1`, `0% packet loss`). Il ping IOS dentro `ClaraSwitchCLI` resta con i `!`.
- **Dialoghi**: `Esc` su un dialogo lo porta a termine con `FinishDialogue()` invece di abbandonarlo; con una scelta a schermo resta sull'ultima battuta.
- **Rapporto**: `N` alle fasi 16 e 110 riapre il rapporto; lì c'è «Nuova partita dal Giorno 1» (`RestartStory()`).
- **Toast**: la durata di `Notify` dipende dalla lunghezza del testo (6–15 s).
- I tasti di studio (R, 0–8) non sono più attivi nella storia, salvo `-ClaraDevKeys`.

## 6. Come consegnare

- Commit piccoli e leggibili sul ramo `feat/day2-story`, uno per problema. Niente `push --force`, niente commit su `main`.
- Per ogni difetto: passaggi per riprodurlo, risultato atteso e osservato, causa verificata nel codice, correzione minima, prova che non regredisca. Estendi `RunSelfTest()` (solo nella parte challenge/Giorno 2) e i test `Clara.SwitchCLI`.
- Prima della PR: build riuscita, `-ClaraStoryCheck` PASS, una partita reale 100→110 in `-ClaraLocalLab`. Aggiungi a `Docs/VERIFICHE.md` una sezione «Giorno 2» che separa ciò che hai **visto a schermo**, ciò che è coperto solo da **test automatici** e ciò che **non hai potuto verificare**.
- Apri una PR verso `main` con: problema, comportamento finale, file toccati, prove, limiti aperti, eventuali richieste per i file non tuoi.
- Non dichiarare «finito» o «perfetto». Non presentare la simulazione locale come una sessione Cisco reale. Non inventare percentuali di completamento.

## 7. Ordine di lavoro consigliato

1. Punti 1–2 del §3 (blocchi di fluidità), con test.
2. Indizi graduati e scheda di avanzamento (punti 5–6).
3. Ripresa della partita nel Giorno 2 (punto 3) e ripartenza del singolo esercizio (punto 4).
4. Storia: Gaia, Jonny, taccuino graduale (punti 8, 10, 11, 12).
5. Rifinitura del foglio subnet e dei messaggi (punti 7, 9).
6. Verifiche, `Docs/VERIFICHE.md`, PR.
