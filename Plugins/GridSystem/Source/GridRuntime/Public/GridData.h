#pragma once
#include "GameplayTagContainer.h"
#include "GridData.generated.h"

UENUM(BlueprintType)
enum class GridEvent : uint8
{
	Fight     UMETA(DisplayName = "战斗"),
	Event    UMETA(DisplayName = "事件"),
};
USTRUCT(BlueprintType)
struct FGridData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 MoveCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	bool bPassable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	GridEvent EventID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FGameplayTagContainer AllowedUnitTypes; // 哪些单位能通行

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FGameplayTagContainer Tags; // 其他扩展属性
};
