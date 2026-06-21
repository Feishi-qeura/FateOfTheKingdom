#include "BPQAMCPBridge.h"

FString FBPQAMCPBridge::MakeGraphSummary(const FBPQAGraphModel& Graph)
{
	return FString::Printf(
		TEXT("%s: %d nodes, %d edges"),
		*Graph.Title,
		Graph.Nodes.Num(),
		Graph.Edges.Num());
}
