# Apparati Cisco — ricostruzioni CLARA

Modelli originali creati nel progetto con Blender 5.2: Cisco Catalyst WS-C2960X-24TS-L, Cisco 1941 ISR e connettore RJ-45 8P8C. Le ricerche dei modelli pronti e i riferimenti ufficiali sono in `../../../../grafica-ufficio/Riferimenti-Cisco-apparati.md`.

Non sono file CAD ufficiali Cisco né copie scaricate dei modelli commerciali trovati. Sagoma, dettagli e disposizione delle porte sono ricostruiti dalle illustrazioni hardware e dalle specifiche; le quote fini di prese, pulsanti, scritte e ventilazione sono approssimazioni artistiche. I marchi indicano il prodotto rappresentato, senza suggerire una certificazione Cisco del modello.

Lo switch rappresentato è la versione non PoE con 24 porte rame e 4 slot SFP. Chassis nominale 44,5 × 27,94 × 4,45 cm. Il router ha chassis nominale 34,3 × 29,2 × 8,9 cm; la profondità del mesh include il rilievo della fascia frontale. GE 0/0, GE 0/1, console e AUX del 1941 sono sul retro. La numerazione dello switch segue la convenzione della famiglia; non viene dichiarata una verifica metrologica di un esemplare reale.

File `.blend` modificabili, `.fbx` importati, materiali e resoconti geometrici sono in questa cartella. L’importazione FBX ha prodotto coordinate native ingrandite di 100 volte: il componente del dispositivo normalizza esplicitamente i bounds alle dimensioni in `Content/Data/network-devices.json`, che definisce anche le prese in centimetri. Le prese non dipendono dai nomi dei poligoni e possono essere selezionate singolarmente.

Materiali di lamiera verniciata, alluminio, nylon, plastica traslucida e contatti sono ricostruzioni PBR a parametri; non sono scansioni certificate delle superfici Cisco. Le immagini del manuale usate come riferimenti non sono incorporate nei mesh.

Nessun acquisto effettuato. Il collegamento delle porte gestisce cavi e occupazione fisica locale: non esegue una configurazione IOS né un’operazione CML.
