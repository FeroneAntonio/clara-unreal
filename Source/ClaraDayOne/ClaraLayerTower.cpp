#include "ClaraLayerTower.h"
#include "Dom/JsonObject.h"

void FClaraLayerTower::Reset(const FString& InKind)
{
    Kind=InKind.Equals(TEXT("tcp"),ESearchCase::IgnoreCase)||InKind.Equals(TEXT("tcpip"),ESearchCase::IgnoreCase)||InKind.Equals(TEXT("tcp/ip"),ESearchCase::IgnoreCase)?TEXT("tcp"):TEXT("osi");
    for(auto& Peg:Pegs)Peg.Empty();
    Pegs[0]=Kind==TEXT("tcp")?TArray<int32>{2,0,3,1}:TArray<int32>{3,0,5,1,6,2,4};
    Selected=-1;Errors=0;Completed=false;PreviewPending=true;PreviewRemaining=10.f;ShakeRemaining=0.f;++Revision;
}
bool FClaraLayerTower::SelectPeg(int32 Peg)
{
    if(Peg<0||Peg>2||IsPreview()||Completed)return false;
    if(Selected<0)
    {if(Pegs[Peg].IsEmpty())return false;Selected=Peg;++Revision;return true;}
    if(Selected!=Peg)
    {
        if(Selected>2||Pegs[Selected].IsEmpty()){Selected=-1;return false;}
        Pegs[Peg].Add(Pegs[Selected].Pop(EAllowShrinking::No));
    }
    Selected=-1;++Revision;return true;
}
bool FClaraLayerTower::IsCorrect() const
{
    const int32 Count=Kind==TEXT("tcp")?4:7;
    if(!Pegs[0].IsEmpty()||!Pegs[1].IsEmpty()||Pegs[2].Num()!=Count)return false;
    for(int32 I=0;I<Count;++I)if(Pegs[2][I]!=I)return false;
    return true;
}
bool FClaraLayerTower::Check()
{
    if(Completed)return true;
    if(IsPreview())return false;
    Selected=-1;++Revision;
    if(IsCorrect()){Completed=true;ShakeRemaining=0.f;return true;}
    Errors=FMath::Min(Errors+1,1000000);ShakeRemaining=.45f;return false;
}
void FClaraLayerTower::Preview(float Seconds)
{
    if(Completed||!FMath::IsFinite(Seconds)||Seconds<=0)return;
    PreviewPending=false;PreviewRemaining=FMath::Clamp(Seconds,.1f,10.f);Selected=-1;++Revision;
}
void FClaraLayerTower::Tick(float Delta,bool Visible)
{
    if(!Visible||!FMath::IsFinite(Delta)||Delta<0)return;
    if(PreviewPending){PreviewPending=false;PreviewRemaining=10.f;++Revision;return;}
    const int32 OldSecond=FMath::CeilToInt(PreviewRemaining);const bool WasShaking=ShakeRemaining>0.f;
    PreviewRemaining=FMath::Max(0.f,PreviewRemaining-Delta);ShakeRemaining=FMath::Max(0.f,ShakeRemaining-Delta);
    if(FMath::CeilToInt(PreviewRemaining)!=OldSecond||(WasShaking&&ShakeRemaining==0.f))++Revision;
}
void FClaraLayerTower::SolveForTest()
{
    for(auto& Peg:Pegs)Peg.Empty();
    for(int32 I=0;I<(Kind==TEXT("tcp")?4:7);++I)Pegs[2].Add(I);
    Selected=-1;PreviewPending=false;PreviewRemaining=0.f;ShakeRemaining=0.f;Completed=false;++Revision;
}
TSharedRef<FJsonObject> FClaraLayerTower::ExportState() const
{
    auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("version"),1);State->SetStringField(TEXT("kind"),Kind);
    TArray<TSharedPtr<FJsonValue>> PegValues;
    for(const auto& Peg:Pegs)
    {TArray<TSharedPtr<FJsonValue>> Blocks;for(int32 Id:Peg)Blocks.Add(MakeShared<FJsonValueNumber>(Id));PegValues.Add(MakeShared<FJsonValueArray>(Blocks));}
    State->SetArrayField(TEXT("pegs"),PegValues);State->SetNumberField(TEXT("selected"),Selected);State->SetNumberField(TEXT("errors"),Errors);State->SetNumberField(TEXT("revision"),Revision);
    State->SetBoolField(TEXT("previewPending"),PreviewPending);State->SetBoolField(TEXT("completed"),Completed);
    State->SetNumberField(TEXT("previewRemaining"),PreviewRemaining);State->SetNumberField(TEXT("shakeRemaining"),ShakeRemaining);return State;
}
bool FClaraLayerTower::ImportState(const TSharedPtr<FJsonObject>& State)
{
    if(!State.IsValid())return false;
    auto Integer=[&](const TCHAR* Field,int32 Minimum,int32 Maximum,int32& Value)
    {double Number=0;if(!State->TryGetNumberField(Field,Number)||!FMath::IsFinite(Number)||Number<Minimum||Number>Maximum||Number!=FMath::FloorToDouble(Number))return false;Value=static_cast<int32>(Number);return true;};
    FClaraLayerTower Restored;int32 Version=0;double PreviewTime=0,ShakeTime=0;
    if(!Integer(TEXT("version"),1,1,Version)||!State->TryGetStringField(TEXT("kind"),Restored.Kind)||(Restored.Kind!=TEXT("osi")&&Restored.Kind!=TEXT("tcp"))||
       !Integer(TEXT("selected"),-1,2,Restored.Selected)||!Integer(TEXT("errors"),0,1000000,Restored.Errors)||!Integer(TEXT("revision"),0,MAX_int32-1,Restored.Revision)||
       !State->TryGetBoolField(TEXT("previewPending"),Restored.PreviewPending)||!State->TryGetBoolField(TEXT("completed"),Restored.Completed)||
       !State->TryGetNumberField(TEXT("previewRemaining"),PreviewTime)||!State->TryGetNumberField(TEXT("shakeRemaining"),ShakeTime)||
       !FMath::IsFinite(PreviewTime)||PreviewTime<0||PreviewTime>10||!FMath::IsFinite(ShakeTime)||ShakeTime<0||ShakeTime>1)return false;
    Restored.PreviewRemaining=PreviewTime;Restored.ShakeRemaining=ShakeTime;
    const TArray<TSharedPtr<FJsonValue>>* PegValues=nullptr;if(!State->TryGetArrayField(TEXT("pegs"),PegValues)||PegValues->Num()!=3)return false;
    const int32 Count=Restored.Kind==TEXT("tcp")?4:7;TSet<int32> Seen;
    for(int32 I=0;I<3;++I)
    {
        const TArray<TSharedPtr<FJsonValue>>* Blocks=nullptr;
        if(!(*PegValues)[I].IsValid()||!(*PegValues)[I]->TryGetArray(Blocks)||Blocks->Num()>Count)return false;
        Restored.Pegs[I].Empty();
        for(const auto& Block:*Blocks)
        {
            double Number=0;if(!Block.IsValid()||!Block->TryGetNumber(Number)||!FMath::IsFinite(Number)||Number<0||Number>=Count||Number!=FMath::FloorToDouble(Number))return false;
            const int32 Id=static_cast<int32>(Number);if(Seen.Contains(Id))return false;Seen.Add(Id);Restored.Pegs[I].Add(Id);
        }
    }
    if(Seen.Num()!=Count||(Restored.Selected>=0&&(Restored.Pegs[Restored.Selected].IsEmpty()||Restored.IsPreview()||Restored.Completed))||
       (Restored.Completed&&(!Restored.IsCorrect()||Restored.IsPreview()))||(Restored.PreviewPending&&Restored.PreviewRemaining!=10.f))return false;
    *this=MoveTemp(Restored);return true;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraTowerMovesTest,"Clara.LayerTower.TopBlocksAndSolution",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraTowerMovesTest::RunTest(const FString& Parameters)
{
    for(const TCHAR* Kind:{TEXT("osi"),TEXT("tcpip")})
    {
        FClaraLayerTower Tower;Tower.Reset(Kind);Tower.Tick(0,true);Tower.Tick(10,true);
        const auto Original=Tower.Pegs[0];const int32 Top=Original.Last();
        TestFalse(TEXT("An empty source cannot be selected"),Tower.SelectPeg(1));TestTrue(TEXT("Select source"),Tower.SelectPeg(0));TestTrue(TEXT("Move top only"),Tower.SelectPeg(1));
        TestEqual(TEXT("Moved block is the former top"),Tower.Pegs[1].Last(),Top);TestEqual(TEXT("Bottom block remains"),Tower.Pegs[0][0],Original[0]);
        Tower.SelectPeg(1);Tower.SelectPeg(0);TestTrue(TEXT("A move can be undone without penalties"),Tower.Pegs[0]==Original&&Tower.Errors==0);
        for(int32 Wanted=0;Wanted<Original.Num();++Wanted)
        {
            int32 Source=Tower.Pegs[0].Contains(Wanted)?0:1;
            while(Tower.Pegs[Source].Last()!=Wanted){Tower.SelectPeg(Source);Tower.SelectPeg(1-Source);}
            Tower.SelectPeg(Source);Tower.SelectPeg(2);
        }
        TestTrue(TEXT("All blocks reachable with top-only moves"),Tower.IsCorrect());TestTrue(TEXT("Explicit check completes"),Tower.Check());TestFalse(TEXT("Completed solution cannot be disturbed"),Tower.SelectPeg(2));
        Tower.Completed=false;Tower.Pegs[0]=MoveTemp(Tower.Pegs[2]);Tower.Pegs[2].Empty();TestFalse(TEXT("Correct ordering on left is not the required right-peg solution"),Tower.IsCorrect());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraTowerPreviewTest,"Clara.LayerTower.PreviewAndHints",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraTowerPreviewTest::RunTest(const FString& Parameters)
{
    FClaraLayerTower Tower;Tower.Tick(30,false);
    TestTrue(TEXT("Opening dialogue cannot consume preview"),Tower.PreviewPending&&Tower.PreviewRemaining==10.f);
    TestFalse(TEXT("Moves disabled during preview"),Tower.SelectPeg(0));TestFalse(TEXT("Checking preview cannot complete"),Tower.Check());TestEqual(TEXT("Preview check is not an error"),Tower.Errors,0);
    Tower.Tick(2,true);TestEqual(TEXT("First visible tick starts full ten seconds"),Tower.PreviewRemaining,10.f);
    Tower.Tick(4,true);Tower.Tick(20,false);TestEqual(TEXT("Hidden puzzle pauses countdown"),Tower.PreviewRemaining,6.f);
    Tower.Tick(6,true);TestFalse(TEXT("Preview expires"),Tower.IsPreview());
    for(int32 I=0;I<3;++I)Tower.Check();TestTrue(TEXT("Third incorrect check offers help"),Tower.OffersHint());TestTrue(TEXT("Wrong check animates shake"),Tower.ShakeRemaining>0.f);
    Tower.Preview();TestEqual(TEXT("Hint is five seconds"),Tower.PreviewRemaining,5.f);Tower.Tick(5,true);TestFalse(TEXT("Hint ends"),Tower.IsPreview());
    TestEqual(TEXT("Hint does not erase learning history"),Tower.Errors,3);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaraTowerSaveTest,"Clara.LayerTower.TransactionalCheckpoint",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FClaraTowerSaveTest::RunTest(const FString& Parameters)
{
    FClaraLayerTower Original;Original.Tick(0,true);Original.Tick(10,true);Original.SelectPeg(0);Original.SelectPeg(1);Original.Check();
    FClaraLayerTower Loaded;TestTrue(TEXT("Valid checkpoint restores"),Loaded.ImportState(Original.ExportState()));TestTrue(TEXT("Pegs restored exactly"),Loaded.Pegs[0]==Original.Pegs[0]&&Loaded.Pegs[1]==Original.Pegs[1]);
    auto Bad=Original.ExportState();Bad->SetNumberField(TEXT("selected"),1.5);TestFalse(TEXT("Fractional peg index rejected"),Loaded.ImportState(Bad));
    Bad=Original.ExportState();Bad->SetStringField(TEXT("kind"),TEXT("other"));TestFalse(TEXT("Unknown model rejected"),Loaded.ImportState(Bad));
    Bad=Original.ExportState();Bad->SetBoolField(TEXT("completed"),true);TestFalse(TEXT("Unsolved cannot claim completion"),Loaded.ImportState(Bad));
    Bad=Original.ExportState();auto Pegs=Bad->GetArrayField(TEXT("pegs"));auto Blocks=Pegs[0]->AsArray();Blocks[0]=MakeShared<FJsonValueNumber>(Blocks[1]->AsNumber());Pegs[0]=MakeShared<FJsonValueArray>(Blocks);Bad->SetArrayField(TEXT("pegs"),Pegs);
    TestFalse(TEXT("Duplicate layer rejected"),Loaded.ImportState(Bad));TestTrue(TEXT("Invalid checkpoints do not mutate valid state"),Loaded.Pegs[0]==Original.Pegs[0]&&Loaded.Errors==Original.Errors);
    Original.SolveForTest();Original.Check();TestTrue(TEXT("Completed solution survives checkpoint"),Loaded.ImportState(Original.ExportState())&&Loaded.Completed&&Loaded.IsCorrect());return true;
}
#endif
