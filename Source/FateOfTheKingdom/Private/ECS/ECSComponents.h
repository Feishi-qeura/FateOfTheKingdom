#pragma once

#include "CoreMinimal.h"
#include "ECSComponents.generated.h"

// 基础属性组件
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FBaseAttributeComponent
{
	GENERATED_BODY()
	//生命值
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentHealth = 100.0f;

	//行动点
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxActionPoint = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentActionPoint = 2;

	//速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 80.0f;

	//移动范围
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveRange = 5.0f;

	bool IsAlive() const { return CurrentHealth > 0.0f; }
};

// 战斗属性组件
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FCombatAttributeComponent
{
	GENERATED_BODY()

	//力量
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Strength = 10.0f;
	//魔抗
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MagicResistance = 5.0f;
	//物抗
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PhysicalResistance = 5.0f;
	//闪避
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Dodge = 10.0f;
	//幸运
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Luck = 5.0f;
	
};