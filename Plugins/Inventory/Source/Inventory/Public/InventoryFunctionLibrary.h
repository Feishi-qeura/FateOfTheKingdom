#pragma once

#include "CoreMinimal.h"
#include "InventoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryFunctionLibrary.generated.h"

UCLASS()
class INVENTORY_API UInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Item Data" , meta = (DisplayName = "物品数据相等" , CompactNodeTitle = "=="))
	static bool ItemDataEquals(FItemData const& A, FItemData const& B)
	{
		return A == B;
	}

    UFUNCTION(BlueprintPure, Category = "Item Data" , meta = (DisplayName = "物品数据不相等" , CompactNodeTitle = "!="))
    static bool ItemDataNotEquals(FItemData const& A, FItemData const& B)
	{
		return !(A == B);
    }

	UFUNCTION(BlueprintPure, Category = "Item Data" , meta = (DisplayName = "Slot为空" , CompactNodeTitle = "IsEmpty"))
	static bool IsEmpty(FInventorySlot const& Slot)
	{
		return Slot.IsEmpty();
	}
};