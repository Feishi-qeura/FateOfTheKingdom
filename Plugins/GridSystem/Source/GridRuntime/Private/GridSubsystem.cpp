#include "GridSubsystem.h"

#include "SquareGridManager.h"
#include "GridDecalPainter.h"
#include "GridOutlinePainter.h"
#include "SquarePathFinder.h"

// ============================================================
//  Initialize
//  关卡开始时自动调用，在这里完成：
//    1. 注册所有 Component 类型（给 Flecs Explorer 可读名称）
//    2. 注册所有 System
//    3. 挂载 UE Tick
// ============================================================
void UGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FActorSpawnParameters Params;
	GridManager = GetWorld()->SpawnActor<ASquareGridManager>(
		ASquareGridManager::StaticClass(), 
		FVector::ZeroVector, 
		FRotator::ZeroRotator, 
		Params
	);
	UE_LOG(LogTemp, Log, TEXT("[UGridSubsystem] Initialized."));
}

// ============================================================
//  Deinitialize
//  关卡结束时自动调用，清理 Ticker
// ============================================================
void UGridSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[UGridSubsystem] Deinitialized."));

	Super::Deinitialize();
}

// ============================================================
//  Tick
//  每帧由 UE Ticker 调用
//  world.progress(DeltaTime) 会按 Pipeline 顺序执行所有 System
// ============================================================
void UGridSubsystem::Tick(float DeltaTime)
{
}
void UGridSubsystem::InitGridManager(
	UDataTable* gridDataTable, 
	float gridSize,
	FVector originPos)
{
	if (gridDataTable == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("GridDataTable Empty"));
		return;
	}
	if (GridManager)
	{
		GridManager->SetGridDataTable(gridDataTable);
		GridManager->SetGridSize(gridSize); // 必须设置，不能为 0
		GridManager->GridOrigin = originPos;
		GridManager->GridPainterClass = UGridOutlinePainter::StaticClass();
		GridManager->PostInitializeComponents();
		
		SquarePathFinder = Cast<USquarePathFinder>(GridManager->GetPathFinder());
		UE_LOG(LogTemp, Log, TEXT("GridManager Spawn 成功"));
	}
	
}

TArray<FVector> UGridSubsystem::DoPathFind(AActor* actor, FVector endPos, int32 MaxCost)
{
	FGridPathfindingRequest Request;
	Request.Sender = actor;
	Request.Start = GridManager->GetGridByPosition(actor->GetActorLocation());
	Request.Destination = GridManager->GetGridByPosition(endPos);
	Request.MaxCost = MaxCost;      // -1 表示不限制移动力
	Request.MaxSearchStep = 1000;
	TArray<UGrid*> GridList;
	bool bFound = SquarePathFinder->FindPath(Request, GridList);
	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("PathFind Failed"));
		return TArray<FVector>();
	}
	TArray<FVector> path;
	for (UGrid* Grid : GridList)
	{
		if (Grid)
			path.Add(FVector(Grid->GetCenter().X, Grid->GetCenter().Y, 0));
	}
	return path;
}

void UGridSubsystem::TestPathFinder()
{
	UGrid* StartGrid = GridManager->GetGridByPosition(FVector(0, 0, 0));
	UGrid* EndGrid = GridManager->GetGridByPosition(FVector(300, 300, 0));
	
	FActorSpawnParameters Params;
	AActor* testActor  = GetWorld()->SpawnActor<AActor>(
		AActor::StaticClass(), 
		FVector::ZeroVector, 
		FRotator::ZeroRotator, 
		Params
	);
	FGridPathfindingRequest Request;
	Request.Sender = testActor;
	Request.Start = StartGrid;
	Request.Destination = EndGrid;
	Request.MaxCost = -1;      // -1 表示不限制移动力
	Request.MaxSearchStep = 1000;
	TArray<UGrid*> Path;
	UE_LOG(LogTemp, Warning, TEXT("TestPathFinder"));
	bool bFound = SquarePathFinder->FindPath(Request, Path);
	if (bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("找到路径，共 %d 步"), Path.Num());
		for (UGrid* Grid : Path)
		{
			FIntVector Coord = Grid->GetCoord();
			UE_LOG(LogTemp, Warning, TEXT("  → (%d, %d)"), Coord.X, Coord.Y);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("没找到路径"));
	}
}
void UGridSubsystem::InitMapGrid(FBox MapBound)
{
	TArray<UGrid*> GridsToShow;
	GridManager->GetGridsByBound(MapBound, GridsToShow);
	//GridManager->GetGridsByRange(CenterGrid, 10, GridsToShow);
	UE_LOG(LogTemp, Log, TEXT("根据坐标初始化的格子数量: %d"), GridsToShow.Num());
	TArray<FName> TileIDs = { TEXT("Grid0"), TEXT("Grid1")};
	GridManager->RandomizeTileIDs(GridsToShow, TileIDs);
	TestPathFinder();
	int i = 0;
	for (UGrid* Grid : GridsToShow)
	{
		Grid->SetVisibility(true);
		//Grid->GridInfo->HitResult.bBlockingHit = true; // TODO:这玩意是检测地面的，测试阶段写死
		DrawGridBorders(Grid, GridManager->GetGridSize(), 10.0f);
		//UE_LOG(LogTemp, Log, TEXT("这格子 %d: %s %s"),i++, *Grid->GetCoord().ToString(), *Grid->GridInfo->TileID.ToString());
	}
}

void UGridSubsystem::DrawGridBorders(UGrid* Grid, float GridSize, float ZOffset)
{
	FVector Center = Grid->GetCenter();
	float Half = GridSize / 2.f - 3.f;
	FColor Color = FColor::Green;
	float Duration = -1.f; // 永久显示
	FGridData* Data = GridManager->GetGridDataFromTable(Grid->GridInfo->TileID);
	if ( Data != nullptr && Data->MoveCost == 9999)
	{
		Color = FColor::Red;
	}
	DrawDebugLine(GetWorld(),
		Center + FVector(-Half, -Half, ZOffset),
		Center + FVector(+Half, -Half, ZOffset),
		Color, true, Duration, 0, 2.f);

	DrawDebugLine(GetWorld(),
		Center + FVector(+Half, -Half, ZOffset),
		Center + FVector(+Half, +Half, ZOffset),
		Color, true, Duration, 0, 2.f);

	DrawDebugLine(GetWorld(),
		Center + FVector(+Half, +Half, ZOffset),
		Center + FVector(-Half, +Half, ZOffset),
		Color, true, Duration, 0, 2.f);

	DrawDebugLine(GetWorld(),
		Center + FVector(-Half, +Half, ZOffset),
		Center + FVector(-Half, -Half, ZOffset),
		Color, true, Duration, 0, 2.f);
}
FIntVector UGridSubsystem::GetCoordByPosition(const FVector& Position)
{
	return GridManager->GetGridByPosition(Position)->GetCoord();
}