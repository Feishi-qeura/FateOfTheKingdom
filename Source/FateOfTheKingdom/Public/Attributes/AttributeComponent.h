#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Attributes/AttributeStructs.h"
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
};