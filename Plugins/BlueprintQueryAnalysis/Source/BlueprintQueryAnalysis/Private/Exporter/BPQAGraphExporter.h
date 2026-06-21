#pragma once

#include "BPQAGraphDataModel.h"

class FBPQAGraphExporter
{
public:
	static FString ExportToJson(const FBPQAGraphModel& Graph);
	static FString ExportToMermaid(const FBPQAGraphModel& Graph);
	static FString ExportToDot(const FBPQAGraphModel& Graph);

	static bool SaveGraphAsJson(const FBPQAGraphModel& Graph, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage);
	static bool SaveGraphAsMermaid(const FBPQAGraphModel& Graph, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage);
	static bool SaveGraphAsDot(const FBPQAGraphModel& Graph, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage);
};
