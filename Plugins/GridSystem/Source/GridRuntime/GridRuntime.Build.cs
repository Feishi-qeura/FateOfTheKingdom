using UnrealBuildTool;
using System.IO;
public class GridRuntime : ModuleRules
{
    public GridRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AIModule",
                "CoreUObject",
                "Engine",
                "GameplayTags",
                "NavigationSystem",
                "RHI",
                "RenderCore"
            }
        );
        
        PrivateIncludePaths.AddRange( new string[]
        {
            Path.Combine(ModuleDirectory, "Public"),
            Path.Combine(ModuleDirectory, "Public/Components"),
            Path.Combine(ModuleDirectory, "Public/GridPainter"),
            Path.Combine(ModuleDirectory, "Public/Hexagon"),
            Path.Combine(ModuleDirectory, "Public/Square"),
            Path.Combine(ModuleDirectory, "Public/Util"),
            Path.Combine(ModuleDirectory, "Private"),
        });
    }
}