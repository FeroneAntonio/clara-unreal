# Backend CML & Splunk Handoff Notes (per `clara-unreal`)

Questo branch (`feat/linux-ux-backend-handoff`) parte da `origin/feat/tnes-demo-mac` e aggiunge:
1. Fix focus/input `WASD` + Frecce su Linux (`SDL_VIDEODRIVER=x11`) in `ClaraRoomVisitor.cpp` e `ClaraStoryDirector.cpp`.
2. Questo riepilogo delle API live esposte dal backend `cisco-cyber-office` (branch `test/cml-engines-sprint`).

## Endpoint Live (`https://api-cml.kappino.me`)

`ClaraGateway.cpp` punta già a `https://api-cml.kappino.me` (quando non si passa `-ClaraLocalLab`). Oltre a `/api/terminal/command`, sul backend sono disponibili:

- **Console stateful (senza Ctrl-C/Z spuri, auth IOS dinamica)**:
  - `POST /api/console-session/{room_id}/{node}` — apre sessione
  - `POST /api/console-session/{room_id}/{node}/send` — invia comando
  - `GET /api/console-session/{room_id}/{node}/read` — legge output (con long-poll e ANSI strip)
  - `DELETE /api/console-session/{room_id}/{node}` — chiude sessione
- **Reset stanza al salvataggio pulito (`autosave: false`)**:
  - `POST /api/room/{room_id}/reload`
- **Hint progressivi a 3 livelli + Ollama `gemma4:12b`**:
  - `GET /api/room/{room_id}/hint?level=1|2|3` (personas: Alessia, Davide, Luca, Gaia)
- **Splunk SOC live (`clara-splunk` locale collegato in `live_splunk`)**:
  - `GET /api/soc/alerts?room=office`
- **Fault injection (training / sfida)**:
  - `POST /api/room/{room_id}/inject-fault` (`vlan_mismatch`, `interface_shutdown`, `missing_static_route`, `trunk_vlan_exclusion`)
- **Traduttore Windows CLI su nodi Alpine**:
  - Supporta `ipconfig`, `ipconfig /all`, `arp -a`, `arp -d *`, `ping`, `tracert`.

## Note operative Linux & CML

- **Avvio Unreal su Linux/Wayland**: lanciare con `SDL_VIDEODRIVER=x11` (su KDE/Wayland nativo SDL 3.4.4 blocca i tasti lettera `WASD`).
- **CML Free (limite 5 nodi)**: assicurarsi che il lab `FuoriRete_Room_Office` sia `STARTED` su CML (`https://cml.kappino.me`) quando si eseguono test live della stanza Office.
