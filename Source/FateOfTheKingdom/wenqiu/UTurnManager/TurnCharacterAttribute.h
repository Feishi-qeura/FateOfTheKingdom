// Copyright wenqiu. Interface implemented by BP_CharacterAttribute so the turn manager
// can pull Speed / initial Priority without referencing the blueprint class.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TurnCharacterAttribute.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, meta = (DisplayName = "Turn Character Attribute"))
class UTurnCharacterAttribute : public UInterface
{
	GENERATED_BODY()
};

/**
 * 角色属性接口。BP_CharacterAttribute 在 Class Settings → Implemented Interfaces
 * 里实现这个接口，override 下面两个事件直接返回组件里的同名变量即可。
 *
 * 数据所有权约定：
 *   - Speed  ：BP 是真相源（设计师设置），C++ 通过 GetSpeed 拉取，吃 buff 后调 RefreshCharacter
 *   - Priority（初值）：BP 是真相源（默认 0，特殊角色可在 BP 里设其它初值）
 *   - Priority（运行时 ±1）：C++ 是真相源（管理器内部状态），UMG 显示请查 UTurnManager
 *   - CurrentAV ：C++ 是真相源（管理器按 10000/Speed 计算并维护），UMG 显示请查 UTurnManager
 *
 * 所以本接口**只暴露 BP→C++ 的两个 Getter**，不需要 SetCurrentAV / SetPriority。
 * 想读 C++ 算出来的 CurrentAV / 运行时 Priority，调 UTurnManager 上的 GetCharacterCurrentAV /
 * GetCharacterRuntimePriority 即可。
 */
class FATEOFTHEKINGDOM_API ITurnCharacterAttribute
{
	GENERATED_BODY()

public:
	// 返回速度（设计师设置 + buff 影响）。FU_TurnEnd 时管理器按 10000/Speed 回填 CurrentAV。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "wenqiu|TurnManager")
	float GetSpeed();
	virtual float GetSpeed_Implementation() { return 100.f; }
};
