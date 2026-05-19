#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "flecs.h"
#include "FlecsSubsystem.generated.h"

// ============================================================
//  UFlecsSubsystem
//  作为 UWorldSubsystem 挂载在 UWorld 上
//  生命周期与关卡一致，自动创建/销毁
//
//  用法：
//    UFlecsSubsystem* Flecs = GetWorld()->GetSubsystem<UFlecsSubsystem>();
//    flecs::entity Unit = Flecs->CreateUnit(FVector(100, 0, 0));
// ============================================================

UCLASS()
class FLECSSYSTEM_API UFlecsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ---- USubsystem 生命周期 ----
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- Tick（每帧推进 Flecs World）----
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
    void         Tick(float DeltaTime);

    // ---- 工厂方法：创建一个带基础组件的单位 Entity ----
    flecs::entity CreateUnit(const FVector& SpawnLocation, float Speed = 300.f);

    // ---- 命令接口：让某个单位移向目标点 ----
    void MoveUnitTo(flecs::entity Unit, const TArray<FVector>& Path);

    // ---- 直接访问 World（供高级用法）----
    flecs::world& GetFlecsWorld() { return World; }

private:
    flecs::world World;

    // 用 FTickerDelegate 在 UE 的 Tick 里驱动 world.progress()
    FTSTicker::FDelegateHandle TickHandle;
};