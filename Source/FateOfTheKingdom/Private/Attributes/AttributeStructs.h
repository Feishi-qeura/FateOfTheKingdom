#pragma once

#include "CoreMinimal.h"
#include "AttributeStructs.generated.h"

// 基础属性
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FBaseAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxActionPoint = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentActionPoint = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveRange = 5.0f;
};

// 战斗属性
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FCombatAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Strength = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MagicResistance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PhysicalResistance = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Dodge = 10.0f;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Luck = 5.0f;
};