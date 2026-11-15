# TNES × CML: specifica del prodotto finale

Nov 15, 2026 · @Gianluigi

TNES diventa un gioco in cui ogni apparato fisico, ogni cavo, ogni LED e ogni console corrisponde a un dispositivo vero in Cisco CML, e la storia di Luca, Gaia, Alessia e Jonny si gioca su IOS reale. Questo documento dice cosa va fatto e perché, non come scriverlo.

## 1. Obiettivo e definizione di finito

Il prodotto finale è TNES (l'app Unreal di Antonio) in cui, in modalità "laboratorio reale", **nulla della rete è simulato dal gioco**: Unreal disegna, CML decide. Il motivo è la challenge stessa: Cisco e Clara premiano un percorso formativo su apparati veri, e un simulatore scritto da noi, per quanto curato, è ciò che Packet Tracer fa già meglio.

Il prodotto è finito quando, in una partita registrata senza tagli:

1. **Apparati 1:1** — ogni router, switch, PC e server visibile e interattivo nel giorno corrente è un nodo di un lab CML acceso, con lo stesso nome; nessun nodo CML del lab manca nel mondo 3D.
2. **Cavi 1:1** — collegare, staccare o spostare un cavo nel gioco crea, toglie o sposta il link in CML; la topologia di CML e quella del gioco coincidono in ogni momento.
3. **LED veri** — i LED di porta e di sistema mostrano lo stato letto da IOS (STP, err-disabled, shutdown, traffico), con i tempi reali: ambra \~30 s dopo un nuovo collegamento, verde lampeggiante con traffico.
4. **Console vera** — il terminale che si apre col cavo console sulla porta CON giusta è la console seriale del nodo CML: Tab, `?`, errori, conferme e domande interattive sono quelle di IOS.
5. **Esiti veri** — ping, traceroute, tabella MAC, ARP, show ip route e il TEST di Gaia leggono il risultato da CML; nessun "Reply from" o "OK" prodotto dal gioco.
6. **Cyber con dati veri** — scansioni, ACL, accessi e allerte dei giorni cyber avvengono nei lab CML e le allerte arrivano da Splunk che riceve i log dei dispositivi.
7. **Onestà dichiarata** — ciò che non esiste in CML (fibra ottica, alimentazione, torre OSI, carte ARP) è esplicitamente didattico e lo si vede.
8. **Si avvia** — dal Mac (pacchetto Shipping) e da Linux, con un solo backend e un solo giocatore alla volta sul CML condiviso.

## 2. Da dove partiamo

I pezzi per il prodotto finale esistono quasi tutti, ma in tre codebase che oggi non si parlano davvero: la storia completa è in Unreal ma gira sul simulatore interno, il motore 1:1 con CML è nel backend ma lo usa solo il gioco Phaser.

| Pezzo (stato al 15/11 mattina) | Cosa porta al prodotto finale | Cosa gli manca |
| --- | --- | --- |
| `clara-unreal` `feat/tnes-demo-mac` | Storia G1-G3 network e G1-G3 cyber, personaggi, voci (309/332 battute), intro, taccuino, app Mac Shipping verificata | In modalità predefinita tutto è simulato; il live (`-ClaraLiveCML`) usa solo le 10 rotte vecchie; il G3 è un simulatore C++; cavi live = shutdown/no shutdown su 4 porte di SW-PIANO1 (Et0/0 è il trunk dell'ufficio) |
| `clara-unreal` `feat/linux-ux-backend-handoff` | Build Linux pulita con Clang, WASD su Linux/Wayland | Correzioni non limitate a Linux; `ResumeWalk` chiamato in 20 punti; note API con percorsi errati (vedi sezione 6) |
| Backend `feat/cml-1to1` (PR #1) | Stanze 1:1, cavi veri (connect/move/unplug/plug con rollback), LED da STP/EtherChannel/err-disabled, Verifica sulla running-config, console WebSocket con mutex e buffer, 37 test dal vivo | Stanze non sono i giorni dei copioni |
| Backend `test/cml-engines-sprint` (Enzo) | Console HTTP a sessione, password IOS, autosave/reload, indizi a 3 livelli + IA, traduttore Windows, guasti, client Splunk | Test solo simulati; controllo LED e SSH/Telnet rotti; Ctrl-C tolto dalle letture automatiche; guasti pericolosi nelle stanze con salvataggio; allerte Splunk vuote (correzioni F1-F10 già specificate) |
| CML 2.10.1 | IOL router/switch, Alpine, Splunk ufficiale `splunk-10-6-1-1` (si avvia), connettori NAT e System Bridge, API di cattura pacchetti | 5 nodi accesi, 7,7 GB di RAM, un solo CML per tutto il team |
| Splunk | Container `clara-splunk` sull'host + nodo in CML | Nessun log di CML arriva ancora; nodo CML senza interfacce |
| Copioni G1-G3 e PDF cyber | Contenuto didattico completo | Interfacce GigabitEthernet, comandi Windows, sfida G2 a 6 nodi, nomi incoerenti |

## 3. Architettura finale e principi

Tre livelli con ruoli netti: **Unreal mostra e raccoglie i gesti, il backend traduce e protegge, CML (e Splunk dentro CML) è la rete**. Ogni regola sotto nasce da un problema già visto in questi giorni.

1. **Unreal (TNES)** — mondo 3D, storia, dialoghi, taccuino, pulsanti. Invia gesti ("cavo da R1 Gi0/1 a SW1 Gi1/0/2", "tasti sulla console di SW-A", "premi TEST") e disegna ciò che il backend gli dice. Non decide mai l'esito di un'operazione di rete in modalità reale.
2. **Backend (cisco-cyber-office, uno solo, dietro api-cml.kappino.me)** — unico che parla con CML. Conosce i lab dei giorni, traduce porte e nomi del gioco in nodi e interfacce CML, serializza l'accesso alle console, legge LED e configurazioni, esegue la Verifica, interroga Splunk.
3. **CML** — un lab per giorno (massimo 5 nodi), più Splunk come nodo del laboratorio nei giorni cyber. Le console passano dal console server di CML.

**Principi**

- **Una sola fonte di verità per la rete.** In modalità reale il simulatore di Unreal non deve rispondere al posto di CML, nemmeno come ripiego silenzioso: se CML non risponde, il gioco lo dice. Perché: un "OK" falso in un video per Cisco distrugge la credibilità di tutto il resto.
- **Due modalità dichiarate.** "Laboratorio reale" (CML) e "laboratorio locale" (simulatore, per sviluppo, prove grafiche e piano B). L'interfaccia mostra sempre in quale si è. Perché: oggi le due cose si mescolano e nessuno sa cosa sta guardando.
- **Un solo regista di CML.** Un backend, un giocatore alla volta, una prenotazione della sessione. Perché: con 5 nodi, due client che cambiano stanza si spengono a vicenda (successo più volte il 14/11).
- **Contratto API scritto e versionato.** Un solo documento (nel repository del backend) con rotte, campi e significato degli stati, da cui Unreal e Phaser leggono. Perché: le note di oggi avevano percorsi sbagliati e Antonio avrebbe scritto codice che dà 404.
- **Eventi al posto delle domande ripetute.** LED e stato del lab arrivano a Unreal come aggiornamenti quando cambiano, con una lettura periodica di sicurezza. Perché: chiedere ogni 2 secondi intasa le console condivise; aspettare 8 secondi rende i LED finti.
- **La console del giocatore è sacra.** Le letture automatiche (LED, Verifica) non devono mai eseguire una riga scritta a metà, cambiare modalità al giocatore o far comparire comandi estranei; se serve la linea, la congelano e la restituiscono com'era.
- **Ogni giorno riparte pulito.** Il lab del giorno si ricrea dalla sua definizione: niente stati sporchi del giorno prima, guasti sempre quelli voluti.

## 4. Mappatura 1:1 giorno per giorno

Ogni giorno della storia diventa un lab CML con gli stessi apparati che il giocatore vede; il limite di 5 nodi decide cosa è un nodo e cosa resta scenografia. Le porte dei modelli 3D (Catalyst 2960X Gi1/0/x, router 1941 Gi0/x) sono collegate alle interfacce IOL (Ethernet0/x) da una tabella dichiarata, visibile anche al giocatore nel primo piano dell'apparato.

| Giorno | Nodi CML (≤ 5) | Cosa è vero in CML | Cosa resta didattico nel gioco |
| --- | --- | --- | --- |
| Network 1 · Connettività | SW1, R1, PORTATILE, PC2 | Cavi, LED ambra→verde da STP, console, ping, tabella MAC, ipconfig | Ordinare i cavi nelle scatole, fibra ottica (R1 1941 non ha SFP: il copione va corretto), torre OSI e TCP/IP |
| Network 2 · Switch di prova | SW-PROVA, PORTATILE, R1 | Configurazione completa di IOS: nome, password, banner, SVI, SSH, salvataggio | Carte ARP, foglio di subnetting /26 |
| Network 2 · Sfida di Gaia | R1, SW-A, SW-B, PC2, PC3 (il portatile fa solo console) | Cavi da rifare, due switch da configurare, guasto IP su PC3, TEST di Gaia, riavvio con "Non hai salvato" | Allarme e luci della scena |
| Network 3 · Routing | R1, SW1, R2, SW2, PC2 | Interfacce, rotte statiche e di default, show ip route, traceroute, collaudo con 4 guasti | Spiegazione del TTL |
| Cyber 1 · Porte | Server servizi, PC-LUCA, PC-SCANNER, switch + Splunk | Servizi veri su 21/22/23/443/8080, scansione vera, chiusura servizi verificata | Bivio Red/Blue, barre |
| Cyber 2 · ACL | FW-RTR, PC-RECEPTION, PC-AULA (anche "ospiti" con un secondo IP), server prenotazioni + Splunk, switch non gestito | ACL IOS vera, ordine delle regole, traffico bloccato registrato in Splunk | Trascinamento delle regole come interfaccia sopra la ACL vera |
| Cyber 3 · Credenziali | FW-RTR, PC-ATTACKER, server con pannello web, switch + Splunk | Login falliti e riuscito, pagina in chiaro e cifrata, pacchetto con password catturato da CML | Lista didattica di credenziali |
| Bonus CCNA 2 | Stanze attuali ufficio, corridoio, datacenter | VLAN, trunk, router-on-a-stick, EtherChannel, STP | — |

**Regole della mappatura**

- Il nome del nodo CML è quello del copione e del cartellino 3D (SW-A, R2…): niente nomi diversi tra mondo e console.
- Il portatile è un nodo solo nei giorni in cui deve fare ping o SSH; quando serve solo come console non occupa un posto.
- Le prese a muro e i patch panel, se presenti, sono solo il percorso visibile del cavo: il link CML va da apparato ad apparato.
- Il conteggio dei nodi è da confermare per switch non gestiti e connettori esterni (se contano, Cyber 2 va ridotto).

## 5. Filoni di lavoro: cosa e perché

Sette filoni; i primi due sono il cuore (senza, non c'è 1:1), gli altri lo rendono credibile e mostrabile. Dimensione indicativa: S piccolo, M medio, L grande.

### W1 · Backend: un motore unico e affidabile (L)

| Cosa | Perché |
| --- | --- |
| Unire `feat/cml-1to1` e lo sprint di Enzo dopo le correzioni F1-F10, in un solo branch di rilascio | Oggi esistono due backend diversi; la produzione esegue codice provvisorio con difetti noti (LED, SSH/Telnet, Ctrl-C, allerte vuote) |
| Lab dei giorni (sezione 4) come file di stanza, con configurazione target separata dai guasti | La Verifica deve confrontare con la soluzione giusta, non con lo stato iniziale guasto |
| Ciclo di vita per giorno: ricreazione pulita, avvio anticipato durante l'intro, stato "pronto" esplicito | Un lab IOL impiega \~1 min ad avviarsi: va nascosto nella narrazione, non mostrato come attesa |
| Contratto porte: per ogni porta del modello 3D, nodo e interfaccia CML | È il cuore dell'1:1 dei cavi; oggi esiste solo per 4 porte di SW-PIANO1 e in modo sbagliato |
| Console a sessione come unico canale per Unreal, con stato di blocco visibile ("Verifica in corso") e buffer dei tasti | Unreal non ha bisogno di WebSocket; ma deve sapere quando la console è occupata, non ricevere silenzio |
| Flusso di eventi (LED, stato lab, cavi) consultabile in modo economico | LED reattivi senza intasare le console |
| Verifica di Gaia completa: running e startup, LED sulle porte collegate, SSH vero dal PC, traceroute con la sequenza dei salti, ACL in ordine, frasi senza soluzione | Il TEST è il momento-chiave dei giorni 2 e 3 |
| Password impostate dal giocatore gestite in tutte le letture automatiche | Dopo il G2 il backend altrimenti resta chiuso fuori dagli apparati |
| Prenotazione della sessione (un giocatore alla volta) e rifiuto chiaro per il secondo | Evita le stanze che si spengono a vicenda |
| Contratto API documentato e versionato, con esempi veri | Unreal e Phaser devono implementare contro la verità, non contro note scritte a memoria |

### W2 · Unreal: dal simulatore al client di CML (L)

| Cosa | Perché |
| --- | --- |
| Cavi: ogni collegamento, distacco e spostamento chiama il backend; il cavo resta "in collegamento" finché CML non conferma | 1:1 dei cavi; oggi il live accende e spegne porte invece di creare link |
| LED: materiali di porta e SYST guidati dallo stato del backend (verde, verde lampeggiante, ambra, ambra lampeggiante, verde/ambra alternati, spento) con il motivo leggibile come aiuto attivabile | La lettura dei LED è una lezione dei copioni (G1 scena 4); deve corrispondere a ciò che fa IOS |
| Console: il terminale del portatile collegato con il cavo console alla porta CON giusta apre la console a sessione del nodo; Tab, `?`, cronologia, domande interattive arrivano da IOS | Toglie la necessità di estendere il modello IOS locale per ogni giorno (G3 compreso) |
| Terminale del PC: comandi Windows tradotti dal backend sui nodi Alpine | I copioni sono scritti con ipconfig, arp, tracert |
| TEST di Gaia e indizi: pulsante → Verifica; indizi a 3 livelli → backend (e IA solo per riformulare) | Esiti veri e indizi coerenti con l'errore reale del giocatore |
| Giorno → lab: il cambio di lab avviene all'inizio del giorno, non attraversando le stanze | Il room-sync ai bordi delle aree spegneva i lab degli altri |
| Stato visibile: laboratorio reale o locale, CML pronto, console occupata, errore | Mai un'azione che sembra riuscita e non lo è |
| Rimuovere il ripiego IOS\_ADMIN su SW-PIANO1 | Spegne il trunk dell'ufficio e non è un cavo |
| Salvataggio dei progressi coerente con il lab (giorno e fase; la rete la ricrea il backend) | Oggi un riavvio fa rifare attività e può ripartire dal giorno sbagliato |

### W3 · Storia e copioni allineati a IOS (M)

| Cosa | Perché |
| --- | --- |
| Ogni \[AZIONE\] e \[ESITO\] dei copioni riletto contro IOL 17.18 e Alpine: nomi interfacce, uscite reali, comandi che esistono | Il copione promette esiti che IOS deve poter dare (es. tabella MAC "vuota" non lo sarà con R1 collegato) |
| Sfida G2 a 5 nodi; fibra del G1 resa coerente (pannello o modulo, non SFP sul 1941) | Limiti reali di licenza e hardware |
| Frasi di Luca e Gaia agganciate agli errori IOS veri (chiave RSA senza dominio, `login` mancante, gateway fuori rete) | Il tutor reagisce a ciò che il giocatore ha davvero sbagliato |
| Nomi unici ovunque: TNES, Alessia, Luca, Gaia, Jonny (gioco, backend, IA, slide) | Oggi convivono Jonny, Davide, Jonathan, Elena |

### W4 · Piattaforme (M)

| Cosa | Perché |
| --- | --- |
| Integrare le correzioni del branch Linux in forma sicura (sezione 6) | Il team sviluppa anche su Linux |
| Build ripetibili: Mac Shipping (già verificata), Linux con lo script di avvio, Windows facoltativo | La demo non deve dipendere da un solo computer |
| Compilazione senza avvisi con Clang come regola | Gli errori corretti oggi su Linux non devono tornare |

### W5 · Cyber e Splunk dentro CML (L)

| Cosa | Perché |
| --- | --- |
| Lab dei tre giorni cyber dai lab di Enzo, entro i nodi disponibili | Le giornate cyber oggi sono solo simulate |
| Splunk come nodo CML collegato alla rete del giorno, ingresso syslog attivo, `logging host` sui dispositivi | Splunk è un prodotto Cisco: deve analizzare i log veri della rete CML, non un contenitore scollegato |
| Pannello Splunk nel gioco con le ricerche vere; nessuna allerta inventata | Coerenza con il principio della sezione 3 |
| Percorso Blue che legge gli eventi veri; percorso Red che li genera (scansione, accessi) | Le due prospettive si vedono sugli stessi dati |
| Cattura pacchetti di CML per mostrare la password in chiaro (Cyber 3) | Il momento "Wireshark" diventa vero |

### W6 · Qualità e prove (M)

| Cosa | Perché |
| --- | --- |
| Collaudo automatico dal vivo per ogni giorno: lab pronto, cavi, LED, guasti trovati, Verifica verde dopo le correzioni | I 61 test dello sprint erano tutti simulati e non hanno visto due difetti gravi |
| Prove Unreal estese (GatewayCheck) sulle nuove rotte | Il client va provato contro il backend vero |
| Partita completa da principiante, registrata, per ogni giorno | I test automatici non certificano i gesti reali |
| Obiettivi di reattività: tasto in console visibile in meno di 150 ms, LED aggiornato entro 2 s da un evento, lab pronto entro 90 s | Sono i numeri che rendono il gioco credibile |

### W7 · Video (S)

| Cosa | Perché |
| --- | --- |
| Scaletta: intro TNES, G1 cavi e LED con CML affiancato, G2 TEST di Gaia e riavvio, G3 senza aiuti, cyber con allerta Splunk vera | Ogni scena dimostra un punto della definizione di finito |
| Registrazione con lab già avviati e una sola istanza attiva | Niente attese e niente interferenze |
| Elenco delle affermazioni del video con la prova accanto | Davanti a Cisco si dice solo ciò che si può mostrare |
| Registrazione di riserva in laboratorio locale, dichiarata come tale | Piano B se la rete della sede cade |

## 6. Spunti dal branch Linux (`feat/linux-ux-backend-handoff`)

Il branch porta tre cose utili e insegna tre regole per il prodotto finale.

| Contenuto | Valutazione | Cosa farne |
| --- | --- | --- |
| Correzioni Clang (cicli con riferimento, parentesi esplicite) | Corrette e innocue su tutte le piattaforme | Tenerle; diventano regola: compilazione pulita con Clang prima di ogni rilascio |
| WASD letti direttamente quando l'input normale non arriva (Linux/Wayland con SDL) | Funziona senza raddoppiare la velocità | Tenerla, ma solo su Linux; avvio Linux sempre con `SDL_VIDEODRIVER=x11` nello script |
| Focus forzato sulla vista di gioco e `ResumeWalk` alla chiusura del portatile | Rischio: `CloseLaptopScreen` è chiamata in 20 punti (anche aprendo un apparato e cambiando giorno) e il focus può essere tolto al terminale | Applicarla solo se il portatile era aperto e solo su Linux; prova manuale dei cinque casi (apparato con portatile aperto, cambio apparato, console col portatile in mano, passaggio G1→G2, terminale + tasto destro) |
| `BACKEND_API_NOTES.md` | Percorsi della console a sessione sbagliati (send, read e delete vogliono il session\_id), Splunk descritto come attivo ma senza dati, nessuna avvertenza sui guasti | Sostituirle con il contratto API versionato del backend (W1) |

**Regole che ne derivano**

- Una correzione per una piattaforma resta confinata a quella piattaforma, finché non è provata sulle altre.
- Le note di passaggio tra team si generano dal contratto vero (o si verificano contro le rotte del server), mai a memoria.
- Ogni modifica all'input o alla telecamera va provata a mano sui percorsi della storia, perché i controlli automatici di storia e piantina non la vedono.

## 7. Decisioni aperte

Sei scelte da fare una volta, prima di aprire i filoni: ognuna cambia il lavoro di più persone.

| # | Domanda | Proposta | Perché |
| --- | --- | --- | --- |
| 1 | Nomi delle porte: cartellini 3D GigabitEthernet o nomi IOL Ethernet? | Tenere i modelli Cisco reali con i loro cartellini, mostrare la corrispondenza nel primo piano e usare Ethernet0/x in console e copioni | Il realismo visivo resta, e il giocatore non scrive mai un nome che IOS rifiuta |
| 2 | Il portatile è un nodo CML? | Sì nei giorni con ping o SSH dal portatile (G1, G2 prova); no quando fa solo console (G2 sfida, G3) | Tiene ogni giorno entro 5 nodi |
| 3 | Splunk sempre acceso o solo nei giorni cyber? | Solo nei giorni cyber, lab dei giorni cyber da 4 nodi + Splunk | Occupa un posto e 4 GB di RAM |
| 4 | Fallimento di CML durante la partita | Il gioco si ferma con un messaggio chiaro e propone di continuare in laboratorio locale, dichiarato | Nessun esito falso, ma la partita non muore |
| 5 | Quanti giocatori? | Uno alla volta per CML; la co-op è lavoro futuro (più licenze o più CML) | Limite di licenza reale |
| 6 | Nomi | TNES (azienda e gioco), Alessia, Luca, Gaia, Jonny ovunque | Le voci registrate usano già Jonny |

## 8. Fasi e dipendenze

Si procede per fette verticali: prima un giorno completamente 1:1 dall'inizio alla fine, poi gli altri. Ogni fase si chiude con un controllo che decide se passare alla successiva.

1. **Fase 0 · Fondamenta.** Backend unico con le correzioni F1-F10, contratto API scritto, decisioni della sezione 7 prese, nomi unificati. *Si passa quando:* il collaudo dal vivo della stanza g3 trova esattamente i 4 guasti e diventa verde dopo le correzioni dalla console a sessione.
2. **Fase 1 · Fetta verticale: Network 1.** Unreal con cavi, LED e console veri sul lab G1 (è il giorno che mostra meglio cavi e LED). *Si passa quando:* una partita del G1 registrata mostra a ogni cavo il link in CML, LED ambra poi verde con i tempi veri, ping e tabella MAC letti da CML.
3. **Fase 2 · Network 2 e 3.** Configurazione, TEST di Gaia, riavvio, guasti casuali, traceroute; il simulatore G3 di Unreal resta solo per la modalità locale. *Si passa quando:* TEST di Gaia rosso e poi verde in entrambi i giorni, "Non hai salvato" dopo il riavvio, collaudo G3 senza aiuti.
4. **Fase 3 · Cyber e Splunk.** Lab cyber, Splunk nel laboratorio, pannello allerte, cattura pacchetti. *Si passa quando:* un evento generato in CML compare nel pannello del gioco da Splunk entro 60 s in ciascun giorno cyber.
5. **Fase 4 · Rifinitura e piattaforme.** Correzioni Linux sicure, build Mac e Linux, reattività nei limiti della sezione 5 (W6), partite complete da principiante.
6. **Fase 5 · Video.** Registrazione secondo la scaletta, elenco delle affermazioni con prova, registrazione di riserva.

**Dipendenze principali**

- Unreal (W2) dipende dal contratto API e dai lab dei giorni (W1): senza, Antonio implementa contro rotte che cambiano.
- I copioni (W3) vanno corretti prima delle frasi di Luca e Gaia agganciate agli errori.
- Splunk (W5) dipende dai lab cyber e dalla verifica del conteggio dei nodi.
- Il video (W7) dipende da tutte le fasi che vuole mostrare: si gira solo ciò che ha passato il proprio controllo.

## 9. Rischi

Il rischio maggiore è di processo: tre codebase e più agenti che toccano lo stesso CML e lo stesso backend senza un contratto comune.

| Rischio | Effetto | Contromisura |
| --- | --- | --- |
| Più client o agenti sullo stesso CML | Lab spenti a vicenda, prove falsate | Un backend, prenotazione della sessione, coda scritta per le prove dal vivo |
| Test simulati con formati inventati | Difetti gravi che passano i test (visto nello sprint) | Fixture registrate da CML vero; test che chiamano il codice di produzione |
| Modalità locale scambiata per reale | Affermazioni false nel video | Stato sempre visibile; elenco affermazioni con prova |
| Particolarità IOL (porte libere "connected", no shutdown ignorato al primo avvio, cavo non rilevato) | LED e cavi incoerenti | Già gestite nel backend 1:1; mantenerle durante l'unione con lo sprint |
| Licenza 5 nodi e 7,7 GB di RAM | Giorni o Splunk che non entrano | Mappatura della sezione 4; misura della RAM con Splunk acceso |
| Correzioni per una piattaforma che rompono le altre | Regressioni su Mac a ridosso della demo | Correzioni confinate e provate a mano (sezione 6) |
| Guasti iniettati in stanze con salvataggio automatico | Lab di base rovinati | Guasti solo nei lab dei giorni, che si ricreano puliti |
| Rete della sede o Cloudflare instabili | Demo dal vivo ferma | Registrazione di riserva; video girato con lab già avviati |

## 10. Criteri di accettazione del prodotto finale

Il prodotto è pronto per il video quando ogni casella è spuntata con una prova (registrazione o file di collaudo).

- [ ] Per ogni giorno: elenco nodi del lab CML = elenco apparati interattivi del gioco, stessi nomi
- [ ] Collegare, staccare e spostare un cavo nel gioco cambia il link in CML entro 3 s, con rollback se CML rifiuta
- [ ] LED: ambra \~30 s su un nuovo collegamento senza portfast, verde lampeggiante col ping, ambra su err-disabled, spento su shutdown, aggiornati entro 2 s dall'evento
- [ ] Console: aperta solo con il cavo console sulla porta CON del nodo giusto; Tab, `?`, domande interattive e password funzionano come in IOS; nessun comando automatico visibile al giocatore
- [ ] TEST di Gaia: rosso con frasi senza soluzione, verde dopo le correzioni; "Non hai salvato" dopo un riavvio senza salvataggio
- [ ] Collaudo G3 senza aiuti con guasti casuali, trovati dalla Verifica
- [ ] Giorni cyber: ogni allerta mostrata viene da una ricerca Splunk su log generati in CML
- [ ] Modalità reale e locale sempre distinguibili a schermo; nessun esito di rete prodotto dal gioco in modalità reale
- [ ] App Mac Shipping e build Linux avviate e giocate dall'inizio alla fine di almeno un giorno
- [ ] Nomi unici (TNES, Alessia, Luca, Gaia, Jonny) in gioco, backend, IA e materiali
- [ ] Collaudo automatico dal vivo verde per ogni giorno e partita completa da principiante registrata
- [ ] Elenco delle affermazioni del video con la prova accanto a ciascuna
