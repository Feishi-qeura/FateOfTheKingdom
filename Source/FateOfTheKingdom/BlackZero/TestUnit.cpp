#include "TestUnit.h"
#include "FlecsSubsystem.h"
#include "FlecsSystem/Public/Components/UnitComponents.h"
#include "GridSubsystem.h"

// ============================================================
//  构造函数
// ============================================================
ATestUnit::ATestUnit()
{
    // Actor 自己的 Tick 只用来同步坐标，开销很低
    PrimaryActorTick.bCanEverTick = true;
}

// ============================================================
//  BeginPlay
//  在 FlecsSubsystem 里创建 Entity，拿到句柄存起来
// ============================================================
void ATestUnit::BeginPlay()
{
    Super::BeginPlay();
    BindEntity();
	UE_LOG(LogTemp, Log, TEXT("Unit Init OK"));
}

// ============================================================
//  EndPlay
//  关卡结束或 Actor 被销毁时，清理 Flecs Entity
// ============================================================
void ATestUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (MyEntity.is_alive())
    {
        MyEntity.destruct(); // 从 Flecs World 中彻底移除
    }

    Super::EndPlay(EndPlayReason);
}

// ============================================================
//  Tick —— 同步层
//  把 Flecs 逻辑坐标写回 UE Actor Transform
//
//  注意执行顺序：
//    FlecsSubsystem::Tick → world.progress() → MoveSystem 更新坐标
//    → AUnitActor::Tick → 读取最新坐标 → SetActorLocationAndRotation
//
//  UE 的 Ticker 和 Actor Tick 都在同一帧内，顺序由 TickGroup 控制
//  FlecsSubsystem 注册在 TG_PrePhysics，Actor 默认也是 TG_PrePhysics
//  如果发现单帧延迟，可以把 Actor Tick 改为 TG_PostPhysics
// ============================================================
void ATestUnit::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!MyEntity.is_alive() || !FlecsSubsystem.IsValid())
    {
        return;
    }

    // 读取 Flecs 里的最新逻辑坐标
    const UnitComponents::FTransformComp* TransformComp =
        MyEntity.get<UnitComponents::FTransformComp>();

    if (TransformComp)
    {
        // 同步到 UE Actor（驱动 Mesh、碰撞体等跟着动）
        SetActorLocationAndRotation(
            TransformComp->Location,
            TransformComp->Rotation,
            false,  // bSweep: 关闭扫描，逻辑移动不需要碰撞检测
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }
}

// ============================================================
//  CommandMoveTo
//  玩家点击地图后调用这里
//  只写 MoveComp，真正的移动在 MoveSystem 里执行
// ============================================================
void ATestUnit::CommandMoveTo(FVector TargetLocation)
{
    if (FlecsSubsystem.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[TestUnit] Start path find ori:%s"), *this->GetActorLocation().ToString());
	    TArray<FVector> Path = GetWorld()->GetSubsystem<UGridSubsystem>()->DoPathFind(this, TargetLocation);
        /*for (FVector node : Path)
        {
            UE_LOG(LogTemp, Warning, TEXT("[AUnitActor] Path: %s"), *node.ToString());
        }*/
        FlecsSubsystem->MoveUnitTo(MyEntity, Path);
    }
}
void ATestUnit::BindEntity()
{
    UFlecsSubsystem* Subsystem = GetWorld()->GetSubsystem<UFlecsSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("[AUnitActor] FlecsSubsystem not found!"));
        return;
    }
    FlecsSubsystem = Subsystem;

    // 用 Actor 当前世界坐标作为初始逻辑坐标
    MyEntity = Subsystem->CreateUnit(GetActorLocation(), MoveSpeed);
}