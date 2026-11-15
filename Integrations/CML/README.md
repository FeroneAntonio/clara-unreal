# TNES × Cisco CML

Consegna del 15 novembre 2026. Nove configurazioni CML per le missioni Network, client API locale, strumenti e registrazioni delle prove. Limite operativo: cinque dispositivi attivi complessivi; i laboratori sono alternativi.

## Materiale

- `network-*.yaml`: missioni Network 1, Network 2 prova, Network 2 Gaia e Network 3; varianti iniziali, soluzioni e guasti.
- `api-client/`: client Node.js e servizio sul socket Unix, collegati al Chrome abituale autenticato.
- `tools/build_mission_labs.py`: generatore dei laboratori.
- `reference/`: contratti OpenAPI acquisiti durante le prove.
- `evidence/` e JSON nella cartella: schermate, registrazioni e risposte raccolte.
- `manifest.json`, `import-results.json`: topologie e ID restituiti dal controller.
- `gateway-office-binding.json`: nomi utilizzati per il laboratorio Office e lo switch.
- `iosvl2-test-node.json`, `iosvl2-boot.json`: dati del nodo Cisco IOSvL2 di prova.

## Nomi e avvio

Il gateway dell'app usa `office`, `corridor`, `datacenter_vault`; lo switch Office predefinito è `SW-PIANO1` nel laboratorio `FuoriRete_Room_Office`. Il laboratorio Network 1 della sessione è stato rinominato per questa mappatura. I YAML conservano i nomi didattici delle singole missioni. Gli ID sono riferimenti a questa installazione; su un nuovo controller usare quelli restituiti dall'importazione.

Le porte IOL `Ethernet0/x` corrispondono alle porte del modello 3D tramite una mappatura esplicita. Network 2 Gaia e Network 3 usano il portatile come console esterna al conteggio dei nodi. Le password nei YAML sono esempi didattici del copione.

Per rigenerare i YAML: installare `tools/requirements.txt` in un ambiente Python, poi eseguire `python tools/build_mission_labs.py CARTELLA_OUTPUT` dalla cartella CML.

Da `api-client/`: `npm ci --ignore-scripts`, poi `node serve.mjs`; in un altro terminale `node cml.mjs health`. Vedere il README del client per importazione, avvio, arresto e console.

L'app Mac è negli allegati della [release cml-new](https://github.com/FeroneAntonio/clara-unreal/releases/tag/cml-new-mac-2026-11-15). Il launcher `Avvia-TNES-CML.command` abilita `-ClaraLiveCML`. La modalità locale è descritta nel README principale del progetto.

I filmati allegati sono registrazioni delle prove, da leggere insieme alle risposte JSON. Gli script `bind-office.mjs`, `restart-office-switch.mjs` e `test-iosvl2.mjs` operano sugli ID della sessione: prima di eseguirli su un'altra installazione controllare i riferimenti.
