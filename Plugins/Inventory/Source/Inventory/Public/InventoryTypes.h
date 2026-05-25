#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemUseElement.h"
#include "InventoryTypes.generated.h"

// 物品类
UENUM(BlueprintType)
enum class EItemType : uint8
{
	None        UMETA(DisplayName = "无"),
	Consumable  UMETA(DisplayName = "消耗品"),
	Equipment   UMETA(DisplayName = "装备"),
	Material    UMETA(DisplayName = "材料"),
	QuestItem   UMETA(DisplayName = "任务物品"),
	Currency    UMETA(DisplayName = "货币"),
	Other       UMETA(DisplayName = "其他")
};

UENUM(BlueprintType)
enum class EItemQuality : uint8
{
	None      UMETA(DisplayName = "无"),
	Common    UMETA(DisplayName = "普通"),
	Rare      UMETA(DisplayName = "稀有"),
	Epic      UMETA(DisplayName = "史诗"),
	Legendary UMETA(DisplayName = "传说"),
	Mythical  UMETA(DisplayName = "绝世")
};

// 物品基础数据 
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "物品ID"))
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "物品名称"))
	FText ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "物品描述"))
	FText ItemDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "物品品质"))
	EItemQuality ItemQuality = EItemQuality::None;//unreal中枚举类型务必拥有默认值

	UPROPERTY(EditAnywhere, BlueprintReadOnly , Category = "Item Data", meta = (AllowedClasses = "Texture2D,MaterialInterface" , displayname = "物品图标"))
	TObjectPtr<UObject> Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "最大堆叠数"))
	int32 MaxStack = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "物品类型"))
	TSet<EItemType> ItemTypes;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data" , meta = (DisplayName = "物品使用类"))
	TArray<TSubclassOf<UItemUseElement>> DropItemClass;

	bool operator==(const FItemData& Other) const
	{
		return (ItemID == Other.ItemID) && ItemName.EqualTo(Other.ItemName) && MaxStack == Other.MaxStack && ItemDescription.EqualTo(Other.ItemDescription) ;
	}


};

// 背包格子数据
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	int32 Quantity = 0;

	bool IsEmpty() const { return ItemID == NAME_None || Quantity <= 0; }
	void ClearSlot() { ItemID = NAME_None; Quantity = 0; }
};
