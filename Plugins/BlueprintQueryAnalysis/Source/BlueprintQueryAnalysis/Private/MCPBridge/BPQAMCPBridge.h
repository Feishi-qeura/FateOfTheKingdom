#pragma once

#include "BPQAGraphDataModel.h"

class FBPQAMCPBridge
{
public:
	// 可选桥接层：给外部 AI/MCP 读取当前图摘要，插件本身不在这里做推理。
	static FString MakeGraphSummary(const FBPQAGraphModel& Graph);
};
