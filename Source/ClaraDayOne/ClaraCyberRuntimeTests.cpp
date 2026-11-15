#include "ClaraCyberRuntimeChecks.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "ClaraCyberSubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/SubsystemCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraCyberTerminalTest,"Clara.Cyber.Terminal",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraCyberTerminalTest::RunTest(const FString&) {
    for(const auto& Failure:ClaraCyberRuntimeChecks())AddError(UTF8_TO_TCHAR(Failure.c_str()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraCyberAllPathsTest,"Clara.Cyber.AllPaths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraCyberAllPathsTest::RunTest(const FString&) {
    for(const auto& Failure:ClaraCyberAllPathChecks())AddError(UTF8_TO_TCHAR(Failure.c_str()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraCyberIsolatedCheckpointTest,"Clara.Cyber.IsolatedCheckpoint",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraCyberIsolatedCheckpointTest::RunTest(const FString&) {
    if(!FApp::IsUnattended()){AddError(TEXT("Run with -unattended: this test must never touch a player's session."));return false;}
    struct FSnapshot{FString Path;bool Existed;TArray<uint8> Bytes;FDateTime Modified;};
    TArray<FSnapshot> Files;
    for(const TCHAR* Suffix:{TEXT(""),TEXT(".bak"),TEXT(".tmp")}){
        FSnapshot Item;Item.Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ClaraCyberState.txt"))+Suffix;
        Item.Existed=IFileManager::Get().FileExists(*Item.Path);
        if(Item.Existed){FFileHelper::LoadFileToArray(Item.Bytes,*Item.Path);Item.Modified=IFileManager::Get().GetTimeStamp(*Item.Path);}
        Files.Add(MoveTemp(Item));
    }
    UGameInstance* Instance=NewObject<UGameInstance>();
    UClaraCyberSubsystem* Lab=NewObject<UClaraCyberSubsystem>(Instance);
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Lab->Initialize(Collection);
    TestTrue(TEXT("Unattended subsystem is isolated"),Lab->IsIsolatedSession());
    TestEqual(TEXT("Existing user save was not loaded"),Lab->Day(),1);
    TestTrue(TEXT("Fresh state"),Lab->GetState().notes.empty());
    TestTrue(TEXT("Intro scene starts"),Lab->StartScene(0));Lab->AdvanceScene();
    const int Offset=Lab->GetState().dialogueOffset;
    const FString Checkpoint=Lab->CaptureForTourCheckpoint();
    TestTrue(TEXT("Reset allowed only in isolated session"),Lab->ResetForTour());
    TestTrue(TEXT("Restore in-memory checkpoint"),Lab->RestoreForTourCheckpoint(Checkpoint));
    TestEqual(TEXT("Dialogue scene restored"),Lab->GetState().dialogueScene,0);
    TestEqual(TEXT("Exact dialogue line restored"),Lab->GetState().dialogueOffset,Offset);
    TestFalse(TEXT("Malformed checkpoint rejected"),Lab->RestoreForTourCheckpoint(TEXT("broken")));
    TestEqual(TEXT("Rejection preserves previous state"),Lab->CaptureForTourCheckpoint(),Checkpoint);
    Lab->ResetForTour();
    for(int Port:{21,22,23})Lab->Action(TEXT("inspect"),Port,TEXT(""));
    Lab->Action(TEXT("choose"),1,TEXT(""));
    const FString Before=Lab->CaptureForTourCheckpoint();
    Lab->Command(TEXT("ping 192.168.10.20"));Lab->TickTerminal(1);
    TestFalse(TEXT("Busy state rejects exercise changes"),Lab->Action(TEXT("classify"),21,TEXT("2")));
    TestTrue(TEXT("Pending ping can be cancelled"),Lab->CancelTerminalCommand());Lab->TickTerminal(20);
    TestEqual(TEXT("Cancel preserves checkpoint"),Lab->CaptureForTourCheckpoint(),Before);
    Lab->Hint();Lab->StartScene(0);Lab->AdvanceScene();
    // A plain-packet inspection cannot satisfy Blue's encrypted-traffic verification.
    ClaraCyber::Model BlueState;BlueState.day=3;BlueState.path={{'A','A','B'}};
    BlueState.rewards={{31,31,3}};BlueState.finished={{true,true,false}};
    BlueState.logRecognized=true;BlueState.passwordChanged=true;BlueState.tls=true;
    TestTrue(TEXT("Restore valid final-day fixture"),Lab->RestoreForTourCheckpoint(UTF8_TO_TCHAR(BlueState.save().c_str())));
    TestFalse(TEXT("Plain packet cannot complete TLS verification"),Lab->Action(TEXT("vision"),0,TEXT("")));
    TestFalse(TEXT("No encrypted-traffic evidence from plain packet"),Lab->GetState().trafficChecked);
    TestTrue(TEXT("Encrypted packet is accepted"),Lab->Action(TEXT("vision"),1,TEXT("")));
    for(const auto& Item:Files){
        TestEqual(TEXT("User save existence unchanged"),IFileManager::Get().FileExists(*Item.Path),Item.Existed);
        if(Item.Existed){TArray<uint8> Bytes;FFileHelper::LoadFileToArray(Bytes,*Item.Path);TestTrue(TEXT("User save bytes unchanged"),Bytes==Item.Bytes);TestTrue(TEXT("User save timestamp unchanged"),IFileManager::Get().GetTimeStamp(*Item.Path)==Item.Modified);}
    }
    Lab->Deinitialize();
    return !HasAnyErrors();
}
#endif
