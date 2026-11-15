#pragma once

#include "CoreMinimal.h"
class FJsonObject;

/** Layer-order lesson: only the top block can move; there is no disk-size restriction. */
struct CLARADAYONE_API FClaraLayerTower
{
    FString Kind;
    TArray<int32> Pegs[3]; // Bottom to top; layer 0 is Physical / Network Access.
    int32 Selected=-1, Errors=0, Revision=0;
    bool PreviewPending=true, Completed=false;
    float PreviewRemaining=10.f, ShakeRemaining=0.f;

    FClaraLayerTower(){Reset(TEXT("osi"));}
    void Reset(const FString& InKind);
    bool SelectPeg(int32 Peg);
    bool Check();
    void Preview(float Seconds=5.f);
    void Tick(float Delta,bool Visible);
    void SolveForTest();
    bool IsCorrect() const;
    bool IsPreview() const{return PreviewPending||PreviewRemaining>0.f;}
    bool OffersHint() const{return Errors>=3;}
    TSharedRef<FJsonObject> ExportState() const;
    bool ImportState(const TSharedPtr<FJsonObject>& State);
};
