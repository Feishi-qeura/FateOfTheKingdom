#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Buserwidget.generated.h"

class UAserwidget;
class UBorder;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

UCLASS(BlueprintType, Blueprintable)
class UBuserwidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBuserwidget(const FObjectInitializer& ObjectInitializer);

	// Create Count Aserwidget items and let Buserwidget own their FIFO display queue.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void BuildAserWidgets(int32 Count);

	// Convenience call for the owner's current test case: create 8 items.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void BuildDefaultEightAserWidgets();

	// Enqueue one item for event-driven UI updates from Blueprint or gameplay code.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	UAserwidget* EnqueueAserWidget();

	// Dequeue the oldest item first to match the requested first-in-first-out behavior.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	UAserwidget* DequeueAserWidget();

	// Remove every item when the UI is refreshed or closed.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void ClearAserWidgets();

	// Sort all stored A widgets by speed, highest speed first like a turn-order preview.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void SortAserWidgetsBySpeed();

	// Start periodic sorting so the UI can validate turn-order behavior automatically.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void StartTurnOrderSortTimer(float InInterval = 3.0f);

	// Stop periodic sorting when closing or when Blueprint wants manual control.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void StopTurnOrderSortTimer();

	// Pass Text_Texture from Blueprint when the real asset path is not /Game/Text_Texture.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Buserwidget")
	void SetEntryTexture(UTexture2D* InEntryTexture);

	// Expose the number of A items currently stored by this B container.
	UFUNCTION(BlueprintPure, Category = "wenqiu|Buserwidget")
	int32 GetAserCount() const { return AserQueue.Num(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// Build B's root frame and scrollable list fully in C++.
	void BuildContainerLayout();

	// Keep batch creation and single enqueue using the same item initialization path.
	void InitializeAserWidget(UAserwidget* NewAserWidget);

	// Update the header after queue changes so runtime state is visible.
	void RefreshHeaderText();

	// Re-attach widgets in queue order after enqueue or sort.
	void RebuildAserList();

	// Debug helper: if BP only creates this widget and calls Build, make sure it is visible.
	void EnsureWidgetVisibleForDebug();

	// Produce the requested 10-80 random speed range in one place.
	int32 GenerateRandomSpeed() const;

private:
	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UScrollBox> AserScrollBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> AserListBox;

	UPROPERTY()
	TArray<TObjectPtr<UAserwidget>> AserQueue;

	UPROPERTY(EditAnywhere, Category = "wenqiu|Buserwidget")
	TObjectPtr<UTexture2D> EntryTexture;

	UPROPERTY(EditAnywhere, Category = "wenqiu|Buserwidget")
	float SortInterval = 3.0f;

	UPROPERTY(EditAnywhere, Category = "wenqiu|Buserwidget")
	int32 MinRandomSpeed = 10;

	UPROPERTY(EditAnywhere, Category = "wenqiu|Buserwidget")
	int32 MaxRandomSpeed = 80;

	UPROPERTY()
	int32 NextAserIndex = 1;

	FTimerHandle SortTimerHandle;
};
