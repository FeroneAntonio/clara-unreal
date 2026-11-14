#include "ClaraPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"

FString AClaraPlayerController::ExecuteSwitchCommand(const FString& Command)
{
    if (!GetClaraPlayer() || !GetClaraPlayer()->IsConsoleConnected()) return TEXT("Console scollegata: non e' stato eseguito alcun comando.");
    if (bCMLMode)
    {
        if (!CanUseCMLInput()) return TEXT("[CML] Console non pronta: nessun comando locale viene eseguito.");
        SendCMLConsoleInput(Command, ECMLAction::Submit);
        return TEXT("[CML] Invio asincrono al laboratorio; il risultato arriva dalla console remota.");
    }
    const FString Result = SwitchCLI.Execute(Command);
    if (auto* Player = GetClaraPlayer()) Player->OnSwitchConfigurationChecked(SwitchCLI.IsMissionConfigured());
    return Result;
}

void AClaraPlayerController::TerminalTextChanged(const FText& Text)
{
    if (!bSwitchTerminal || bUpdatingTerminalText || !TerminalInput.IsValid()) return;
    const FString Input = Text.ToString();
    int32 Question;
    if (!Input.FindChar(TEXT('?'), Question)) return;
    const FString Prefix = Input.Left(Question);
    if (bCMLMode) SendCMLConsoleInput(Prefix, ECMLAction::Help);
    else TerminalOutput += TEXT("\n") + SwitchCLI.GetPrompt() + Prefix + TEXT("?\n") + SwitchCLI.Help(Prefix) + TEXT("\n");
    if (TerminalScroll.IsValid()) TerminalScroll->ScrollToEnd();
    bUpdatingTerminalText = true;
    TerminalInput->SetText(FText::FromString(Prefix));
    TerminalInput->GoTo(ETextLocation::EndOfDocument);
    bUpdatingTerminalText = false;
}

FReply AClaraPlayerController::TerminalKeyDown(const FGeometry&, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape) { CloseTerminal(); return FReply::Handled(); }
    if (!TerminalInput.IsValid()) return FReply::Unhandled();
    if (Event.GetKey() == EKeys::Tab && bSwitchTerminal)
    {
        const FString Prefix = TerminalInput->GetText().ToString();
        if (bCMLMode)
        {
            SendCMLConsoleInput(Prefix, ECMLAction::Completion);
            return FReply::Handled();
        }
        const FString Completed = SwitchCLI.Complete(Prefix);
        if (Completed == Prefix) TerminalOutput += TEXT("\n") + SwitchCLI.Help(Prefix) + TEXT("\n");
        bUpdatingTerminalText = true;
        TerminalInput->SetText(FText::FromString(Completed));
        TerminalInput->GoTo(ETextLocation::EndOfDocument);
        bUpdatingTerminalText = false;
        return FReply::Handled();
    }
    if (Event.GetKey() == EKeys::Up || Event.GetKey() == EKeys::Down)
    {
        HistoryIndex = FMath::Clamp(HistoryIndex + (Event.GetKey() == EKeys::Up ? -1 : 1), 0, TerminalHistory.Num());
        TerminalInput->SetText(FText::FromString(HistoryIndex < TerminalHistory.Num() ? TerminalHistory[HistoryIndex] : FString()));
        TerminalInput->GoTo(ETextLocation::EndOfDocument);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void AClaraPlayerController::AskSwitchMentor()
{
    if (bCMLMode) return;
    TerminalOutput += TEXT("\nLuca: ") + SwitchCLI.GetMentorFeedback() + TEXT("\n");
    if (TerminalScroll.IsValid()) TerminalScroll->ScrollToEnd();
    if (TerminalInput.IsValid()) FSlateApplication::Get().SetKeyboardFocus(TerminalInput);
}

void AClaraPlayerController::ResetSwitchWithMentor()
{
    if (bCMLMode) return;
    if (!bResetConfirmation)
    {
        bResetConfirmation = true;
        TerminalOutput += TEXT("\nLuca: posso ripristinare lo switch di questa esercitazione. Perderai la configurazione corrente e quella salvata del solo modello locale. Premi ancora Conferma per ricominciare.\n");
        return;
    }
    SwitchCLI.Reset();
    bResetConfirmation = false;
    if (auto* Player = GetClaraPlayer()) Player->RestartSwitchExercise();
    TerminalOutput = TEXT("Luca: switch ripristinato. Il cavo console e' ancora collegato; puoi riprovare.\nModello didattico locale: nessun apparato esterno modificato.\n");
    if (TerminalInput.IsValid())
    {
        TerminalInput->SetText(FText::GetEmpty());
        FSlateApplication::Get().SetKeyboardFocus(TerminalInput);
    }
}
