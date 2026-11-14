# Verifiche della versione pubblicata

14 novembre2026, Unreal Engine5.8.4 su Mac.

- Build ClaraDayOneEditor Mac Development: PASS.
- StoryCheck: PASS (tre bersagli cavi, portatile preso e aperto in mano, camera/aggancio conservati, rete/MAC/console/fibra e challenge con salvataggio/riavvio).
- LayoutCheck: PASS, dieci destinazioni raggiungibili, cinque passaggi liberi, inspector aperto e richiuso.
- GatewayCheck HTTP dal client Unreal: PASS. Health, bootstrap, office, lettura IOS reale su SW-PIANO1, elenco missioni, rifiuto netAgent di consegna senza configurazioni. Secondo tentativo dopo una risposta vuota durante il boot.
- Desktop Linux con icone e taskbar visto nel gioco. Verifica manuale delle altre schermate incompleta per timeout intermittenti del controllo finestra.

## Limiti confermati

Il patch endpoint del server risponde ancora con nodo inesistente SW-CORE-01 e link_id:null: il client rifiuta di considerarlo cablaggio CML confermato. Console remota a righe; Tab/? del modello locale non sono completamento byte-stream remoto. I capitoli guidati locali e gli esercizi CML rimangono distinti. Il backend condivide laboratorio e telemetria tra utenti.

L’importazione delle mesh ha completato il salvataggio; l’editor batch ha poi avuto un crash di chiusura in una notifica Slate/ICU. Non si è verificato nel codice di gameplay. Non viene presentato come risolto un problema dell’engine non corretto nel suo sorgente.

Per i dettagli operativi vedere CML_GATEWAY.md. Report grezzi e crash log sono locali e restano esclusi da Git.

## Aggiornamento terminale, Ethernet e router Claude

- Terminale scuro uniforme, input monospazio e comandi compatti; nessun pannello bianco nel terminale locale o CML.
- Regressione `CLARA_ETHERNET_INTERACTION_PASS`: primo capo sullo switch, secondo sul portatile trasportato, ripetizione di Ethernet e avanzamento della fase5→6.
- Regressione `CLARA_CARRIED_LAPTOP_PORT_PICKING_PASS`: tracce reali sulle prese Ethernet e USB console dell’anteprima, mantenendo ignorato il modello trasportato nella stanza.
- Nuovo Cisco1941 di Claude importato con21materiali e coordinate reali delle8porte; predecessore conservato. Percorsi delle texture relativi al progetto. Ingombri UE verificati34,3×29,8×8,9cm.
- Il modulo compila senza Unity per evitare collisioni tra gli spazi dei nomi dei vecchi sorgenti quando Git li raggruppa in una build pulita.

- Prova HTTP reale del nuovo adattatore: `CLARA_GATEWAY_PORT_LIVE_PASS`, primo tentativo, `IOS_ADMIN` su SW-PIANO1 / Ethernet0/1, con lettura indipendente di conferma.
