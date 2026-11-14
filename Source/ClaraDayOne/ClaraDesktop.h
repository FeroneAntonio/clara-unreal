#pragma once

#include "CoreMinimal.h"

class AClaraPlayer;
class SWidget;

/** A teaching desktop UI inside the PC; it does not boot or embed a full operating system. */
TSharedRef<SWidget> MakeClaraDesktop(AClaraPlayer* Player, FSimpleDelegate OnClose);
