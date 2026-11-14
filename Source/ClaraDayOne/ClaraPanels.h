#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SWidget;
class AClaraPlayer;

TSharedRef<SWidget> MakeClaraNotebook(AClaraPlayer* Player, FSimpleDelegate OnClose);
TSharedRef<SWidget> MakeClaraInventory(AClaraPlayer* Player, FSimpleDelegate OnClose);
TSharedRef<SWidget> MakeClaraDialogue(AClaraPlayer* Player);
