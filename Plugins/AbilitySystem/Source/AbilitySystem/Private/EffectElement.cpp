// Fill out your copyright notice in the Description page of Project Settings.


#include "EffectElement.h"

UEffectElement::UEffectElement(){
    World = GetOuter()->GetWorld();
}

void UEffectElement::PostInitProperties()
{
    Super::PostInitProperties();
    OnBeginPlay();
}

void UEffectElement::ActivateEffect()
{
    OnActivate(); 
}

UWorld* UEffectElement::GetWorld() const
{
    if(Owner) return Owner->GetWorld();
    if(World) return World;

        UE_LOG(LogTemp, Warning, TEXT("%s::GetWorld() - Owner and Outer have no valid world!") , *GetName());
        return nullptr;
}

void UEffectElement::DestroyEffectElement()
{
    if(!IsValid(this))
    {
        return;
    };
    OnDestroy(); 
    if (IsValid(Owner))
    {
        Owner = nullptr;
    };
    MarkAsGarbage();
}

// 修改效果层数的默认实现：直接修改EffectStacks变量
void UEffectElement::SetEffectStacks_Implementation(int32 TargetStacks)
{
    EffectStacks = FMath::Clamp(TargetStacks, 0, MaxEffectStacks);
    return;
}
