# Provenienza e condizioni dei materiali

Questo repository pubblico è l’ambiente di lavoro del team TNES. Non è pubblicato con una licenza open source complessiva. La visibilità pubblica non applica automaticamente una licenza libera al codice originale né concede diritti sui marchi. I registri di provenienza accompagnano i materiali in `SourceAssets`.

## Codice, storia e ricostruzioni del progetto

Il codice e i contenuti originali del progetto restano materiali del progetto, salvo accordi distinti dei rispettivi titolari. Non viene applicata automaticamente una licenza MIT, CC0 o equivalente.

Switch Catalyst 2960-X, router Cisco 1941, connettori e portatile in `SourceAssets/NetworkAssets` sono ricostruzioni originali, con sorgenti Blender e FBX. **Non sono CAD ufficiali Cisco**, scansioni metrologiche né modelli certificati. I nomi e i marchi Cisco descrivono gli apparati rappresentati; non indicano approvazione del progetto da parte di Cisco. Riferimenti e limiti geometrici sono nel relativo `LEGGIMI.md`.

## Epic Games / Unreal Engine — esclusi dalla pubblicazione

Manny, Quinn, scheletro, materiali, texture e animazioni Epic **non vengono inclusi nel repository pubblico**. La cartella `Content/Characters/Mannequins` è esclusa da Git. Ogni sviluppatore usa la propria installazione autorizzata di Unreal Engine 5.8 e lancia `Scripts/setup_epic_mannequins.py --engine-root <UE_5.8>` per copiare localmente i soli asset necessari dai template già installati.

Il manifesto `Content/Characters/mannequin-template-manifest.json` contiene soltanto nomi e dimensioni; non incorpora gli asset. Lo script non scarica file Epic e non modifica le loro condizioni di utilizzo. I file locali restano soggetti ai termini Epic applicabili e non diventano CC0.

## MetaHuman — generati localmente, esclusi dalla pubblicazione

Alessia (preset Jelena), Luca (preset Lorenzo) e Jonathan/Jonny (preset Mateo) sono MetaHuman ricavati dai preset gratuiti inclusi nel plugin MetaHumanCharacter di Unreal Engine 5.8. Rig e texture vengono generati dal servizio cloud MetaHuman con l'account Epic di chi lancia `Scripts/setup_metahuman_cast.py`. Le cartelle `Content/Characters/MetaHumans` e `Content/MetaHumans` sono escluse da Git: il repository contiene solo lo script e il codice che usa i personaggi quando sono presenti. Senza di essi il gioco torna ai manichini Epic provvisori. I file locali restano soggetti ai termini Epic/MetaHuman applicabili.

## Poly Haven

Gli asset e le texture Poly Haven sono accompagnati da manifesti che indicano **CC0-1.0**, autori, URL e hash quando disponibili. Cartelle interessate: `SourceAssets/OfficeAssets`, `SourceAssets/PolyHaven` e relativi importati in `Content/OfficeAssets` / `Content/Textures`. Conservare i manifesti quando si sostituiscono o aggiornano i file.

Riferimento registrato: https://polyhaven.com/license

## Innerscene

Sedia operativa, case desktop e monitor provengono dalla libreria Innerscene. Il manifesto in `SourceAssets/OfficeAssets2/Innerscene/manifest.json` registra **CC0-1.0**, URL della scheda, termini e hash di download. Gli importati Unreal mantengono quella provenienza; il progetto non rivendica di aver modellato questi oggetti da zero.

Riferimento registrato: https://www.innerscene.com/tools/library/terms

## Materiali Seaprop — esclusi dalla pubblicazione

Tessuti, fotografie e derivati Seaprop non hanno una licenza pubblica verificata e **non vengono distribuiti qui**. Sono escluse interamente `SourceAssets/ChairMaterials` e `Content/OfficeAssets/ChairMaterials`, compresi report personali e texture derivate. Il riuso locale autorizzato da Antonio non viene trasformato in una licenza pubblica.

La copia pubblica usa i materiali originali CC0 della sedia Innerscene quando i materiali locali non sono presenti.

## Materiali esclusi

`Content/Characters/Prototypes` contiene prototipi personali non utilizzati nella scena corrente ed è esclusa da Git. Anche `SourceAssets/Renderpeople` e le eventuali risorse personali di `SourceAssets/Characters` restano fuori dal repository. Non presumere che l’esclusione significhi cancellazione o che tali materiali siano coperti da questo archivio pubblico.

Per nuovi acquisti o download aggiungere provenienza, versione e condizioni prima dell’importazione. La disponibilità di un link o di un file gratuito non basta a dichiarare libera la ridistribuzione.

Verifica dei termini pubblicati: 14 novembre 2026. Fonti ufficiali consultate: [Innerscene](https://www.innerscene.com/tools/library/terms), [Poly Haven](https://polyhaven.com/license). I manifesti dei singoli modelli restano il riferimento per la provenienza concreta.

## Somiglianza personale di Luca

`Content/Luca` contiene testa, texture e capelli personali forniti per questa prova locale. È escluso da Git: non redistribuire questi asset né le fotografie di riferimento. Il codice può usare tali file quando presenti; la loro presenza non concede diritti sulla somiglianza per altri usi.

## Intro aereo TNES

Cabina, sedili e CV sono geometrie originali del progetto, generate con Blender.
La fotografia esterna è **Carsten Steger, “Aerial image of Pompeii and Mount Vesuvius (view from the southeast)”**, [fonte](https://commons.wikimedia.org/wiki/File:Aerial_image_of_Pompeii_and_Mount_Vesuvius_(view_from_the_southeast).jpg), licenza [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). L’originale non è modificato; nel gioco viene inquadrato attraverso il finestrino. Il file e il registro di attribuzione sono in Content/Data/Intro.

## City Sample Crowds — personaggi d’ufficio locali

I due personaggi vestiti derivano dal pacchetto gratuito ufficiale Epic [City Sample Crowds](https://www.fab.com/listings/903037e9-e1ac-4f41-96e8-1683c6fa7ad4), con materiali, rig e vestiti originali. Sono contenuti UE-Only distribuiti nel gioco compilato, non nel repository sorgente pubblico. Content/CitySampleCrowd e Content/Characters/OfficeCast sono esclusi da Git. Non sono modelli originali del team né risorse CC0.

## Audio del progetto

Ambienti ed effetti sono sintetizzati dagli script del progetto. Le battute sono generate localmente con Qwen3-TTS e controllate prima dell’inclusione; i pesi del modello non sono distribuiti con il gioco o nel repository. I file di riferimento e i WAV generati sono materiali del progetto, non una libreria di suoni di terzi. I testi a schermo restano disponibili quando una battuta non ha ancora un audio validato.
