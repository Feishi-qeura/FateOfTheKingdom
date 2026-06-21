#include "BPQAStaticDependencyAnalyzer.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/PackageName.h"

#include <initializer_list>

namespace BPQAStatic
{
	static const FName ParentClassTag(TEXT("ParentClass"));
	static const FName GeneratedClassTag(TEXT("GeneratedClass"));
	static const FName ImplementedInterfacesTag(TEXT("ImplementedInterfaces"));
	static const FName NativeParentClassTag(TEXT("NativeParentClass"));

	static bool IsGamePackage(FName PackageName)
	{
		return PackageName.ToString().StartsWith(TEXT("/Game/"));
	}

	static FString PackageToNodeId(FName PackageName)
	{
		return PackageName.ToString();
	}

	static FString FolderToNodeId(const FString& FolderPath)
	{
		return FString::Printf(TEXT("Folder:%s"), *FolderPath);
	}

	static FString NormalizeFolderPath(FString FolderPath)
	{
		FolderPath.TrimStartAndEndInline();
		FolderPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		FolderPath.RemoveFromEnd(TEXT("/"));

		if (FolderPath == TEXT("/All/Game"))
		{
			return TEXT("/Game");
		}
		if (FolderPath.StartsWith(TEXT("/All/Game/")))
		{
			FolderPath = TEXT("/Game/") + FolderPath.RightChop(10);
		}
		if (FolderPath.IsEmpty())
		{
			return TEXT("/Game");
		}
		return FolderPath;
	}

	static bool IsUnderFolder(const FString& PackagePath, const FString& FolderPath)
	{
		const FString NormalizedPackagePath = NormalizeFolderPath(PackagePath);
		const FString NormalizedFolderPath = NormalizeFolderPath(FolderPath);
		return NormalizedPackagePath == NormalizedFolderPath || NormalizedPackagePath.StartsWith(NormalizedFolderPath + TEXT("/"));
	}

	static FString GetImmediateChildFolderForPackagePath(const FString& PackagePath, const FString& ParentFolderPath);

	static FString GetFolderAtDepth(const FString& PackagePath, int32 Depth)
	{
		const FString NormalizedPath = NormalizeFolderPath(PackagePath);
		TArray<FString> Parts;
		NormalizedPath.ParseIntoArray(Parts, TEXT("/"), true);

	// /Game/A/B becomes Game, A, B after ParseIntoArray.
if (Parts.Num() <= 1)
		{
			return TEXT("/Game");
		}

		const int32 LastIndex = FMath::Clamp(Depth, 1, Parts.Num() - 1);
		FString FolderPath = TEXT("/Game");
		for (int32 Index = 1; Index <= LastIndex; ++Index)
		{
			FolderPath += TEXT("/") + Parts[Index];
		}
		return FolderPath;
	}

	static FString GetOverviewFolderForPackagePath(const FString& PackagePath)
	{
		return GetFolderAtDepth(PackagePath, 1);
	}

	static FString DetermineOverviewRootFolder(const TArray<FAssetData>& Assets)
	{
		TSet<FString> TopLevelFolders;
		for (const FAssetData& AssetData : Assets)
		{
			TopLevelFolders.Add(GetOverviewFolderForPackagePath(AssetData.PackagePath.ToString()));
		}

		if (TopLevelFolders.Num() == 1)
		{
			for (const FString& FolderPath : TopLevelFolders)
			{
				return FolderPath;
			}
		}

		return TEXT("/Game");
	}

	static FString GetOverviewChildFolderForPackagePath(const FString& PackagePath, const FString& OverviewRootFolder)
	{
		const FString ChildFolder = GetImmediateChildFolderForPackagePath(PackagePath, OverviewRootFolder);
		return ChildFolder.IsEmpty() ? NormalizeFolderPath(OverviewRootFolder) : ChildFolder;
	}

	static FString GetFolderLabel(const FString& FolderPath)
	{
		if (FolderPath == TEXT("/Game"))
		{
			return TEXT("Game");
		}
		return FPackageName::GetShortName(FolderPath);
	}

	static int32 GetFolderDepth(const FString& FolderPath)
	{
		TArray<FString> Parts;
		NormalizeFolderPath(FolderPath).ParseIntoArray(Parts, TEXT("/"), true);
		return FMath::Max(0, Parts.Num() - 1);
	}

	static FString GetParentFolderPath(const FString& FolderPath)
	{
		const FString NormalizedFolderPath = NormalizeFolderPath(FolderPath);
		if (NormalizedFolderPath == TEXT("/Game"))
		{
			return FString();
		}

		const int32 SlashIndex = NormalizedFolderPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		return SlashIndex > 0 ? NormalizedFolderPath.Left(SlashIndex) : TEXT("/Game");
	}

	static FString MakeClassNodeId(const FString& ClassPath)
	{
		return FString::Printf(TEXT("Class:%s"), *ClassPath);
	}

	static FString CleanExportPath(FString Value)
	{
		Value.TrimStartAndEndInline();
		Value.RemoveFromStart(TEXT("Class'"));
		Value.RemoveFromStart(TEXT("BlueprintGeneratedClass'"));
		Value.RemoveFromStart(TEXT("WidgetBlueprintGeneratedClass'"));
		Value.RemoveFromStart(TEXT("AnimBlueprintGeneratedClass'"));
		Value.RemoveFromEnd(TEXT("'"));
		Value.ReplaceInline(TEXT("\""), TEXT(""));
		return Value;
	}

	static FString MakeReadableLabelFromPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return TEXT("Unknown");
		}

		const FString CleanPath = CleanExportPath(Path);
		const FString PackageName = FPackageName::ObjectPathToPackageName(CleanPath);
		const FString ShortName = PackageName.IsEmpty() ? FPackageName::GetShortName(CleanPath) : FPackageName::GetShortName(PackageName);
		return ShortName.IsEmpty() ? CleanPath : ShortName;
	}

	static bool IsBlueprintLikeAsset(const FAssetData& AssetData)
	{
		const FString AssetClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		if (AssetClassName.Contains(TEXT("Blueprint")))
		{
			return true;
		}

	// Some blueprint-derived assets have vague class names; GeneratedClass is a safer fallback.
		FString GeneratedClass;
		return AssetData.GetTagValue(GeneratedClassTag, GeneratedClass) && !GeneratedClass.IsEmpty();
	}

	static FString DetectNodeType(const FAssetData& AssetData)
	{
		const FString AssetClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		return AssetClassName.IsEmpty() ? TEXT("Asset") : AssetClassName;
	}

	static FString FirstAvailableTag(const FAssetData& AssetData, std::initializer_list<FName> Tags)
	{
		for (const FName Tag : Tags)
		{
			FString Value;
			if (AssetData.GetTagValue(Tag, Value) && !Value.IsEmpty())
			{
				return CleanExportPath(Value);
			}
		}
		return FString();
	}

	static TArray<FString> ExtractQuotedPaths(const FString& RawValue)
	{
		TArray<FString> Result;

		int32 SearchStart = 0;
		while (SearchStart < RawValue.Len())
		{
			const int32 QuoteStart = RawValue.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStart);
			if (QuoteStart == INDEX_NONE)
			{
				break;
			}

			const int32 QuoteEnd = RawValue.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, QuoteStart + 1);
			if (QuoteEnd == INDEX_NONE)
			{
				break;
			}

			FString Path = RawValue.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
			Path.ReplaceInline(TEXT("\""), TEXT(""));
			Path.TrimStartAndEndInline();
			if (!Path.IsEmpty())
			{
				Result.AddUnique(Path);
			}

			SearchStart = QuoteEnd + 1;
		}

		if (Result.Num() == 0 && !RawValue.IsEmpty())
		{
			TArray<FString> Chunks;
			RawValue.ParseIntoArray(Chunks, TEXT(","), true);
			for (FString& Chunk : Chunks)
			{
				Chunk = CleanExportPath(Chunk);
				Chunk.TrimStartAndEndInline();
				if (!Chunk.IsEmpty())
				{
					Result.AddUnique(Chunk);
				}
			}
		}

		return Result;
	}

	static void AddComponentTagSummary(const FAssetData& AssetData, FBPQAGraphNode& Node)
	{
		AssetData.EnumerateTags([&Node](const TPair<FName, FAssetTagValueRef>& Pair)
		{
			const FString Key = Pair.Key.ToString();
			if (!Key.Contains(TEXT("Component")))
			{
				return;
			}

			FString Value = Pair.Value.AsString();
			if (Value.Len() > 160)
			{
				Value = Value.Left(160) + TEXT("...");
			}
			FBPQAGraphModel::AddUniqueString(Node.Components, FString::Printf(TEXT("%s = %s"), *Key, *Value));
		});
	}

	static FBPQAGraphNode MakeNodeFromAssetData(const FAssetData& AssetData, bool bIsBlueprint)
	{
		FBPQAGraphNode Node;
		Node.Id = PackageToNodeId(AssetData.PackageName);
		Node.Label = AssetData.AssetName.ToString();
		Node.Type = DetectNodeType(AssetData);
		Node.AssetPath = AssetData.GetObjectPathString();
		Node.PackageName = AssetData.PackageName.ToString();
		Node.FolderPath = AssetData.PackagePath.ToString();
		Node.bIsBlueprint = bIsBlueprint;

		Node.ClassName = FirstAvailableTag(AssetData, { GeneratedClassTag });
		Node.ParentClass = FirstAvailableTag(AssetData, { ParentClassTag, NativeParentClassTag });

		FString RawInterfaces;
		if (AssetData.GetTagValue(ImplementedInterfacesTag, RawInterfaces))
		{
			Node.Interfaces = ExtractQuotedPaths(RawInterfaces);
		}

		AddComponentTagSummary(AssetData, Node);

		Node.Tooltip = FString::Printf(TEXT("%s\n%s"), *Node.Type, *Node.PackageName);
		return Node;
	}

	static FBPQAGraphNode MakeFolderNode(const FString& FolderPath, int32 AssetCount, bool bExternal, int32 Depth = 0)
	{
		FBPQAGraphNode Node;
		Node.Id = FolderToNodeId(FolderPath);
		Node.Label = FString::Printf(TEXT("%s\n%d assets"), *GetFolderLabel(FolderPath), AssetCount);
		Node.Type = bExternal ? TEXT("ExternalFolder") : TEXT("Folder");
		Node.AssetPath = FolderPath;
		Node.PackageName = FolderPath;
		Node.FolderPath = FolderPath;
		Node.Depth = Depth;
		Node.Tooltip = FString::Printf(TEXT("%s\n双击进入文件夹资产依赖图。"), *FolderPath);
		return Node;
	}

	static FBPQAGraphNode MakePackagePlaceholder(FName PackageName)
	{
		FBPQAGraphNode Node;
		Node.Id = PackageToNodeId(PackageName);
		Node.Label = FPackageName::GetShortName(PackageName.ToString());
		Node.Type = TEXT("Package");
		Node.PackageName = PackageName.ToString();
		Node.FolderPath = FPackageName::GetLongPackagePath(PackageName.ToString());
		Node.AssetPath = PackageName.ToString();
		Node.Tooltip = TEXT("AssetRegistry 只返回了包依赖；该包可能不是蓝图资产，或资产数据尚未完成索引。");
		return Node;
	}

	static bool GatherGameAssets(IAssetRegistry& AssetRegistry, TArray<FAssetData>& OutAssets, FString* OutErrorMessage)
	{
		AssetRegistry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		if (!AssetRegistry.GetAssets(Filter, Assets))
		{
			if (OutErrorMessage)
			{
				*OutErrorMessage = TEXT("AssetRegistry.GetAssets 失败，Content 目录可能还在索引中。");
			}
			return false;
		}

		for (const FAssetData& AssetData : Assets)
		{
			if (AssetData.IsValid() && !AssetData.IsRedirector() && IsGamePackage(AssetData.PackageName))
			{
				OutAssets.Add(AssetData);
			}
		}
		return true;
	}

	static void BuildPackageAssetMap(const TArray<FAssetData>& Assets, TMap<FName, FAssetData>& OutPackageAssetMap)
	{
		for (const FAssetData& AssetData : Assets)
		{
			if (!OutPackageAssetMap.Contains(AssetData.PackageName) || IsBlueprintLikeAsset(AssetData))
			{
				OutPackageAssetMap.Add(AssetData.PackageName, AssetData);
			}
		}
	}

	static FAssetRegistryModule* GetAssetRegistryModule(FString* OutErrorMessage)
	{
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
		if (!AssetRegistryModule && OutErrorMessage)
		{
			*OutErrorMessage = TEXT("AssetRegistry 模块未加载，无法扫描资产依赖。");
		}
		return AssetRegistryModule;
	}

	static void GetHardPackageDependencies(IAssetRegistry& AssetRegistry, FName PackageName, TArray<FName>& OutDependencies)
	{
		const UE::AssetRegistry::FDependencyQuery HardDependencyQuery(UE::AssetRegistry::EDependencyQuery::Hard);
		AssetRegistry.GetDependencies(
			PackageName,
			OutDependencies,
			UE::AssetRegistry::EDependencyCategory::Package,
			HardDependencyQuery);
	}

	static TMap<FString, int32> CountAssetsByOverviewFolder(const TArray<FAssetData>& Assets)
	{
		TMap<FString, int32> Result;
		for (const FAssetData& AssetData : Assets)
		{
			const FString FolderPath = GetOverviewFolderForPackagePath(AssetData.PackagePath.ToString());
			Result.FindOrAdd(FolderPath)++;
		}
		return Result;
	}

	static FString GetImmediateChildFolderForPackagePath(const FString& PackagePath, const FString& ParentFolderPath)
	{
		const FString NormalizedPackagePath = NormalizeFolderPath(PackagePath);
		const FString NormalizedParentFolderPath = NormalizeFolderPath(ParentFolderPath);
		if (NormalizedPackagePath == NormalizedParentFolderPath || !NormalizedPackagePath.StartsWith(NormalizedParentFolderPath + TEXT("/")))
		{
			return FString();
		}

		const FString RelativePath = NormalizedPackagePath.RightChop(NormalizedParentFolderPath.Len() + 1);
		FString FirstChildName;
		FString RemainingPath;
		if (RelativePath.Split(TEXT("/"), &FirstChildName, &RemainingPath))
		{
			return NormalizedParentFolderPath + TEXT("/") + FirstChildName;
		}

		return NormalizedPackagePath;
	}

	static TMap<FString, int32> CountAssetsByImmediateChildFolder(const TArray<FAssetData>& Assets, const FString& ParentFolderPath)
	{
		TMap<FString, int32> Result;
		for (const FAssetData& AssetData : Assets)
		{
			const FString ChildFolder = GetImmediateChildFolderForPackagePath(AssetData.PackagePath.ToString(), ParentFolderPath);
			if (!ChildFolder.IsEmpty())
			{
				Result.FindOrAdd(ChildFolder)++;
			}
		}
		return Result;
	}

	static FAssetData FindBestAssetForPackage(IAssetRegistry& AssetRegistry, FName PackageName, const TMap<FName, FAssetData>& PackageAssetMap)
	{
		if (const FAssetData* CachedData = PackageAssetMap.Find(PackageName))
		{
			return *CachedData;
		}

		TArray<FAssetData> PackageAssets;
		AssetRegistry.GetAssetsByPackageName(PackageName, PackageAssets);
		for (const FAssetData& AssetData : PackageAssets)
		{
			if (AssetData.IsValid() && !AssetData.IsRedirector())
			{
				return AssetData;
			}
		}

		return FAssetData();
	}

	static void AddClassMetadataEdges(FBPQAGraphModel& OutModel, const FBPQAGraphNode& BlueprintNode)
	{
		if (!BlueprintNode.ParentClass.IsEmpty())
		{
			FBPQAGraphNode ParentNode;
			ParentNode.Id = MakeClassNodeId(BlueprintNode.ParentClass);
			ParentNode.Label = MakeReadableLabelFromPath(BlueprintNode.ParentClass);
			ParentNode.Type = TEXT("ParentClass");
			ParentNode.ClassName = BlueprintNode.ParentClass;
			ParentNode.Tooltip = TEXT("蓝图父类，由 AssetData ParentClass/NativeParentClass 标签读取。");
			OutModel.AddOrUpdateNode(ParentNode);

			OutModel.AddEdgeUnique({
				BlueprintNode.Id,
				ParentNode.Id,
				TEXT("继承父类"),
				TEXT("ParentClass"),
				0.0
			});
		}

		for (const FString& InterfacePath : BlueprintNode.Interfaces)
		{
			FBPQAGraphNode InterfaceNode;
			InterfaceNode.Id = MakeClassNodeId(InterfacePath);
			InterfaceNode.Label = MakeReadableLabelFromPath(InterfacePath);
			InterfaceNode.Type = TEXT("Interface");
			InterfaceNode.ClassName = InterfacePath;
			InterfaceNode.Tooltip = TEXT("蓝图实现的接口，由 AssetData ImplementedInterfaces 标签读取。");
			OutModel.AddOrUpdateNode(InterfaceNode);

			OutModel.AddEdgeUnique({
				BlueprintNode.Id,
				InterfaceNode.Id,
				TEXT("实现接口"),
				TEXT("Interface"),
				0.0
			});
		}
	}

	static void AnnotateReciprocalCycles(FBPQAGraphModel& OutModel)
	{
		for (const FBPQAGraphEdge& Edge : OutModel.Edges)
		{
			if (Edge.Type != TEXT("HardDependency"))
			{
				continue;
			}

			const bool bHasReverse = OutModel.Edges.ContainsByPredicate([&Edge](const FBPQAGraphEdge& Other)
			{
				return Other.Type == TEXT("HardDependency") && Other.From == Edge.To && Other.To == Edge.From;
			});

			if (bHasReverse)
			{
				if (FBPQAGraphNode* FromNode = OutModel.FindNodeById(Edge.From))
				{
					FBPQAGraphModel::AddUniqueString(FromNode->Components, FString::Printf(TEXT("循环依赖: %s"), *Edge.To));
				}
				if (FBPQAGraphNode* ToNode = OutModel.FindNodeById(Edge.To))
				{
					FBPQAGraphModel::AddUniqueString(ToNode->Components, FString::Printf(TEXT("循环依赖: %s"), *Edge.From));
				}
			}
		}
	}

	static void ComputeGraphDepths(FBPQAGraphModel& OutModel)
	{
	// This is not a strict topological sort; bounded relaxation keeps cyclic graphs layoutable.
		for (FBPQAGraphNode& Node : OutModel.Nodes)
		{
			Node.Depth = 0;
		}

		const int32 MaxIterations = FMath::Min(OutModel.Nodes.Num(), 12);
		for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
		{
			for (const FBPQAGraphEdge& Edge : OutModel.Edges)
			{
				if (Edge.Type != TEXT("HardDependency")
					&& Edge.Type != TEXT("FolderDependency")
					&& Edge.Type != TEXT("SubFolderDependency")
					&& Edge.Type != TEXT("FolderContains")
					&& Edge.Type != TEXT("AssetDependency")
					&& Edge.Type != TEXT("ExternalFolderDependency")
					&& Edge.Type != TEXT("ExternalAssetDependency")
					&& Edge.Type != TEXT("ParentClass")
					&& Edge.Type != TEXT("Interface"))
				{
					continue;
				}

				const FBPQAGraphNode* FromNode = OutModel.FindNodeById(Edge.From);
				FBPQAGraphNode* ToNode = OutModel.FindNodeById(Edge.To);
				if (FromNode && ToNode)
				{
					ToNode->Depth = FMath::Clamp(FMath::Max(ToNode->Depth, FromNode->Depth + 1), 0, 12);
				}
			}
		}
	}

	static bool BuildFullProjectAssetTree(
		IAssetRegistry& AssetRegistry,
		const TArray<FAssetData>& AllAssets,
		const TMap<FName, FAssetData>& PackageAssetMap,
		FBPQAGraphModel& OutModel)
	{
		OutModel.Reset(TEXT("/All/Game Full Static Dependency Tree"), EBPQAViewMode::FolderAssetDependency);

		const FString RootFolder = DetermineOverviewRootFolder(AllAssets);
		TMap<FString, int32> DirectFolderAssetCounts;
		TSet<FString> FolderPaths;
		FolderPaths.Add(RootFolder);

		for (const FAssetData& AssetData : AllAssets)
		{
			if (!IsUnderFolder(AssetData.PackagePath.ToString(), RootFolder))
			{
				continue;
			}

			const FString AssetFolder = NormalizeFolderPath(AssetData.PackagePath.ToString());
			DirectFolderAssetCounts.FindOrAdd(AssetFolder)++;

			FString CurrentFolder = AssetFolder;
			while (!CurrentFolder.IsEmpty() && IsUnderFolder(CurrentFolder, RootFolder))
			{
				FolderPaths.Add(CurrentFolder);
				CurrentFolder = GetParentFolderPath(CurrentFolder);
			}
		}

		for (const FString& FolderPath : FolderPaths)
		{
			int32 RecursiveAssetCount = 0;
			for (const TPair<FString, int32>& Pair : DirectFolderAssetCounts)
			{
				if (IsUnderFolder(Pair.Key, FolderPath))
				{
					RecursiveAssetCount += Pair.Value;
				}
			}

			OutModel.AddOrUpdateNode(MakeFolderNode(
				FolderPath,
				RecursiveAssetCount,
				false,
				GetFolderDepth(FolderPath) - GetFolderDepth(RootFolder)));

			const FString ParentFolder = GetParentFolderPath(FolderPath);
			if (!ParentFolder.IsEmpty() && FolderPaths.Contains(ParentFolder))
			{
				OutModel.AddEdgeUnique({
					FolderToNodeId(ParentFolder),
					FolderToNodeId(FolderPath),
					TEXT("Folder contains folder"),
					TEXT("FolderContains"),
					0.0
				});
			}
		}

		for (const FAssetData& AssetData : AllAssets)
		{
			if (!IsUnderFolder(AssetData.PackagePath.ToString(), RootFolder))
			{
				continue;
			}

			FBPQAGraphNode AssetNode = MakeNodeFromAssetData(AssetData, IsBlueprintLikeAsset(AssetData));
			const FString AssetFolder = NormalizeFolderPath(AssetData.PackagePath.ToString());
			AssetNode.Depth = GetFolderDepth(AssetFolder) - GetFolderDepth(RootFolder) + 1;
			OutModel.AddOrUpdateNode(AssetNode);
			OutModel.AddEdgeUnique({
				FolderToNodeId(AssetFolder),
				PackageToNodeId(AssetData.PackageName),
				TEXT("Folder contains asset"),
				TEXT("FolderContains"),
				0.0
			});
		}

		for (const FAssetData& SourceAsset : AllAssets)
		{
			if (!IsUnderFolder(SourceAsset.PackagePath.ToString(), RootFolder))
			{
				continue;
			}

			TArray<FName> Dependencies;
			GetHardPackageDependencies(AssetRegistry, SourceAsset.PackageName, Dependencies);
			for (const FName DependencyPackage : Dependencies)
			{
				if (!IsGamePackage(DependencyPackage) || DependencyPackage == SourceAsset.PackageName)
				{
					continue;
				}

				const FAssetData DependencyAsset = FindBestAssetForPackage(AssetRegistry, DependencyPackage, PackageAssetMap);
				if (!DependencyAsset.IsValid())
				{
					continue;
				}

				if (!IsUnderFolder(DependencyAsset.PackagePath.ToString(), RootFolder))
				{
					const FString ExternalFolder = NormalizeFolderPath(DependencyAsset.PackagePath.ToString());
					OutModel.AddOrUpdateNode(MakeFolderNode(ExternalFolder, 1, true, 1));
					FBPQAGraphNode ExternalAssetNode = MakeNodeFromAssetData(DependencyAsset, IsBlueprintLikeAsset(DependencyAsset));
					ExternalAssetNode.Depth = 2;
					OutModel.AddOrUpdateNode(ExternalAssetNode);
					OutModel.AddEdgeUnique({
						FolderToNodeId(ExternalFolder),
						PackageToNodeId(DependencyAsset.PackageName),
						TEXT("External folder contains asset"),
						TEXT("ExternalAssetDependency"),
						0.0
					});
				}

				OutModel.AddEdgeUnique({
					PackageToNodeId(SourceAsset.PackageName),
					PackageToNodeId(DependencyAsset.PackageName),
					TEXT("Asset hard dependency"),
					TEXT("AssetDependency"),
					0.0
				});

				if (FBPQAGraphNode* SourceNode = OutModel.FindNodeById(PackageToNodeId(SourceAsset.PackageName)))
				{
					FBPQAGraphModel::AddUniqueString(SourceNode->References, FString::Printf(
						TEXT("%s -> %s (%s)"),
						*SourceAsset.PackageName.ToString(),
						*DependencyPackage.ToString(),
						*NormalizeFolderPath(DependencyAsset.PackagePath.ToString())));
				}
				if (FBPQAGraphNode* TargetNode = OutModel.FindNodeById(PackageToNodeId(DependencyPackage)))
				{
					FBPQAGraphModel::AddUniqueString(TargetNode->ReferencedBy, FString::Printf(
						TEXT("%s (%s)"),
						*SourceAsset.PackageName.ToString(),
						*NormalizeFolderPath(SourceAsset.PackagePath.ToString())));
				}
			}
		}

		AnnotateReciprocalCycles(OutModel);
		return true;
	}
}

bool FBPQAStaticDependencyAnalyzer::AnalyzeProject(FBPQAGraphModel& OutModel, FString* OutErrorMessage) const
{
	OutModel.Reset(TEXT("/All/Game 文件夹依赖总览"), EBPQAViewMode::FolderDependency);

	FAssetRegistryModule* AssetRegistryModule = BPQAStatic::GetAssetRegistryModule(OutErrorMessage);
	if (!AssetRegistryModule)
	{
		return false;
	}

	IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
	TArray<FAssetData> AllAssets;
	if (!BPQAStatic::GatherGameAssets(AssetRegistry, AllAssets, OutErrorMessage))
	{
		return false;
	}

	TMap<FName, FAssetData> PackageAssetMap;
	BPQAStatic::BuildPackageAssetMap(AllAssets, PackageAssetMap);
	const FString OverviewRootFolder = BPQAStatic::DetermineOverviewRootFolder(AllAssets);
	const TMap<FString, int32> OverviewFolderCounts = BPQAStatic::CountAssetsByOverviewFolder(AllAssets);
	const TMap<FString, int32> ChildFolderCounts = BPQAStatic::CountAssetsByImmediateChildFolder(AllAssets, OverviewRootFolder);
	int32 RootAssetCount = 0;
	for (const FAssetData& AssetData : AllAssets)
	{
		if (BPQAStatic::IsUnderFolder(AssetData.PackagePath.ToString(), OverviewRootFolder))
		{
			++RootAssetCount;
		}
	}

	OutModel.AddOrUpdateNode(BPQAStatic::MakeFolderNode(OverviewRootFolder, RootAssetCount, false, 0));
	for (const TPair<FString, int32>& ChildFolderCount : ChildFolderCounts)
	{
		OutModel.AddOrUpdateNode(BPQAStatic::MakeFolderNode(ChildFolderCount.Key, ChildFolderCount.Value, false, 1));
		OutModel.AddEdgeUnique({
			BPQAStatic::FolderToNodeId(OverviewRootFolder),
			BPQAStatic::FolderToNodeId(ChildFolderCount.Key),
			TEXT("Project 下一级文件夹"),
			TEXT("FolderContains"),
			0.0
		});
	}

	for (const FAssetData& SourceAsset : AllAssets)
	{
		if (!BPQAStatic::IsUnderFolder(SourceAsset.PackagePath.ToString(), OverviewRootFolder))
		{
			continue;
		}

		const FString SourceFolder = BPQAStatic::GetOverviewChildFolderForPackagePath(SourceAsset.PackagePath.ToString(), OverviewRootFolder);

		TArray<FName> Dependencies;
		BPQAStatic::GetHardPackageDependencies(AssetRegistry, SourceAsset.PackageName, Dependencies);
		for (const FName DependencyPackage : Dependencies)
		{
			if (!BPQAStatic::IsGamePackage(DependencyPackage) || DependencyPackage == SourceAsset.PackageName)
			{
				continue;
			}

			const FAssetData DependencyAsset = BPQAStatic::FindBestAssetForPackage(AssetRegistry, DependencyPackage, PackageAssetMap);
			if (!DependencyAsset.IsValid())
			{
				continue;
			}

			const bool bDependencyUnderRoot = BPQAStatic::IsUnderFolder(DependencyAsset.PackagePath.ToString(), OverviewRootFolder);
			const FString TargetFolder = bDependencyUnderRoot
				? BPQAStatic::GetOverviewChildFolderForPackagePath(DependencyAsset.PackagePath.ToString(), OverviewRootFolder)
				: BPQAStatic::NormalizeFolderPath(DependencyAsset.PackagePath.ToString());
			if (SourceFolder == TargetFolder)
			{
				continue;
			}

			if (!bDependencyUnderRoot)
			{
				OutModel.AddOrUpdateNode(BPQAStatic::MakeFolderNode(TargetFolder, FMath::Max(1, OverviewFolderCounts.FindRef(BPQAStatic::GetOverviewFolderForPackagePath(TargetFolder))), true, 1));
			}

			OutModel.AddEdgeUnique({
				BPQAStatic::FolderToNodeId(SourceFolder),
				BPQAStatic::FolderToNodeId(TargetFolder),
				bDependencyUnderRoot ? TEXT("Folder dependency") : TEXT("Cross-folder dependency"),
				bDependencyUnderRoot ? TEXT("FolderDependency") : TEXT("ExternalFolderDependency"),
				0.0
			});

			if (FBPQAGraphNode* SourceNode = OutModel.FindNodeById(BPQAStatic::FolderToNodeId(SourceFolder)))
			{
				FBPQAGraphModel::AddUniqueString(SourceNode->References, FString::Printf(
					TEXT("%s -> %s (%s)"),
					*SourceAsset.PackageName.ToString(),
					*DependencyPackage.ToString(),
					*TargetFolder));
			}
			if (FBPQAGraphNode* TargetNode = OutModel.FindNodeById(BPQAStatic::FolderToNodeId(TargetFolder)))
			{
				FBPQAGraphModel::AddUniqueString(TargetNode->ReferencedBy, FString::Printf(
					TEXT("%s (%s)"),
					*SourceAsset.PackageName.ToString(),
					*SourceFolder));
			}
		}
	}

	return true;
}

bool FBPQAStaticDependencyAnalyzer::AnalyzeFolder(const FString& FolderPath, FBPQAGraphModel& OutModel, FString* OutErrorMessage) const
{
	const FString NormalizedFolderPath = BPQAStatic::NormalizeFolderPath(FolderPath);
	OutModel.Reset(FString::Printf(TEXT("%s 资产依赖图"), *NormalizedFolderPath), EBPQAViewMode::FolderAssetDependency);

	FAssetRegistryModule* AssetRegistryModule = BPQAStatic::GetAssetRegistryModule(OutErrorMessage);
	if (!AssetRegistryModule)
	{
		return false;
	}

	IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
	TArray<FAssetData> AllAssets;
	if (!BPQAStatic::GatherGameAssets(AssetRegistry, AllAssets, OutErrorMessage))
	{
		return false;
	}

	TMap<FName, FAssetData> PackageAssetMap;
	BPQAStatic::BuildPackageAssetMap(AllAssets, PackageAssetMap);
	const TMap<FString, int32> OverviewFolderCounts = BPQAStatic::CountAssetsByOverviewFolder(AllAssets);

	TArray<FAssetData> FolderAssets;
	for (const FAssetData& AssetData : AllAssets)
	{
		if (BPQAStatic::IsUnderFolder(AssetData.PackagePath.ToString(), NormalizedFolderPath))
		{
			FolderAssets.Add(AssetData);
			OutModel.AddOrUpdateNode(BPQAStatic::MakeNodeFromAssetData(AssetData, BPQAStatic::IsBlueprintLikeAsset(AssetData)));
		}
	}

	for (const FAssetData& SourceAsset : FolderAssets)
	{
		const FString SourceNodeId = BPQAStatic::PackageToNodeId(SourceAsset.PackageName);
		TArray<FName> Dependencies;
		BPQAStatic::GetHardPackageDependencies(AssetRegistry, SourceAsset.PackageName, Dependencies);

		for (const FName DependencyPackage : Dependencies)
		{
			if (!BPQAStatic::IsGamePackage(DependencyPackage) || DependencyPackage == SourceAsset.PackageName)
			{
				continue;
			}

			const FAssetData DependencyAsset = BPQAStatic::FindBestAssetForPackage(AssetRegistry, DependencyPackage, PackageAssetMap);
			if (!DependencyAsset.IsValid())
			{
				continue;
			}

			if (BPQAStatic::IsUnderFolder(DependencyAsset.PackagePath.ToString(), NormalizedFolderPath))
			{
				OutModel.AddOrUpdateNode(BPQAStatic::MakeNodeFromAssetData(DependencyAsset, BPQAStatic::IsBlueprintLikeAsset(DependencyAsset)));
				OutModel.AddEdgeUnique({
					SourceNodeId,
					BPQAStatic::PackageToNodeId(DependencyAsset.PackageName),
					TEXT("同文件夹硬引用"),
					TEXT("AssetDependency"),
					0.0
				});
			}
			else
			{
				const FString ExternalFolder = BPQAStatic::NormalizeFolderPath(DependencyAsset.PackagePath.ToString());
				const FString ExternalOverviewFolder = BPQAStatic::GetOverviewFolderForPackagePath(DependencyAsset.PackagePath.ToString());
				OutModel.AddOrUpdateNode(BPQAStatic::MakeFolderNode(ExternalFolder, FMath::Max(1, OverviewFolderCounts.FindRef(ExternalOverviewFolder)), true));
				OutModel.AddOrUpdateNode(BPQAStatic::MakeNodeFromAssetData(DependencyAsset, BPQAStatic::IsBlueprintLikeAsset(DependencyAsset)));

				OutModel.AddEdgeUnique({
					SourceNodeId,
					BPQAStatic::FolderToNodeId(ExternalFolder),
					TEXT("跨文件夹硬引用"),
					TEXT("ExternalFolderDependency"),
					0.0
				});

				OutModel.AddEdgeUnique({
					BPQAStatic::FolderToNodeId(ExternalFolder),
					BPQAStatic::PackageToNodeId(DependencyAsset.PackageName),
					TEXT("外部文件夹中的目标资产"),
					TEXT("ExternalAssetDependency"),
					0.0
				});
			}

			if (FBPQAGraphNode* SourceNode = OutModel.FindNodeById(SourceNodeId))
			{
				FBPQAGraphModel::AddUniqueString(SourceNode->References, FString::Printf(
					TEXT("%s -> %s (%s)"),
					*SourceAsset.PackageName.ToString(),
					*DependencyPackage.ToString(),
					*BPQAStatic::NormalizeFolderPath(DependencyAsset.PackagePath.ToString())));
			}
			if (FBPQAGraphNode* TargetNode = OutModel.FindNodeById(BPQAStatic::PackageToNodeId(DependencyPackage)))
			{
				FBPQAGraphModel::AddUniqueString(TargetNode->ReferencedBy, FString::Printf(
					TEXT("%s (%s)"),
					*SourceAsset.PackageName.ToString(),
					*BPQAStatic::NormalizeFolderPath(SourceAsset.PackagePath.ToString())));
			}
		}
	}

	BPQAStatic::AnnotateReciprocalCycles(OutModel);
	BPQAStatic::ComputeGraphDepths(OutModel);
	return true;
}
