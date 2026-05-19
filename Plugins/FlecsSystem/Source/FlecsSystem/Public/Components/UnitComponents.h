#pragma once

#include "CoreMinimal.h"
#include "flecs.h"

// ============================================================
//  所有组件都是纯 C++ struct，不继承任何 UE 类
//  原则：只存数据，零逻辑
// ============================================================

namespace UnitComponents
{
    // ----------------------------------------------------------
    // TransformComponent
    // 存储单位的"逻辑坐标"，与 UE Actor 的 Transform 解耦
    // Flecs 负责逻辑运算，UE 负责渲染表现，两者通过 SyncSystem 同步
    // ----------------------------------------------------------
    struct FTransformComp
    {
        FVector Location   = FVector::ZeroVector;
        FRotator Rotation  = FRotator::ZeroRotator;
    };

    // ----------------------------------------------------------
    // MoveComponent
    // 描述单位"想去哪"以及"能跑多快"
    // MoveSystem 每帧读取这里，写入 FTransformComp::Location
    // ----------------------------------------------------------
    struct FMoveComp
    {
        TArray<FVector> Path;          // A* 返回的路径点队列
        int32           PathIndex = 0; // 当前正在走向的点索引
        FVector  TargetLocation = FVector::ZeroVector; // 目标点（世界坐标）
        float    Speed          = 300.f;               // 单位/秒
        bool     bIsMoving      = false;               // 是否正在移动
        
        // 便捷方法：设置新路径（可打断当前移动）
        void SetPath(const TArray<FVector>& NewPath)
        {
            Path       = NewPath;
            PathIndex  = 0;
            bIsMoving  = !NewPath.IsEmpty();
        }
    };

    // ----------------------------------------------------------
    // Tag：标记单位已死亡，方便 Query 过滤
    // Tag 不含任何数据，只用来筛选 Entity
    // ----------------------------------------------------------
    struct FDeadTag {};

    // ----------------------------------------------------------
    // 统一注册入口，在 FlecsSubsystem::Initialize() 里调用一次
    // ----------------------------------------------------------
    inline void RegisterAll(flecs::world& World)
    {
        World.component<FTransformComp>("TransformComp");
        World.component<FMoveComp>("MoveComp");
        World.component<FDeadTag>("DeadTag");
    }
}
