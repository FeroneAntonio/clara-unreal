#include "ClaraCyberRuntime.h"
#include <algorithm>
#include <sstream>

namespace ClaraCyber {
namespace {
std::vector<std::string> words(const std::string& input) {
    std::istringstream stream(input); std::vector<std::string> result; std::string word;
    while (stream >> word) result.push_back(word);
    return result;
}
bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
bool bit(const Model& model, unsigned value) { return (model.rewards[model.day - 1] & value) != 0; }
}

void Terminal::append(const std::string& text) {
    if (text.empty()) return;
    history += text + "\n";
    if (history.size() > 32768) {
        const auto line = history.find('\n', history.size() - 24000);
        if (line != std::string::npos) history.erase(0, line + 1);
    }
}

std::string Terminal::command(Model& model, const std::string& input) {
    if (active) return "Una prova è già in corso. Attendi il risultato oppure premi Ctrl+C.";
    const auto args = words(input);
    if (args.empty()) return "";
    append("cyber$ " + input);
    std::string result;
    if (args[0] == "ls" && (args.size() == 1 || (args.size() == 2 && (args[1] == "-l" || args[1] == "-la")))) {
        result = "guida/  registro.log  rapporto.txt\nFile didattici del laboratorio.";
    } else if (args.size() == 1 && args[0] == "pwd") {
        result = "/home/studente/cyber";
    } else if (args.size() == 1 && args[0] == "whoami") {
        result = "studente";
    } else if (args[0] == "ping") {
        Model prediction = model;
        preview = prediction.command(input);
        const bool dayOne = model.day == 1 && args.size() == 2 && args[1] == "192.168.10.20";
        const bool dayTwo = model.day == 2 && args.size() == 4 && args[1] == "-I";
        const bool testResult = has(preview, "SIMULAZIONE LOCALE") || preview == "Nessun dispositivo risponde.";
        if ((dayOne || dayTwo) && testResult) {
            active = true; pending = input; day = model.day; path = model.path[day - 1];
            reachable = has(preview, "risposta dal server") || has(preview, "Connessione riuscita");
            elapsed = 0; sent = 0; interval = reachable ? 1.0 : 4.0;
            address = args.back();
            result = "PING " + address + " (4 richieste, 32 byte)";
            if (dayTwo) result += " da " + args[2];
            result += "\nCtrl+C interrompe la prova.";
        } else result = preview;
    } else {
        result = model.command(input);
    }
    append(result);
    return result;
}

bool Terminal::tick(Model& model, double seconds) {
    if (!active || !(seconds > 0)) return false;
    elapsed += seconds;
    while (active && sent < 4 && elapsed + 0.000001 >= interval) {
        elapsed -= interval;
        ++sent;
        append(reachable ? "32 byte da " + address + ": icmp_seq=" + std::to_string(sent) + " tempo<1 ms"
                         : "Richiesta " + std::to_string(sent) + ": tempo scaduto.");
        if (sent != 4) continue;
        active = false;
        append("Pacchetti: 4 inviati, " + std::string(reachable ? "4 ricevuti, 0% persi." : "0 ricevuti, 100% persi."));
        Model prediction = model;
        if (model.day != day || model.path[day - 1] != path || prediction.command(pending) != preview) {
            append("La configurazione è cambiata durante la prova. Ripeti il comando per registrare un risultato valido.");
        } else {
            append(model.command(pending));
        }
        pending.clear();
    }
    return true;
}

bool Terminal::cancel() {
    if (!active) return false;
    active = false; pending.clear();
    append("^C\nProva interrotta: nessun risultato registrato.");
    return true;
}

const char* CurrentTask(const Model& m) {
    if (m.day > 3) return "Gli incarichi Cyber sono conclusi. Puoi rileggere il taccuino.";
    if (m.finished[m.day - 1]) return "Il rapporto è pronto. Leggi l'altra prospettiva oppure passa al prossimo incarico.";
    const char path = m.path[m.day - 1];
    if (!path) {
        if (m.day == 1) return "Esamina almeno tre porte del server, poi scegli il tuo punto di vista.";
        if (m.day == 2) return "Leggi la scheda ACL, poi scegli il tuo punto di vista.";
        return "Leggi le schede sulle credenziali e sulla cifratura, poi scegli il tuo punto di vista.";
    }
    if (path == 'B' && !m.logRecognized) return "Confronta origine, frequenza ed esito delle righe del registro prima di intervenire.";
    if (m.day == 1 && path == 'A') {
        if (!m.scanned) return "Usa nmap sull'indirizzo del server dei servizi per vedere quali porte rispondono.";
        if (m.classification != std::array<int,5>{{2,3,2,1,2}}) return "Classifica le cinque porte e confronta ciascuna scelta con la descrizione del servizio.";
        if (!m.seen22) return "Controlla nella guida chi usa la porta 22, prima di compilare il rapporto.";
    } else if (m.day == 1) {
        if (!m.ports[1] || !m.ports[3]) return "Ripristina i servizi necessari al team e alle dispense, poi verifica quelli inutili.";
        if (m.ports[0] || m.ports[2] || m.ports[4]) return "Verifica chi usa i servizi prima di fermarli. La scheda della porta 8080 contiene lo storico degli accessi.";
        if (!m.scanned) return "Ripeti la scansione dopo le modifiche, per vedere quali porte sono ancora aperte.";
        if (!m.materials) return "Apri le dispense e verifica che gli utenti possano ancora consultarle.";
    } else if (m.day == 2) {
        if (path == 'A' && std::find(m.expected.begin(), m.expected.end(), -1) != m.expected.end())
            return "Indica cosa ti aspetti da ciascuna postazione prima di provare il collegamento.";
        if (path == 'B' && (!m.evaluate("192.168.10.50", "192.168.20.10").allowed || m.evaluate("192.168.10.30", "192.168.20.10").allowed || m.evaluate("192.168.10.99", "192.168.20.10").allowed))
            return "Confronta l'ordine delle regole: la reception deve passare, aula e ospiti devono essere bloccati.";
        if ((path == 'A' && std::find(m.observed.begin(), m.observed.end(), -1) != m.observed.end()) ||
            (path == 'B' && std::find(m.verified.begin(), m.verified.end(), false) != m.verified.end()))
            return "Completa un ping da ciascuna delle tre postazioni e confronta i risultati.";
        if (path == 'A' && m.expected != std::array<int,3>{{1,0,0}}) return "Confronta ciò che avevi indicato con i permessi richiesti: solo la reception dovrebbe passare.";
        if (path == 'A' && !bit(m, 8)) return "Indica quale regola ha consentito il traffico che doveva essere bloccato.";
    } else if (path == 'A') {
        if (!bit(m, 2)) return "Prova le coppie fittizie del laboratorio nel pannello di accesso.";
        if (!m.plainSeen || !m.tlsSeen) return "In Network Vision confronta un accesso in chiaro con la versione cifrata.";
        if (!m.weaknesses) return "Indica entrambi i problemi osservati: credenziali predefinite e traffico non cifrato.";
    } else {
        if (!m.adminEnabled) return "Riattiva l'account del team: dobbiamo proteggerne l'accesso senza impedirgli di lavorare.";
        if (!m.passwordChanged) return "Sostituisci la password predefinita rispettando i quattro requisiti.";
        if (!m.tls) return "Attiva la connessione cifrata, così chi osserva la rete non legge le credenziali.";
        if (!m.oldRejected) return "Verifica che la vecchia password venga rifiutata.";
        if (!m.teamAccepted) return "Verifica che il team tecnico riesca ancora ad accedere.";
        if (!m.trafficChecked) return "Controlla in Network Vision che il traffico sia cifrato.";
    }
    return "Hai raccolto le verifiche necessarie. Compila il rapporto.";
}

const char* CommandFeedback(const Model& m, const std::string& input, const std::string& result) {
    const auto args = words(input);
    if (args.empty()) return "";
    if (has(result, "Postazione fuori autorizzazione")) return "Fermati. Il test copre solo le tre postazioni indicate.";
    if (has(result, "Destinatario fuori autorizzazione")) return "Fermati. L'autorizzazione copre solo il server indicato nella scheda del servizio.";
    if (has(result, "Host seems down") || has(result, "Nessun dispositivo risponde")) return "Nessun dispositivo risponde a quell'indirizzo. Controlla la scheda del servizio.";
    if (args[0] == "ls" || args[0] == "pwd" || args[0] == "whoami") return "Questo comando ora non ci serve. Torniamo alla prova della rete.";
    if (has(result, "command not found") || has(result, "Comando sconosciuto")) return "Questo comando non è disponibile nel laboratorio. Guarda il manuale e l'obiettivo.";
    if (has(result, "Manca il destinatario")) return "Manca il destinatario. Quale indirizzo vuoi verificare?";
    if (args[0] == "ping" && has(result, "Uso:")) return "Manca un dato nella prova. Indica la postazione di partenza e l'indirizzo del server.";
    if (has(result, "Indica prima")) return "Indica prima il risultato atteso, poi confrontalo con quello del ping.";
    if (has(result, "non disponibile in questo scenario") || has(result, "Giornata non attiva")) return CurrentTask(m);
    return "";
}

const char* NormalLogFeedback(int day) {
    if (day == 1) return "Luca va in un posto solo. Questo indirizzo bussa a tutte le porte.";
    if (day == 2) return "L'aula non dovrebbe arrivare al server. Leggi quale regola ha consentito la richiesta.";
    return "Confronta con le righe del team tecnico: un tentativo, un successo. Qui sono cinque in due minuti.";
}

const char* ActionFeedback(const Model& m, const std::string& name, int number, const std::string& value, bool accepted) {
    if (name == "classify" && accepted) {
        const int c = value == "1" ? 1 : value == "2" ? 2 : 3;
        if (number == 21 && c == 1) return "Chi la usa, secondo te?";
        if (number == 22 && c == 1) return "Può darsi. Ma come lo scopriamo?";
        if (number == 22 && c == 2) return "Sicuro? Qualcuno dovrà pure amministrare il server.";
        if (number == 23 && c == 1) return "Leggi la descrizione: i dati passano in chiaro.";
        if (number == 443 && c != 1) return "Quella la usa Luca per aprire le dispense. Come potevi saperlo?";
        if (number == 8080 && c == 1) return "Non ha nemmeno una descrizione. Cosa significa?";
        return "Nota registrata. Puoi cambiarla dopo aver verificato chi usa il servizio.";
    }
    if (name == "port") {
        if (number == 8080 && value == "off" && !m.inspected.count(8080)) return "Se nessuno lo registra, forse nessuno lo usa. Come lo scopriamo?";
        if (accepted && value == "off" && number == 22) return "Il team non riesce più ad amministrare il server. Riattiva la porta 22.";
        if (accepted && value == "off" && number == 443) return "Le dispense non sono più raggiungibili. Riattiva il servizio sulla porta 443.";
        if (accepted) return value == "off" ? "Servizio fermato. Verifica le porte rimaste aperte e l'accesso alle dispense." : "Servizio riattivato. Verifica che chi lo usa riesca a raggiungerlo.";
    }
    if (name == "rules" && accepted) {
        if (!m.evaluate("192.168.10.50", "192.168.20.10").allowed) return "La reception non riesce più ad aprire le prenotazioni. Il blocco viene letto prima del permesso.";
        if (m.evaluate("192.168.10.30", "192.168.20.10").allowed || m.evaluate("192.168.10.99", "192.168.20.10").allowed) return "Aula e ospiti passano ancora. Controlla quale permesso viene letto per primo.";
        return "L'ordine rispetta i permessi richiesti. Verifica ora il risultato dalle tre postazioni.";
    }
    if (name == "admin" && accepted && !m.adminEnabled) return "Non riusciamo più ad amministrare il server. Riattiva l'account del team tecnico.";
    if (name == "password") {
        if (accepted && !m.tls) return "Bene. E cosa leggerebbe chi osserva la rete mentre il team entra?";
        if (!accepted && m.logRecognized) return "Controlla i quattro requisiti della password. Non salviamo il testo che scegli.";
    }
    if (name == "tls" && accepted && m.tls && !m.passwordChanged) return "Il canale è protetto, ma la password è ancora quella di tutti. Manca qualcosa.";
    if (name == "tls" && accepted && !m.tls) return "Il canale è tornato in chiaro. Riattiva la cifratura prima di verificare il traffico.";
    if (name == "login") return accepted ? "Accesso consentito con credenziali predefinite. Ora osserva il traffico in Network Vision." : "Accesso rifiutato. Confronta un'altra coppia del laboratorio, senza modificare la lista.";
    if (name == "rule" && !accepted && m.day == 2 && m.observed[0] >= 0 && m.observed[1] >= 0 && m.observed[2] >= 0) return "La prima regola corrispondente decide. Confronta la regola scelta con quella indicata dalle prove.";
    if (name == "report" && !accepted) return CurrentTask(m);
    if (!accepted) return CurrentTask(m);
    if (name == "check22") return "La porta 22 serve al team tecnico: va mantenuta e protetta.";
    if (name == "materials") return "Le dispense si aprono. Il servizio necessario agli utenti è ancora disponibile.";
    if (name == "old") return "La vecchia password viene rifiutata.";
    if (name == "team") return "Il team tecnico riesce ad accedere con le nuove credenziali.";
    if (name == "vision") return number ? "Con la cifratura il contenuto del pacchetto non è leggibile." : "Il pacchetto didattico mostra le credenziali di esempio in chiaro.";
    if (name == "log") return "L'anomalia è stata riconosciuta. Ora confronta i servizi o le regole con ciò che dovrebbe essere consentito.";
    return "";
}
}
