// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintQueryAnalysisStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBlueprintQueryAnalysisStyle::StyleInstance = nullptr;

void FBlueprintQueryAnalysisStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBlueprintQueryAnalysisStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBlueprintQueryAnalysisStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BlueprintQueryAnalysisStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FBlueprintQueryAnalysisStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("BlueprintQueryAnalysisStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BlueprintQueryAnalysis")->GetBaseDir() / TEXT("Resources"));

	// The supplied unreal-engine-transparent-128x128.svg only wraps a base64 PNG in
	// an <image> element, which Slate's vector (nanosvg) renderer cannot draw. The
	// sibling PNG holds the same artwork, so register it through IMAGE_BRUSH so the
	// Unreal icon actually shows on the toolbar button.
	Style->Set("BlueprintQueryAnalysis.OpenPluginWindow", new IMAGE_BRUSH(TEXT("unreal-engine-transparent-128x128"), Icon20x20));

	return Style;
}

void FBlueprintQueryAnalysisStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBlueprintQueryAnalysisStyle::Get()
{
	return *StyleInstance;
}
