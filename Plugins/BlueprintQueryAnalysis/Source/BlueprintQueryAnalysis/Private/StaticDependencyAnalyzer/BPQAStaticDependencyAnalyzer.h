#pragma once

#include "BPQAGraphDataModel.h"

class FBPQAStaticDependencyAnalyzer
{
public:
	// 扫描 /Game 下用户内容文件夹，生成“文件夹 -> 文件夹”的依赖总览图。
	bool AnalyzeProject(FBPQAGraphModel& OutModel, FString* OutErrorMessage = nullptr) const;

	// 进入某个 /Game 子文件夹，显示该文件夹内资产以及跨文件夹依赖。
	bool AnalyzeFolder(const FString& FolderPath, FBPQAGraphModel& OutModel, FString* OutErrorMessage = nullptr) const;
};
