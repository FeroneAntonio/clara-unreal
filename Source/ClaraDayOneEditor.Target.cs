using UnrealBuildTool;

public class ClaraDayOneEditorTarget : TargetRules
{
    public ClaraDayOneEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("ClaraDayOne");
    }
}
