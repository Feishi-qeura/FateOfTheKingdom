using UnrealBuildTool;

public class GridEditor : ModuleRules
{
    public GridEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "RHI",
                "RenderCore",
                "UnrealEd",
                "EditorFramework",
            }
        );
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "GridRuntime"
            }
        );
    }
}