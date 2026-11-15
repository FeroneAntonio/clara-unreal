# Consegna cml-new

Questo branch unisce lo snapshot dei sorgenti TNES per Mac e il materiale CML in [Integrations/CML](../Integrations/CML/README.md).

La build Mac disponibile è TNES 0.1.0 per Apple Silicon, confezionata il 15 novembre 2026. È distribuita come allegato della [release](https://github.com/FeroneAntonio/clara-unreal/releases/tag/cml-new-mac-2026-11-15), insieme a checksum e istruzioni di ricomposizione.

I sorgenti includono le modifiche locali presenti al momento della consegna per tour di collaudo, disposizione dell'ufficio, preset dei personaggi e voce di Alessia. La build allegata è il pacchetto Shipping già esportato, distinto da queste modifiche successive ai sorgenti.

Per l'integrazione usare `Source/ClaraDayOne/ClaraGateway*`, `ClaraStoryGateway.cpp`, [Docs/CML_GATEWAY.md](CML_GATEWAY.md) e il contratto del gateway nella cartella CML. Gli esercizi locali e l'app Laboratorio Cisco sono percorsi distinti.
