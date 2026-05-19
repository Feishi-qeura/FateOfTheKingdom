#include "../Public/ECS/UECSEntityComponent.h"
#include "ECS/ECSWorldSubsystem.h"
#include "Engine/GameInstance.h"

UUECSEntityComponent::UUECSEntityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUECSEntityComponent::BeginPlay()
{
    Super::BeginPlay();
    
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            EntityHandle = ECS->CreateEntity();
        }
    }
}

void UUECSEntityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 自动销毁实体
    if (EntityHandle.IsValid())
    {
        if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
        {
            if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
            {
                ECS->DestroyEntity(EntityHandle);
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}

bool UUECSEntityComponent::AddBaseAttribute(FBaseAttributeComponent Attribute)
{
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            return ECS->AddBaseAttribute(EntityHandle, Attribute);
        }
    }
    return false;
}

bool UUECSEntityComponent::AddCombatAttribute(FCombatAttributeComponent Attribute)
{
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            return ECS->AddCombatAttribute(EntityHandle, Attribute);
        }
    }
    return false;
}

bool UUECSEntityComponent::GetBaseAttribute(FBaseAttributeComponent& OutAttribute) const
{
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            return ECS->GetBaseAttribute(EntityHandle, OutAttribute);
        }
    }
    return false;
}

bool UUECSEntityComponent::GetCombatAttribute(FCombatAttributeComponent& OutAttribute) const
{
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            return ECS->GetCombatAttribute(EntityHandle, OutAttribute);
        }
    }
    return false;
}

bool UUECSEntityComponent::SetBaseAttribute(FBaseAttributeComponent Attribute)
{
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            return ECS->SetBaseAttribute(EntityHandle, Attribute);
        }
    }
    return false;
}

bool UUECSEntityComponent::SetCombatAttribute(FCombatAttributeComponent Attribute)
{
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UECSWorldSubsystem* ECS = GameInstance->GetSubsystem<UECSWorldSubsystem>())
        {
            return ECS->SetCombatAttribute(EntityHandle, Attribute);
        }
    }
    return false;
}