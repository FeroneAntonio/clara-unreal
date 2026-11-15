#pragma once
#include "CoreMinimal.h"
class UWorld;
class AClaraStoryDirector;
// Giro di verifica: esegue i passi di un file di testo sulla scena reale e ne salva le schermate.
//   -ClaraTour=<file>  -ClaraShotDir=<cartella>
// Ogni riga: <secondi di attesa> <azione> [argomenti]. Elenco delle azioni in ClaraDevTour.cpp e Docs/VERIFICHE.md.
void StartClaraDevTour(UWorld* World,AClaraStoryDirector* Story,const FString& File);
