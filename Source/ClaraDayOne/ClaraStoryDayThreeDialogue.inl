// Generated from work/reti-day3/build_dialogue.py; voice text and runtime share this source.
void AClaraStoryDirector::SayDayThree(const FString& Key,int32 Next,const TArray<FString>& Options)
{
    if(Key==TEXT("opening"))
    {
        Say(TEXT("Alessia"),{TEXT("Eccoti. Oggi è l’ultimo giorno."),TEXT("Hai già visto cavi, indirizzi e switch. Oggi si esce dalla rete. Fai quello che ti dice Luca, e guarda sempre bene dove sbagli: è lì che si impara."),TEXT("Alla fine ti dico come è andata. Luca ti aspetta in area scrivanie.")},Next,Options);return;
    }
    if(Key==TEXT("network"))
    {
        Say(TEXT("Luca"),{TEXT("Buongiorno. Oggi vediamo cosa succede quando il traffico deve uscire dalla rete."),TEXT("Il Network Layer, livello 3 di OSI, si occupa degli indirizzi IP e del percorso tra reti diverse. Qui lavora il router."),TEXT("Il PC confronta destinazione e mask: nella stessa rete cerca direttamente il destinatario; per un’altra rete manda il pacchetto al default gateway."),TEXT("In sala server hanno montato un rack nuovo, con R2 e SW2. Prima il fisico: collega R1 a R2, poi R2 a SW2 con due Cat 6.")},Next,Options);return;
    }
    if(Key==TEXT("console"))
    {
        Say(TEXT("Luca"),{TEXT("Bene. Controlla ogni LED: ambra all’inizio è normale, poi verde. Il cavo console porta i comandi, non il traffico."),TEXT("Prendi il portatile in mano e collega la sua USB console alla porta CONSOLE di R1. Nel terminale scegli R1."),TEXT("Tra i router usiamo 10.0.0.0/30: 255.255.255.252, due host utilizzabili. R1 è 10.0.0.1, R2 è 10.0.0.2."),TEXT("Un router ha un indirizzo per ogni rete. Configura Gi0/1, attivala con no shutdown e salva. La scheda sul portatile riepiloga i comandi.")},Next,Options);return;
    }
    if(Key==TEXT("r2"))
    {
        Say(TEXT("Luca"),{TEXT("R1 è configurato e salvato. Adesso sposta il cavo console su R2."),TEXT("Imposta nome R2, enable secret Cisco123, password Console123 e login sulla console. Sono credenziali di questo laboratorio."),TEXT("Gi0/0 guarda R1: 10.0.0.2/30. Gi0/1 guarda la rete B: 192.168.2.1/24. Attiva entrambe e salva.")},Next,Options);return;
    }
    if(Key==TEXT("sw2"))
    {
        Say(TEXT("Luca"),{TEXT("Leggi show ip route su R2. Di suo conosce soltanto le reti collegate: C indica la rete, L il suo indirizzo locale."),TEXT("Ora sposta la console su SW2. Nome SW2 e le stesse password del laboratorio."),TEXT("Configura VLAN 1 con 192.168.2.2/24, no shutdown e gateway 192.168.2.1. Poi salva.")},Next,Options);return;
    }
    if(Key==TEXT("firstping"))
    {
        Say(TEXT("Luca"),{TEXT("I dispositivi sono pronti. Il portatile oggi serve soltanto alla console e non è un host della rete."),TEXT("Vai a PC2 e prova ping 192.168.2.2. Osserva la risposta prima di aggiungere le rotte.")},Next,Options);return;
    }
    if(Key==TEXT("forward"))
    {
        Say(TEXT("Luca"),{TEXT("Un router impara le reti direttamente collegate. Per le altre gli serve una rotta: la tabella dice dove inoltrare il pacchetto. Senza una rotta adatta la destinazione è irraggiungibile."),TEXT("Una rotta statica è una riga scritta da te: per questa rete, passa da quel router. Quel router è il next-hop."),TEXT("Leggi show ip route su R1. In configure terminal aggiungi la rotta per 192.168.2.0/24 via 10.0.0.2; poi end, salva e controlla la tabella."),TEXT("Ripeti il ping da PC2. Il next-hop deve essere raggiungibile su una rete collegata al router.")},Next,Options);return;
    }
    if(Key==TEXT("return"))
    {
        Say(TEXT("Luca"),{TEXT("Una rotta vale in un solo senso: il pacchetto può arrivare, ma la risposta ha bisogno di una strada per tornare."),TEXT("Su R2 usa una rotta di default: tutto ciò che non conosce va a R1, 10.0.0.1. La rete e la mask della rotta sono entrambe 0.0.0.0."),TEXT("Entra in configure terminal, aggiungi la rotta, poi end e salva. Salva anche R1. Riprova il ping da PC2.")},Next,Options);return;
    }
    if(Key==TEXT("trace"))
    {
        Say(TEXT("Luca"),{TEXT("Il ping risponde. Nelle tabelle C indica una rete collegata, L un indirizzo locale, S una rotta statica e S con asterisco la candidata di default."),TEXT("Il traceroute invia sonde con TTL crescente. Ogni router lo diminuisce: a zero scarta la sonda e risponde con ICMP time exceeded."),TEXT("Da PC2 esegui traceroute 192.168.2.2. Vedrai R1, R2 e SW2. PC2 è la sorgente e non compare tra i salti."),TEXT("Un asterisco indica una risposta mancante: su una rete reale può dipendere anche da filtri o limiti ICMP. In questo laboratorio confronta i salti con interfacce e rotte, senza attribuire il guasto al solo timeout.")},Next,Options);return;
    }
    if(Key==TEXT("gaiaready"))
    {
        Say(TEXT("Luca"),{TEXT("Il percorso è quello giusto. Una rotta statica dipende da chi la configura: un next-hop modificato può deviare il traffico. Proteggi gli accessi di amministrazione."),TEXT("Gaia è pronta per il collaudo. Parla con lei quando vuoi cominciare.")},Next,Options);return;
    }
    if(Key==TEXT("challenge"))
    {
        Say(TEXT("Gaia"),{TEXT("Il rack nuovo domani va in produzione. Prima lo collaudiamo."),TEXT("Durante il montaggio alcune impostazioni sono state cambiate. Trovale e correggile."),TEXT("Hai il portatile con il cavo console, PC2 e i dispositivi. Quando pensi di aver finito, premi TEST. Controllo io.")},Next,Options);return;
    }
    if(Key==TEXT("independent"))
    {
        Say(TEXT("Luca"),{TEXT("Ora lavori da solo. Se chiedi aiuto: cavo, IP, gateway, interfaccia, rotta, configurazione salvata. Dal basso verso l’alto.")},Next,Options);return;
    }
    if(Key==TEXT("testpass"))
    {
        Say(TEXT("Gaia"),{TEXT("Cavi, accessi, interfacce, ping, percorso e configurazioni salvate: tutto a posto."),TEXT("Ultima verifica. Premi RIAVVIA per caricare le configurazioni salvate e ripetere i controlli.")},Next,Options);return;
    }
    if(Key==TEXT("rebootpass"))
    {
        Say(TEXT("Gaia"),{TEXT("Rete su, anche dopo il riavvio. Va bene. Alessia ti aspetta alla reception.")},Next,Options);return;
    }
    if(Key==TEXT("closingluca"))
    {
        Say(TEXT("Luca"),{TEXT("Bel lavoro. Oggi hai portato il traffico fuori dalla rete: due router, due reti, rotte statiche e quattro guasti da trovare.")},Next,Options);return;
    }
    if(Key==TEXT("closing"))
    {
        Say(TEXT("Alessia"),{TEXT("Ti ho osservato per tre giorni. Cavi, indirizzi, router. Non sapevi tutto, ma hai provato, hai sbagliato e hai ricominciato: è quello che cerchiamo."),TEXT("Benvenuto in TNES.")},Next,Options);return;
    }
    if(Key==TEXT("retry"))
    {
        Say(TEXT("Luca"),{TEXT("Dai, riprova. Leggi il messaggio del terminale e controlla il modo del prompt, l’interfaccia e la parte di rete dell’indirizzo.")},Next,Options);return;
    }
    if(Key==TEXT("hintphysical"))
    {
        Say(TEXT("Luca"),{TEXT("Controlla ogni cavo. R1 Gi0/1 guarda R2 Gi0/0; R2 Gi0/1 guarda SW2. Un LED acceso da solo non basta.")},Next,Options);return;
    }
    if(Key==TEXT("hintconfig"))
    {
        Say(TEXT("Luca"),{TEXT("Controlla show running-config e show ip interface brief. Nome, accessi, indirizzo e mask, interfaccia attiva. Alla fine salva.")},Next,Options);return;
    }
    if(Key==TEXT("hintforward"))
    {
        Say(TEXT("Luca"),{TEXT("Confronta la destinazione con show ip route su R1. Il next-hop deve essere raggiungibile sul collegamento fra i router.")},Next,Options);return;
    }
    if(Key==TEXT("hintreturn"))
    {
        Say(TEXT("Luca"),{TEXT("R2 sa rispondere alla rete A? Una rotta va in un solo senso. Controlla la tabella e la configurazione salvata.")},Next,Options);return;
    }
    if(Key==TEXT("hinttrace"))
    {
        Say(TEXT("Luca"),{TEXT("Esegui traceroute da PC2. Un timeout è un indizio: confrontalo con show ip route e show ip interface brief sui due router.")},Next,Options);return;
    }
    if(Key==TEXT("hintgateway"))
    {
        Say(TEXT("Luca"),{TEXT("Switch e gateway devono avere la stessa parte di rete. Controlla anche la maschera.")},Next,Options);return;
    }
    if(Key==TEXT("hintinterface"))
    {
        Say(TEXT("Luca"),{TEXT("Controlla show ip interface brief: le interfacce dei router sono tutte up?")},Next,Options);return;
    }
    if(Key==TEXT("hintsave"))
    {
        Say(TEXT("Luca"),{TEXT("Confronta running-config e startup-config. Il riavvio carica soltanto ciò che hai salvato.")},Next,Options);return;
    }
    ensureMsgf(false,TEXT("Unknown Day 3 dialogue: %s"),*Key);
}
