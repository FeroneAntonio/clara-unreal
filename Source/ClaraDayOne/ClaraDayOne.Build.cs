using UnrealBuildTool;

public class ClaraDayOne : ModuleRules
{
    public ClaraDayOne(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseUnity = false;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "UMG", "HTTP", "Json"});
    }
}
