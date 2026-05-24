// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DialogCondCheckNode.generated.h"

/**
 * 
 */
UCLASS(Abstract,Blueprintable, BlueprintType, Category = "Dialogue System")
class FATEOFTHEKINGDOM_API UDialogCondCheckNode : public UObject
{
	GENERATED_BODY()
	
public:
	UDialogCondCheckNode();

	UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue System", meta = (DisplayName = "开始运行"))
    void OnBeginplay();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Dialogue System", meta = (DisplayName = "执行条件检查"))
	bool ExecuteCondCheck();

	UPROPERTY(VisibleAnywhere , BlueprintReadOnly, Category = "Dialouge System",Meta = (ExposeOnSpawn=true))
	AActor* Owner;

private:
    virtual UWorld* GetWorld() const override;
	UWorld* World;
};
