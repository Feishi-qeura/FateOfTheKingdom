#pragma once

#include "CoreMinimal.h"
#include "GridPathfindingParams.h"
#include "SquarePathFinder.generated.h"

USTRUCT()
struct FPathNode
{
	GENERATED_BODY()

	UGrid* Grid = nullptr;
	int32 GCost = 0;
	int32 HCost = 0;
	UGrid* Parent = nullptr;

	int32 FCost() const { return GCost + HCost; }
};

UCLASS()
class GRIDRUNTIME_API USquarePathFinder : public UGridPathFinder
{
	GENERATED_BODY()

public:
	USquarePathFinder() {};
	virtual ~USquarePathFinder() {};

	virtual int32 GetCost_Implementation(UGrid* From, UGrid* To) override;
	virtual bool IsReachable_Implementation(UGrid* Start, UGrid* Dest) override;
	// 寻路，返回起点到终点的路径
	bool FindPath(const FGridPathfindingRequest& InRequest, TArray<UGrid*>& OutPath);

	// 获取所有可达格子（移动范围）
	void FindReachableGrids(const FGridPathfindingRequest& InRequest, TArray<UGrid*>& OutReachable);

	virtual void Reset_Implementation() override;
private:
	TMap<UGrid*, FPathNode> NodeMap;  // 所有已访问节点
	TArray<UGrid*> OpenList;
	TSet<UGrid*> ClosedList;

	UGrid* GetLowestFCostNode();
	void BuildPath(UGrid* EndGrid, TArray<UGrid*>& OutPath);
};
