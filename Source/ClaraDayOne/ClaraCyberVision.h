#pragma once
#include "CoreMinimal.h"
class UClaraCyberSubsystem;
class SWidget;
TSharedRef<SWidget> MakeClaraCyberVision(UClaraCyberSubsystem* Lab,TFunction<void(const FString&)> OnResult);
