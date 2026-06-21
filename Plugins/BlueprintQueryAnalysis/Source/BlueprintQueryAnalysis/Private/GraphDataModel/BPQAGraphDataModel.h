#pragma once

#include "CoreMinimal.h"

// 这个文件只放轻量数据结构，不依赖 UObject。
// 目的：静态分析、运行采集、Slate 绘制、JSON/Mermaid 导出都使用同一份图数据。

enum class EBPQAViewMode : uint8
{
	StaticDependency,
	FolderDependency,
	FolderAssetDependency,
	RuntimeFlow,
	NodeFlow
};

enum class EBPQAFlowEventType : uint8
{
	PreLoadMap,
	PostLoadMap,
	PackageLoaded,
	AssetLoaded,
	ObjectConstructed,
	WorldInitialized,
	ActorAdded,
	WorldBeginPlay,
	BeginPIE,
	EndPIE
};

struct FBPQAFlowEvent
{
	double TimeSeconds = 0.0;
	EBPQAFlowEventType Type = EBPQAFlowEventType::ObjectConstructed;
	FString ObjectName;
	FString ClassName;
	FString ClassPackageName;
	FString PackageName;
	FString OuterName;
	FString WorldName;
};

struct FBPQAGraphNode
{
	FString Id;
	FString Label;
	FString Type;
	FString AssetPath;
	FString ClassName;
	FString ParentClass;
	FString PackageName;
	FString FolderPath;
	FString Tooltip;
	double TimeSeconds = 0.0;
	int32 Depth = 0;
	bool bIsBlueprint = false;

	TArray<FString> References;
	TArray<FString> ReferencedBy;
	TArray<FString> Interfaces;
	TArray<FString> Components;
};

struct FBPQAGraphEdge
{
	FString From;
	FString To;
	FString Reason;
	FString Type;
	double TimeSeconds = 0.0;
	FString MotionShape;
	double MotionDuration = 0.0;
};

struct FBPQAGraphModel
{
	FString Title;
	EBPQAViewMode Mode = EBPQAViewMode::StaticDependency;
	TArray<FBPQAGraphNode> Nodes;
	TArray<FBPQAGraphEdge> Edges;

	void Reset(const FString& InTitle, EBPQAViewMode InMode)
	{
		Title = InTitle;
		Mode = InMode;
		Nodes.Reset();
		Edges.Reset();
	}

	FBPQAGraphNode* FindNodeById(const FString& Id)
	{
		return Nodes.FindByPredicate([&Id](const FBPQAGraphNode& Node)
		{
			return Node.Id == Id;
		});
	}

	const FBPQAGraphNode* FindNodeById(const FString& Id) const
	{
		return Nodes.FindByPredicate([&Id](const FBPQAGraphNode& Node)
		{
			return Node.Id == Id;
		});
	}

	FBPQAGraphNode& AddOrUpdateNode(const FBPQAGraphNode& InNode)
	{
		if (FBPQAGraphNode* ExistingNode = FindNodeById(InNode.Id))
		{
			// 同一个节点可能先作为依赖占位出现，后续又读到完整 AssetData。
			MergeNode(*ExistingNode, InNode);
			return *ExistingNode;
		}

		return Nodes.Add_GetRef(InNode);
	}

	void AddEdgeUnique(const FBPQAGraphEdge& InEdge)
	{
		if (InEdge.From.IsEmpty() || InEdge.To.IsEmpty() || InEdge.From == InEdge.To)
		{
			return;
		}

		const bool bAlreadyExists = Edges.ContainsByPredicate([&InEdge](const FBPQAGraphEdge& Edge)
		{
			return Edge.From == InEdge.From && Edge.To == InEdge.To && Edge.Reason == InEdge.Reason && Edge.Type == InEdge.Type;
		});

		if (!bAlreadyExists)
		{
			Edges.Add(InEdge);
		}
	}

	static void AddUniqueString(TArray<FString>& Target, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Target.AddUnique(Value);
		}
	}

private:
	static void MergeString(FString& Target, const FString& Value)
	{
		if (Target.IsEmpty() && !Value.IsEmpty())
		{
			Target = Value;
		}
	}

	static void MergeStringArray(TArray<FString>& Target, const TArray<FString>& Values)
	{
		for (const FString& Value : Values)
		{
			AddUniqueString(Target, Value);
		}
	}

	static void MergeNode(FBPQAGraphNode& Target, const FBPQAGraphNode& Source)
	{
		MergeString(Target.Label, Source.Label);
		MergeString(Target.Type, Source.Type);
		MergeString(Target.AssetPath, Source.AssetPath);
		MergeString(Target.ClassName, Source.ClassName);
		MergeString(Target.ParentClass, Source.ParentClass);
		MergeString(Target.PackageName, Source.PackageName);
		MergeString(Target.FolderPath, Source.FolderPath);
		MergeString(Target.Tooltip, Source.Tooltip);
		Target.TimeSeconds = Target.TimeSeconds == 0.0 ? Source.TimeSeconds : Target.TimeSeconds;
		Target.Depth = FMath::Max(Target.Depth, Source.Depth);
		Target.bIsBlueprint = Target.bIsBlueprint || Source.bIsBlueprint;
		MergeStringArray(Target.References, Source.References);
		MergeStringArray(Target.ReferencedBy, Source.ReferencedBy);
		MergeStringArray(Target.Interfaces, Source.Interfaces);
		MergeStringArray(Target.Components, Source.Components);
	}
};

const TCHAR* LexToString(EBPQAFlowEventType Type);
