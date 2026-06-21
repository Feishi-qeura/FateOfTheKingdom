// Copyright wenqiu. Turn-based combat manager inspired by Honkai: Star Rail.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TurnManager.generated.h"

class AActor;
class UActorComponent;

//BP_TurnManager转换C++，使用WorldSubSystem，启用AActor，好处：自动实现生命周期，无需手动管理

//回合开始事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnStartedDelegate, AActor*, ActionCharacter);

//回合结束事件
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnEndedDelegate, AActor*, ActionCharacter);

//队列重排事件，监听它刷新头像顺序
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTurnQueueSortedDelegate);


/**
 * 单个角色在回合队列里的条目。
 * AV/Speed/Priority，从BP_CharacterAttribute通过ITurnCharacterAttribute抽象接口
 */
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FTurnCharacterEntry
{
	GENERATED_BODY()

	//角色弱引用。Destroy后队列自动清理掉这一条。
	UPROPERTY(BlueprintReadOnly, Category = "wenqiu|TurnManager")
	TWeakObjectPtr<AActor> Character;

	//角色身上实现了ITurnCharacterAttribute的组件（BP_CharacterAttribute）
	//管理器写SetCurrentAV/SetPriority时通过这个指针调过去
	UPROPERTY(BlueprintReadOnly, Category = "wenqiu|TurnManager")
	TWeakObjectPtr<UActorComponent> AttributeComponent;

	//当前行动值
	UPROPERTY(BlueprintReadOnly, Category = "wenqiu|TurnManager")
	float CurrentAV = 10000.0f;

	//速度
	UPROPERTY(BlueprintReadOnly, Category = "wenqiu|TurnManager")
	float Speed = 100.0f;

	//优先级
	UPROPERTY(BlueprintReadOnly, Category = "wenqiu|TurnManager")
	int32 Priority = 0;

	//注册顺序
	UPROPERTY(BlueprintReadOnly, Category = "wenqiu|TurnManager")
	int32 RegisterOrder = 0;
};

//继承UWorldSubsystem子系统
UCLASS(BlueprintType)
class FATEOFTHEKINGDOM_API UTurnManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//注册一个角色。要求Character身上至少挂一个实现ITurnCharacterAttribute的组件
	//（BP_CharacterAttribute）注册时管理器立刻通过接口pull一次AV/Speed/Priority。
	//返回false表示找不到属性组件，没有入队。
	UFUNCTION(BlueprintCallable, Category = "wenqiu|TurnManager")
	bool RegisterCharacter(AActor* Character);

	//把角色从队列里摘掉（死亡/撤退/召唤物到期都走它）。扩展功能
	UFUNCTION(BlueprintCallable, Category = "wenqiu|TurnManager")
	void UnregisterCharacter(AActor* Character);

	//外部改完BP_CharacterAttribute（比如吃了减速buff、复活）后调一次，
	//管理器会通过接口重新pull最新值并队列排序。
	UFUNCTION(BlueprintCallable, Category = "wenqiu|TurnManager")
	void RefreshCharacter(AActor* Character);

	
	
	
																//核心回合API

	//FU_TurnStart：取出队首角色作为本回合行动者，处理同等行动值优先级规则
	//通过接口把新Priority写回组件，广播OnTurnStarted。重入保护：未FU_TurnEnd时返回当前行动者
	UFUNCTION(BlueprintCallable, Category = "wenqiu|TurnManager", DisplayName = "FU_TurnStart")
	AActor* FU_TurnStart();

	//FU_TurnEnd：刚行动完的角色，通过接口写回组件
	//FU_SortQueue，广播OnTurnEnded。ActionCharacter传nullptr时用当前行动者
	UFUNCTION(BlueprintCallable, Category = "wenqiu|TurnManager", DisplayName = "FU_TurnEnd")
	void FU_TurnEnd(AActor* ActionCharacter);

	//FU_QueryNextActionCharacter：只读peek队首，不修改任何状态
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager", DisplayName = "FU_QueryNextActionCharacter")
	AActor* FU_QueryNextActionCharacter() const;

	//FU_队列排序：按(CurrentAV升序,Priority升序,RegisterOrder升序)
	//排完广播OnTurnQueueSorted
	UFUNCTION(BlueprintCallable, Category = "wenqiu|TurnManager", DisplayName = "FU_SortQueue")
	void FU_SortQueue();
	

	//完整条目数组（含AV/Speed/Priority），需要排序详情时用
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager")
	const TArray<FTurnCharacterEntry>& GetTurnQueue() const { return TurnQueue; }

	//UMG_PlayerUI，UMG_Running，UMG_RunningTou主要用这个：按行动顺序排好的
	//有序BP_CharacterBs Actor数组
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager")
	TArray<AActor*> GetTurnQueueCharacters() const;

	//当前是否有角色正在行动，避免BluePrint误触发两次FU_TurnStart
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager")
	bool IsTurnInProgress() const { return CurrentActionCharacter.IsValid(); }

	//当前正在行动的角色，没有则返回nullptr
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager")
	AActor* GetCurrentActionCharacter() const { return CurrentActionCharacter.Get(); }

	//C++ → BP：返回管理器算出来的当前行动值。UMG 显示用。Character 不在队列时返回 -1。
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager")
	float GetCharacterCurrentAV(AActor* Character) const;

	//C++ → BP：返回运行时 Priority（包含 +1 / 复位 0 的累积结果）。Character 不在队列时返回 0。
	UFUNCTION(BlueprintPure, Category = "wenqiu|TurnManager")
	int32 GetCharacterRuntimePriority(AActor* Character) const;

	//事件 

	UPROPERTY(BlueprintAssignable, Category = "wenqiu|TurnManager")
	FOnTurnStartedDelegate OnTurnStarted;

	UPROPERTY(BlueprintAssignable, Category = "wenqiu|TurnManager")
	FOnTurnEndedDelegate OnTurnEnded;

	UPROPERTY(BlueprintAssignable, Category = "wenqiu|TurnManager")
	FOnTurnQueueSortedDelegate OnTurnQueueSorted;

protected:
	virtual void Deinitialize() override;

private:
	//通过角色指针找到对应条目下标，找不到返回INDEX_NONE
	int32 FindEntryIndex(const AActor* Character) const;

	//清理弱引用已失效（被GC,Destroy）的条目，所有公开API进入前都先调用一次
	void PruneInvalidEntries();

	//在Character身上找第一个实现ITurnCharacterAttribute的组件
	static UActorComponent* FindAttributeComponent(AActor* Character);

	//通过接口拉 BP 那边的 Speed（CurrentAV/Priority 都是 C++ 内部状态，不在这拉）
	void PullSpeedFromInterface(FTurnCharacterEntry& Entry) const;

	//浮点CurrentAV的相等判定，避免精度抖动导致同CurrentAV误判
	static bool IsSameActionValue(float A, float B) { return FMath::IsNearlyEqual(A, B, 0.001f); }

	//维护的有序队列FU_SortQueue，[0]为下一个行动者
	UPROPERTY()
	TArray<FTurnCharacterEntry> TurnQueue;

	//当前正在行动的角色；FU_TurnStart写入，FU_TurnEnd清空
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentActionCharacter;

	//RegisterCharacter自增计数，提供最终稳定排序键
	int32 NextRegisterOrder = 0;
	
	static constexpr float ActionValueFull = 100.0f;
};
