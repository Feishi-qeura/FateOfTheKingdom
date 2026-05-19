#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ECSCommon.h"
#include "ECSComponents.h"
#include "ECSWorldSubsystem.generated.h"

// ECS世界管理器
UCLASS()
class FATEOFTHEKINGDOM_API UECSWorldSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 实体
    UFUNCTION(BlueprintCallable)
    int32 GetEntityID(FEntityHandle Handle);
	
    UFUNCTION(BlueprintCallable)
    FEntityHandle CreateEntity();

    UFUNCTION(BlueprintCallable)
    void DestroyEntity(FEntityHandle Handle);

    UFUNCTION(BlueprintPure)
    bool IsEntityValid(FEntityHandle Handle) const;

    // 基础属性
    UFUNCTION(BlueprintCallable)
    bool AddBaseAttribute(FEntityHandle Handle, FBaseAttributeComponent Attribute);

    UFUNCTION(BlueprintPure)
    bool GetBaseAttribute(FEntityHandle Handle, FBaseAttributeComponent& OutAttribute) const;

    UFUNCTION(BlueprintCallable)
    bool SetBaseAttribute(FEntityHandle Handle, FBaseAttributeComponent Attribute);

    // 战斗属性
    UFUNCTION(BlueprintCallable)
    bool AddCombatAttribute(FEntityHandle Handle, FCombatAttributeComponent Attribute);

    UFUNCTION(BlueprintPure)
    bool GetCombatAttribute(FEntityHandle Handle, FCombatAttributeComponent& OutAttribute) const;

    UFUNCTION(BlueprintCallable)
    bool SetCombatAttribute(FEntityHandle Handle, FCombatAttributeComponent Attribute);

private:
    TMap<int32, FBaseAttributeComponent> BaseAttributeMap;
    TMap<int32, FCombatAttributeComponent> CombatAttributeMap;

    int32 NextEntityID = 1;
    TArray<int32> FreeEntityIDs;

    int32 GenerateEntityID();
    void ReleaseEntityID(int32 EntityID);
};