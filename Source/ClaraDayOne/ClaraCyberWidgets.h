#pragma once
#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

inline TSharedRef<SWidget> ClaraCyberButtonLabel(const FString& Text) {
    return SNew(SBox).MinDesiredHeight(32)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFitX).StretchDirection(EStretchDirection::DownOnly)
            [SNew(STextBlock).Text(FText::FromString(Text))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular",15)).ColorAndOpacity(FLinearColor::White).AutoWrapText(false)]];
}
