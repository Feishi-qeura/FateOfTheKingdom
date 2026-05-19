#include "TestGameMode.h"
#include "TestUnit.h"
#include "Landscape.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "GridSubsystem.h"
#include "FlecsSubsystem.h"

ATestGameMode::ATestGameMode()
{
	PlayerControllerClass = ATestUnit::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ATestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	InitGridSystem();
	// 在这里执行最早的逻辑，甚至在 Actor 的 BeginPlay 之前
	UE_LOG(LogTemp, Log, TEXT("Level Lifecycle: 系统初始化中..."));
}
void ATestGameMode::BeginPlay()
{
	Super::BeginPlay();
	// 此时所有关卡中的 Actor 已经 BeginPlay，可以进行交互
	UE_LOG(LogTemp, Log, TEXT("Level Lifecycle: 游戏逻辑正式开始"));
}

void ATestGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 可以在这里统一驱动不需要独立 Tick 的子系统
}

void ATestGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 关卡切换或退出时执行清理
    
	Super::EndPlay(EndPlayReason);
}
void ATestGameMode::InitGridSystem()
{
	UGridSubsystem* GridSys = GetWorld()->GetSubsystem<UGridSubsystem>();
	GridSys->InitGridManager(GridDataTable, 100.0f);
	
	FBox MapBox(ForceInit);
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		AStaticMeshActor* MeshActor = *It;
		// 建议：给你的底图 Actor 加一个 Tag，比如 "Floor"
		if (MeshActor && MeshActor->ActorHasTag(FName("Floor")))
		{
			// 计算这个 Actor 在世界空间占用的所有组件的包围盒
			MapBox += MeshActor->GetComponentsBoundingBox(true);
		}
	}
	FBox map1(FVector(-2000, -2000, -100), FVector(2000, 2000, -100));
	GridSys->InitMapGrid(map1);
	/*if (MapBox.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("Map Min (LeftBottom): %s"), *MapBox.Min.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Map Max (RightTop): %s"), *MapBox.Max.ToString());
		FBox map(MapBox.Min, MapBox.Max);
		GridSys->InitMapGrid(map);
	}*/
}
FVector LeftBottom(0,0,0);
FVector RightTop(0,0,0);
void ATestGameMode::GetLandscape()
{
	// 1. 初始化一个空的 Box
	FBox WorldBounds(ForceInit);

	// 2. 遍历场景中所有的 Landscape
	for (TActorIterator<ALandscape> It(GetWorld()); It; ++It)
	{
		ALandscape* L = *It;
		if (L)
		{
			// 累加每个地形块的包围盒
			WorldBounds += L->GetComponentsBoundingBox(true);
		}
	}

	// 3. 检查是否找到了地形
	if (WorldBounds.IsValid)
	{
		LeftBottom = WorldBounds.Min; // 物理坐标最小值 (X最小, Y最小)
		RightTop = WorldBounds.Max;    // 物理坐标最大值 (X最大, Y最大)

		// 打印结果到 Log，方便你调试确认
		UE_LOG(LogTemp, Warning, TEXT("Map Min (LeftBottom): %s"), *LeftBottom.ToString());
		UE_LOG(LogTemp, Warning, TEXT("Map Max (RightTop): %s"), *RightTop.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("未能在场景中找到任何 Landscape!"));
	}
}
