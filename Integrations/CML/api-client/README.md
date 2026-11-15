# API locale TNES → Cisco CML

Client operativo per Codex e terminale. Usa soltanto il Chrome abituale già autenticato su https://cml.kappino.me/. Non avvia browser e non salva password o token. Il processo API mantiene il collegamento alla scheda esistente; ogni richiesta usa l'autenticazione di quella scheda.

Il trasporto locale è HTTP su un socket Unix con permessi 0600, senza porte TCP pubbliche. Il socket corrente è `work/cml-api.sock`, relativo alla cartella da cui parte il servizio. `CML_API_SOCKET` permette di indicarne il percorso assoluto.

## Uso

Dalla radice del repository, dopo aver avviato il servizio:

```sh
node Integrations/CML/api-client/cml.mjs health
node Integrations/CML/api-client/cml.mjs labs
node Integrations/CML/api-client/cml.mjs import Integrations/CML/network-1-inizio.yaml
node Integrations/CML/api-client/cml.mjs start LAB_ID
node Integrations/CML/api-client/cml.mjs stop LAB_ID
node Integrations/CML/api-client/cml.mjs console LAB_ID SW1 'show ip interface brief'
```

L'importazione rifiuta titoli già presenti. I nove YAML della consegna sono già importati: non serve ripetere l'import.

Per avviare nuovamente il servizio dopo la sua chiusura, dalla stessa cartella:

```sh
npm ci --prefix Integrations/CML/api-client --ignore-scripts
node Integrations/CML/api-client/serve.mjs
```

Serve il controllo remoto del Chrome abituale già abilitato. Il launcher legge `DevToolsActivePort` dal profilo Chrome esistente; non cambia le impostazioni del browser. Il servizio autonomo è stato avviato e verificato dopo l’approvazione di Antonio alla richiesta nativa Chrome «Allow remote debugging?». Un nuovo collegamento può richiedere nuovamente questa approvazione. Questa autorizzazione è del browser e riguarda la sessione Chrome, mentre il client limita le proprie operazioni al CML. Non rimuovere un socket mentre un'altra istanza del servizio lo usa.

## Contratto locale

`POST /api` sul socket Unix, body JSON:

```json
{"operation":"health"}
```

Operazioni: `health`, `labs`, `import` con `topology` (oggetto CML), `start` e `stop` con `lab_id`, `console` con `lab_id`, `node`, `command`.

Le operazioni di modifica e console sono serializzate. `start` conta i nodi avviati o in coda di tutti i laboratori accessibili e rifiuta un totale oltre 5; non spegne altri laboratori. `DEFINED_ON_CORE` e `STOPPED` non consumano posti. La licenza CML resta l'autorità finale anche rispetto ad altri client esterni.

Solo le letture GET vengono ritentate, fino a tre volte in caso di 502/503/504. Le mutazioni non vengono ripetute automaticamente. La console invia una riga reale: se non riceve testo restituisce un errore esplicito; non produce risposte simulate.
