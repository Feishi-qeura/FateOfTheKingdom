#pragma once

#include "BPQAGraphDataModel.h"

class UBlueprint;

class FBPQABlueprintNodeAnalyzer
{
public:
	// 后续扩展入口：分析 UbergraphPages / FunctionGraphs / MacroGraphs 中的 UK2Node 调用关系。
	bool AnalyzeBlueprint(UBlueprint* Blueprint, FBPQAGraphModel& OutModel, FString* OutErrorMessage = nullptr) const;
};
