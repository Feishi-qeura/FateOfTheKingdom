#pragma once

#include "BPQAGraphDataModel.h"
#include "Layout/SlateRect.h"

class FBPQAGraphLayout
{
public:
	static FVector2D GetNodeSize(EBPQAViewMode Mode, float Zoom);
	static void BuildNodeRects(const FBPQAGraphModel& Graph, float Zoom, const FVector2D& PanOffset, TMap<FString, FSlateRect>& OutRects);
};
