#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThirdCameraManager.generated.h"

UCLASS()
class AThirdCameraManager : public AActor
{
	GENERATED_BODY()

public:
	AThirdCameraManager();

	void SetFollowTarget(AActor* Target) { FollowTarget = Target; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	//virtual void SetupPlayerInputComponent(); // 不用这个，滚轮在 PC 里绑

public:
	// 暴露给 PCTest 调用滚轮
	void ZoomIn();
	void ZoomOut();

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomSpeed = 300.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinArmLength = 200.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxArmLength = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float FollowInterpSpeed = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ZoomInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FRotator ArmRotation = FRotator(-45.f, 0.f, 0.f); // 俯仰角

private:
	UPROPERTY(VisibleAnywhere)
	class USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere)
	class UCameraComponent* Camera;

	AActor* FollowTarget = nullptr;

	float TargetArmLength = 800.f;
	float CurrentArmLength = 800.f;
};