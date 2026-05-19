#include "GridEditor.h"
#include "GridEditorCommands.h"
#include "GridEditorPCH.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "GridEditorMode.h"
#include "SquareSettingsDetails.h"
#include "Components/GridSensingComponent.h"
#include "ComponentVisualizers/GridSensingComponentVisualizer.h"
#include "EditorModeManager.h"

DEFINE_LOG_CATEGORY(GridEditor)

const FEditorModeID EM_GridEditor(TEXT("EM_GridEditor"));

#define LOCTEXT_NAMESPACE "GridEditorModule" 

IMPLEMENT_MODULE(FGridEditorModule, GridEditor)

void FGridEditorModule::StartupModule()
{
	RegisterComponentVisualizer();

	FGridEditorCommands::Register();

	FEditorModeRegistry::Get().RegisterMode<FEdModeGridEditor>(EM_GridEditor, NSLOCTEXT("EditorModes", "GridEditorMode", "GridEditor"), FSlateIcon(), true, 1000);

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	RegisterCustomClassLayout("SquareGridSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FSquareSettingsDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FGridEditorModule::ShutdownModule()
{
	UnregisterComponentVisualizer();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Unregister all classes customized by name
		for (auto It = RegisteredClassNames.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomClassLayout(*It);
			}
		}

		// Unregister all structures
		for (auto It = RegisteredPropertyTypes.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomPropertyTypeLayout(*It);
			}
		}

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	FEditorModeRegistry::Get().UnregisterMode(EM_GridEditor);
}

void FGridEditorModule::RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check(ClassName != NAME_None);

	RegisteredClassNames.Add(ClassName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomClassLayout(ClassName, DetailLayoutDelegate);
}

void FGridEditorModule::RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate)
{
	check(PropertyTypeName != NAME_None);

	RegisteredPropertyTypes.Add(PropertyTypeName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomPropertyTypeLayout(PropertyTypeName, PropertyTypeLayoutDelegate);
}

void FGridEditorModule::RegisterComponentVisualizer()
{
	RegisterComponentVisualizer(UGridSensingComponent::StaticClass()->GetFName(), MakeShareable(new FGridSensingComponentVisualizer));
}

void FGridEditorModule::RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
{
	if (GUnrealEd != nullptr)
	{
		GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
	}

	RegisteredComponentClassNames.Add(ComponentClassName);

	if (Visualizer.IsValid())
	{
		Visualizer->OnRegister();
	}
}

void FGridEditorModule::UnregisterComponentVisualizer()
{
	if (GUnrealEd != NULL)
	{
		for (FName ClassName : RegisteredComponentClassNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(ClassName);
		}
	}
}

#undef LOCTEXT_NAMESPACE
