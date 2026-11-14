# Gateway CML — integrazione Unreal

Base URL predefinita: `https://api-cml.kappino.me`. Si può sostituire con `CML_GATEWAY_URL`; si accettano origini HTTPS senza credenziali nell’URL. Lo schema pubblicato è su `/openapi.json`, la documentazione su `/docs`.

Il riferimento funzionale è il branch `feat/fuori-rete-phaser` di `kappino/cisco-cyber-office`, commit `decc6d764917c6f847ac78578c3b86e2be46f7fb`. Non si incorpora Phaser nel renderer Unreal: il client nativo riusa il gateway e i contratti delle attività.

## Avvio e uso

Il gioco contatta health, bootstrap e room-sync senza bloccare il rendering. T apre il desktop Linux simulato; l’app Laboratorio Cisco espone console remota, verifiche, indizi, azioni DNS e generazione scenari. Gli esercizi guidati dei Giorni 1 e 2 conservano il proprio modello locale, distinto dalla console remota. Il desktop non è una VM Linux.

`-ClaraLocalLab` disattiva la rete, utile a chi lavora sugli asset senza interferire con il laboratorio condiviso. Non ci sono password CML nel client: ogni operazione passa dal gateway.

Questa infrastruttura demo ha una sessione e telemetria condivise: un cambio stanza ferma gli altri laboratori. Per sviluppare in parallelo, usare la modalità locale e riservare una sola sessione alla prova CML.

## Stanze e nodi

| Area Unreal | room_id | Switch predefinito |
|---|---|---|
| Uffici, reception, briefing, laboratorio reti | office | SW-PIANO1 |
| Corridoio Y 540–760 | corridor | SW-ACC |
| Sala apparati X ≥ 1350, Y ≥ 760 | datacenter_vault | SW-DC1 |

Il nome `SW-CORE-01` indicato nel primo manuale non esiste nell’office attualmente esposto. La console di `SW-PIANO1` ha restituito dati IOS reali: Ethernet0/0…Ethernet1/3 e Vlan99. Il nome del nodo resta modificabile nell’app.

Il cambio stanza è stabilizzato per evitare chiamate ripetute ai bordi. Le richieste sono asincrone e serializzate; una risposta superata non viene presentata come risultato della stanza corrente.

## Porte del banco

| Porta modello 3D | port_id gateway | Interfaccia IOL dichiarata |
|---|---|---|
| Gi1/0/1 | sw-g1 | Ethernet0/0 |
| Gi1/0/2 | sw-1 | Ethernet0/1 |
| Gi1/0/3 | sw-2 | Ethernet0/2 |
| Gi1/0/4 | sw-3 | Ethernet0/3 |

Questa è una mappatura esplicita di abilitazione delle quattro porte, non identità del modello hardware Cisco con IOL. Non cambia liberamente i due estremi della topologia CML. I collegamenti sulle altre porte restano locali. All’avvio si registra soltanto lo stato dei cavi: non si spengono automaticamente tutte le porte remote. Le transizioni successive inviano patch e si annota la conferma remota solo dopo una risposta valida.

## Contratti implementati

| Operazione | Endpoint | Timeout |
|---|---|---|
| Stato | GET /api/health | 10 s |
| Avvio | POST /api/cml/bootstrap-demo | 30 s |
| Stanza | POST /api/cml/room-sync | 30 s |
| Porta | POST /api/rack/patch | 10 s |
| Riga console | POST /api/terminal/command | 10 s |
| DNS | POST /api/soc/sinkhole | 10 s |
| AI | POST /api/ai/interact | 30 s |
| Specifiche | GET /api/mission/specs | 10 s |
| Verifica | POST /api/mission/verify | 10 s |
| Generazione | POST /api/mission/generate | 90 s |

Non si disabilita la verifica TLS. HTTP non valido, JSON malformato, risposta eccessiva, errore di trasporto e `fallback_sim` non diventano successi CML. Anche un output `consoles>` viene rifiutato: è la console server, non IOS. Le etichette distinguono stato del gateway e risposta IOS riconosciuta.

L’API pubblicata è a righe, mentre il branch include anche una console WebSocket e API topologia più recenti non presenti nello schema live verificato. Tab e ? del terminale didattico locale non vanno dichiarati completamento remoto interattivo CML. Per quello occorre esporre e integrare il canale byte-stream del branch.

## Verifica e AI

Il verificatore netAgent confronta comandi consegnati e stato delle azioni con una specifica. Non è una lettura indipendente di running-config. I risultati non sbloccano automaticamente i capitoli locali.

Il client invia la propria consegna e il proprio stato, evitando la telemetria globale. Poiché il backend sostituisce un array di comandi vuoto con la cronologia condivisa, una sessione senza comandi invia un commento neutro non vuoto. Lo stato patch e domini è sempre esplicito.

I nomi visibili restano Luca, Jonny e Alessia; il gateway riceve gli identificativi compatibili marco, ciro e rosaria. Gli indizi AI accompagnano la storia, non sostituiscono le condizioni di completamento.

La generazione presenta separatamente l’anteprima (`deploy_to_cml: false`) e il comando di deploy. Un deploy interrompe l’autosincronizzazione della stanza per non sostituire subito lo scenario appena creato. Il limite dei nodi resta responsabilità del gateway CML; il client non dichiara successo senza leggerne la risposta.

## Collaudo

`-ClaraGatewayCheck -unattended -nullrhi -game` esegue il test HTTP dal client Unreal: health, bootstrap, office, lettura IOS, specifiche e verifica negativa di una consegna senza configurazioni. Il report locale è `Saved/Logs/GatewayIntegrationReport.json` e non va pubblicato automaticamente.

Gli altri test sono `-ClaraStoryCheck`, `-ClaraLayoutCheck` e i test Automation `Clara.SwitchCLI`.

## Problemi osservati sul server pubblicato

Il 14 novembre 2026 le API `/api/rooms` e `/api/room/office/topology` presenti nel branch rispondono 404 sul gateway pubblico. L’app usa le dieci rotte effettivamente pubblicate.

`POST /api/rack/patch` con `sw-1`, `connected: true` ha restituito `mode: live_cml`, ma `node: SW-CORE-01` e `link_id: null`. SW-CORE-01 non è il nodo office verificato. **Questa risposta non conferma alcun cavo remoto**: il client richiede nome del nodo corretto, identificativo link non vuoto ed esito coerente prima di registrare la patch. Per il cablaggio 1:1 va aggiornato il gateway al gestore stanze del branch o corretto il suo adattatore delle porte. Il client non ha accesso amministrativo al server per effettuare quel deploy.

Durante il boot, la console può rispondere `SW-PIANO1# (OK)` senza dati del comando: anche questo segnaposto viene rifiutato. La prova HTTP ripete soltanto la lettura IOS ogni cinque secondi, massimo sedici tentativi ed entro 110 secondi complessivi; non ripete bootstrap o cambi stanza durante il boot.

## Correzione del cablaggio e adattatore IOS

Il primo piano del portatile trasportato conserva ora le proprie prese cliccabili: solo il modello tenuto in mano nella stanza ignora il puntamento. La selezione ripetuta dello stesso tipo di cavo conserva il primo capo; la camera inquadra direttamente il lato Ethernet o console del portatile.

Quando il patch endpoint legacy non identifica un link sul nodo corretto, il client può impostare lo stato amministrativo della porta di SW-PIANO1 tramite IOS e verificarlo con una lettura successiva. Una sequenza di controllo preparata e limitata alle quattro interfacce esposte viene inviata in un unico pacchetto, per ridurre le interferenze tra richieste della console condivisa; non è un lock del server. È previsto un solo tentativo di riaggancio al percorso console office conosciuto, qualora il gateway resti su TermServ.

La conferma è marcata `IOS_ADMIN`, distinta da `CML_LINK`. Non ricabla la topologia e non viene contata come un nuovo collegamento fisico nella verifica rack. Comandi interattivi e cambi stanza di questo client attendono la fine dell’operazione. Le modifiche di altri client possono ancora interferire con la console server condivisa.

Prova dedicata: `-ClaraGatewayPortCheck -game -nullrhi -unattended`, abilitazione idempotente di Ethernet0/1 con output e lettura IOS registrati localmente in `Saved/Logs/GatewayPortRepairReport.json`.
