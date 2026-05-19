#include "ECS/ECSWorldSubsystem.h"

void UECSWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UECSWorldSubsystem::Deinitialize()
{
    BaseAttributeMap.Empty();
    CombatAttributeMap.Empty();
    FreeEntityIDs.Empty();
    Super::Deinitialize();
}

int32 UECSWorldSubsystem::GetEntityID(FEntityHandle Handle)
{
    return Handle.EntityID;
}

FEntityHandle UECSWorldSubsystem::CreateEntity()
{
    FEntityHandle Handle;
    Handle.EntityID = GenerateEntityID();
    return Handle;
}

void UECSWorldSubsystem::DestroyEntity(FEntityHandle Handle)
{
    if (!IsEntityValid(Handle)) return;
    BaseAttributeMap.Remove(Handle.EntityID);
    CombatAttributeMap.Remove(Handle.EntityID);
    ReleaseEntityID(Handle.EntityID);
}

bool UECSWorldSubsystem::IsEntityValid(FEntityHandle Handle) const
{
    return Handle.IsValid() && Handle.EntityID < NextEntityID;
}

bool UECSWorldSubsystem::AddBaseAttribute(FEntityHandle Handle, FBaseAttributeComponent Attribute)
{
    if (!IsEntityValid(Handle)) return false;
    
    Attribute.MaxHealth = FMath::Max(1.0f, Attribute.MaxHealth);
    Attribute.CurrentHealth = FMath::Clamp(Attribute.CurrentHealth, 0.0f, Attribute.MaxHealth);
    Attribute.MaxActionPoint = FMath::Max(0.0f, Attribute.MaxActionPoint);
    Attribute.CurrentActionPoint = FMath::Clamp(Attribute.CurrentActionPoint, 0.0f, Attribute.MaxActionPoint);
    Attribute.Speed = FMath::Max(0.0f, Attribute.Speed);
    Attribute.MoveRange = FMath::Max(0.0f, Attribute.MoveRange);

    BaseAttributeMap.Add(Handle.EntityID, Attribute);
    return true;
}

bool UECSWorldSubsystem::GetBaseAttribute(FEntityHandle Handle, FBaseAttributeComponent& OutAttribute) const
{
    if (const FBaseAttributeComponent* Found = BaseAttributeMap.Find(Handle.EntityID))
    {
        OutAttribute = *Found;
        return true;
    }
    return false;
}

bool UECSWorldSubsystem::SetBaseAttribute(FEntityHandle Handle, FBaseAttributeComponent Attribute)
{
    if (!IsEntityValid(Handle)) return false;
    
    Attribute.MaxHealth = FMath::Max(1.0f, Attribute.MaxHealth);
    Attribute.CurrentHealth = FMath::Clamp(Attribute.CurrentHealth, 0.0f, Attribute.MaxHealth);
    Attribute.MaxActionPoint = FMath::Max(0.0f, Attribute.MaxActionPoint);
    Attribute.CurrentActionPoint = FMath::Clamp(Attribute.CurrentActionPoint, 0.0f, Attribute.MaxActionPoint);
    Attribute.Speed = FMath::Max(0.0f, Attribute.Speed);
    Attribute.MoveRange = FMath::Max(0.0f, Attribute.MoveRange);

    BaseAttributeMap[Handle.EntityID] = Attribute;
    return true;
}

bool UECSWorldSubsystem::AddCombatAttribute(FEntityHandle Handle, FCombatAttributeComponent Attribute)
{
    if (!IsEntityValid(Handle)) return false;
    
    Attribute.Strength = FMath::Max(0.0f, Attribute.Strength);
    Attribute.MagicResistance = FMath::Max(0.0f, Attribute.MagicResistance);
    Attribute.PhysicalResistance = FMath::Max(0.0f, Attribute.PhysicalResistance);
    Attribute.Dodge = FMath::Clamp(Attribute.Dodge, 0.0f, 100.0f);
    Attribute.Luck = FMath::Max(0.0f, Attribute.Luck);

    CombatAttributeMap.Add(Handle.EntityID, Attribute);
    return true;
}

bool UECSWorldSubsystem::GetCombatAttribute(FEntityHandle Handle, FCombatAttributeComponent& OutAttribute) const
{
    if (const FCombatAttributeComponent* Found = CombatAttributeMap.Find(Handle.EntityID))
    {
        OutAttribute = *Found;
        return true;
    }
    return false;
}

bool UECSWorldSubsystem::SetCombatAttribute(FEntityHandle Handle, FCombatAttributeComponent Attribute)
{
    if (!IsEntityValid(Handle)) return false;
    
    Attribute.Strength = FMath::Max(0.0f, Attribute.Strength);
    Attribute.MagicResistance = FMath::Max(0.0f, Attribute.MagicResistance);
    Attribute.PhysicalResistance = FMath::Max(0.0f, Attribute.PhysicalResistance);
    Attribute.Dodge = FMath::Clamp(Attribute.Dodge, 0.0f, 100.0f);
    Attribute.Luck = FMath::Max(0.0f, Attribute.Luck);

    CombatAttributeMap[Handle.EntityID] = Attribute;
    return true;
}

int32 UECSWorldSubsystem::GenerateEntityID()
{
    return FreeEntityIDs.Num() > 0 ? FreeEntityIDs.Pop() : NextEntityID++;
}

void UECSWorldSubsystem::ReleaseEntityID(int32 EntityID)
{
    FreeEntityIDs.Add(EntityID);
}