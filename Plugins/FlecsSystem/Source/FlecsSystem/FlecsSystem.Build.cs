// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class FlecsSystem : ModuleRules
{
	public FlecsSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(new string[] 
			{
				Path.Combine(ModuleDirectory, "Public/Systems"),
				Path.Combine(ModuleDirectory, "Public/Components"),
			}
			);
				
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"FlecsLibrary"
			}
			);
	}
}
