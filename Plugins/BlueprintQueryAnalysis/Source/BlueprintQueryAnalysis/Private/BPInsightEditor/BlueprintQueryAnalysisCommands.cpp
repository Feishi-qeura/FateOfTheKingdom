// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintQueryAnalysisCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintQueryAnalysisModule"

void FBlueprintQueryAnalysisCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Blueprint Insight", "Open blueprint dependency and runtime flow analysis window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
