// Copyright wenqiu. Turn-based combat manager inspired by Honkai: Star Rail.

#include "TurnManager.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "TurnCharacterAttribute.h"

void UTurnManager::Deinitialize()
{
	TurnQueue.Reset();
	CurrentActionCharacter.Reset();
	NextRegisterOrder = 0;
	Super::Deinitialize();
}

bool UTurnManager::RegisterCharacter(AActor* Character)
{
	if (!IsValid(Character))
	{
		return false;
	}

	//同一个角色重复注册：只刷新 Speed，不重复入队，不动 CurrentAV / Priority 运行时状态
	if (const int32 ExistIdx = FindEntryIndex(Character); ExistIdx != INDEX_NONE)
	{
		PullSpeedFromInterface(TurnQueue[ExistIdx]);
		FU_SortQueue();
		return true;
	}

	UActorComponent* AttrComp = FindAttributeComponent(Character);
	if (!AttrComp)
	{
		//角色没有实现ITurnCharacterAttribute的组件，没法进队
		return false;
	}

	FTurnCharacterEntry Entry;
	Entry.Character = Character;
	Entry.AttributeComponent = AttrComp;
	Entry.RegisterOrder = NextRegisterOrder++;

	//首次注册：通过接口拉 Speed；CurrentAV 按 10000/Speed 初始化；Priority 一律从 0 起步（纯 C++ 状态）
	PullSpeedFromInterface(Entry);
	Entry.Priority = 0;
	Entry.CurrentAV = Entry.Speed > KINDA_SMALL_NUMBER ? ActionValueFull / Entry.Speed : ActionValueFull;

	TurnQueue.Add(Entry);
	FU_SortQueue();
	return true;
}

void UTurnManager::UnregisterCharacter(AActor* Character)
{
	if (!Character)
	{
		return;
	}

	const int32 Index = FindEntryIndex(Character);
	if (Index == INDEX_NONE)
	{
		return;
	}

	TurnQueue.RemoveAt(Index);

	//正在行动的角色被强制移除（死亡触发），把状态清掉，继续FU_TurnStart
	if (CurrentActionCharacter.Get() == Character)
	{
		CurrentActionCharacter.Reset();
	}

	FU_SortQueue();
}

void UTurnManager::RefreshCharacter(AActor* Character)
{
	const int32 Index = FindEntryIndex(Character);
	if (Index == INDEX_NONE)
	{
		return;
	}

	//只重新拉 Speed；CurrentAV / 运行时 Priority 是 C++ 内部状态，不能被外部覆盖。
	PullSpeedFromInterface(TurnQueue[Index]);
	FU_SortQueue();
}

AActor* UTurnManager::FU_TurnStart()
{
	PruneInvalidEntries();

	//保护：上一个角色还没FU_TurnEnd，不再发起新回合
	if (CurrentActionCharacter.IsValid())
	{
		return CurrentActionCharacter.Get();
	}

	if (TurnQueue.Num() == 0)
	{
		return nullptr;
	}

	FU_SortQueue();

	FTurnCharacterEntry& Picked = TurnQueue[0];
	AActor* ActionCharacter = Picked.Character.Get();
	if (!ActionCharacter)
	{
		return nullptr;
	}

	const float PickedAV = Picked.CurrentAV;

	//星穹铁道倒计时模型：CurrentAV = "距离自己出手还剩多少"，0 即当前回合。
	//把队首的 AV 从全队同步减掉：picked 归零，其他人按比例靠前。
	//全队减同一个数 ⇒ 相对顺序不变，所以这里不需要重排。
	//注意：Priority 不在 FU_TurnStart 改。改了会让"正在行动的角色"被排到后面，
	//UMG 上就会出现"player3 跳到第一位"的现象。Priority 调整放在 FU_TurnEnd。
	for (FTurnCharacterEntry& E : TurnQueue)
	{
		E.CurrentAV = FMath::Max(0.f, E.CurrentAV - PickedAV);
	}

	CurrentActionCharacter = ActionCharacter;

	//不调 FU_SortQueue —— 当前行动者就在 [0]，要保持。但 UMG 需要刷新 AV 显示数字，
	//所以单独广播 OnTurnQueueSorted（语义上是"队列状态变了"，UMG 重读即可）。
	OnTurnQueueSorted.Broadcast();
	OnTurnStarted.Broadcast(ActionCharacter);
	return ActionCharacter;
}

void UTurnManager::FU_TurnEnd(AActor* ActionCharacter)
{
	//容错：传nullptr时用记录的当前行动者。
	if (!ActionCharacter)
	{
		ActionCharacter = CurrentActionCharacter.Get();
	}

	const int32 Index = FindEntryIndex(ActionCharacter);
	if (Index != INDEX_NONE)
	{
		FTurnCharacterEntry& Entry = TurnQueue[Index];

		//Priority +1：行动者把"下次轮到我"的资格往后排一格，单调累加。
		//
		//**不复位**其它同 AV 角色的 Priority —— 复位会把它们之间已经积累的
		//相对顺序（比如 e2=1, e3=2, p=4 这种"刚轮过谁、还没轮到谁"的历史）抹平为 0，
		//下一轮选谁就完全看 RegisterOrder 谁先 Spawn，会出现 player 莫名跳到第一的 bug。
		//
		//单调 ++ 让每个角色每回合 P 涨 1，相对顺序锁死，循环节稳定。
		//同 AV、同 P 的特例由 sort 的第三键 RegisterOrder 自动兜底。
		Entry.Priority += 1;

		//按 10000/Speed 回填 CurrentAV
		const float Refill = Entry.Speed > KINDA_SMALL_NUMBER ? ActionValueFull / Entry.Speed : ActionValueFull;
		Entry.CurrentAV += Refill;
	}

	CurrentActionCharacter.Reset();

	//到这里才真正重排：行动者的 AV / Priority 都涨了，会被排到后面。
	FU_SortQueue();

	OnTurnEnded.Broadcast(ActionCharacter);
}

TArray<AActor*> UTurnManager::GetTurnQueueCharacters() const
{
	TArray<AActor*> Result;
	Result.Reserve(TurnQueue.Num());
	for (const FTurnCharacterEntry& Entry : TurnQueue)
	{
		if (AActor* Character = Entry.Character.Get())
		{
			Result.Add(Character);
		}
	}
	return Result;
}

AActor* UTurnManager::FU_QueryNextActionCharacter() const
{
	//只读查询：不修改 TurnQueue。
	//如果有人正在行动（queue[0] 是当前行动者），跳过 [0]，返回真正的"下一个"。
	//否则直接返回 [0]。
	const int32 StartIndex = CurrentActionCharacter.IsValid() ? 1 : 0;
	for (int32 i = StartIndex; i < TurnQueue.Num(); ++i)
	{
		if (AActor* Character = TurnQueue[i].Character.Get())
		{
			return Character;
		}
	}
	return nullptr;
}

void UTurnManager::FU_SortQueue()
{
	PruneInvalidEntries();

	// 稳定排序：CurrentAV 升序 → Priority 升序 → RegisterOrder 升序（兜底）。
	TurnQueue.StableSort([](const FTurnCharacterEntry& A, const FTurnCharacterEntry& B)
	{
		if (!IsSameActionValue(A.CurrentAV, B.CurrentAV))
		{
			return A.CurrentAV < B.CurrentAV;
		}
		if (A.Priority != B.Priority)
		{
			return A.Priority < B.Priority;
		}
		return A.RegisterOrder < B.RegisterOrder;
	});

	OnTurnQueueSorted.Broadcast();
}

int32 UTurnManager::FindEntryIndex(const AActor* Character) const
{
	if (!Character)
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < TurnQueue.Num(); ++i)
	{
		if (TurnQueue[i].Character.Get() == Character)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UTurnManager::PruneInvalidEntries()
{
	TurnQueue.RemoveAll([](const FTurnCharacterEntry& Entry)
	{
		return !Entry.Character.IsValid() || !Entry.AttributeComponent.IsValid();
	});
}

UActorComponent* UTurnManager::FindAttributeComponent(AActor* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	// AActor::GetComponentsByInterface 接受 UInterface 子类，返回所有实现该接口的组件。
	TArray<UActorComponent*> Found = Character->GetComponentsByInterface(UTurnCharacterAttribute::StaticClass());
	return Found.Num() > 0 ? Found[0] : nullptr;
}

void UTurnManager::PullSpeedFromInterface(FTurnCharacterEntry& Entry) const
{
	UActorComponent* AttrComp = Entry.AttributeComponent.Get();
	if (!AttrComp)
	{
		return;
	}

	//Execute_xxx是UHT为BlueprintNativeEvent接口函数生成的静态分发器，
	//自动走BP override → C++ _Implementation 的查找链。
	Entry.Speed = ITurnCharacterAttribute::Execute_GetSpeed(AttrComp);
}

float UTurnManager::GetCharacterCurrentAV(AActor* Character) const
{
	const int32 Index = FindEntryIndex(Character);
	return Index != INDEX_NONE ? TurnQueue[Index].CurrentAV : -1.f;
}

int32 UTurnManager::GetCharacterRuntimePriority(AActor* Character) const
{
	const int32 Index = FindEntryIndex(Character);
	return Index != INDEX_NONE ? TurnQueue[Index].Priority : 0;
}
