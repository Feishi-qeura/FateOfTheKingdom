#include "BPQAGraphExporter.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace BPQAExport
{
	static TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	static FString EscapeMermaidLabel(FString Value)
	{
		Value.ReplaceInline(TEXT("\""), TEXT("'"));
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT("<br/>"));
		return Value;
	}

	static FString EscapeDotLabel(FString Value)
	{
		Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Value.ReplaceInline(TEXT("\r"), TEXT(" "));
		Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		return Value;
	}

	static FString MakeExportDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintQueryAnalysis"));
	}

	static const TCHAR* LexToString(EBPQAViewMode Mode)
	{
		switch (Mode)
		{
		case EBPQAViewMode::StaticDependency:
			return TEXT("StaticDependency");
		case EBPQAViewMode::FolderDependency:
			return TEXT("FolderDependency");
		case EBPQAViewMode::FolderAssetDependency:
			return TEXT("FolderAssetDependency");
		case EBPQAViewMode::RuntimeFlow:
			return TEXT("RuntimeFlow");
		case EBPQAViewMode::NodeFlow:
			return TEXT("NodeFlow");
		default:
			return TEXT("Unknown");
		}
	}

	static bool SaveText(const FString& Text, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage)
	{
		const FString ExportDirectory = MakeExportDirectory();
		IFileManager::Get().MakeDirectory(*ExportDirectory, true);

		OutFullPath = FPaths::Combine(ExportDirectory, FileName);
		if (!FFileHelper::SaveStringToFile(Text, *OutFullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutErrorMessage = FString::Printf(TEXT("写入文件失败: %s"), *OutFullPath);
			return false;
		}

		OutErrorMessage.Reset();
		return true;
	}
}

FString FBPQAGraphExporter::ExportToJson(const FBPQAGraphModel& Graph)
{
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("title"), Graph.Title);
	RootObject->SetStringField(TEXT("mode"), BPQAExport::LexToString(Graph.Mode));

	TArray<TSharedPtr<FJsonValue>> NodeValues;
	for (const FBPQAGraphNode& Node : Graph.Nodes)
	{
		TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("id"), Node.Id);
		NodeObject->SetStringField(TEXT("label"), Node.Label);
		NodeObject->SetStringField(TEXT("type"), Node.Type);
		NodeObject->SetStringField(TEXT("assetPath"), Node.AssetPath);
		NodeObject->SetStringField(TEXT("className"), Node.ClassName);
		NodeObject->SetStringField(TEXT("parentClass"), Node.ParentClass);
		NodeObject->SetStringField(TEXT("packageName"), Node.PackageName);
		NodeObject->SetStringField(TEXT("folderPath"), Node.FolderPath);
		NodeObject->SetStringField(TEXT("tooltip"), Node.Tooltip);
		NodeObject->SetNumberField(TEXT("timeSeconds"), Node.TimeSeconds);
		NodeObject->SetNumberField(TEXT("depth"), Node.Depth);
		NodeObject->SetBoolField(TEXT("isBlueprint"), Node.bIsBlueprint);
		NodeObject->SetArrayField(TEXT("references"), BPQAExport::StringArrayToJson(Node.References));
		NodeObject->SetArrayField(TEXT("referencedBy"), BPQAExport::StringArrayToJson(Node.ReferencedBy));
		NodeObject->SetArrayField(TEXT("interfaces"), BPQAExport::StringArrayToJson(Node.Interfaces));
		NodeObject->SetArrayField(TEXT("components"), BPQAExport::StringArrayToJson(Node.Components));
		NodeValues.Add(MakeShared<FJsonValueObject>(NodeObject));
	}
	RootObject->SetArrayField(TEXT("nodes"), NodeValues);

	TArray<TSharedPtr<FJsonValue>> EdgeValues;
	for (const FBPQAGraphEdge& Edge : Graph.Edges)
	{
		TSharedPtr<FJsonObject> EdgeObject = MakeShared<FJsonObject>();
		EdgeObject->SetStringField(TEXT("from"), Edge.From);
		EdgeObject->SetStringField(TEXT("to"), Edge.To);
		EdgeObject->SetStringField(TEXT("reason"), Edge.Reason);
		EdgeObject->SetStringField(TEXT("type"), Edge.Type);
		EdgeObject->SetNumberField(TEXT("timeSeconds"), Edge.TimeSeconds);
		if (!Edge.MotionShape.IsEmpty())
		{
			EdgeObject->SetStringField(TEXT("shape"), Edge.MotionShape);
		}
		if (Edge.MotionDuration > 0.0)
		{
			EdgeObject->SetNumberField(TEXT("duration"), Edge.MotionDuration);
		}
		EdgeValues.Add(MakeShared<FJsonValueObject>(EdgeObject));
	}
	RootObject->SetArrayField(TEXT("edges"), EdgeValues);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(RootObject, Writer);
	return Output;
}

FString FBPQAGraphExporter::ExportToMermaid(const FBPQAGraphModel& Graph)
{
	FString Output = TEXT("graph TD\n");

	TMap<FString, FString> MermaidIds;
	for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
	{
		const FBPQAGraphNode& Node = Graph.Nodes[Index];
		const FString MermaidId = FString::Printf(TEXT("N%d"), Index);
		MermaidIds.Add(Node.Id, MermaidId);
		Output += FString::Printf(
			TEXT("  %s[\"%s<br/><small>%s</small>\"]\n"),
			*MermaidId,
			*BPQAExport::EscapeMermaidLabel(Node.Label),
			*BPQAExport::EscapeMermaidLabel(Node.Type));
	}

	for (const FBPQAGraphEdge& Edge : Graph.Edges)
	{
		const FString* FromId = MermaidIds.Find(Edge.From);
		const FString* ToId = MermaidIds.Find(Edge.To);
		if (!FromId || !ToId)
		{
			continue;
		}

		Output += FString::Printf(
			TEXT("  %s -->|%s| %s\n"),
			**FromId,
			*BPQAExport::EscapeMermaidLabel(Edge.Reason),
			**ToId);
	}

	return Output;
}

FString FBPQAGraphExporter::ExportToDot(const FBPQAGraphModel& Graph)
{
	FString Output = TEXT("digraph BlueprintInsight {\n");
	Output += TEXT("  graph [rankdir=LR, bgcolor=\"#1b1d24\"];\n");
	Output += TEXT("  node [shape=box, style=\"filled\", fillcolor=\"#24324a\", fontcolor=\"#f0f3f8\", color=\"#606a7d\"];\n");
	Output += TEXT("  edge [color=\"#8a94a6\", fontcolor=\"#d7dbe3\"];\n");

	TMap<FString, FString> DotIds;
	for (int32 Index = 0; Index < Graph.Nodes.Num(); ++Index)
	{
		const FBPQAGraphNode& Node = Graph.Nodes[Index];
		const FString DotId = FString::Printf(TEXT("N%d"), Index);
		DotIds.Add(Node.Id, DotId);

		const FString Label = FString::Printf(TEXT("%s\\n%s"), *Node.Label, *Node.Type);
		Output += FString::Printf(TEXT("  %s [label=\"%s\"];\n"), *DotId, *BPQAExport::EscapeDotLabel(Label));
	}

	for (const FBPQAGraphEdge& Edge : Graph.Edges)
	{
		const FString* FromId = DotIds.Find(Edge.From);
		const FString* ToId = DotIds.Find(Edge.To);
		if (!FromId || !ToId)
		{
			continue;
		}

		Output += FString::Printf(
			TEXT("  %s -> %s [label=\"%s\"];\n"),
			**FromId,
			**ToId,
			*BPQAExport::EscapeDotLabel(Edge.Reason));
	}

	Output += TEXT("}\n");
	return Output;
}

bool FBPQAGraphExporter::SaveGraphAsJson(const FBPQAGraphModel& Graph, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage)
{
	return BPQAExport::SaveText(ExportToJson(Graph), FileName, OutFullPath, OutErrorMessage);
}

bool FBPQAGraphExporter::SaveGraphAsMermaid(const FBPQAGraphModel& Graph, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage)
{
	return BPQAExport::SaveText(ExportToMermaid(Graph), FileName, OutFullPath, OutErrorMessage);
}

bool FBPQAGraphExporter::SaveGraphAsDot(const FBPQAGraphModel& Graph, const FString& FileName, FString& OutFullPath, FString& OutErrorMessage)
{
	return BPQAExport::SaveText(ExportToDot(Graph), FileName, OutFullPath, OutErrorMessage);
}
