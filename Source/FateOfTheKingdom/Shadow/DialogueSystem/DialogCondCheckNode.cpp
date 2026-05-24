// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogCondCheckNode.h"

UDialogCondCheckNode::UDialogCondCheckNode()
{
    OnBeginplay();
    
    World = GetOuter()->GetWorld();

}

UWorld *UDialogCondCheckNode::GetWorld() const
{
    if(Owner)
    {
        return Owner->GetWorld();
    }
    else if(World)
    {
        return World;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UDialogCondCheckNode::GetWorld() - Owner and Outer have no valid world!"));
        return nullptr;
    }
    // return Owner ? Owner->GetWorld() : nullptr;
}