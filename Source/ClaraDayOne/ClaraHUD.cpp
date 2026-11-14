#include "ClaraHUD.h"
#include "ClaraPlayer.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
TArray<FString> WrapClaraText(AHUD& HUD, const FString& Value, float Width, UFont* Font, float FontScale)
{
    TArray<FString> Lines;
    TArray<FString> Paragraphs;
    Value.ParseIntoArray(Paragraphs, TEXT("\n"), false);
    for (const FString& Paragraph : Paragraphs)
    {
        TArray<FString> Words;
        Paragraph.ParseIntoArrayWS(Words);
        FString Line;
        for (const FString& Word : Words)
        {
            const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
            float TextWidth = 0, TextHeight = 0;
            HUD.GetTextSize(Candidate, TextWidth, TextHeight, Font, FontScale);
            if (!Line.IsEmpty() && TextWidth > Width)
            {
                Lines.Add(Line);
                Line = Word;
            }
            else Line = Candidate;
        }
        Lines.Add(Line);
    }
    return Lines;
}
}

void AClaraHUD::Text(const FString& Value, float X, float Y, FLinearColor Color, float Size)
{
    const FLinearColor Shadow(0.f, 0.f, 0.f, Color.A * 0.85f);
    DrawText(Value, Shadow, X * Scale + 1.2f * Scale, Y * Scale + 1.2f * Scale,
        GEngine->GetMediumFont(), Size * Scale, false);
    DrawText(Value, Color, X * Scale, Y * Scale, GEngine->GetMediumFont(), Size * Scale, false);
}

float AClaraHUD::Wrapped(const FString& Value, float X, float Y, float Width, FLinearColor Color, float Size)
{
    float TextWidth = 0, TextHeight = 0;
    GetTextSize(TEXT("Ag"), TextWidth, TextHeight, GEngine->GetMediumFont(), Size * Scale);
    const float LineHeight = TextHeight / Scale * 1.45f;
    for (const FString& Line : WrapClaraText(*this, Value, Width * Scale, GEngine->GetMediumFont(), Size * Scale))
    {
        Text(Line, X, Y, Color, Size);
        Y += LineHeight;
    }
    return Y;
}

void AClaraHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner || !GEngine) return;
    AClaraPlayer* Player = Cast<AClaraPlayer>(PlayerOwner->GetPawn());
    if (!Player) return;
    const auto* Controller = Cast<AClaraPlayerController>(PlayerOwner);
    if (Player->HasDialogue() || (Controller && (Controller->IsTerminalOpen() || Controller->IsPanelOpen()))) return;

    Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1440.f, Canvas->SizeY / 900.f), 0.65f, 2.f);
    const float Width = Canvas->SizeX / Scale;
    const float Height = Canvas->SizeY / Scale;
    float BaseWidth = 0, BaseHeight = 0;
    GetTextSize(TEXT("Ag"), BaseWidth, BaseHeight, GEngine->GetMediumFont(), 1.f);
    BaseHeight = FMath::Max(BaseHeight, 1.f);
    const float Small = 13.f / BaseHeight;
    const float Heading = 14.f / BaseHeight;
    const float Body = 18.f / BaseHeight;
    const float Subtitle = 20.f / BaseHeight;
    const FLinearColor Ivory(0.95f, 0.93f, 0.86f);
    const FLinearColor Muted(0.76f, 0.75f, 0.69f, 0.88f);
    const FLinearColor Amber(0.87f, 0.72f, 0.48f);
    auto Rect = [&](FLinearColor Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, X * Scale, Y * Scale, W * Scale, H * Scale);
    };
    auto Measure = [&](const FString& Value, float Size)
    {
        float W = 0, H = 0;
        GetTextSize(Value, W, H, GEngine->GetMediumFont(), Size * Scale);
        return FVector2D(W / Scale, H / Scale);
    };
    auto Lines = [&](const FString& Value, float MaxWidth, float Size)
    {
        return WrapClaraText(*this, Value, MaxWidth * Scale, GEngine->GetMediumFont(), Size * Scale);
    };
    auto Centered = [&](const FString& Value, float Y, FLinearColor Color, float Size)
    {
        Text(Value, (Width - Measure(Value, Size).X) * 0.5f, Y, Color, Size);
    };

    const float Margin = 44.f;
    Text(Player->GetStageTitle().ToUpper(), Margin, 38, Amber, Heading);
    const float ObjectiveEnd = Wrapped(Player->GetObjective(), Margin, 65, 420, Ivory, Body);

    if (!Player->GetToast().IsEmpty())
    {
        TArray<FString> HintLines = Lines(Player->GetToast(), 440, Heading);
        if (HintLines.Num() > 2)
        {
            HintLines.SetNum(2);
            FString& Last = HintLines.Last();
            while (!Last.IsEmpty() && Measure(Last + TEXT("..."), Heading).X > 440) Last.LeftChopInline(1);
            Last += TEXT("...");
        }
        const float HintY = ObjectiveEnd + 47;
        Rect(FLinearColor(0.87f, 0.72f, 0.48f, 0.65f), Margin, HintY + 1, 2, HintLines.Num() * 21.f - 4);
        for (int32 Index = 0; Index < HintLines.Num(); ++Index)
            Text(HintLines[Index], Margin + 12, HintY + Index * 21.f, Ivory, Heading);
    }

    if (!Player->IsInputBlocked())
    {
        AActor* Focus = Player->GetFocusedActor();
        if (Focus)
        {
            FVector Origin, Extent;
            Focus->GetActorBounds(false, Origin, Extent);
            FVector2D Low(FLT_MAX), High(-FLT_MAX);
            for (int32 Corner = 0; Corner < 8; ++Corner)
            {
                FVector2D Point;
                const FVector Offset((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
                if (PlayerOwner->ProjectWorldLocationToScreen(Origin + Offset, Point, true))
                {
                    Point /= Scale;
                    Low.X = FMath::Min(Low.X, Point.X); Low.Y = FMath::Min(Low.Y, Point.Y);
                    High.X = FMath::Max(High.X, Point.X); High.Y = FMath::Max(High.Y, Point.Y);
                }
            }
            if (Low.X < High.X)
            {
                const FVector2D Center = (Low + High) * .5f;
                const FVector2D Radius(FMath::Max(16.f, (High.X - Low.X) * .5f + 5), FMath::Max(14.f, (High.Y - Low.Y) * .5f + 5));
                for (int32 Corner = 0; Corner < 4; ++Corner)
                {
                    const float SX = (Corner & 1) ? 1 : -1, SY = (Corner & 2) ? 1 : -1;
                    const float X = Center.X + SX * Radius.X, Y = Center.Y + SY * Radius.Y;
                    Rect(Amber, FMath::Min(X, X - SX * 10), Y, 10, 1.5f);
                    Rect(Amber, X, FMath::Min(Y, Y - SY * 10), 1.5f, 10);
                }
            }
            FString Label = Player->GetInteractionLabel();
            if (Label.StartsWith(TEXT("E  "))) Label.RightChopInline(3);
            const float LabelWidth = FMath::Min(Measure(Label, Body).X, 750.f);
            const float PromptWidth = LabelWidth + 89;
            const float Left = (Width - PromptWidth) * 0.5f;
            const float Y = Height - 116;
            Rect(FLinearColor(0.015f, 0.015f, 0.012f, 0.32f), Left - 12, Y - 10, PromptWidth + 24, 43);
            Rect(FLinearColor(0.95f, 0.93f, 0.86f, 0.84f), Left, Y - 1, 55, 25);
            Text(TEXT("Clic"), Left + 11, Y + 3, FLinearColor(0.08f, 0.08f, 0.065f), Heading);
            Text(Label, Left + 70, Y + 1, Ivory, Body);
        }
        if (Player->IsHoldingCable())
            Text(TEXT("Connettore Ethernet  ·  Q posa"), Margin, Height - 47, Muted, Heading);
        if (Player->IsVisionEnabled())
        {
            const FString Vision = TEXT("Visione rete · illustrazione");
            Text(Vision, Width - Margin - Measure(Vision, Small).X, Height - 44, Muted, Small);
        }
    }

    if (Player->HasDialogue())
    {
        const float ContentWidth = FMath::Min(960.f, Width - 100.f);
        const float Left = (Width - ContentWidth) * 0.5f;
        const TArray<FString>& Choices = Player->GetChoices();
        const float SubtitleLineHeight = Measure(TEXT("Ag"), Subtitle).Y * 1.45f;
        const float BodyHeight = Lines(Player->GetDialogue(), ContentWidth, Subtitle).Num() * SubtitleLineHeight;
        float ChoiceHeight = 0;
        for (const FString& Choice : Choices)
            ChoiceHeight += Lines(Choice, ContentWidth - 45, Body).Num() * 26.1f + 12;
        const float ContentHeight = 28 + BodyHeight + (Choices.IsEmpty() ? 0 : ChoiceHeight + 17) + 36;
        const float Top = FMath::Max(220.f, Height - 65 - ContentHeight);
        const bool bNotebook = Player->GetSpeaker().StartsWith(TEXT("Taccuino"));
        const float BackdropAlpha = bNotebook ? 0.60f : 0.28f;
        Rect(FLinearColor(0.012f, 0.012f, 0.01f, BackdropAlpha), Left - 25, Top - 17, ContentWidth + 50, ContentHeight + 28);
        Text(Player->GetSpeaker(), Left, Top, Amber, Heading);
        float Y = Wrapped(Player->GetDialogue(), Left, Top + 30, ContentWidth, Ivory, Subtitle);
        if (!Choices.IsEmpty()) Y += 17;
        for (int32 Index = 0; Index < Choices.Num(); ++Index)
        {
            Text(FString::FromInt(Index + 1), Left, Y + 1, Amber, Body);
            Y = Wrapped(Choices[Index], Left + 31, Y, ContentWidth - 31, Ivory, Body) + 12;
        }
        Text(Choices.IsEmpty() ? TEXT("Invio  Continua") : TEXT("Scegli una risposta  1 - 4"), Left, Y + 12, Muted, Small);
    }
    else
    {
        const float Elapsed = GetWorld()->GetTimeSeconds();
        if (Elapsed < 8.f)
            Text(TEXT("Clic sugli oggetti · Tieni premuto destro per guardare · WASD muoviti"), Margin, Height - 45, Muted, Small);
    }
}
