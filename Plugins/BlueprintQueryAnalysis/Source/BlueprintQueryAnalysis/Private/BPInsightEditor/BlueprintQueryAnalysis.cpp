// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintQueryAnalysis.h"
#include "BlueprintQueryAnalysisStyle.h"
#include "BlueprintQueryAnalysisCommands.h"
#include "BPQARuntimeFlowTracer.h"
#include "LevelEditor.h"
#include "SBlueprintQueryAnalysisWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"

static const FName BlueprintQueryAnalysisTabName("BlueprintQueryAnalysis");

#define LOCTEXT_NAMESPACE "FBlueprintQueryAnalysisModule"

void FBlueprintQueryAnalysisModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBlueprintQueryAnalysisStyle::Initialize();
	FBlueprintQueryAnalysisStyle::ReloadTextures();

	FBlueprintQueryAnalysisCommands::Register();

	RuntimeTracer = MakeShared<FBPQARuntimeFlowTracer>();
	RuntimeTracer->RegisterDelegates();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintQueryAnalysisCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FBlueprintQueryAnalysisModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintQueryAnalysisModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BlueprintQueryAnalysisTabName, FOnSpawnTab::CreateRaw(this, &FBlueprintQueryAnalysisModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBlueprintQueryAnalysisTabTitle", "Blueprint Insight"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBlueprintQueryAnalysisModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBlueprintQueryAnalysisStyle::Shutdown();

	FBlueprintQueryAnalysisCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlueprintQueryAnalysisTabName);

	if (RuntimeTracer.IsValid())
	{
		RuntimeTracer->UnregisterDelegates();
		RuntimeTracer.Reset();
	}
}

TSharedRef<SDockTab> FBlueprintQueryAnalysisModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBlueprintQueryAnalysisWidget)
			.RuntimeTracer(RuntimeTracer)
		];
}

void FBlueprintQueryAnalysisModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BlueprintQueryAnalysisTabName);
}

void FBlueprintQueryAnalysisModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FBlueprintQueryAnalysisCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBlueprintQueryAnalysisCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintQueryAnalysisModule, BlueprintQueryAnalysis)
