#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Aserwidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(BlueprintType, Blueprintable)
class UAserwidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAserwidget(const FObjectInitializer& ObjectInitializer);

	// Buserwidget fills item data through this API so outside code does not touch inner controls.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Aserwidget")
	void SetAserData(int32 InIndex, const FText& InTitle, const FText& InDescription);

	// Battle order data includes speed and an icon, used by Buserwidget sorting simulation.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Aserwidget")
	void SetAserBattleData(int32 InIndex, const FText& InTitle, const FText& InDescription, int32 InSpeed, UTexture2D* InEntryTexture);

	// Allow Buserwidget or Blueprint to update only speed before the next sort tick.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Aserwidget")
	void SetAserSpeed(int32 InSpeed);

	// Allow Blueprint to pass Text_Texture when the asset path is different from the default.
	UFUNCTION(BlueprintCallable, Category = "wenqiu|Aserwidget")
	void SetEntryTexture(UTexture2D* InEntryTexture);

	// Expose creation order for FIFO debugging and Blueprint display.
	UFUNCTION(BlueprintPure, Category = "wenqiu|Aserwidget")
	int32 GetAserIndex() const { return AserIndex; }

	// Buserwidget reads this value when sorting turn order.
	UFUNCTION(BlueprintPure, Category = "wenqiu|Aserwidget")
	int32 GetAserSpeed() const { return Speed; }

protected:
	virtual void NativeConstruct() override;

private:
	// Build a Star-Rail-like tactical card fully in C++ so the class works without UMG assets.
	void BuildStarRailLayout();

	// Refresh text after data changes while tolerating calls before the widget tree exists.
	void RefreshText();

private:
	UPROPERTY()
	TObjectPtr<UBorder> CardBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> IndexText;

	UPROPERTY()
	TObjectPtr<UImage> EntryImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(EditAnywhere, Category = "wenqiu|Aserwidget")
	TObjectPtr<UTexture2D> EntryTexture;

	UPROPERTY()
	int32 AserIndex = INDEX_NONE;

	UPROPERTY()
	int32 Speed = 10;

	UPROPERTY()
	FText Title = FText::FromString(TEXT("Trailblaze Entry"));

	UPROPERTY()
	FText Description = FText::FromString(TEXT("Queued tactical UI card"));
};
