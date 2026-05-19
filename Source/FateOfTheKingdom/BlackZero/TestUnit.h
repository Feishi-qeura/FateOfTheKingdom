#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "flecs.h"
#include "TestUnit.generated.h"

UCLASS()
class FATEOFTHEKINGDOM_API ATestUnit : public APawn
{
	GENERATED_BODY()

public:
	ATestUnit();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

	// ---- 外部调用：命令这个单位移动到目标点 ----
	UFUNCTION(BlueprintCallable, Category = "Unit|Move")
	void CommandMoveTo(FVector TargetLocation);
	// ---- 初始移动速度（BeginPlay 时传给 FMoveComp）----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit")
	float MoveSpeed = 300.f;

private:
	// Flecs Entity 句柄，是这个 Actor 和 ECS 世界之间唯一的桥梁
	flecs::entity MyEntity;
	void BindEntity();

	// 缓存 Subsystem 指针，避免每帧 GetSubsystem 查找
	TWeakObjectPtr<class UFlecsSubsystem> FlecsSubsystem;
};