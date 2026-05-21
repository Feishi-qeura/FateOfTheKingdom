#include "InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
    Inventory.SetNum(InventorySize);
    Super::BeginPlay();
}

int32 UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
    if (Quantity <= 0 || ItemID == NAME_None) return Quantity;
    
    FItemData ItemData;
    if (!GetItemData(ItemID, ItemData)) return Quantity; 
    
    
    int32 RemainingAmount = Quantity;
    
    for (FInventorySlot& Slot : Inventory)
    {
        if (Slot.ItemID == ItemID && Slot.Quantity < ItemData.MaxStack)
        {
            int32 SpaceLeft = ItemData.MaxStack - Slot.Quantity; 
            if (RemainingAmount <= SpaceLeft)
            {
                Slot.Quantity += RemainingAmount;
                RemainingAmount = 0;
                break; 
            }
            else
            {
                Slot.Quantity = ItemData.MaxStack;
                RemainingAmount -= SpaceLeft;
            }
        }
    }
    

    if (RemainingAmount > 0)
    {
        for (FInventorySlot& Slot : Inventory)
        {
            if (Slot.IsEmpty())
            {
                Slot.ItemID = ItemID; 
                
                
                if (RemainingAmount <= ItemData.MaxStack)
                {
                    Slot.Quantity = RemainingAmount;
                    RemainingAmount = 0;
                    break; 
                }
                else
                {
                    Slot.Quantity = ItemData.MaxStack;
                    RemainingAmount -= ItemData.MaxStack;
                }
            }
        }
    }
    
    UpdateInventory();
    
    return RemainingAmount;
}

void UInventoryComponent::UseItem(int32 SlotIndex , int32 Quantity)
{
    if (!Inventory.IsValidIndex(SlotIndex)) return;
    
    FInventorySlot& Slot = Inventory[SlotIndex];
    if (Slot.IsEmpty()) return;
    
    FItemData ItemData;
    if (GetItemData(Slot.ItemID, ItemData))
    {
        if (ItemData.ItemTypes.Contains(EItemType::Consumable))
        {
            HandleConsumable(SlotIndex, Quantity , Slot.ItemID);
        }
    }
}

FInventorySlot UInventoryComponent::RemoveItem(int32 SlotIndex)
{
    if (!Inventory.IsValidIndex(SlotIndex)) return FInventorySlot();
    
    FInventorySlot& Slot = Inventory[SlotIndex];
    if (Slot.IsEmpty()) return Slot;
    
    FInventorySlot SlotCopy = Slot;
    Slot.ClearSlot();
    
    UpdateInventory();
    return SlotCopy;
}

bool UInventoryComponent::GetItemData(FName ItemID, FItemData& OutItemData) const
{
    if (ItemDataTable)
    {
        static const FString ContextString(TEXT("Inventory System Context"));
        FItemData* Row = ItemDataTable->FindRow<FItemData>(ItemID, ContextString);
        if (Row)
        {
            OutItemData = *Row;
            return true;
        }
    }
    return false;
}

void UInventoryComponent::HandleConsumable(int32 SlotIndex, int32 Quantity, FName ItemID)
{
    FInventorySlot& Slot = Inventory[SlotIndex];
    int32 TempQuantity = Slot.Quantity;
    Slot.Quantity -= Quantity;
    if (Slot.Quantity <= 0)
    {
        Slot.ClearSlot();
        OnConsumableUsed_Implementation(ItemID , TempQuantity);
        OnConsumableUsed.Broadcast(ItemID , TempQuantity);
        
    }
    else
    {
        OnConsumableUsed_Implementation(ItemID , Quantity);
        OnConsumableUsed.Broadcast(ItemID , Quantity);
    }
    UpdateInventory();
}

void UInventoryComponent::SwapItem(int32 IndexA, int32 IndexB)
{
    if (!Inventory.IsValidIndex(IndexA) || !Inventory.IsValidIndex(IndexB)) return;
    if (IndexA == IndexB) return;
    
    Inventory.Swap(IndexA, IndexB);
    
    UpdateInventory();
}

int32 UInventoryComponent::DropItem(int32 SlotIndex , int32 Quantity)
{
    if (!Inventory.IsValidIndex(SlotIndex)) return 0;
    FInventorySlot& Slot = Inventory[SlotIndex]; 
    if (Slot.IsEmpty()) return 0;
    if (Quantity <= 0) return Slot.Quantity;

    int32 TempQuantity = Slot.Quantity;
    Slot.Quantity = FMath::Max(Slot.Quantity - Quantity, 0 );

    TempQuantity -= Slot.Quantity;
    
    OnItemDropped_Implementation(Slot.ItemID, TempQuantity);
    OnItemDropped.Broadcast(Slot.ItemID, TempQuantity);

    if (Slot.Quantity <= 0)
    {
        Slot.ClearSlot();
    }
    UpdateInventory();
    return Slot.Quantity;
}

bool UInventoryComponent::IncreaseInventorySize(int32 IncreaseSize)
{
    if (IncreaseSize <= 0) return false;
    
    int32 NewSize = InventorySize + IncreaseSize;
    if (NewSize <= 0) return false;
    
    Inventory.SetNum(NewSize);
    InventorySize = NewSize;
    
    UpdateInventory();
    
    return true;
}

bool UInventoryComponent::MergeSlot(int32 IndexA, int32 IndexB)
{
    if (!Inventory.IsValidIndex(IndexA) || !Inventory.IsValidIndex(IndexB)) return false;
    if (IndexA == IndexB || Inventory[IndexA].IsEmpty() || Inventory[IndexB].IsEmpty()) return false;

    FInventorySlot& SlotA = Inventory[IndexA];
    FInventorySlot& SlotB = Inventory[IndexB];
    if (SlotA.ItemID != SlotB.ItemID) return false;

    FItemData ItemData;

    if (!GetItemData(SlotA.ItemID, ItemData)) return false;
    if(SlotA.Quantity == ItemData.MaxStack || SlotB.Quantity == ItemData.MaxStack) return false;
    
    int32 TotalQuantity = SlotA.Quantity + SlotB.Quantity;
    if (TotalQuantity > ItemData.MaxStack)
    {
        SlotA.Quantity = ItemData.MaxStack;
        SlotB.Quantity = TotalQuantity - ItemData.MaxStack;
    }
    else
    {
        SlotA.Quantity = TotalQuantity;
        SlotB.ClearSlot();
    }

    UpdateInventory();
    return true;
}

void UInventoryComponent::UpdateInventory()
{
    OnInventoryUpdated_Implementation();
    OnInventoryUpdated.Broadcast();
}