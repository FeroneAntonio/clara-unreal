using UnrealBuildTool;

public class ClaraDayOne : ModuleRules
{
    public ClaraDayOne(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "UMG", "HTTP", "Json"});
    }
}
