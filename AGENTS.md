# CLARA Unreal — lavoro del team

Leggere README.md, TEAM.md e Docs/CML_GATEWAY.md prima di modificare il progetto.

- Conservare la piantina attuale e i passaggi liberi: non introdurre arredi davanti a porte, banco cavi o apparati.
- Desktop del portatile: stile Linux della versione 1, con icone e finestre. Non ripristinare il dashboard a grandi schede.
- Personaggi provvisori Epic; installarli localmente con Scripts/setup_epic_mannequins.py. Non pubblicare asset Epic, foto personali o tessuti Seaprop.
- Il client CML deve distinguere health del gateway, esecuzione IOS verificata e risposte simulate. Non sostituire risultati remoti con successi locali.
- Source/ClaraDayOne/ClaraGateway* gestisce trasporto e prove; ClaraStoryGateway.cpp adatta la storia. Non inserire credenziali.
- Il backend demo condivide la sessione: usare -ClaraLocalLab quando si lavora su grafica o UI.
- Build Unreal e test pertinenti prima di una PR. Documentare chiaramente ciò che non si è potuto verificare.
