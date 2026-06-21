#include "Aserwidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "UObject/ConstructorHelpers.h"

UAserwidget::UAserwidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Try the requested default image first; if the asset does not exist, the UI still shows text.
	static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultEntryTexture(TEXT("/Game/Text_Texture.Text_Texture"));
	if (DefaultEntryTexture.Succeeded())
	{
		EntryTexture = DefaultEntryTexture.Object;
	}
}

void UAserwidget::SetAserData(int32 InIndex, const FText& InTitle, const FText& InDescription)
{
	AserIndex = InIndex;
	Title = InTitle;
	Description = InDescription;
	RefreshText();
}

void UAserwidget::SetAserBattleData(int32 InIndex, const FText& InTitle, const FText& InDescription, int32 InSpeed, UTexture2D* InEntryTexture)
{
	AserIndex = InIndex;
	Title = InTitle;
	Description = InDescription;
	Speed = InSpeed;
	if (InEntryTexture)
	{
		EntryTexture = InEntryTexture;
	}
	RefreshText();
}

void UAserwidget::SetAserSpeed(int32 InSpeed)
{
	// Speed is clamped to the requested simulation range so sorting always has valid data.
	Speed = FMath::Clamp(InSpeed, 10, 80);
	RefreshText();
}

void UAserwidget::SetEntryTexture(UTexture2D* InEntryTexture)
{
	// Texture is optional; a missing asset should not make the whole row disappear.
	EntryTexture = InEntryTexture;
	RefreshText();
}

void UAserwidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Rebuild on construct to avoid stale controls after repeated widget lifecycle calls.
	BuildStarRailLayout();
	RefreshText();
}

void UAserwidget::BuildStarRailLayout()
{
	// WidgetTree is the C++ entry point for dynamic UMG controls; guard unusual preview states.
	if (!WidgetTree)
	{
		return;
	}

	USizeBox* RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AserRootSizeBox"));
	RootSizeBox->SetHeightOverride(118.0f);
	RootSizeBox->SetMinDesiredWidth(420.0f);
	WidgetTree->RootWidget = RootSizeBox;

	CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StarRailCardBorder"));
	CardBorder->SetPadding(FMargin(16.0f, 12.0f));
	CardBorder->SetBrushColor(FLinearColor(0.035f, 0.041f, 0.055f, 0.92f));
	RootSizeBox->SetContent(CardBorder);

	UHorizontalBox* MainRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StarRailMainRow"));
	CardBorder->SetContent(MainRow);

	UBorder* IndexPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("IndexPlate"));
	IndexPlate->SetPadding(FMargin(10.0f, 6.0f));
	IndexPlate->SetBrushColor(FLinearColor(0.88f, 0.72f, 0.36f, 1.0f));

	UHorizontalBoxSlot* IndexPlateSlot = MainRow->AddChildToHorizontalBox(IndexPlate);
	IndexPlateSlot->SetHorizontalAlignment(HAlign_Left);
	IndexPlateSlot->SetVerticalAlignment(VAlign_Center);
	IndexPlateSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));

	USizeBox* IndexSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IndexSizeBox"));
	IndexSizeBox->SetWidthOverride(58.0f);
	IndexSizeBox->SetHeightOverride(58.0f);
	IndexPlate->SetContent(IndexSizeBox);

	IndexText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("IndexText"));
	IndexText->SetJustification(ETextJustify::Center);
	IndexText->SetColorAndOpacity(FSlateColor(FLinearColor(0.03f, 0.032f, 0.04f, 1.0f)));
	IndexText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
	IndexSizeBox->SetContent(IndexText);

	USizeBox* ImageSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EntryImageSizeBox"));
	ImageSizeBox->SetWidthOverride(64.0f);
	ImageSizeBox->SetHeightOverride(64.0f);
	UHorizontalBoxSlot* ImageSlot = MainRow->AddChildToHorizontalBox(ImageSizeBox);
	ImageSlot->SetHorizontalAlignment(HAlign_Left);
	ImageSlot->SetVerticalAlignment(VAlign_Center);
	ImageSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));

	EntryImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EntryImage"));
	EntryImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	ImageSizeBox->SetContent(EntryImage);

	UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TextColumn"));
	UHorizontalBoxSlot* TextColumnSlot = MainRow->AddChildToHorizontalBox(TextColumn);
	TextColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	TextColumnSlot->SetVerticalAlignment(VAlign_Center);
	TextColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.92f, 0.82f, 1.0f)));
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	UVerticalBoxSlot* TitleSlot = TextColumn->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DescriptionText"));
	DescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.63f, 0.75f, 0.86f, 1.0f)));
	DescriptionText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16));
	TextColumn->AddChildToVerticalBox(DescriptionText);

	SpeedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeedText"));
	SpeedText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.72f, 0.36f, 1.0f)));
	SpeedText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
	UHorizontalBoxSlot* SpeedSlot = MainRow->AddChildToHorizontalBox(SpeedText);
	SpeedSlot->SetHorizontalAlignment(HAlign_Right);
	SpeedSlot->SetVerticalAlignment(VAlign_Center);
	SpeedSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));

	UBorder* AccentLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RightAccentLine"));
	AccentLine->SetBrushColor(FLinearColor(0.18f, 0.62f, 0.95f, 0.9f));
	UHorizontalBoxSlot* AccentSlot = MainRow->AddChildToHorizontalBox(AccentLine);
	AccentSlot->SetHorizontalAlignment(HAlign_Right);
	AccentSlot->SetVerticalAlignment(VAlign_Fill);
	AccentSlot->SetPadding(FMargin(14.0f, 8.0f, 0.0f, 8.0f));

	USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentSize"));
	AccentSize->SetWidthOverride(5.0f);
	AccentLine->SetContent(AccentSize);
}

void UAserwidget::RefreshText()
{
	// Controls are null before layout construction, so data is cached until this can safely run.
	if (IndexText)
	{
		IndexText->SetText(AserIndex == INDEX_NONE ? FText::FromString(TEXT("--")) : FText::AsNumber(AserIndex));
	}

	if (TitleText)
	{
		TitleText->SetText(Title);
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
	}

	if (SpeedText)
	{
		SpeedText->SetText(FText::Format(FText::FromString(TEXT("SPD {0}")), FText::AsNumber(Speed)));
	}

	if (EntryImage && EntryTexture)
	{
		EntryImage->SetBrushFromTexture(EntryTexture, true);
	}
}
