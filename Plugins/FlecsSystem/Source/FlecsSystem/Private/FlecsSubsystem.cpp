#include "FlecsSubsystem.h"
#include "UnitComponents.h"
#include "MoveSystem.h"

// ============================================================
//  Initialize
//  关卡开始时自动调用，在这里完成：
//    1. 注册所有 Component 类型（给 Flecs Explorer 可读名称）
//    2. 注册所有 System
//    3. 挂载 UE Tick
// ============================================================
void UFlecsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 1. 注册组件
    UnitComponents::RegisterAll(World);

    // 2. 注册系统
    MoveSystems::RegisterMoveSystem(World);
    // 未来在这里继续追加：AttackSystem、DeathSystem...

    // 3. 挂载到 UE 的全局 Ticker
    //    Lambda 返回 true 表示持续执行，返回 false 则自动解除
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
        {
            Tick(DeltaTime);
            return true; // 持续运行
        })
    );

    UE_LOG(LogTemp, Log, TEXT("[FlecsSubsystem] Initialized."));
}

// ============================================================
//  Deinitialize
//  关卡结束时自动调用，清理 Ticker
// ============================================================
void UFlecsSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    UE_LOG(LogTemp, Log, TEXT("[FlecsSubsystem] Deinitialized."));

    Super::Deinitialize();
}

// ============================================================
//  Tick
//  每帧由 UE Ticker 调用
//  world.progress(DeltaTime) 会按 Pipeline 顺序执行所有 System
// ============================================================
void UFlecsSubsystem::Tick(float DeltaTime)
{
    World.progress(DeltaTime);
}

// ============================================================
//  CreateUnit
//  工厂方法：创建 Entity 并挂载初始组件
//  调用方（AUnitActor）保存返回的 Entity 作为桥接句柄
// ============================================================
flecs::entity UFlecsSubsystem::CreateUnit(const FVector& SpawnLocation, float Speed)
{
    flecs::entity Unit = World.entity()
        .set<UnitComponents::FTransformComp>({
            .Location = SpawnLocation,
            .Rotation = FRotator::ZeroRotator
        })
        .set<UnitComponents::FMoveComp>({
            .TargetLocation = SpawnLocation,
            .Speed          = Speed,
            .bIsMoving      = false
        });

    UE_LOG(LogTemp, Log, TEXT("[FlecsSubsystem] Created Unit Entity id=%llu at %s"),
        Unit.id(), *SpawnLocation.ToString());

    return Unit;
}

// ============================================================
//  MoveUnitTo
//  外部命令接口：设置移动目标
//  只写 MoveComp，不直接碰 TransformComp
//  真正的位移由 MoveSystem 在下一帧完成
// ============================================================
void UFlecsSubsystem::MoveUnitTo(flecs::entity Unit, const TArray<FVector>& Path)
{
    if (!Unit.is_alive() || Path.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[FlecsSubsystem] MoveUnitTo: Entity is not alive."));
        return;
    }

    if (UnitComponents::FMoveComp* Move = Unit.get_mut<UnitComponents::FMoveComp>())
    {
        // SetPath 内部会重置 PathIndex，天然支持打断中途移动
        Move->SetPath(Path);
    }
}