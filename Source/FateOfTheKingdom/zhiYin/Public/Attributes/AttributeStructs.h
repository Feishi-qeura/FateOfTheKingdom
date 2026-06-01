#pragma once

#include "CoreMinimal.h"
#include "AttributeStructs.generated.h"

// 基础属性
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FBaseAttribute
{
	GENERATED_BODY()

	//最大生命
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;

	//当前生命
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentHealth = 100.0f;

	//最大行动点
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxActionPoint = 4;
	
	//当前行动点
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentActionPoint = 2;
	
	//速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 80.0f;
	
	//移动距离
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MoveRange = 5.0f;
	
	//行动值
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentAV = 0.0f;
	
	//优先级
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority = 0;
	
};

// 战斗属性
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FCombatAttribute
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