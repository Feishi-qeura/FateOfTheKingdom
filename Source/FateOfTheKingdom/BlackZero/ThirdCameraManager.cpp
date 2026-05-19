#include "ThirdCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

AThirdCameraManager::AThirdCameraManager()
{
	PrimaryActorTick.bCanEverTick = true;

	// 根组件（弹簧臂需要挂在某个根上）
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = TargetArmLength;
	SpringArm->SetRelativeRotation(ArmRotation);
	SpringArm->bDoCollisionTest = false;        // 碰墙自动收臂
	SpringArm->bEnableCameraLag = false;       // 位置 lag 我们自己做 interp
	SpringArm->bEnableCameraRotationLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void AThirdCameraManager::BeginPlay()
{
	Super::BeginPlay();
	CurrentArmLength = TargetArmLength;
	SpringArm->TargetArmLength = CurrentArmLength;
	SpringArm->SetRelativeRotation(ArmRotation);
}

void AThirdCameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// === 1. 跟随目标位置（带插值，有重量感）===
	if (FollowTarget)
	{
		FVector TargetLocation = FollowTarget->GetActorLocation();
		FVector NewLocation = FMath::VInterpTo(
			GetActorLocation(), TargetLocation, DeltaTime, FollowInterpSpeed);
		SetActorLocation(NewLocation);
	}

	// === 2. 平滑缩放插值 ===
	CurrentArmLength = FMath::FInterpTo(
		CurrentArmLength, TargetArmLength, DeltaTime, ZoomInterpSpeed);
	SpringArm->TargetArmLength = CurrentArmLength;
}

void AThirdCameraManager::ZoomIn()
{
	TargetArmLength = FMath::Clamp(
		TargetArmLength - ZoomSpeed, MinArmLength, MaxArmLength);
}

void AThirdCameraManager::ZoomOut()
{
	TargetArmLength = FMath::Clamp(
		TargetArmLength + ZoomSpeed, MinArmLength, MaxArmLength);
}