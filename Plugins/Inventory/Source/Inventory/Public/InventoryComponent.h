// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "InventoryComponent.generated.h"

//UI 刷新广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

// 使用广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConsumableUsed, FName, ItemID , int32 , Quantity);

//丢弃广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemDropped, FName, ItemID, int32 , Quantity);

UCLASS(ClassGroup=(Inventory), Blueprintable, meta = (BlueprintSpawnableComponent))
class INVENTORY_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory" , meta = (DisplayName = "物品数据表"))
	UDataTable* ItemDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory"  , meta = (DisplayName = "背包容量"))
	int32 InventorySize = 36;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory" , meta = (DisplayName = "背包"))
	TArray<FInventorySlot> Inventory;


	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events" , meta = (DisplayName = "背包刷新事件"))
	FOnInventoryUpdated OnInventoryUpdated;
	//可监听的消耗品使用事件
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events" , meta = (DisplayName = "消耗品使用事件"))
	FOnConsumableUsed OnConsumableUsed;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events" , meta = (DisplayName = "物品丢弃事件"))
	FOnItemDropped OnItemDropped;

	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "添加物品到背包"))
	int32 AddItem(FName ItemID, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "移除背包物品"))
	FInventorySlot RemoveItem(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "使用背包物品"))
	void UseItem(int32 SlotIndex , int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "获取物品数据"))
	bool GetItemData(FName ItemID, FItemData& OutItemData) const;
	
	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "交换背包物品位置"))
	void SwapItem(int32 IndexA, int32 IndexB);
	
	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "丢弃背包物品"))
	int32 DropItem(int32 SlotIndex , int32 Quantity = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "合并背包格子"))
	bool MergeSlot(int32 IndexA, int32 IndexB);

	//无法减少背包容量，增加背包容量时会在末尾添加空槽
	//输入负数或0将不会增加背包容量，返回false
	UFUNCTION(BlueprintCallable, Category = "Inventory" , meta = (DisplayName = "增加背包容量"))
	bool IncreaseInventorySize(int32 IncreaseSize = 1);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Inventory" , meta = (DisplayName = "打开背包"))
	void OpenInventory();
	
public:	
	
	void UpdateInventory();
	UFUNCTION(BlueprintImplementableEvent , BlueprintCallable , Category = "Inventory" , meta = (DisplayName = "刷新背包"))
	void OnInventoryUpdated_Implementation();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory" , meta = (DisplayName = "物品丢弃"))
	void OnItemDropped_Implementation(FName ItemID , int32 Quantity);

	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory" , meta = (DisplayName = "消耗品使用"))
	void OnConsumableUsed_Implementation(FName ItemID , int32 Quantity);


protected:
	void HandleConsumable(int32 SlotIndex, int32 Quantity, FName ItemID);
	//Ciallo～(∠・ω< )⌒★
};
