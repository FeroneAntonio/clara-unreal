# Integrazione CLARA — variante reale

Questa cartella contiene solo la variante reale di Claude, con le porte sul lato corretto. Le versioni JsonCompat e le anteprime restano nel laboratorio grafico esterno.

Importazione: `work/network-assets/import_claude_router.py`, da eseguire nell’editor Unreal in modalità batch. Crea un asset distinto `/Game/NetworkAssets/SM_Cisco1941_Claude`, senza sovrascrivere il modello precedente. Tutti i materiali sono importati come PBR nativi, con le normali DirectX e mappe lineari.

Lo script cambia esclusivamente la voce Cisco1941 nel JSON del gioco, dopo il salvataggio e la verifica di mesh, materiali, texture e quote. Gli otto identificatori delle porte sono conservati; le coordinate sono quelle di `ports-unreal-real.json`.

Ripristino: riassegnare la precedente voce Cisco1941 conservata nel checkpoint `work/network-assets/claude-router-checkpoint/`; gli altri modelli e i dati di gioco non richiedono modifiche.

Le geometrie e texture sono ricostruzioni originali, non un CAD ufficiale Cisco. La resa va controllata in Unreal: le immagini del laboratorio sono render Blender.
