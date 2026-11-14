#!/bin/zsh
set -eu
project_folder="$(cd -- "$(dirname -- "$0")" && pwd)"
editor_app='/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app'
open -a "$editor_app" --args "$project_folder/ClaraDayOne.uproject"
