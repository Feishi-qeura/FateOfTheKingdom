#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/NoExportTypes.h"
#include "ItemUseElement.generated.h"

UCLASS(Abstract , Blueprintable , BlueprintType)

class INVENTORY_API UItemUseElement : public UObject
{
    GENERATED_BODY()

public:
    UItemUseElement();

    UFUNCTION(BlueprintImplementableEvent , Category = "Inventory" , meta = (DisplayName = "使用物品时"))
    void OnUse( AActor* User , FName ItemID , int32 Count );

    UFUNCTION(BlueprintCallable , Category = "Inventory" , meta = (DisplayName = "使用物品"))
    void UseItem(AActor* User , FName ItemID , int32 Count);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory",Meta = (ExposeOnSpawn=true))
	AActor* Owner;


private:

    virtual UWorld* GetWorld() const override;
    UWorld* World;


};