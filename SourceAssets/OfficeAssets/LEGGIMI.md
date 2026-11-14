# Asset ufficio acquisiti e importati

Quattro modelli reali Poly Haven, licenza CC0: https://polyhaven.com/license .
Le fonti, gli autori, gli URL e i checksum MD5 verificati sono registrati in `manifest.json` e nei file `provenance.json` delle singole cartelle.

Sono stati scaricati complessivamente 26.953.608 byte, con mappe PBR 2K. La geometria FBX è stata importata nei quattro StaticMesh sotto `/Game/OfficeAssets`, assegnando i materiali corrispondenti alle parti originali; il descrittore glTF ufficiale ha guidato i collegamenti delle texture. Le normal map OpenGL sono state convertite mediante inversione del canale verde. La pianta usa materiale a due facce e la modalità di mascheratura dichiarata dalla fonte.

`import-result.json` contiene percorsi nativi, dimensioni in centimetri, pivot/bounds, slot, texture collegate e note di orientamento. I grafi dei sei materiali e le dimensioni sono stati verificati in Unreal; il controllo visivo nella nuova stanza resta da fare.

L'editor d'importazione senza rendering si è chiuso e non è rimasto attivo. Il processo ha restituito codice 1 all'uscita anche nella seconda esecuzione: il report dello script risulta completo con quattro asset e nessun errore, e nel secondo log non sono presenti Error/Fatal/Traceback. Questo limite del processo di uscita è registrato, senza dichiarare un codice di ritorno zero.
