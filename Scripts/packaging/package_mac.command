#!/bin/zsh
set -euo pipefail
tnes_root="${0:A:h:h:h}"
tnes_engine="${TNES_UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
tnes_archive="${TNES_ARCHIVE_DIR:-$tnes_root/Dist/Mac}"
"$tnes_engine/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$tnes_root/ClaraDayOne.uproject" -noP4 -platform=Mac -clientconfig=Shipping -build -cook -stage -pak -package -archive -archivedirectory="$tnes_archive" -unattended -utf8output -nocompileeditor -nodebuginfo
