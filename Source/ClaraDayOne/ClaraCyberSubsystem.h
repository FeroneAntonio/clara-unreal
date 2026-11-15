#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ClaraCyberRuntime.h"
#include "ClaraCyberSubsystem.generated.h"
UCLASS()
class CLARADAYONE_API UClaraCyberSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UFUNCTION(BlueprintCallable, Category="Cyber") FString Command(const FString& Input);
    UFUNCTION(BlueprintCallable, Category="Cyber") bool Action(const FString& Name, int32 Number, const FString& Value);
    UFUNCTION(BlueprintPure, Category="Cyber") int32 Day() const { return State.day; }
    UFUNCTION(BlueprintPure, Category="Cyber") int32 Red() const { return State.red; }
    UFUNCTION(BlueprintPure, Category="Cyber") int32 Blue() const { return State.blue; }
    UFUNCTION(BlueprintPure, Category="Cyber") TArray<FString> Notebook() const;
    UFUNCTION(BlueprintPure, Category="Cyber") bool PersistenceAvailable() const { return Writable; }
    const ClaraCyber::Model& GetState() const { return State; }
    FString Hint();
    bool Login(const FString& User, const FString& Password);
    bool StartScene(int32 Scene);
    void AdvanceScene();
    void SetExpectation(const FString& Text);
    bool IsTerminalBusy() const { return Terminal.busy(); }
    bool TickTerminal(float DeltaSeconds);
    bool CancelTerminalCommand();
    FString TerminalTranscript() const;
    FString TaskDescription() const;
    const FString& LastFeedback() const { return Feedback; }
    const FString& LastTutorLine() const { return TutorLine; }
    bool IsIsolatedSession() const { return Isolated; }
    bool ResetForTour();
    FString CaptureForTourCheckpoint() const;
    bool RestoreForTourCheckpoint(const FString& Checkpoint);
private:
    ClaraCyber::Model State;
    ClaraCyber::Terminal Terminal;
    FString Feedback, TutorLine;
    bool Writable = true, Recovered = false, Isolated = false;
    void Save();
};
