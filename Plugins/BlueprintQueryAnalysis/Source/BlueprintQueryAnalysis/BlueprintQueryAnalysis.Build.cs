// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BlueprintQueryAnalysis : ModuleRules
{
	public BlueprintQueryAnalysis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "Public", "BPInsightEditor")
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "Private", "BPInsightEditor"),
				Path.Combine(ModuleDirectory, "Private", "StaticDependencyAnalyzer"),
				Path.Combine(ModuleDirectory, "Private", "RuntimeFlowTracer"),
				Path.Combine(ModuleDirectory, "Private", "BlueprintNodeAnalyzer"),
				Path.Combine(ModuleDirectory, "Private", "GraphDataModel"),
				Path.Combine(ModuleDirectory, "Private", "GraphLayout"),
				Path.Combine(ModuleDirectory, "Private", "SlateGraphViewer"),
				Path.Combine(ModuleDirectory, "Private", "Exporter"),
				Path.Combine(ModuleDirectory, "Private", "MCPBridge"),
				Path.Combine(ModuleDirectory, "Private", "Dependencylibraries")
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"ApplicationCore",
				"AssetRegistry",
				"BlueprintGraph",
				"Kismet",
				"Json",
				"Slate",
				"SlateCore",
				"WebBrowser",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
