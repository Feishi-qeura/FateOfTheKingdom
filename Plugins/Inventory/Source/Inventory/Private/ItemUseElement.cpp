#include "ItemUseElement.h"

UItemUseElement::UItemUseElement()
{
    World = GetOuter()->GetWorld();
}

void UItemUseElement::UseItem(AActor* User, FName ItemID, int32 Count)
{
    OnUse(User, ItemID, Count);
    Owner = User;
}

UWorld* UItemUseElement::GetWorld() const
{
    if(Owner) return Owner->GetWorld();
    if(World) return World;
        UE_LOG(LogTemp, Warning, TEXT("UItemUseElement::GetWorld() - Owner and Outer have no valid world!"));
        return nullptr;
}
