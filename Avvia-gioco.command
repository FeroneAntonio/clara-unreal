#!/bin/zsh
set -eu
project_folder="$(cd -- "$(dirname -- "$0")" && pwd)"
engine='/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
mkdir -p "$project_folder/Saved/Logs"
nohup "$engine" "$project_folder/ClaraDayOne.uproject" -game -windowed -ResX=1280 -ResY=800 -nosplash > "$project_folder/Saved/Logs/Launcher.log" 2>&1 &
