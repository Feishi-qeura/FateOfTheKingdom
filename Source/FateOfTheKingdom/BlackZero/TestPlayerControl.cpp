#include "TestPlayerControl.h"

#include "ThirdCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "TestUnit.h"
#include "GridRuntime/Public/GridSubsystem.h"

void ATestPlayerControl::BeginPlay()
{
	Super::BeginPlay();
	// 显示鼠标指针
	bShowMouseCursor = true;
    
	// 启用鼠标点击事件的回调
	bEnableClickEvents = true; 
    
	// 启用鼠标悬停事件（如果需要的话）
	bEnableMouseOverEvents = true;

	// 设置输入模式：不仅可以点游戏世界，也可以点 UI
	FInputModeGameAndUI InputMode;
	SetInputMode(InputMode);
	GridSubsystem = GetWorld()->GetSubsystem<UGridSubsystem>();
	UE_LOG(LogTemp, Log, TEXT("PCTest Init OK"));
}
void ATestPlayerControl::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn); // 必须调用父类，它负责内部指针赋值

	Pawn = Cast<ATestUnit>(GetPawn());
	if (Pawn == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Pawn empty"));
	}
	SetupCamera();
}
void ATestPlayerControl::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// 检查鼠标左键是否刚刚按下
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		FHitResult HitResult;
		// 获取鼠标射线碰撞到的信息（寻找常规可见物体）
		bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

		if (bHit)
		{
			AActor* ClickedActor = HitResult.GetActor();
			FVector ClickLocation = HitResult.Location;
			FVector moveTarget = FVector(ClickLocation.X, ClickLocation.Y, 0);

			FIntVector coord = GridSubsystem->GetCoordByPosition(ClickLocation);
			Pawn->CommandMoveTo(moveTarget);
			UE_LOG(LogTemp, Warning, TEXT("点击:(%.1f, %.1f);格子:(%d, %d)"),
				ClickLocation.X, ClickLocation.Y, coord.X, coord.Y);
			// 在这里处理你的意图逻辑
			/*UE_LOG(LogTemp, Warning, TEXT("点击了物体: %s, 坐标: %s"), 
				*ClickedActor->GetName(), *ClickLocation.ToString());*/
		}
	}
}
void ATestPlayerControl::SetupInputComponent()
{
	Super::SetupInputComponent();
	//InputComponent->BindAction("Click", IE_Pressed, this, &APCTest::OnMouseClick);
	InputComponent->BindAction("ZoomIn",  IE_Pressed, this, &ATestPlayerControl::OnZoomIn);
	InputComponent->BindAction("ZoomOut", IE_Pressed, this, &ATestPlayerControl::OnZoomOut);
}

void ATestPlayerControl::OnMouseClick()
{
	if (!GridSubsystem->GridManager) return;

	FHitResult HitResult;
	GetHitResultUnderCursor(ECC_WorldStatic, false, HitResult);

	if (HitResult.bBlockingHit)
	{
		/*UGrid* ClickedGrid = GridSubsystem->GridManager->GetGridByPosition(HitResult.ImpactPoint);
		if (ClickedGrid)
		{
			FIntVector Coord = ClickedGrid->GetCoord();
			UE_LOG(LogTemp, Warning, TEXT("点击坐标: %f, %f;格子: X=%d, Y=%d"),
				HitResult.ImpactPoint.X, HitResult.ImpactPoint.Y, Coord.X, Coord.Y);
		}*/
	}
}
void ATestPlayerControl::SetupCamera() 
{
	FActorSpawnParameters Params;
	MyCam = GetWorld()->SpawnActor<AThirdCameraManager>(
		AThirdCameraManager::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		Params);

	MyCam->SetFollowTarget(Pawn);
	SetViewTarget(MyCam);
}

void ATestPlayerControl::OnZoomIn()
{
	if (MyCam) MyCam->ZoomIn();
}

void ATestPlayerControl::OnZoomOut()
{
	if (MyCam) MyCam->ZoomOut();
}