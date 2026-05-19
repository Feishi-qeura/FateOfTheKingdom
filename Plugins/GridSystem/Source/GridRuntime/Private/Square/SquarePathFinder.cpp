#include "Square/SquarePathFinder.h"
#include "GridManager.h"
#include "GridData.h"
#include "Grid.h"

int32 USquarePathFinder::GetCost_Implementation(UGrid* From, UGrid* To)
{
    UGridInfo* Info = Cast<UGridInfo>(To->GridInfo);
    if (!Info) return 1;

    FGridData* Data = GridManager->GetGridDataFromTable(Info->TileID);
    if (!Data)
    {
        UE_LOG(LogTemp, Error, TEXT("Data empty"));
    }
    if (!Data) return 1; // 没配表默认代价为1
    return Data->MoveCost;
}


bool USquarePathFinder::FindPath(const FGridPathfindingRequest& InRequest, TArray<UGrid*>& OutPath)
{
    Reset();
    Request = InRequest;
    OutPath.Reset();

    if (!Request.Start || !Request.Destination) return false;

    // 初始化起点节点
    FPathNode StartNode;
    StartNode.Grid = Request.Start;
    StartNode.GCost = 0;
    StartNode.HCost = Heuristic(Request.Start, Request.Destination);
    StartNode.Parent = nullptr;
    NodeMap.Add(Request.Start, StartNode);
    OpenList.Add(Request.Start);

    int32 StepCount = 0;

    while (OpenList.Num() > 0)
    {
        // 超出最大步数限制
        if (Request.MaxSearchStep > 0 && StepCount >= Request.MaxSearchStep)
            break;

        // 取 FCost 最小的节点
        UGrid* CurrentGrid = GetLowestFCostNode();
        FPathNode& CurrentNode = NodeMap.FindChecked(CurrentGrid);

        OpenList.Remove(CurrentGrid);
        ClosedList.Add(CurrentGrid);
        StepCount++;

        // 到达终点
        if (CurrentGrid == Request.Destination)
        {
            BuildPath(CurrentGrid, OutPath);
            return true;
        }

        // 扩展邻居
        TArray<UGrid*> Neighbors;
        CurrentGrid->GetNeighbors(Neighbors);

        for (UGrid* Neighbor : Neighbors)
        {
            if (!Neighbor) continue;
            if (ClosedList.Contains(Neighbor)) continue;
            if (!IsReachable(CurrentGrid, Neighbor)) continue;

            int32 NewGCost = CurrentNode.GCost + GetCost(CurrentGrid, Neighbor);

            // 超出最大代价（移动力）限制
            if (Request.MaxCost > 0 && NewGCost > Request.MaxCost)
                continue;

            FPathNode* ExistingNode = NodeMap.Find(Neighbor);

            if (!ExistingNode)
            {
                // 新节点，加入 OpenList
                FPathNode NewNode;
                NewNode.Grid = Neighbor;
                NewNode.GCost = NewGCost;
                NewNode.HCost = Heuristic(Neighbor, Request.Destination);
                NewNode.Parent = CurrentGrid;
                NodeMap.Add(Neighbor, NewNode);
                OpenList.Add(Neighbor);
            }
            else if (NewGCost < ExistingNode->GCost)
            {
                // 找到更短路径，更新节点
                ExistingNode->GCost = NewGCost;
                ExistingNode->Parent = CurrentGrid;
            }
        }
    }

    return false; // 没找到路径
}

void USquarePathFinder::FindReachableGrids(const FGridPathfindingRequest& InRequest, TArray<UGrid*>& OutReachable)
{
    Reset();
    Request = InRequest;
    OutReachable.Reset();

    if (!Request.Start) return;

    FPathNode StartNode;
    StartNode.Grid = Request.Start;
    StartNode.GCost = 0;
    StartNode.Parent = nullptr;
    NodeMap.Add(Request.Start, StartNode);
    OpenList.Add(Request.Start);

    int32 StepCount = 0;

    while (OpenList.Num() > 0)
    {
        if (Request.MaxSearchStep > 0 && StepCount >= Request.MaxSearchStep)
            break;

        UGrid* CurrentGrid = GetLowestFCostNode();
        FPathNode& CurrentNode = NodeMap.FindChecked(CurrentGrid);

        OpenList.Remove(CurrentGrid);
        ClosedList.Add(CurrentGrid);
        OutReachable.Add(CurrentGrid); // 所有关闭节点都是可达的
        StepCount++;

        TArray<UGrid*> Neighbors;
        CurrentGrid->GetNeighbors(Neighbors);

        for (UGrid* Neighbor : Neighbors)
        {
            if (!Neighbor) continue;
            if (ClosedList.Contains(Neighbor)) continue;
            if (!IsReachable(CurrentGrid, Neighbor)) continue;

            int32 NewGCost = CurrentNode.GCost + GetCost(CurrentGrid, Neighbor);

            if (Request.MaxCost > 0 && NewGCost > Request.MaxCost)
                continue;

            FPathNode* ExistingNode = NodeMap.Find(Neighbor);
            if (!ExistingNode)
            {
                FPathNode NewNode;
                NewNode.Grid = Neighbor;
                NewNode.GCost = NewGCost;
                NewNode.HCost = 0; // 范围查找不需要 Heuristic
                NewNode.Parent = CurrentGrid;
                NodeMap.Add(Neighbor, NewNode);
                OpenList.Add(Neighbor);
            }
            else if (NewGCost < ExistingNode->GCost)
            {
                ExistingNode->GCost = NewGCost;
                ExistingNode->Parent = CurrentGrid;
            }
        }
    }
}

UGrid* USquarePathFinder::GetLowestFCostNode()
{
    UGrid* Best = nullptr;
    int32 BestFCost = TNumericLimits<int32>::Max();

    for (UGrid* Grid : OpenList)
    {
        FPathNode& Node = NodeMap.FindChecked(Grid);
        if (Node.FCost() < BestFCost)
        {
            BestFCost = Node.FCost();
            Best = Grid;
        }
    }
    return Best;
}

void USquarePathFinder::BuildPath(UGrid* EndGrid, TArray<UGrid*>& OutPath)
{
    UGrid* Current = EndGrid;
    while (Current != nullptr)
    {
        OutPath.Insert(Current, 0); // 反向插入，得到从起点到终点的顺序
        FPathNode& Node = NodeMap.FindChecked(Current);
        Current = Node.Parent;
    }
}

void USquarePathFinder::Reset_Implementation()
{
    NodeMap.Reset();
    OpenList.Reset();
    ClosedList.Reset();
}