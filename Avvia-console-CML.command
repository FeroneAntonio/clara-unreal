#!/bin/zsh
set -eu
project_folder="$(cd -- "$(dirname -- "$0")" && pwd)"
editor_binary='/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
# The bridge owns controller credentials. The game only receives its local URL.
unset CML_PASSWORD CML_PASS
export CLARA_CML_BRIDGE="${CLARA_CML_BRIDGE:-http://127.0.0.1:8766}"
exec "$editor_binary" "$project_folder/ClaraDayOne.uproject" -game -windowed -ResX=1280 -ResY=800 -nosplash -ClaraConsolePreview
