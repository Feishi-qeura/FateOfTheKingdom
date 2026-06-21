#include "Buserwidget.h"

#include "Aserwidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UBuserwidget::UBuserwidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Try the requested default image path; Blueprint can override it if the asset lives elsewhere.
	static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultEntryTexture(TEXT("/Game/Text_Texture.Text_Texture"));
	if (DefaultEntryTexture.Succeeded())
	{
		EntryTexture = DefaultEntryTexture.Object;
	}
}

void UBuserwidget::BuildAserWidgets(int32 Count)
{
	// Clamp negative input to zero so gameplay data cannot create an accidental loop.
	const int32 SafeCount = FMath::Max(0, Count);

	ClearAserWidgets();
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		EnqueueAserWidget();
	}
	RefreshHeaderText();
	StartTurnOrderSortTimer(SortInterval);

	UE_LOG(LogTemp, Log, TEXT("Buserwidget BuildAserWidgets Count=%d"), AserQueue.Num());
}

void UBuserwidget::BuildDefaultEightAserWidgets()
{
	// The owner's current validation case wants exactly eight entries.
	EnsureWidgetVisibleForDebug();
	BuildAserWidgets(8);
}

UAserwidget* UBuserwidget::EnqueueAserWidget()
{
	// If this is called before NativeConstruct, build the container before storing child widgets.
	if (!AserListBox)
	{
		BuildContainerLayout();
	}

	// Use World as the creation context so menu widgets without an owning player can still spawn A.
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UAserwidget* NewAserWidget = CreateWidget<UAserwidget>(World, UAserwidget::StaticClass());
	if (!NewAserWidget)
	{
		return nullptr;
	}

	InitializeAserWidget(NewAserWidget);
	AserQueue.Add(NewAserWidget);

	RebuildAserList();
	RefreshHeaderText();
	return NewAserWidget;
}

UAserwidget* UBuserwidget::DequeueAserWidget()
{
	// FIFO means the array head is always the first widget to leave the container.
	if (AserQueue.Num() <= 0)
	{
		return nullptr;
	}

	UAserwidget* OldestWidget = AserQueue[0];
	AserQueue.RemoveAt(0);

	if (OldestWidget)
	{
		OldestWidget->RemoveFromParent();
	}

	RefreshHeaderText();
	return OldestWidget;
}

void UBuserwidget::ClearAserWidgets()
{
	// Remove parent links before clearing the queue so old A widgets do not remain in UMG hierarchy.
	for (UAserwidget* AserWidget : AserQueue)
	{
		if (AserWidget)
		{
			AserWidget->RemoveFromParent();
		}
	}

	AserQueue.Reset();
	NextAserIndex = 1;

	if (AserListBox)
	{
		AserListBox->ClearChildren();
	}

	StopTurnOrderSortTimer();
	RefreshHeaderText();
}

void UBuserwidget::SortAserWidgetsBySpeed()
{
	// Highest speed goes first, matching a simple Star-Rail-like turn-order preview.
	AserQueue.Sort([](const TObjectPtr<UAserwidget>& Left, const TObjectPtr<UAserwidget>& Right)
	{
		const UAserwidget* LeftWidget = Left.Get();
		const UAserwidget* RightWidget = Right.Get();
		if (!LeftWidget || !RightWidget)
		{
			return LeftWidget != nullptr;
		}
		return LeftWidget->GetAserSpeed() > RightWidget->GetAserSpeed();
	});

	RebuildAserList();
	RefreshHeaderText();

	FString OrderLog;
	for (const UAserwidget* AserWidget : AserQueue)
	{
		if (AserWidget)
		{
			OrderLog += FString::Printf(TEXT("[%d:SPD%d] "), AserWidget->GetAserIndex(), AserWidget->GetAserSpeed());
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Buserwidget sorted turn order: %s"), *OrderLog);
}

void UBuserwidget::StartTurnOrderSortTimer(float InInterval)
{
	// Timer lives on the owning world; guard null world so editor preview never crashes.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SortInterval = FMath::Max(0.1f, InInterval);
	World->GetTimerManager().ClearTimer(SortTimerHandle);
	World->GetTimerManager().SetTimer(SortTimerHandle, this, &UBuserwidget::SortAserWidgetsBySpeed, SortInterval, true);
}

void UBuserwidget::StopTurnOrderSortTimer()
{
	// Clear the world timer when the widget is rebuilt or destroyed to avoid dangling callbacks.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(SortTimerHandle);
}

void UBuserwidget::SetEntryTexture(UTexture2D* InEntryTexture)
{
	// Apply the new texture to both future entries and already-created A widgets.
	EntryTexture = InEntryTexture;
	for (UAserwidget* AserWidget : AserQueue)
	{
		if (AserWidget)
		{
			AserWidget->SetEntryTexture(EntryTexture);
		}
	}
}

void UBuserwidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Prepare B's visual container first; A items are created later when Count is passed in.
	BuildContainerLayout();
	RefreshHeaderText();
}

void UBuserwidget::NativeDestruct()
{
	// Widget destruction must stop the sort timer before UObject references become stale.
	StopTurnOrderSortTimer();
	Super::NativeDestruct();
}

void UBuserwidget::BuildContainerLayout()
{
	// WidgetTree can be null during unusual editor preview timing, so guard before UMG API calls.
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuserRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QueueRootBorder"));
	RootBorder->SetPadding(FMargin(18.0f));
	RootBorder->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.026f, 0.96f));
	RootBorder->SetVisibility(ESlateVisibility::Visible);

	UCanvasPanelSlot* RootBorderSlot = RootCanvas->AddChildToCanvas(RootBorder);
	RootBorderSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	RootBorderSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* RootColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QueueRootColumn"));
	RootBorder->SetContent(RootColumn);

	UBorder* HeaderBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QueueHeaderBorder"));
	HeaderBorder->SetPadding(FMargin(14.0f, 10.0f));
	HeaderBorder->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.12f, 1.0f));
	UVerticalBoxSlot* HeaderSlot = RootColumn->AddChildToVerticalBox(HeaderBorder);
	HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	HeaderSlot->SetHorizontalAlignment(HAlign_Fill);

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QueueHeaderRow"));
	HeaderBorder->SetContent(HeaderRow);

	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QueueHeaderText"));
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.78f, 1.0f)));
	HeaderText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
	UHorizontalBoxSlot* HeaderTextSlot = HeaderRow->AddChildToHorizontalBox(HeaderText);
	HeaderTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UBorder* HeaderAccent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeaderAccent"));
	HeaderAccent->SetBrushColor(FLinearColor(0.86f, 0.68f, 0.31f, 1.0f));
	UHorizontalBoxSlot* HeaderAccentSlot = HeaderRow->AddChildToHorizontalBox(HeaderAccent);
	HeaderAccentSlot->SetPadding(FMargin(12.0f, 6.0f, 0.0f, 6.0f));
	HeaderAccentSlot->SetHorizontalAlignment(HAlign_Right);
	HeaderAccentSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* HeaderAccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HeaderAccentSize"));
	HeaderAccentSize->SetWidthOverride(72.0f);
	HeaderAccent->SetContent(HeaderAccentSize);

	AserScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AserScrollBox"));
	UVerticalBoxSlot* ScrollSlot = RootColumn->AddChildToVerticalBox(AserScrollBox);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	AserListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AserListBox"));
	if (UScrollBoxSlot* ListSlot = Cast<UScrollBoxSlot>(AserScrollBox->AddChild(AserListBox)))
	{
		ListSlot->SetPadding(FMargin(0.0f));
	}

	RebuildAserList();
}

void UBuserwidget::InitializeAserWidget(UAserwidget* NewAserWidget)
{
	// Number from 1 and use one Star-Rail-style label path for both batch and single enqueue.
	if (!NewAserWidget)
	{
		return;
	}

	const int32 CurrentIndex = NextAserIndex++;
	const int32 RandomSpeed = GenerateRandomSpeed();
	NewAserWidget->SetAserBattleData(
		CurrentIndex,
		FText::Format(FText::FromString(TEXT("Memory Segment {0}")), FText::AsNumber(CurrentIndex)),
		FText::FromString(TEXT("FIFO insert, timer sorted by speed")),
		RandomSpeed,
		EntryTexture);
}

void UBuserwidget::RefreshHeaderText()
{
	// HeaderText can be null before NativeConstruct, so only touch the control when it exists.
	if (!HeaderText)
	{
		return;
	}

	HeaderText->SetText(FText::Format(
		FText::FromString(TEXT("WENQIU FIFO CONTAINER  |  A COUNT: {0}")),
		FText::AsNumber(AserQueue.Num())));
}

void UBuserwidget::RebuildAserList()
{
	// Rebuild visual order from the data queue so enqueue and sort share one code path.
	if (!AserListBox)
	{
		return;
	}

	AserListBox->ClearChildren();
	for (UAserwidget* AserWidget : AserQueue)
	{
		if (AserWidget)
		{
			UVerticalBoxSlot* ItemSlot = AserListBox->AddChildToVerticalBox(AserWidget);
			ItemSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
			ItemSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UBuserwidget::EnsureWidgetVisibleForDebug()
{
	// BP_PlayerUI may create this widget and call Build without adding it to any visible parent.
	// In that case data/logs exist but nothing can be painted, so add this test widget to viewport.
	if (!GetParent() && !IsInViewport())
	{
		AddToViewport(50);
		UE_LOG(LogTemp, Warning, TEXT("Buserwidget was not attached to UI tree; added to viewport for debug display."));
	}
}

int32 UBuserwidget::GenerateRandomSpeed() const
{
	// Keep the requested random speed range robust even if Blueprint edits min/max backwards.
	const int32 LowerSpeed = FMath::Min(MinRandomSpeed, MaxRandomSpeed);
	const int32 UpperSpeed = FMath::Max(MinRandomSpeed, MaxRandomSpeed);
	return FMath::RandRange(LowerSpeed, UpperSpeed);
}
