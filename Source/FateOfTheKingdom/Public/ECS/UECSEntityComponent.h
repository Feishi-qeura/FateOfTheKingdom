#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ECS/ECSCommon.h"
#include "UECSEntityComponent.generated.h"


/**
 * ECS 实体组件
 * 挂载到蓝图后自动创建 ECS 实体，无需手动管理 Handle
 * 
 * 使用方式：
 *   1. 蓝图添加此组件
 *   2. BeginPlay 自动创建实体
 *   3. 调用 AddBaseAttribute / GetBaseAttribute 等方法
 *   4. EndPlay 自动销毁实体
 */
UCLASS(ClassGroup=(ECS), meta=(BlueprintSpawnableComponent))
class FATEOFTHEKINGDOM_API UUECSEntityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUECSEntityComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintPure, Category = "ECS")
    FEntityHandle GetEntityHandle() const { return EntityHandle; }
	
    UFUNCTION(BlueprintPure, Category = "ECS")
    int32 GetEntityID() const { return EntityHandle.EntityID; }
	
    UFUNCTION(BlueprintCallable, Category = "ECS")
    bool AddBaseAttribute(FBaseAttributeComponent Attribute);

    UFUNCTION(BlueprintCallable, Category = "ECS")
    bool AddCombatAttribute(FCombatAttributeComponent Attribute);
	
    UFUNCTION(BlueprintPure, Category = "ECS")
    bool GetBaseAttribute(FBaseAttributeComponent& OutAttribute) const;

    UFUNCTION(BlueprintPure, Category = "ECS")
    bool GetCombatAttribute(FCombatAttributeComponent& OutAttribute) const;
	
    UFUNCTION(BlueprintCallable, Category = "ECS")
    bool SetBaseAttribute(FBaseAttributeComponent Attribute);

    UFUNCTION(BlueprintCallable, Category = "ECS")
    bool SetCombatAttribute(FCombatAttributeComponent Attribute);

private:
    FEntityHandle EntityHandle;
};