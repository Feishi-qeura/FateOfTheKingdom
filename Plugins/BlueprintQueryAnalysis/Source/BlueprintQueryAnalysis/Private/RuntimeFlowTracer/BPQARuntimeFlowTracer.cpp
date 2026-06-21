#include "BPQARuntimeFlowTracer.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace BPQARuntime
{
	static const FName GeneratedClassTag(TEXT("GeneratedClass"));
	static constexpr int32 MaxBlueprintFlowNodes = 240;

	static bool IsProjectRelatedPackage(const FString& PackageName)
	{
		return PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/Script/FateOfTheKingdom"));
	}

	static FString GetObjectPackageName(UObject* Object)
	{
		return Object && Object->GetOutermost() ? Object->GetOutermost()->GetName() : FString();
	}

	static FString GetObjectClassPackageName(UObject* Object)
	{
		const UClass* ObjectClass = Object ? Object->GetClass() : nullptr;
		return ObjectClass && ObjectClass->GetOutermost() ? ObjectClass->GetOutermost()->GetName() : FString();
	}

	static UWorld* ResolveWorld(UObject* Object, UWorld* OverrideWorld)
	{
		if (OverrideWorld)
		{
			return OverrideWorld;
		}

		if (!Object)
		{
			return nullptr;
		}

		if (AActor* Actor = Cast<AActor>(Object))
		{
			return Actor->GetWorld();
		}

		if (UWorld* DirectWorld = Object->GetWorld())
		{
			return DirectWorld;
		}

		return Object->GetTypedOuter<UWorld>();
	}

	static FString NormalizeMapName(FString MapName)
	{
		MapName.TrimStartAndEndInline();
		if (MapName.IsEmpty())
		{
			return FString();
		}

		MapName = FPackageName::GetShortName(MapName);
		if (MapName.StartsWith(TEXT("UEDPIE_")))
		{
			const int32 PrefixEnd = MapName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
			if (PrefixEnd != INDEX_NONE)
			{
				MapName = MapName.Mid(PrefixEnd + 1);
			}
		}

		return MapName;
	}

	static FString GetSafeWorldName(UWorld* World)
	{
		if (!World)
		{
			return FString();
		}

		FString MapName = World->GetMapName();
		if (MapName.IsEmpty())
		{
			MapName = World->GetName();
		}
		if (!World->StreamingLevelsPrefix.IsEmpty())
		{
			MapName.RemoveFromStart(World->StreamingLevelsPrefix);
		}
		return NormalizeMapName(MapName);
	}

	static bool IsPlayableWorld(UWorld* World)
	{
		return World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game);
	}

	static UWorld* ResolveCurrentEditorWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}

		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			return EditorWorld;
		}

		return GEditor->PlayWorld;
	}

	static FString MakeRuntimeNodeLabel(const FBPQAFlowEvent& Event)
	{
		const FString Subject = !Event.ObjectName.IsEmpty()
			? Event.ObjectName
			: (!Event.PackageName.IsEmpty() ? Event.PackageName : Event.WorldName);

		return FString::Printf(TEXT("%06.3f  %s\n%s"), Event.TimeSeconds, LexToString(Event.Type), *Subject);
	}

	static bool IsRuntimeFunctionOrEvent(EBPQAFlowEventType Type)
	{
		switch (Type)
		{
		case EBPQAFlowEventType::BeginPIE:
		case EBPQAFlowEventType::PostLoadMap:
		case EBPQAFlowEventType::WorldInitialized:
		case EBPQAFlowEventType::ActorAdded:
		case EBPQAFlowEventType::WorldBeginPlay:
		case EBPQAFlowEventType::EndPIE:
			return true;
		default:
			return false;
		}
	}

	static bool CanEventIdentifyRuntimeBlueprint(EBPQAFlowEventType Type)
	{
		switch (Type)
		{
		case EBPQAFlowEventType::ActorAdded:
		case EBPQAFlowEventType::ObjectConstructed:
		case EBPQAFlowEventType::WorldBeginPlay:
		case EBPQAFlowEventType::PostLoadMap:
		case EBPQAFlowEventType::WorldInitialized:
			return true;
		default:
			return false;
		}
	}

	static TSet<FString> MakeObservedBlueprintPackageSet(const TArray<FBPQAFlowEvent>& Events)
	{
		TSet<FString> Result;
		for (const FBPQAFlowEvent& Event : Events)
		{
			if (!CanEventIdentifyRuntimeBlueprint(Event.Type))
			{
				continue;
			}

			if (Event.ClassPackageName.StartsWith(TEXT("/Game/")))
			{
				Result.Add(Event.ClassPackageName);
			}
			if ((Event.Type == EBPQAFlowEventType::WorldBeginPlay
				|| Event.Type == EBPQAFlowEventType::PostLoadMap
				|| Event.Type == EBPQAFlowEventType::WorldInitialized)
				&& Event.PackageName.StartsWith(TEXT("/Game/")))
			{
				Result.Add(Event.PackageName);
			}
		}
		return Result;
	}

	static int32 GetRuntimeEventDepth(EBPQAFlowEventType Type)
	{
		switch (Type)
		{
		case EBPQAFlowEventType::BeginPIE:
		case EBPQAFlowEventType::PreLoadMap:
			return 0;
		case EBPQAFlowEventType::PostLoadMap:
		case EBPQAFlowEventType::WorldInitialized:
		case EBPQAFlowEventType::WorldBeginPlay:
			return 1;
		case EBPQAFlowEventType::EndPIE:
			return 3;
		default:
			return 2;
		}
	}

	static FString ResolveRuntimeRootName(const TArray<FBPQAFlowEvent>& Events, const FString& PreferredWorldName)
	{
		if (!PreferredWorldName.IsEmpty())
		{
			return PreferredWorldName;
		}

		for (const FBPQAFlowEvent& Event : Events)
		{
			if ((Event.Type == EBPQAFlowEventType::WorldBeginPlay
				|| Event.Type == EBPQAFlowEventType::PostLoadMap
				|| Event.Type == EBPQAFlowEventType::WorldInitialized)
				&& !Event.WorldName.IsEmpty())
			{
				return Event.WorldName;
			}
			if (Event.Type == EBPQAFlowEventType::PreLoadMap && !Event.ObjectName.IsEmpty())
			{
				return Event.ObjectName;
			}
		}

		if (GEditor && GEditor->PlayWorld)
		{
			return GetSafeWorldName(GEditor->PlayWorld);
		}

		if (UWorld* EditorWorld = ResolveCurrentEditorWorld())
		{
			const FString EditorWorldName = GetSafeWorldName(EditorWorld);
			if (!EditorWorldName.IsEmpty())
			{
				return EditorWorldName;
			}
		}

		return TEXT("Current PIE Level");
	}

	static FString MakeSafeId(FString Value)
	{
		Value.ReplaceInline(TEXT("\\"), TEXT("_"));
		Value.ReplaceInline(TEXT("/"), TEXT("_"));
		Value.ReplaceInline(TEXT("."), TEXT("_"));
		Value.ReplaceInline(TEXT(":"), TEXT("_"));
		Value.ReplaceInline(TEXT(" "), TEXT("_"));
		return Value;
	}

	static FString NormalizeLookupName(FString Value)
	{
		Value.TrimStartAndEndInline();
		Value.ReplaceInline(TEXT(" "), TEXT(""));
		return Value.ToLower();
	}

	static FString GetNodeTitle(const UEdGraphNode* Node)
	{
		return Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
	}

	static bool NodeClassContains(const UEdGraphNode* Node, const TCHAR* Token)
	{
		return Node && Node->GetClass() && Node->GetClass()->GetName().Contains(Token);
	}

	static bool IsEventNode(const UEdGraphNode* Node)
	{
		return NodeClassContains(Node, TEXT("K2Node_Event")) || NodeClassContains(Node, TEXT("K2Node_CustomEvent"));
	}

	static bool IsCallFunctionNode(const UEdGraphNode* Node)
	{
		return NodeClassContains(Node, TEXT("K2Node_CallFunction"));
	}

	static bool IsCallMacroNode(const UEdGraphNode* Node)
	{
		return NodeClassContains(Node, TEXT("K2Node_MacroInstance"));
	}

	static bool IsBlueprintAssetData(const FAssetData& AssetData)
	{
		const FString AssetClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		if (AssetClassName.Contains(TEXT("Blueprint")))
		{
			return true;
		}

		FString GeneratedClass;
		return AssetData.GetTagValue(GeneratedClassTag, GeneratedClass) && !GeneratedClass.IsEmpty();
	}

	static int32 GetBlueprintRuntimeDepth(const UBlueprint* Blueprint)
	{
		const UClass* ParentClass = Blueprint ? Blueprint->ParentClass : nullptr;
		const FString ParentName = ParentClass ? ParentClass->GetName() : FString();

		if (ParentName.Contains(TEXT("LevelScript")))
		{
			return 0;
		}
		if (ParentName.Contains(TEXT("GameMode"))
			|| ParentName.Contains(TEXT("GameState"))
			|| ParentName.Contains(TEXT("PlayerState"))
			|| ParentName.Contains(TEXT("PlayerController"))
			|| ParentName.Contains(TEXT("AIController"))
			|| ParentName.Contains(TEXT("HUD")))
		{
			return 1;
		}
		if (ParentName.Contains(TEXT("GameInstance")) || ParentName.Contains(TEXT("FunctionLibrary")))
		{
			return 3;
		}
		return 2;
	}

	static FString GetBlueprintRoleName(const UBlueprint* Blueprint)
	{
		switch (GetBlueprintRuntimeDepth(Blueprint))
		{
		case 0:
			return TEXT("Root");
		case 1:
			return TEXT("TreeTrunk");
		case 3:
			return TEXT("Static");
		case 2:
		default:
			return TEXT("Branch");
		}
	}

	static FString MakeRuntimeGraphTitle(int32 Revision)
	{
		return Revision > 0
			? FString::Printf(TEXT("运行时真实加载/初始化顺序图 #%d"), Revision)
			: TEXT("运行时真实加载/初始化顺序图");
	}

	static FBPQAGraphNode MakeBlueprintFlowNode(
		const UBlueprint* Blueprint,
		const FString& GraphName,
		const FString& Type,
		int32 Depth,
		double TimeSeconds,
		int32 Serial)
	{
		const FString PackageName = Blueprint && Blueprint->GetOutermost() ? Blueprint->GetOutermost()->GetName() : FString();
		const FString BlueprintName = Blueprint ? Blueprint->GetName() : TEXT("Blueprint");

		FBPQAGraphNode Node;
		Node.Id = FString::Printf(TEXT("BlueprintFlow:%s:%s:%s:%d"), *MakeSafeId(PackageName), *Type, *MakeSafeId(GraphName), Serial);
		Node.Label = FString::Printf(TEXT("%06.3f  %s\n%s: %s"), TimeSeconds, *Type, *BlueprintName, *GraphName);
		Node.Type = Type;
		Node.AssetPath = Blueprint ? Blueprint->GetPathName() : PackageName;
		Node.PackageName = PackageName;
		Node.ClassName = BlueprintName;
		Node.ParentClass = Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();
		Node.Depth = Depth;
		Node.TimeSeconds = TimeSeconds;
		Node.bIsBlueprint = true;
		FBPQAGraphModel::AddUniqueString(Node.Components, TEXT("ExecutionState:Inactive"));
		Node.Tooltip = FString::Printf(
			TEXT("Blueprint: %s\nGraph: %s\nLayer: %s\nParentClass: %s"),
			*BlueprintName,
			*GraphName,
			*GetBlueprintRoleName(Blueprint),
			*Node.ParentClass);
		return Node;
	}

	static FString FindCalledFunctionId(const FString& CallTitle, const TMap<FString, FString>& FunctionNodeIds)
	{
		const FString NormalizedCallTitle = NormalizeLookupName(CallTitle);
		if (const FString* ExactMatch = FunctionNodeIds.Find(NormalizedCallTitle))
		{
			return *ExactMatch;
		}

		for (const TPair<FString, FString>& Pair : FunctionNodeIds)
		{
			if (!Pair.Key.IsEmpty() && NormalizedCallTitle.Contains(Pair.Key))
			{
				return Pair.Value;
			}
		}

		return FString();
	}

	static void AppendBlueprintFunctionEventNodes(FBPQAGraphModel& RuntimeGraph)
	{
		FAssetRegistryModule* AssetRegistryModule = FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
		if (!AssetRegistryModule)
		{
			return;
		}

		IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
		AssetRegistry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		if (!AssetRegistry.GetAssets(Filter, Assets))
		{
			return;
		}

		int32 Serial = 0;
		for (const FAssetData& AssetData : Assets)
		{
			if (!AssetData.IsValid() || AssetData.IsRedirector() || !IsBlueprintAssetData(AssetData))
			{
				continue;
			}

			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
			if (!Blueprint)
			{
				continue;
			}

			const int32 BaseDepth = GetBlueprintRuntimeDepth(Blueprint);
			const double BaseTime = 10.0 + static_cast<double>(Serial) * 0.001;
			TMap<FString, FString> CallableNodeIds;
			TMap<UEdGraph*, TArray<FString>> GraphEntryNodeIds;

			if (Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(AActor::StaticClass()) && RuntimeGraph.Nodes.Num() < MaxBlueprintFlowNodes)
			{
				FBPQAGraphNode ConstructionNode = MakeBlueprintFlowNode(
					Blueprint,
					TEXT("ConstructionScript"),
					TEXT("BlueprintConstructionScript"),
					BaseDepth,
					BaseTime,
					Serial++);
				RuntimeGraph.AddOrUpdateNode(ConstructionNode);
				CallableNodeIds.Add(BPQARuntime::NormalizeLookupName(TEXT("ConstructionScript")), ConstructionNode.Id);
			}

			for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
			{
				if (!FunctionGraph || RuntimeGraph.Nodes.Num() >= MaxBlueprintFlowNodes)
				{
					continue;
				}

				const FString FunctionName = FunctionGraph->GetName();
				FBPQAGraphNode FunctionNode = MakeBlueprintFlowNode(
					Blueprint,
					FunctionName,
					TEXT("BlueprintFunction"),
					BaseDepth,
					BaseTime,
					Serial++);
				RuntimeGraph.AddOrUpdateNode(FunctionNode);
				CallableNodeIds.Add(NormalizeLookupName(FunctionName), FunctionNode.Id);
				GraphEntryNodeIds.FindOrAdd(FunctionGraph).Add(FunctionNode.Id);
			}

			for (UEdGraph* MacroGraph : Blueprint->MacroGraphs)
			{
				if (!MacroGraph || RuntimeGraph.Nodes.Num() >= MaxBlueprintFlowNodes)
				{
					continue;
				}

				const FString MacroName = MacroGraph->GetName();
				FBPQAGraphNode MacroNode = MakeBlueprintFlowNode(
					Blueprint,
					MacroName,
					TEXT("BlueprintMacro"),
					BaseDepth,
					BaseTime,
					Serial++);
				RuntimeGraph.AddOrUpdateNode(MacroNode);
				CallableNodeIds.Add(NormalizeLookupName(MacroName), MacroNode.Id);
				GraphEntryNodeIds.FindOrAdd(MacroGraph).Add(MacroNode.Id);
			}

			for (UEdGraph* EventGraph : Blueprint->UbergraphPages)
			{
				if (!EventGraph)
				{
					continue;
				}

				for (UEdGraphNode* GraphNode : EventGraph->Nodes)
				{
					if (!IsEventNode(GraphNode) || RuntimeGraph.Nodes.Num() >= MaxBlueprintFlowNodes)
					{
						continue;
					}

					const FString EventName = GetNodeTitle(GraphNode).IsEmpty() ? GraphNode->GetName() : GetNodeTitle(GraphNode);
					const FString EventType = NodeClassContains(GraphNode, TEXT("CustomEvent"))
						? TEXT("CustomEvent")
						: TEXT("BlueprintEvent");
					FBPQAGraphNode EventNode = MakeBlueprintFlowNode(
						Blueprint,
						EventName,
						EventType,
						BaseDepth,
						BaseTime,
						Serial++);
					RuntimeGraph.AddOrUpdateNode(EventNode);
					GraphEntryNodeIds.FindOrAdd(EventGraph).Add(EventNode.Id);
				}
			}

			auto AddCallEdgesForGraph = [&RuntimeGraph, &CallableNodeIds, &GraphEntryNodeIds](UEdGraph* Graph)
			{
				if (!Graph)
				{
					return;
				}

				const TArray<FString>* SourceNodeIds = GraphEntryNodeIds.Find(Graph);
				if (!SourceNodeIds || SourceNodeIds->Num() == 0)
				{
					return;
				}

				for (UEdGraphNode* GraphNode : Graph->Nodes)
				{
					if (!IsCallFunctionNode(GraphNode) && !IsCallMacroNode(GraphNode))
					{
						continue;
					}

					const FString CalledFunctionId = FindCalledFunctionId(GetNodeTitle(GraphNode), CallableNodeIds);
					if (CalledFunctionId.IsEmpty())
					{
						continue;
					}

					for (const FString& SourceNodeId : *SourceNodeIds)
					{
						RuntimeGraph.AddEdgeUnique({
							SourceNodeId,
							CalledFunctionId,
							TEXT("蓝图函数/事件调用"),
							TEXT("BlueprintCall"),
							0.0
						});
					}
				}
			};

			for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
			{
				AddCallEdgesForGraph(FunctionGraph);
			}
			for (UEdGraph* MacroGraph : Blueprint->MacroGraphs)
			{
				AddCallEdgesForGraph(MacroGraph);
			}
			for (UEdGraph* EventGraph : Blueprint->UbergraphPages)
			{
				AddCallEdgesForGraph(EventGraph);
			}

			if (RuntimeGraph.Nodes.Num() >= MaxBlueprintFlowNodes)
			{
				break;
			}
		}
	}
}

FBPQARuntimeFlowTracer::FBPQARuntimeFlowTracer()
{
	RuntimeGraph.Reset(BPQARuntime::MakeRuntimeGraphTitle(RuntimeGraphRevision), EBPQAViewMode::RuntimeFlow);
}

FBPQARuntimeFlowTracer::~FBPQARuntimeFlowTracer()
{
	UnregisterDelegates();
}

void FBPQARuntimeFlowTracer::RegisterDelegates()
{
	if (bDelegatesRegistered)
	{
		return;
	}

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FBPQARuntimeFlowTracer::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FBPQARuntimeFlowTracer::HandlePostLoadMap);
	AssetLoadedHandle = FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FBPQARuntimeFlowTracer::HandleAssetLoaded);
	ObjectConstructedHandle = FCoreUObjectDelegates::OnObjectConstructed.AddRaw(this, &FBPQARuntimeFlowTracer::HandleObjectConstructed);
	EndLoadPackageHandle = FCoreUObjectDelegates::OnEndLoadPackage.AddRaw(this, &FBPQARuntimeFlowTracer::HandleEndLoadPackage);
	PostWorldInitializationHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FBPQARuntimeFlowTracer::HandlePostWorldInitialization);
	BeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FBPQARuntimeFlowTracer::HandleBeginPIE);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FBPQARuntimeFlowTracer::HandleEndPIE);

	bDelegatesRegistered = true;
}

void FBPQARuntimeFlowTracer::UnregisterDelegates()
{
	if (!bDelegatesRegistered)
	{
		return;
	}

	RemoveWorldDelegates();

	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreUObjectDelegates::OnAssetLoaded.Remove(AssetLoadedHandle);
	FCoreUObjectDelegates::OnObjectConstructed.Remove(ObjectConstructedHandle);
	FCoreUObjectDelegates::OnEndLoadPackage.Remove(EndLoadPackageHandle);
	FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
	FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);

	bDelegatesRegistered = false;
}

void FBPQARuntimeFlowTracer::StartCapture()
{
	++RuntimeGraphRevision;
	Events.Reset();
	SeenConstructedObjects.Reset();
	RuntimeGraph.Reset(BPQARuntime::MakeRuntimeGraphTitle(RuntimeGraphRevision), EBPQAViewMode::RuntimeFlow);
	RemoveWorldDelegates();
	bBlueprintFlowCacheValid = false;
	CaptureWorld.Reset();
	CaptureWorldName = BPQARuntime::GetSafeWorldName(BPQARuntime::ResolveCurrentEditorWorld());

	CaptureStartSeconds = FPlatformTime::Seconds();
	bIsCapturing = true;
}

void FBPQARuntimeFlowTracer::StopCapture(bool bRebuildGraph)
{
	bIsCapturing = false;
	RemoveWorldDelegates();

	if (bRebuildGraph)
	{
		RebuildRuntimeGraph();
	}
}

void FBPQARuntimeFlowTracer::Clear()
{
	RuntimeGraph.Reset(BPQARuntime::MakeRuntimeGraphTitle(RuntimeGraphRevision), EBPQAViewMode::RuntimeFlow);
	Events.Reset();
	SeenConstructedObjects.Reset();
	BlueprintFlowCache.Reset(TEXT("蓝图函数/事件候选缓存"), EBPQAViewMode::RuntimeFlow);
	bBlueprintFlowCacheValid = false;
	CaptureWorld.Reset();
	CaptureWorldName.Reset();
}

void FBPQARuntimeFlowTracer::RebuildRuntimeGraph()
{
	RuntimeGraph.Reset(BPQARuntime::MakeRuntimeGraphTitle(RuntimeGraphRevision), EBPQAViewMode::RuntimeFlow);

	if (Events.Num() == 0)
	{
		return;
	}

	const FString RootName = BPQARuntime::ResolveRuntimeRootName(Events, CaptureWorldName);
	const FString RootNodeId = FString::Printf(TEXT("RuntimeRoot:%s"), *BPQARuntime::MakeSafeId(RootName));
	FBPQAGraphNode RootNode;
	RootNode.Id = RootNodeId;
	RootNode.Label = FString::Printf(TEXT("%06.3f  当前运行关卡\n%s"), 0.0, *RootName);
	RootNode.Type = TEXT("RuntimeRoot");
	RootNode.AssetPath = RootName;
	RootNode.PackageName = RootName;
	RootNode.TimeSeconds = 0.0;
	RootNode.Depth = 0;
	RootNode.Tooltip = TEXT("运行流程树根：当前 PIE 运行关卡。");
	RuntimeGraph.AddOrUpdateNode(RootNode);

	FString PreviousNodeId = RootNodeId;
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const FBPQAFlowEvent& Event = Events[Index];
		if (!BPQARuntime::IsRuntimeFunctionOrEvent(Event.Type))
		{
			continue;
		}

		const FString NodeId = FString::Printf(TEXT("RuntimeEvent_%04d_%s"), Index, LexToString(Event.Type));

		FBPQAGraphNode Node;
		Node.Id = NodeId;
		Node.Label = BPQARuntime::MakeRuntimeNodeLabel(Event);
		Node.Type = LexToString(Event.Type);
		Node.AssetPath = Event.PackageName;
		Node.ClassName = Event.ClassName;
		Node.PackageName = Event.PackageName;
		Node.TimeSeconds = Event.TimeSeconds;
		Node.Depth = BPQARuntime::GetRuntimeEventDepth(Event.Type);
		Node.Tooltip = FString::Printf(
			TEXT("Object: %s\nClass: %s\nClass Package: %s\nPackage: %s\nOuter: %s\nWorld: %s"),
			*Event.ObjectName,
			*Event.ClassName,
			*Event.ClassPackageName,
			*Event.PackageName,
			*Event.OuterName,
			*Event.WorldName);
		RuntimeGraph.AddOrUpdateNode(Node);

		if (!PreviousNodeId.IsEmpty())
		{
			RuntimeGraph.AddEdgeUnique({
				PreviousNodeId,
				NodeId,
				TEXT("真实采集顺序"),
				TEXT("RuntimeOrder"),
				Event.TimeSeconds
			});
		}

		PreviousNodeId = NodeId;
	}

	const bool bHasWorldBeginPlay = Events.ContainsByPredicate([](const FBPQAFlowEvent& Event)
	{
		return Event.Type == EBPQAFlowEventType::WorldBeginPlay;
	});
	const bool bHasBlueprintConstruction = Events.ContainsByPredicate([](const FBPQAFlowEvent& Event)
	{
		return Event.Type == EBPQAFlowEventType::ObjectConstructed || Event.Type == EBPQAFlowEventType::ActorAdded;
	});

	EnsureBlueprintFlowCache();
	const TSet<FString> ObservedBlueprintPackages = BPQARuntime::MakeObservedBlueprintPackageSet(Events);
	TSet<FString> ReachableBlueprintNodeIds;
	for (const FBPQAGraphNode& Node : BlueprintFlowCache.Nodes)
	{
		if (!ObservedBlueprintPackages.Contains(Node.PackageName))
		{
			continue;
		}

		const bool bActivateBeginPlay =
			bHasWorldBeginPlay
			&& (Node.Type == TEXT("BlueprintEvent") || Node.Type == TEXT("CustomEvent"))
			&& Node.Label.Contains(TEXT("BeginPlay"));
		const bool bActivateConstruction =
			bHasBlueprintConstruction
			&& Node.Type == TEXT("BlueprintConstructionScript");
		if (bActivateBeginPlay || bActivateConstruction)
		{
			ReachableBlueprintNodeIds.Add(Node.Id);
		}
	}

	bool bExpandedReachableNodes = true;
	while (bExpandedReachableNodes)
	{
		bExpandedReachableNodes = false;
		for (const FBPQAGraphEdge& Edge : BlueprintFlowCache.Edges)
		{
			if (ReachableBlueprintNodeIds.Contains(Edge.From) && !ReachableBlueprintNodeIds.Contains(Edge.To))
			{
				ReachableBlueprintNodeIds.Add(Edge.To);
				bExpandedReachableNodes = true;
			}
		}
	}

	for (const FBPQAGraphNode& Node : BlueprintFlowCache.Nodes)
	{
		if (!ReachableBlueprintNodeIds.Contains(Node.Id))
		{
			continue;
		}

		FBPQAGraphNode RuntimeNode = Node;
		RuntimeNode.Components.Remove(TEXT("ExecutionState:Inactive"));
		RuntimeGraph.AddOrUpdateNode(RuntimeNode);

		const bool bRootActivated =
			RuntimeNode.Type == TEXT("BlueprintConstructionScript")
			|| ((RuntimeNode.Type == TEXT("BlueprintEvent") || RuntimeNode.Type == TEXT("CustomEvent"))
				&& RuntimeNode.Label.Contains(TEXT("BeginPlay")));
		if (bRootActivated)
		{
			RuntimeGraph.AddEdgeUnique({
				RootNodeId,
				RuntimeNode.Id,
				TEXT("Runtime blueprint activation"),
				TEXT("BlueprintRuntimeActivation"),
				RuntimeNode.TimeSeconds
			});
		}
	}

	for (const FBPQAGraphEdge& Edge : BlueprintFlowCache.Edges)
	{
		if (ReachableBlueprintNodeIds.Contains(Edge.From) && ReachableBlueprintNodeIds.Contains(Edge.To))
		{
			RuntimeGraph.AddEdgeUnique(Edge);
		}
	}
}

void FBPQARuntimeFlowTracer::EnsureBlueprintFlowCache()
{
	if (bBlueprintFlowCacheValid)
	{
		return;
	}

	BlueprintFlowCache.Reset(TEXT("蓝图函数/事件候选缓存"), EBPQAViewMode::RuntimeFlow);
	BPQARuntime::AppendBlueprintFunctionEventNodes(BlueprintFlowCache);
	bBlueprintFlowCacheValid = true;
}

void FBPQARuntimeFlowTracer::RecordEvent(EBPQAFlowEventType Type, UObject* Object, const FString& OverrideName, UWorld* OverrideWorld)
{
	if (!bIsCapturing)
	{
		return;
	}

	if (Object && !ShouldRecordObject(Object))
	{
		return;
	}

	FBPQAFlowEvent Event;
	Event.TimeSeconds = FMath::Max(0.0, FPlatformTime::Seconds() - CaptureStartSeconds);
	Event.Type = Type;
	Event.ObjectName = OverrideName.IsEmpty() && Object ? Object->GetName() : OverrideName;
	Event.ClassName = Object && Object->GetClass() ? Object->GetClass()->GetName() : FString();
	Event.ClassPackageName = BPQARuntime::GetObjectClassPackageName(Object);
	Event.PackageName = BPQARuntime::GetObjectPackageName(Object);
	Event.OuterName = Object && Object->GetOuter() ? Object->GetOuter()->GetName() : FString();

	UWorld* World = BPQARuntime::ResolveWorld(Object, OverrideWorld);
	if (World && !ShouldCaptureWorld(World))
	{
		return;
	}
	Event.WorldName = BPQARuntime::GetSafeWorldName(World);
	if (Event.WorldName.IsEmpty()
		&& (Type == EBPQAFlowEventType::BeginPIE || Type == EBPQAFlowEventType::EndPIE)
		&& !CaptureWorldName.IsEmpty())
	{
		Event.WorldName = CaptureWorldName;
	}

	if (Event.ObjectName.IsEmpty() && Event.PackageName.IsEmpty() && Event.WorldName.IsEmpty())
	{
		return;
	}

	Events.Add(MoveTemp(Event));
	if (Events.Num() > MaxEventCount)
	{
		Events.RemoveAt(0, Events.Num() - MaxEventCount);
	}
}

void FBPQARuntimeFlowTracer::RecordPackageLoaded(UPackage* Package)
{
	if (!Package)
	{
		return;
	}

	const FString PackageName = Package->GetName();
	if (!BPQARuntime::IsProjectRelatedPackage(PackageName))
	{
		return;
	}

	RecordEvent(EBPQAFlowEventType::PackageLoaded, Package, PackageName);
}

void FBPQARuntimeFlowTracer::RegisterWorldDelegates(UWorld* World)
{
	if (!bIsCapturing || !World)
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld(World);
	if (!ActorSpawnedHandles.Contains(WeakWorld))
	{
		const FDelegateHandle ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateRaw(this, &FBPQARuntimeFlowTracer::HandleActorSpawned));
		ActorSpawnedHandles.Add(WeakWorld, ActorSpawnedHandle);
	}

	if (!WorldBeginPlayHandles.Contains(WeakWorld))
	{
		const FDelegateHandle BeginPlayHandle = World->OnWorldBeginPlay.AddRaw(this, &FBPQARuntimeFlowTracer::HandleWorldBeginPlay, World);
		WorldBeginPlayHandles.Add(WeakWorld, BeginPlayHandle);
	}
}

void FBPQARuntimeFlowTracer::RemoveWorldDelegates()
{
	for (const TPair<TWeakObjectPtr<UWorld>, FDelegateHandle>& Pair : ActorSpawnedHandles)
	{
		if (UWorld* World = Pair.Key.Get())
		{
			World->RemoveOnActorSpawnedHandler(Pair.Value);
		}
	}
	ActorSpawnedHandles.Reset();

	for (const TPair<TWeakObjectPtr<UWorld>, FDelegateHandle>& Pair : WorldBeginPlayHandles)
	{
		if (UWorld* World = Pair.Key.Get())
		{
			World->OnWorldBeginPlay.Remove(Pair.Value);
		}
	}
	WorldBeginPlayHandles.Reset();
}

void FBPQARuntimeFlowTracer::HandlePreLoadMap(const FString& MapName)
{
	if (!bIsCapturing)
	{
		return;
	}

	const FString NormalizedMapName = BPQARuntime::NormalizeMapName(MapName);
	if (!NormalizedMapName.IsEmpty())
	{
		CaptureWorldName = NormalizedMapName;
	}
	RecordEvent(EBPQAFlowEventType::PreLoadMap, nullptr, NormalizedMapName.IsEmpty() ? MapName : NormalizedMapName);
}

void FBPQARuntimeFlowTracer::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!bIsCapturing)
	{
		return;
	}

	if (!ShouldCaptureWorld(LoadedWorld))
	{
		return;
	}

	RecordEvent(EBPQAFlowEventType::PostLoadMap, LoadedWorld, BPQARuntime::GetSafeWorldName(LoadedWorld), LoadedWorld);
	RegisterWorldDelegates(LoadedWorld);
}

void FBPQARuntimeFlowTracer::HandleAssetLoaded(UObject* Asset)
{
	RecordEvent(EBPQAFlowEventType::AssetLoaded, Asset);
}

void FBPQARuntimeFlowTracer::HandleObjectConstructed(UObject* Object)
{
	if (!bIsCapturing || !ShouldRecordObject(Object))
	{
		return;
	}

	const FString ObjectKey = MakeObjectKey(Object);
	if (SeenConstructedObjects.Contains(ObjectKey))
	{
		return;
	}

	SeenConstructedObjects.Add(ObjectKey);
	RecordEvent(EBPQAFlowEventType::ObjectConstructed, Object);
}

void FBPQARuntimeFlowTracer::HandleEndLoadPackage(const FEndLoadPackageContext& Context)
{
	if (!bIsCapturing)
	{
		return;
	}

	for (UPackage* Package : Context.LoadedPackages)
	{
		RecordPackageLoaded(Package);
	}
}

void FBPQARuntimeFlowTracer::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (!bIsCapturing)
	{
		return;
	}

	if (!ShouldCaptureWorld(World))
	{
		return;
	}

	RecordEvent(EBPQAFlowEventType::WorldInitialized, World, BPQARuntime::GetSafeWorldName(World), World);
	RegisterWorldDelegates(World);
}

void FBPQARuntimeFlowTracer::HandleActorSpawned(AActor* Actor)
{
	RecordEvent(EBPQAFlowEventType::ActorAdded, Actor);
}

void FBPQARuntimeFlowTracer::HandleWorldBeginPlay(UWorld* World)
{
	if (!ShouldCaptureWorld(World))
	{
		return;
	}

	RecordEvent(EBPQAFlowEventType::WorldBeginPlay, World, BPQARuntime::GetSafeWorldName(World), World);
}

void FBPQARuntimeFlowTracer::HandleBeginPIE(bool bIsSimulating)
{
	if (!bIsCapturing)
	{
		if (!bAutoCaptureOnPIE)
		{
			return;
		}

		StartCapture();
	}

	if (const FString CurrentWorldName = BPQARuntime::GetSafeWorldName(BPQARuntime::ResolveCurrentEditorWorld()); !CurrentWorldName.IsEmpty())
	{
		CaptureWorldName = CurrentWorldName;
	}
	CaptureWorld.Reset();

	const FString BeginLabel = CaptureWorldName.IsEmpty()
		? (bIsSimulating ? TEXT("BeginPIE Simulate") : TEXT("BeginPIE Play"))
		: FString::Printf(TEXT("%s: %s"), bIsSimulating ? TEXT("BeginPIE Simulate") : TEXT("BeginPIE Play"), *CaptureWorldName);
	RecordEvent(EBPQAFlowEventType::BeginPIE, nullptr, BeginLabel);
}

void FBPQARuntimeFlowTracer::HandleEndPIE(bool bIsSimulating)
{
	if (!bIsCapturing)
	{
		return;
	}

	RecordEvent(EBPQAFlowEventType::EndPIE, nullptr, bIsSimulating ? TEXT("EndPIE Simulate") : TEXT("EndPIE Play"));
	StopCapture(true);
}

bool FBPQARuntimeFlowTracer::ShouldCaptureWorld(UWorld* World)
{
	if (!BPQARuntime::IsPlayableWorld(World))
	{
		return false;
	}

	const FString WorldName = BPQARuntime::GetSafeWorldName(World);
	if (WorldName.IsEmpty())
	{
		return false;
	}

	if (CaptureWorldName.IsEmpty())
	{
		CaptureWorldName = WorldName;
	}
	if (!WorldName.Equals(CaptureWorldName, ESearchCase::IgnoreCase))
	{
		return false;
	}

	CaptureWorld = World;
	return true;
}

bool FBPQARuntimeFlowTracer::ShouldRecordObject(UObject* Object) const
{
	if (!Object)
	{
		return false;
	}

	const FString PackageName = BPQARuntime::GetObjectPackageName(Object);
	const FString ClassPackageName = BPQARuntime::GetObjectClassPackageName(Object);
	const FString ClassName = Object->GetClass() ? Object->GetClass()->GetName() : FString();

	if (BPQARuntime::IsProjectRelatedPackage(PackageName) || BPQARuntime::IsProjectRelatedPackage(ClassPackageName))
	{
		return true;
	}

	// Track common blueprint-generated runtime objects without requiring plugin base classes.
	return Object->IsAsset()
		|| Object->IsA<UClass>()
		|| Object->IsA<AActor>()
		|| ClassName.Contains(TEXT("Blueprint"))
		|| ClassName.Contains(TEXT("Widget"));
}

FString FBPQARuntimeFlowTracer::MakeObjectKey(UObject* Object) const
{
	if (!Object)
	{
		return FString();
	}

	return FString::Printf(TEXT("%s|%s|%p"), *Object->GetName(), *BPQARuntime::GetObjectPackageName(Object), Object);
}


