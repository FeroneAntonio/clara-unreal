# Provenienza e condizioni dei materiali

Questo repository pubblico è l’ambiente di lavoro del team CLARA. Non è pubblicato con una licenza open source complessiva. La visibilità pubblica non applica automaticamente una licenza libera al codice originale né concede diritti sui marchi. I registri di provenienza accompagnano i materiali in `SourceAssets`.

## Codice, storia e ricostruzioni del progetto

Il codice e i contenuti originali del progetto restano materiali del progetto, salvo accordi distinti dei rispettivi titolari. Non viene applicata automaticamente una licenza MIT, CC0 o equivalente.

Switch Catalyst 2960-X, router Cisco 1941, connettori e portatile in `SourceAssets/NetworkAssets` sono ricostruzioni originali, con sorgenti Blender e FBX. **Non sono CAD ufficiali Cisco**, scansioni metrologiche né modelli certificati. I nomi e i marchi Cisco descrivono gli apparati rappresentati; non indicano approvazione del progetto da parte di Cisco. Riferimenti e limiti geometrici sono nel relativo `LEGGIMI.md`.

## Epic Games / Unreal Engine — esclusi dalla pubblicazione

Manny, Quinn, scheletro, materiali, texture e animazioni Epic **non vengono inclusi nel repository pubblico**. La cartella `Content/Characters/Mannequins` è esclusa da Git. Ogni sviluppatore usa la propria installazione autorizzata di Unreal Engine 5.8 e lancia `Scripts/setup_epic_mannequins.py --engine-root <UE_5.8>` per copiare localmente i soli asset necessari dai template già installati.

Il manifesto `Content/Characters/mannequin-template-manifest.json` contiene soltanto nomi e dimensioni; non incorpora gli asset. Lo script non scarica file Epic e non modifica le loro condizioni di utilizzo. I file locali restano soggetti ai termini Epic applicabili e non diventano CC0.

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
