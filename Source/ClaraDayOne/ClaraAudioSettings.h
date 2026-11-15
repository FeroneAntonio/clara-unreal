#pragma once
#include "ClaraAudio.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

class SClaraAudioSettings final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClaraAudioSettings){}
        SLATE_ARGUMENT(TWeakObjectPtr<AClaraAudio>,Audio)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Audio=Args._Audio;
        TSharedRef<SVerticalBox> Rows=SNew(SVerticalBox);
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)
            [SNew(STextBlock).ColorAndOpacity(FLinearColor(.065f,.14f,.16f,1.f)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text(FText::FromString(TEXT("Audio"))).AutoWrapText(false)];
        const TCHAR* Labels[]={TEXT("Voci"),TEXT("Effetti"),TEXT("Ambiente")};
        for(int32 Channel=0;Channel<3;++Channel)
        {
            Rows->AddSlot().AutoHeight().Padding(0,8)
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(100)
                    [SNew(STextBlock).ColorAndOpacity(FLinearColor(.065f,.14f,.16f,1.f)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text(FText::FromString(Labels[Channel])).AutoWrapText(false)]]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [SNew(SSlider).SliderBarColor(FLinearColor(.30f,.40f,.42f,1.f)).SliderHandleColor(FLinearColor(.02f,.40f,.39f,1.f)).ToolTipText(FText::FromString(Labels[Channel]))
                        .Value_Lambda([this,Channel]{return Value(Channel);})
                        .OnValueChanged_Lambda([this,Channel](float NewValue)
                        {
                            bChanged=true;
                            if(AClaraAudio* A=Audio.Get())A->SetVolumes(Channel==0?NewValue:Value(0),Channel==1?NewValue:Value(1),Channel==2?NewValue:Value(2));
                        })
                        .OnMouseCaptureEnd_Lambda([this]{Save();})
                        .OnControllerCaptureEnd_Lambda([this]{Save();})]
                +SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)[SNew(SBox).WidthOverride(44)
                    [SNew(STextBlock).ColorAndOpacity(FLinearColor(.065f,.14f,.16f,1.f)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).AutoWrapText(false).Text_Lambda([this,Channel]
                        {return FText::FromString(FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Value(Channel)*100.f)));})]]];
        }
        Rows->AddSlot().AutoHeight().Padding(0,12)
            [SNew(STextBlock).ColorAndOpacity(FLinearColor(.065f,.14f,.16f,1.f)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text(FText::FromString(TEXT("I dialoghi restano sempre leggibili a schermo."))).AutoWrapText(true)];
        Rows->AddSlot().AutoHeight()[SNew(SButton).ButtonColorAndOpacity(FLinearColor(.85f,.88f,.85f,1.f)).ContentPadding(FMargin(16,10))
            .OnClicked_Lambda([this]{if(AClaraAudio* A=Audio.Get()){A->SetVolumes(1.f,1.f,1.f);bChanged=true;Save();}return FReply::Handled();})
            [SNew(STextBlock).ColorAndOpacity(FLinearColor(.065f,.14f,.16f,1.f)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text(FText::FromString(TEXT("Ripristina volumi"))).AutoWrapText(false)]];
        ChildSlot[SNew(SBox).WidthOverride(420)[Rows]];
    }
    virtual ~SClaraAudioSettings(){Save();}
private:
    TWeakObjectPtr<AClaraAudio> Audio;
    bool bChanged=false;
    float Value(int32 Channel) const
    {
        const AClaraAudio* A=Audio.Get();
        return !A?1.f:Channel==0?A->GetVoiceVolume():Channel==1?A->GetEffectsVolume():A->GetAmbienceVolume();
    }
    void Save() {if(bChanged)if(AClaraAudio* A=Audio.Get()){A->SavePreferences();bChanged=false;}}
};

inline TSharedRef<SWidget> MakeClaraAudioSettings(const UObject* WorldContext)
{
    return SNew(SClaraAudioSettings).Audio(AClaraAudio::Get(WorldContext));
}
