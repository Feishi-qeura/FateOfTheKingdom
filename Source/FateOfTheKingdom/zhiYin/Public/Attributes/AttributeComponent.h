#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeStructs.h"
#include "AttributeComponent.generated.h"

UCLASS(ClassGroup=(Attributes), meta=(BlueprintSpawnableComponent))
class FATEOFTHEKINGDOM_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FBaseAttribute BaseAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FCombatAttribute CombatAttribute;
	
	UFUNCTION(BlueprintPure, Category = "Attributes")
	FBaseAttribute GetBaseAttribute() const { return BaseAttribute; }

	UFUNCTION(BlueprintPure, Category = "Attributes")
	FCombatAttribute GetCombatAttribute() const { return CombatAttribute; }
	
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetBaseAttribute(FBaseAttribute Base) { BaseAttribute = Base; }

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetCombatAttribute(FCombatAttribute Combat) { CombatAttribute = Combat; }
	
	//设置角色属性覆盖，Type选择覆盖类型的值，value覆盖值的大小
	UFUNCTION(BlueprintCallable, Category = "AttributesManager")
	void SetAttribute(enum EAttribute Type, float Value);
};