# Cisco 1941 ISR — modello CLARA (asset-lab, Claude)

Ricostruzione 3D originale del router **Cisco 1941/K9** (versione non wireless), da usare in primo piano per collegare i cavi.
È un modello procedurale costruito con Blender 5.2.4 dallo script in `source/`. Non è un file CAD Cisco.
Questa cartella non modifica il progetto Unreal: integrazione e verifica in gioco spettano al coordinatore.

## Contenuto

| File | Uso |
|---|---|
| `SM_Cisco1941_Claude.fbx` | Mesh unica `SM_Cisco1941`, **disposizione reale**: dal retro, porte a destra (vedi «Orientamento»). |
| `SM_Cisco1941_Claude_JsonCompat.fbx` | Stessa mesh con la disposizione specchiata per corrispondere alle X attuali di `network-devices.json`. Testi e logo restano leggibili. |
| `Cisco1941_Claude.blend`, `Cisco1941_Claude_JsonCompat.blend` | Sorgenti modificabili, con 8 oggetti separati (coperchio, frontale, griglia, pannello posteriore…). |
| `textures/` | Mappe tileabili 1024²: verniciatura a polvere (N, R) e zinco spazzolato (N, R). Normali in versione OpenGL (Blender) e `_DirectX` (Unreal). |
| `materials.json` | 21 materiali PBR (`M_C1941_*`): colore base, metallic, roughness e texture collegate. |
| `ports-unreal-real.json`, `ports-unreal-compat.json` | Centro della faccia di ogni presa in cm e in assi Unreal, nello stesso schema del JSON di gioco. |
| `build-report-*.json` | Bounds, triangoli e materiali misurati sulla mesh esportata. |
| `renders/` | Render Cycles: fronte, retro, retro e fronte ortogonali, dettaglio porte, dettaglio alimentazione, vista radente per i riflessi, controllo della variante compat. |
| `source/build_cisco1941.py`, `source/verify_fbx.py` | Ricostruzione completa e verifica dell'FBX. |

## Dimensioni e scala

- **Unità: centimetri.** Nel `.blend` 1 unità = 1 cm (Metric, `scale_length 0.01`). Nell'FBX la geometria è in cm, quindi in UE va importata con scala uniforme 1.0.
- **Bounds esportati: 34,3 × 29,8 × 8,9 cm** (L × P × A). Coincidono con `size` del JSON di gioco.
  - Dimensioni ufficiali Cisco: 89 × 343 × 292 mm (A × L × P), cioè 3,5 × 13,5 × 11,5 in, 2 RU.
  - La profondità include 6 mm di rilievo del frontale e i piedini (3,5 mm) sono inclusi nell'altezza.
- Origine: centro in X/Y, Z = 0 sotto i piedini.
- Mesh: 36.310 triangoli, tutti triangolati in export, con normali custom e una UV (`UVMap`). Il modello precedente ne aveva 114.990.
- Verifica eseguita: ho reimportato l'FBX in una scena vuota in metri e ho ottenuto 0,343 × 0,298 × 0,089 m, nessun poligono non triangolare e normali custom presenti.

## Orientamento: da leggere prima dell'integrazione

In Blender il frontale è su +Y e il retro su −Y. L'import FBX standard di Unreal nega Y, quindi in UE il frontale va su −Y e il retro su +Y, come dichiara il JSON (`front -Y`).

**Ho trovato una specchiatura nel JSON attuale.** Le porte del 1941 hanno X negativa (Gi0/0 a −14,9) sul retro +Y. In Unreal, chi guarda il retro da +Y ha +X alla propria destra. Con queste coordinate le porte compaiono quindi a **sinistra**, mentre nel router reale (figura Cisco 1-5 e foto) dal retro stanno a **destra**, con alimentazione e logo a sinistra. Anche il vecchio mesh segue questa disposizione specchiata (console a X = −9,9 nel suo `.blend`).

Per questo consegno due varianti:

1. **`SM_Cisco1941_Claude.fbx` (consigliata).** Disposizione fedele. Per usarla vanno negate le X di tutte le porte del Cisco1941 nel JSON: i valori pronti sono in `ports-unreal-real.json`. Y e Z non cambiano, a parte quanto indicato sotto.
2. **`SM_Cisco1941_Claude_JsonCompat.fbx`.** Si sostituisce al mesh attuale senza toccare il JSON, perché le X coincidono. La disposizione resta però specchiata rispetto al router vero, anche se ogni scritta è leggibile.

Profondità delle prese: la faccia delle prese RJ-45/USB è a Y = 14,66 cm e quella della presa IEC a 14,84. Il JSON attuale usa 14,83 per tutte. Lo scarto è di 1,7 mm e non serve correggerlo, a meno che la spina debba entrare nella presa.

Non ho verificato in Unreal quale variante appaia corretta: non ho avviato istanze UE, come richiesto. Il controllo rapido in gioco è questo: guardando il retro, il logo «cisco» e l'interruttore devono stare in alto a sinistra e GE 0/0 con le due USB all'estrema destra.

## Importazione in Unreal Engine 5.8

1. Importa l'FBX in `/Game/NetworkAssets/` con nome a scelta, ad esempio `SM_Cisco1941_Claude`, senza sovrascrivere l'asset in uso.
2. Opzioni Static Mesh:
   - Import Uniform Scale **1.0**
   - Convert Scene **on**, Force Front X Axis **off**
   - Normal Import Method **Import Normals and Tangents**, oppure *Import Normals*
   - Generate Lightmap UVs **on**
   - Combine Meshes **on**
   - Material Import: *Create New Materials*, oppure assegna i materiali a mano
3. Materiali: l'FBX porta colore base, metallic e roughness. Per coperchio (`CoverPowderCoat`) e pannello posteriore (`RearPanelZinc`) collega le texture `_R` (Roughness, sRGB off) e `_N_DirectX` (Normal) indicate in `materials.json`, con coordinate UV 0.
4. Colore dei LED: gli slot `LedGreen` e `LedAmber` sono già separati. Per accenderli basta un parametro Emissive.
5. Il componente `ClaraNetworkDevice` normalizza i bounds a `size`. Con questo mesh il fattore vale circa 1,0 su tutti gli assi.

## Cosa è modellato

- **Coperchio** in lamiera verniciata nero opaco, con raccordi arrotondati, cornice posteriore con battuta di 3 mm, 2 × 13 asole di ventilazione sui fianchi con fondo scuro, 4 viti brunite e piedini in gomma.
- **Frontale** in plastica nera lucida, con sopracciglio arrotondato, finestra del logo, LED SYS/ACT/POE, scritta «Cisco 1900 Series» e griglia esagonale traforata (≈500 fori) con fondo scuro a 4 mm.
- **Retro** in zinco spazzolato:
  - Modulo di alimentazione con interruttore a bilanciere (I/O), dati di targa e presa IEC C14 con tre spinotti.
  - Tre coperchi EHWIC con feritoie e viti zigrinate imperdibili, due coperchi CompactFlash con etichetta.
  - Slot Kensington, vite di terra con simbolo.
  - Prese: GE 0/0, GE 0/1, AUX, CONSOLE (RJ-45 schermate, cavità profonda 15 mm, 8 contatti dorati, linguette), console mini-USB e due USB-A sovrapposte (linguetta e contatti). Ognuna ha etichette gialle, nere o blu e LED S/L.
- **Scritte**: tutte in geometria piatta sollevata di 0,1–0,2 mm, leggibili dall'esterno su ogni faccia. Il controllo visivo sui render non mostra testo specchiato.
- **Normali**: le facce piane tengono la propria normale, le superfici curve sono mediate solo tra loro. Sulle superfici visibili non ho usato booleane: le aperture sono costruite con poligoni forati. L'unica booleana sono le asole dei fianchi, su lamiera piana, e i render radenti non mostrano triangoli o riflessi deformati.

## Limiti, da non nascondere

- Non è fotorealistico in senso stretto. Nei render la resa è convincente da 30–60 cm e nel dettaglio porte, ma mancano usura, polvere, etichette di serie o regolamentari e la goffratura del logo sul coperchio, che è resa come stampa lucida.
- Posizioni delle porte: seguono il JSON di gioco, che a sua volta approssima la figura Cisco 1-5. Le quote millimetriche di feritoie, viti, coperchi EHWIC/CF e scritte sono stime dalle foto, non misure su un esemplare.
- Il coperchio EHWIC centrale e le etichette «ISM/WLAN» e «CF 0/1» sono interpretati dalle foto di 1941 usati: la disposizione degli slot varia con i moduli installati.
- Il carattere usato è Inter, non il carattere tipografico Cisco. Nelle scritte piccole l'«1» assomiglia a una «I».
- Texture a 1024² tileabili: a distanza molto ravvicinata la verniciatura ripete il motivo ogni 25 cm.

## Fonti

- Cisco, *Cisco 1900 Series ISR Hardware Installation Guide — Overview*, figura 1-5 (retro 1941) e figura del frontale: <https://www.cisco.com/c/en/us/td/docs/routers/access/1900/hardware/installation/guide/1900_HIG/19over.html> (immagini 273452 e 250995, copie di lavoro già in `work/assets-research/cisco/`).
- Cisco, *Cisco 1900 Series Integrated Services Routers Data Sheet* (dimensioni 3,5 × 13,5 × 11,5 in, 2 RU): <https://www.cisco.com/c/en/us/products/collateral/routers/1900-series-integrated-services-routers-isr/data_sheet_c78_556319.pdf>.
- Foto di esemplari reali consultate solo come riferimento visivo per colori e materiali, senza scaricarle né incorporarle: retro (annuncio itinstock.com, «CISCO 1941/K9 Integrated Services Gigabit Router») e fronte (wisp.ly, «Cisco 1941 Router»), trovate tramite Bing Immagini.
- Dossier del progetto: `outputs/grafica-ufficio/Riferimenti-Cisco-apparati.md`.

## Licenze

- **Geometria, materiali e texture**: creati da zero dallo script in questa cartella. Nessun modello di terzi scaricato o acquistato. Le texture sono rumore procedurale generato con numpy.
- **Carattere**: Inter, incluso in Blender, con licenza SIL Open Font License 1.1. I glifi sono convertiti in mesh.
- **HDRI dei soli render**: `studio.exr` incluso in Blender, CC0 (Poly Haven). Non fa parte dell'asset.
- **Marchi**: «Cisco», il logo a barre e i nomi dei prodotti sono marchi di Cisco Systems e sono riprodotti solo per rappresentare l'apparato nel gioco, senza implicare un'approvazione. Prima di un'eventuale distribuzione commerciale del gioco conviene una verifica legale sull'uso del logo.
- Alternativa a pagamento valutata e **non acquistata**: «Cisco 1941 router SME» su CGTrader, 10 USD, licenza Royalty Free (pagina riportata nel dossier del progetto). Non serve per questa consegna.

## Rigenerare

```bash
B=/Applications/Blender.app/Contents/MacOS/Blender
$B -b --factory-startup --python source/build_cisco1941.py -- . real render
$B -b --factory-startup --python source/build_cisco1941.py -- . compat
$B -b --factory-startup --python source/verify_fbx.py -- SM_Cisco1941_Claude.fbx
```

Esegui i comandi dalla cartella `outputs/asset-lab/claude-cisco1941/`. Senza `render` la costruzione e l'export richiedono circa 2 secondi; i 7 render richiedono circa 4 minuti su GPU Apple M5.
