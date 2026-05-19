#pragma once

#include "CoreMinimal.h"
#include "flecs.h"
#include "Components/UnitComponents.h"

// ============================================================
//  MoveSystem
//  职责：每帧把 FMoveComp 的意图转换为 FTransformComp 的位移
//
//  Query 条件：拥有 FMoveComp + FTransformComp，且没有 FDeadTag
//  执行阶段：flecs::OnUpdate（每次 world.progress() 时触发）
// ============================================================

namespace MoveSystems
{
    inline void RegisterMoveSystem(flecs::world& World)
    {
        World.system<UnitComponents::FTransformComp,
                     UnitComponents::FMoveComp>("MoveSystem")

            // 过滤掉死亡单位，Dead 实体不参与移动计算
            .without<UnitComponents::FDeadTag>()

            .kind(flecs::OnUpdate)

            .each([](flecs::iter& It,
                     size_t               Index,
                     UnitComponents::FTransformComp& Transform,
                     UnitComponents::FMoveComp&      Move)
            {
                // 没有移动意图则跳过，避免无效计算
                if (!Move.bIsMoving || Move.Path.IsEmpty())
                {
                    return;
                }
                // 越界保护（路径被外部打断时 PathIndex 可能过期）
                if (!Move.Path.IsValidIndex(Move.PathIndex))
                {
                    Move.bIsMoving = false;
                    return;
                }
                // ---- 到达检测 ----
                const FVector CurrentTarget = Move.Path[Move.PathIndex];
                const FVector Delta         = CurrentTarget - Transform.Location;
                const float   DistSq    = Delta.SizeSquared();
                const float   StopDistSq = 5.f * 5.f; // 5 cm 内视为到达

                if (DistSq <= Move.Speed * It.delta_time())
                {
                    Transform.Location = CurrentTarget;
                    Move.PathIndex++;
                    // 检查是否走完整条路径
                    if (!Move.Path.IsValidIndex(Move.PathIndex))
                    {
                        Move.bIsMoving = false;
                        Move.Path.Empty();
                    }
                    return;
                }
                //UE_LOG(LogTemp, Warning, TEXT("在跑了 tar:%s"), *CurrentTarget.ToString());
                // ---- 匀速移动（帧率无关，乘以 DeltaTime）----
                const float   DeltaTime = It.delta_time();
                const FVector Direction = Delta.GetSafeNormal();
                Transform.Location += Direction * Move.Speed * DeltaTime;

                // ---- 朝向目标旋转 ----
                // 只更新 Yaw，保持单位直立
                Transform.Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
                Transform.Rotation.Pitch = 0.f;
                Transform.Rotation.Roll  = 0.f;
            });
    }

    // ----------------------------------------------------------
    //  SyncSystem
    //  职责：将 Flecs 逻辑坐标同步回 UE Actor（表现层）
    //  执行阶段：PostUpdate，保证在移动逻辑之后运行
    //
    //  Component: FActorRefComp 存一个 AActor 弱指针
    //  （在下一步扩展时添加，此处预留注释占位）
    // ----------------------------------------------------------
    // inline void RegisterSyncSystem(flecs::world& World) { ... }
}
