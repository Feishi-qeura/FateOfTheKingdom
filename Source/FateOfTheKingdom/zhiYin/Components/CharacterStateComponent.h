#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterStateComponent.generated.h"

class UAttributeComponent;
class IGridMapInterface;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FATEOFTHEKINGDOM_API UCharacterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	UCharacterStateComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	
	
	//键盘移动
	UFUNCTION(BlueprintCallable, Category = "CharacterState|Combat")
	void MoveInputWASD(FVector2D Direction);
	
	
	UPROPERTY(Transient)
	UAttributeComponent* OwnerAttributeComp;
	
	UPROPERTY(BlueprintReadOnly, Category = "CharacterState")
	bool bIsMoving;

	


private:
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MoveDistance = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MoveLerpSpeed = 12.0f;
	
	// 寻路路径缓存
	FVector TargetWorldLocation;
	
	bool IsGridMove(FVector2D Direction,FVector& OutTargetLoc) const;
};