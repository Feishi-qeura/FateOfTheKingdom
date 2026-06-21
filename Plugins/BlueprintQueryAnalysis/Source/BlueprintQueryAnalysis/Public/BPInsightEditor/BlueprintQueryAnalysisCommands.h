// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "BlueprintQueryAnalysisStyle.h"

class FBlueprintQueryAnalysisCommands : public TCommands<FBlueprintQueryAnalysisCommands>
{
public:

	FBlueprintQueryAnalysisCommands()
		: TCommands<FBlueprintQueryAnalysisCommands>(TEXT("BlueprintQueryAnalysis"), NSLOCTEXT("Contexts", "BlueprintQueryAnalysis", "BlueprintQueryAnalysis Plugin"), NAME_None, FBlueprintQueryAnalysisStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};