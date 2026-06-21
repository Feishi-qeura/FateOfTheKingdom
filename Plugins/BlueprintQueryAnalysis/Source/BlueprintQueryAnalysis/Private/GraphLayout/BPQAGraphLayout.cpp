#include "BPQAGraphLayout.h"

namespace BPQAGraphLayout
{
	static const FVector2D StaticNodeSize(230.0f, 86.0f);
	static const FVector2D RuntimeNodeSize(360.0f, 76.0f);
}

FVector2D FBPQAGraphLayout::GetNodeSize(EBPQAViewMode Mode, float Zoom)
{
	return (Mode == EBPQAViewMode::RuntimeFlow ? BPQAGraphLayout::RuntimeNodeSize : BPQAGraphLayout::StaticNodeSize) * Zoom;
}

void FBPQAGraphLayout::BuildNodeRects(const FBPQAGraphModel& Graph, float Zoom, const FVector2D& PanOffset, TMap<FString, FSlateRect>& OutRects)
{
	OutRects.Reset();

	TMap<int32, int32> RowsByDepth;
	for (const FBPQAGraphNode& Node : Graph.Nodes)
	{
		const FVector2D NodeSize = GetNodeSize(Graph.Mode, Zoom);
		const int32 Depth = Graph.Mode == EBPQAViewMode::RuntimeFlow ? 0 : FMath::Max(0, Node.Depth);
		int32& Row = RowsByDepth.FindOrAdd(Depth);

		FVector2D BasePosition;
		if (Graph.Mode == EBPQAViewMode::RuntimeFlow)
		{
			BasePosition = FVector2D(64.0f, 24.0f + Row * 106.0f);
		}
		else
		{
			BasePosition = FVector2D(34.0f + Depth * 296.0f, 28.0f + Row * 118.0f);
		}

		const FVector2D Position = BasePosition * Zoom + PanOffset;
		OutRects.Add(Node.Id, FSlateRect(Position.X, Position.Y, Position.X + NodeSize.X, Position.Y + NodeSize.Y));
		++Row;
	}
}
