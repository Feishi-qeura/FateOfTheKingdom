#include "../../Public/Attributes/AttributeComponent.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


//设置属性，强转结构体
void UAttributeComponent::SetAttribute(enum EAttribute Type, float Value)
{
	switch (Type)
	{
	case EAttribute::MAXHP:BaseAttribute.MaxHealth = Value;
		break;
		
	case EAttribute::HP:BaseAttribute.CurrentHealth = Value;
		break;
		
	case EAttribute::MaxActionPoint:BaseAttribute.CurrentActionPoint = Value;
		break;
		
	case EAttribute::Speed:BaseAttribute.Speed = Value;
		break;
		
	case EAttribute::MoveRange:BaseAttribute.MoveRange = Value;
		break;
		
	case EAttribute::CurrentActionPoint:BaseAttribute.CurrentActionPoint = Value;
		break;
		
	case EAttribute::Strength:CombatAttribute.Strength = Value;
		break;
		
	case EAttribute::MagicResistance:CombatAttribute.MagicResistance = Value;
		break;
		
	case EAttribute::PhysicalResistance:CombatAttribute.PhysicalResistance = Value;
		break;
		
	case EAttribute::Dodge:CombatAttribute.Dodge = Value;
		break;
		
	case EAttribute::Luck:CombatAttribute.Luck = Value;
		break;
		
	}
}
