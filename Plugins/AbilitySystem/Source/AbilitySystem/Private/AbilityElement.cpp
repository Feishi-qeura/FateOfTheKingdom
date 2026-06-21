// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilityElement.h"


UAbilityElement::UAbilityElement(){
    World = GetOuter()->GetWorld();
    SetAbilityLevel(InitialAbilityLevel);
}

void UAbilityElement::PostInitProperties()
{
    Super::PostInitProperties();
    OnBeginPlay();
}

void UAbilityElement::ActivateAbility()
{
    OnActivate(); 
    OnVFXActivate();
}

UWorld* UAbilityElement::GetWorld() const
{
    if(Owner) return Owner->GetWorld();
    if(World) return World;
        UE_LOG(LogTemp, Warning, TEXT("%s::GetWorld() - Owner and Outer have no valid world!") , *GetName());
        return nullptr;
}

void UAbilityElement::DestroyAbilityElement()
{
    if(!IsValid(this)){
        return;
    };
    OnDeactivate();
    if (IsValid(Owner))
    {
        Owner = nullptr;
    };
    MarkAsGarbage();
}

void UAbilityElement::SetAbilityLevel(int32 Level)
{
    AbilityLevel = FMath::Clamp<int32>(Level, 0, MaxAbilityLevel);
    return;
}

bool UAbilityElement::SetCooldownTime(float NewCooldownTime)
{
    if(NewCooldownTime < 0.0f)
    {
        return false;
    };
    CooldownTime = NewCooldownTime;
    return true;
}