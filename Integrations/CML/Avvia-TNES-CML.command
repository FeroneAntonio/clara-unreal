#!/bin/zsh
set -eu
app="${1:-$HOME/Applications/TNES.app}"
if [[ ! -d "$app" ]]; then
  echo "Trascina TNES.app in Applicazioni nella tua cartella utente oppure passa il percorso come primo argomento."
  exit 1
fi
exec /usr/bin/open "$app" --args -ClaraLiveCML -windowed
