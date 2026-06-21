#include "BPQABlueprintNodeAnalyzer.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"

namespace BPQANodeFlow
{
	static constexpr int32 MaxNodeFlowNodes = 240;

	static FString MakeSafeId(FString Value)
	{
		Value.ReplaceInline(TEXT("\\"), TEXT("_"));
		Value.ReplaceInline(TEXT("/"), TEXT("_"));
		Value.ReplaceInline(TEXT("."), TEXT("_"));
		Value.ReplaceInline(TEXT(":"), TEXT("_"));
		Value.ReplaceInline(TEXT(" "), TEXT("_"));
		Value.ReplaceInline(TEXT("{"), TEXT("_"));
		Value.ReplaceInline(TEXT("}"), TEXT("_"));
		return Value;
	}

	static FString GetNodeTitle(const UEdGraphNode* Node)
	{
		return Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
	}

	static bool NodeClassContains(const UEdGraphNode* Node, const TCHAR* Token)
	{
		return Node && Node->GetClass() && Node->GetClass()->GetName().Contains(Token);
	}

	static bool IsTrackedBlueprintNode(const UEdGraphNode* Node)
	{
		return NodeClassContains(Node, TEXT("K2Node_Event"))
			|| NodeClassContains(Node, TEXT("K2Node_CustomEvent"))
			|| NodeClassContains(Node, TEXT("K2Node_FunctionEntry"))
			|| NodeClassContains(Node, TEXT("K2Node_FunctionResult"))
			|| NodeClassContains(Node, TEXT("K2Node_CallFunction"))
			|| NodeClassContains(Node, TEXT("K2Node_CallParentFunction"))
			|| NodeClassContains(Node, TEXT("K2Node_MacroInstance"))
			|| NodeClassContains(Node, TEXT("ConstructionScript"));
	}

	static FString GetTrackedNodeType(const UEdGraphNode* Node)
	{
		if (NodeClassContains(Node, TEXT("K2Node_CustomEvent")) || NodeClassContains(Node, TEXT("K2Node_Event")))
		{
			return TEXT("BlueprintEvent");
		}
		if (NodeClassContains(Node, TEXT("K2Node_MacroInstance")))
		{
			return TEXT("BlueprintMacro");
		}
		if (NodeClassContains(Node, TEXT("ConstructionScript")))
		{
			return TEXT("BlueprintConstructionScript");
		}
		if (
			NodeClassContains(Node, TEXT("K2Node_FunctionEntry"))
			|| NodeClassContains(Node, TEXT("K2Node_FunctionResult"))
			|| NodeClassContains(Node, TEXT("K2Node_CallFunction"))
			|| NodeClassContains(Node, TEXT("K2Node_CallParentFunction")))
		{
			return TEXT("BlueprintFunction");
		}

		return TEXT("BlueprintNode");
	}

	static FString GetSectionTypeForGraph(const UBlueprint* Blueprint, const UEdGraph* Graph)
	{
		if (!Blueprint || !Graph)
		{
			return TEXT("BlueprintGraphSection");
		}

		if (Blueprint->MacroGraphs.Contains(const_cast<UEdGraph*>(Graph)))
		{
			return TEXT("BlueprintMacroGraph");
		}

		if (Blueprint->FunctionGraphs.Contains(const_cast<UEdGraph*>(Graph)))
		{
			return TEXT("BlueprintFunctionGraph");
		}

		if (Graph->GetName().Contains(TEXT("ConstructionScript")))
		{
			return TEXT("BlueprintConstructionGraph");
		}

		return TEXT("BlueprintEventGraph");
	}

	static bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	static FBPQAGraphNode MakeSectionNode(const UBlueprint* Blueprint, const UEdGraph* Graph)
	{
		FBPQAGraphNode Node;
		const FString BlueprintPath = Blueprint ? Blueprint->GetPathName() : FString();
		const FString GraphName = Graph ? Graph->GetName() : TEXT("Graph");
		Node.Id = FString::Printf(TEXT("NodeFlow:%s:Graph:%s"), *MakeSafeId(BlueprintPath), *MakeSafeId(GraphName));
		Node.Label = GraphName;
		Node.Type = GetSectionTypeForGraph(Blueprint, Graph);
		Node.AssetPath = BlueprintPath;
		Node.PackageName = Blueprint && Blueprint->GetOutermost() ? Blueprint->GetOutermost()->GetName() : FString();
		Node.ClassName = Blueprint ? Blueprint->GetName() : FString();
		Node.Tooltip = FString::Printf(TEXT("Blueprint: %s\nGraph: %s"), *Node.ClassName, *GraphName);
		Node.bIsBlueprint = true;
		return Node;
	}

	static FBPQAGraphNode MakeTrackedNode(
		const UBlueprint* Blueprint,
		const UEdGraph* Graph,
		const UEdGraphNode* GraphNode,
		int32 Serial)
	{
		FBPQAGraphNode Node;
		const FString BlueprintPath = Blueprint ? Blueprint->GetPathName() : FString();
		const FString GraphName = Graph ? Graph->GetName() : TEXT("Graph");
		const FString NodeGuid = GraphNode ? GraphNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString();
		const FString NodeTitle = GetNodeTitle(GraphNode);
		const FString DisplayTitle = !NodeTitle.IsEmpty() ? NodeTitle : (GraphNode ? GraphNode->GetName() : TEXT("Node"));

		Node.Id = FString::Printf(
			TEXT("NodeFlow:%s:Node:%s:%s"),
			*MakeSafeId(BlueprintPath),
			*MakeSafeId(GraphName),
			*MakeSafeId(NodeGuid));
		Node.Label = DisplayTitle;
		Node.Type = GetTrackedNodeType(GraphNode);
		Node.AssetPath = BlueprintPath;
		Node.PackageName = Blueprint && Blueprint->GetOutermost() ? Blueprint->GetOutermost()->GetName() : FString();
		Node.ClassName = Blueprint ? Blueprint->GetName() : FString();
		Node.ParentClass = Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();
		Node.TimeSeconds = static_cast<double>(Serial);
		Node.Tooltip = FString::Printf(
			TEXT("Blueprint: %s\nGraph: %s\nNode: %s\nGuid: %s"),
			*Node.ClassName,
			*GraphName,
			*DisplayTitle,
			*NodeGuid);
		Node.bIsBlueprint = true;
		return Node;
	}
}

bool FBPQABlueprintNodeAnalyzer::AnalyzeBlueprint(UBlueprint* Blueprint, FBPQAGraphModel& OutModel, FString* OutErrorMessage) const
{
	OutModel.Reset(TEXT("类内节点流程图"), EBPQAViewMode::NodeFlow);

	if (!Blueprint)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("未提供蓝图资产，无法分析类内节点流程。");
		}
		return false;
	}

	FBPQAGraphNode RootNode;
	RootNode.Id = FString::Printf(TEXT("NodeFlow:%s:Root"), *BPQANodeFlow::MakeSafeId(Blueprint->GetPathName()));
	RootNode.Label = Blueprint->GetName();
	RootNode.Type = TEXT("BlueprintRoot");
	RootNode.AssetPath = Blueprint->GetPathName();
	RootNode.PackageName = Blueprint->GetOutermost() ? Blueprint->GetOutermost()->GetName() : FString();
	RootNode.ClassName = Blueprint->GetName();
	RootNode.ParentClass = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : FString();
	RootNode.Tooltip = FString::Printf(TEXT("Blueprint: %s"), *Blueprint->GetName());
	RootNode.bIsBlueprint = true;
	OutModel.AddOrUpdateNode(RootNode);

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	TSet<const UEdGraph*> VisitedGraphs;
	int32 Serial = 0;
	bool bAddedAnyTrackedNode = false;

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph || Graph->Nodes.Num() == 0 || VisitedGraphs.Contains(Graph))
		{
			continue;
		}
		VisitedGraphs.Add(Graph);

		TArray<const UEdGraphNode*> TrackedNodes;
		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			if (GraphNode && BPQANodeFlow::IsTrackedBlueprintNode(GraphNode))
			{
				TrackedNodes.Add(GraphNode);
			}
		}

		if (TrackedNodes.Num() == 0)
		{
			continue;
		}

		bAddedAnyTrackedNode = true;
		const FBPQAGraphNode SectionNode = BPQANodeFlow::MakeSectionNode(Blueprint, Graph);
		OutModel.AddOrUpdateNode(SectionNode);
		OutModel.AddEdgeUnique({
			RootNode.Id,
			SectionNode.Id,
			TEXT("Graph"),
			TEXT("BlueprintGraph"),
			static_cast<double>(Serial),
		});

		TMap<const UEdGraphNode*, FString> TrackedNodeIds;
		for (const UEdGraphNode* TrackedNode : TrackedNodes)
		{
			if (OutModel.Nodes.Num() >= BPQANodeFlow::MaxNodeFlowNodes)
			{
				break;
			}

			const FBPQAGraphNode Node = BPQANodeFlow::MakeTrackedNode(Blueprint, Graph, TrackedNode, Serial++);
			TrackedNodeIds.Add(TrackedNode, Node.Id);
			OutModel.AddOrUpdateNode(Node);
			OutModel.AddEdgeUnique({
				SectionNode.Id,
				Node.Id,
				TEXT("Contains"),
				TEXT("BlueprintGraph"),
				Node.TimeSeconds,
			});
		}

		if (OutModel.Nodes.Num() >= BPQANodeFlow::MaxNodeFlowNodes)
		{
			break;
		}

		for (const UEdGraphNode* SourceNode : TrackedNodes)
		{
			const FString* SourceNodeId = TrackedNodeIds.Find(SourceNode);
			if (!SourceNodeId)
			{
				continue;
			}

			for (const UEdGraphPin* Pin : SourceNode->Pins)
			{
				if (!BPQANodeFlow::IsExecPin(Pin))
				{
					continue;
				}

				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					const UEdGraphNode* TargetNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
					const FString* TargetNodeId = TargetNode ? TrackedNodeIds.Find(TargetNode) : nullptr;
					if (!TargetNodeId)
					{
						continue;
					}

					OutModel.AddEdgeUnique({
						*SourceNodeId,
						*TargetNodeId,
						TEXT("Exec"),
						TEXT("BlueprintExec"),
						static_cast<double>(Serial),
					});
				}
			}
		}
	}

	if (!bAddedAnyTrackedNode)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("未找到可跟踪的蓝图事件、函数、宏或构造脚本节点。");
		}
		return false;
	}

	return true;
}
