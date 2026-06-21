#include "SBlueprintQueryAnalysisWidget.h"

#include "BPQAGraphExporter.h"
#include "BPQABlueprintNodeAnalyzer.h"
#include "BPQARuntimeFlowTracer.h"
#include "BPQAStaticDependencyAnalyzer.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Editor.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Interfaces/IPluginManager.h"
#include "IWebBrowserWindow.h"
#include "HAL/FileManager.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SWebBrowser.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

namespace BPQAWidget
{
	static constexpr uint8 DeferredActionNone = 0;
	static constexpr uint8 DeferredActionAnalyzeStatic = 1;
	static constexpr uint8 DeferredActionShowRuntime = 2;
	static constexpr uint8 DeferredActionStopRuntime = 3;
	static constexpr uint8 DeferredActionAnalyzeFolder = 4;
	static constexpr uint8 DeferredActionAnalyzeNodeFlow = 5;
	static constexpr int32 BrowserReadyTimeoutAttempts = 60;
	static constexpr int32 BrowserRenderedTimeoutAttempts = 60;

	static const FSlateBrush* WhiteBrush()
	{
		return FAppStyle::GetBrush(TEXT("WhiteBrush"));
	}

	static FLinearColor SidePanelColor()
	{
		return FLinearColor(0.026f, 0.029f, 0.036f, 1.0f);
	}

	static FLinearColor CenterPanelColor()
	{
		return FLinearColor(0.018f, 0.020f, 0.026f, 1.0f);
	}

	static FString GetGraphViewerHtmlPath()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintQueryAnalysis"));
		const FString PluginBaseDir = Plugin.IsValid()
			? Plugin->GetBaseDir()
			: FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("BlueprintQueryAnalysis"));
		return FPaths::Combine(
			PluginBaseDir,
			TEXT("Source/BlueprintQueryAnalysis/Private/Dependencylibraries/ReactFlow/dist/index.html"));
	}

	static FString GetGraphViewerFileUrl()
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(GetGraphViewerHtmlPath());
		FullPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		return FString::Printf(TEXT("file:///%s"), *FullPath);
	}

	static FString GetGraphViewerAbsoluteHtmlPath()
	{
		return FPaths::ConvertRelativePathToFull(GetGraphViewerHtmlPath());
	}

	static const TCHAR* ConsoleSeverityToString(EWebBrowserConsoleLogSeverity Severity)
	{
		switch (Severity)
		{
		case EWebBrowserConsoleLogSeverity::Verbose:
			return TEXT("Verbose");
		case EWebBrowserConsoleLogSeverity::Debug:
			return TEXT("Debug");
		case EWebBrowserConsoleLogSeverity::Info:
			return TEXT("Info");
		case EWebBrowserConsoleLogSeverity::Warning:
			return TEXT("Warning");
		case EWebBrowserConsoleLogSeverity::Error:
			return TEXT("Error");
		case EWebBrowserConsoleLogSeverity::Fatal:
			return TEXT("Fatal");
		case EWebBrowserConsoleLogSeverity::Default:
		default:
			return TEXT("Default");
		}
	}

	static FString EscapeJavaScriptSingleQuotedString(FString Value)
	{
		Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Value.ReplaceInline(TEXT("'"), TEXT("\\'"));
		Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		return Value;
	}

	static FString JoinArrayLines(const TCHAR* Title, const TArray<FString>& Values, int32 MaxCount = 12)
	{
		FString Output = FString::Printf(TEXT("\n%s:\n"), Title);
		if (Values.Num() == 0)
		{
			Output += TEXT("  <none>\n");
			return Output;
		}

		for (int32 Index = 0; Index < Values.Num() && Index < MaxCount; ++Index)
		{
			Output += FString::Printf(TEXT("  - %s\n"), *Values[Index]);
		}
		if (Values.Num() > MaxCount)
		{
			Output += FString::Printf(TEXT("  ... 还有 %d 项\n"), Values.Num() - MaxCount);
		}
		return Output;
	}

	static bool ParseNodeFlowNodeId(const FString& NodeId, FString& OutGraphName, FString& OutNodeGuid)
	{
		TArray<FString> Parts;
		NodeId.ParseIntoArray(Parts, TEXT(":"), true);
		if (Parts.Num() < 5 || Parts[0] != TEXT("NodeFlow") || Parts[2] != TEXT("Node"))
		{
			return false;
		}

		OutGraphName = Parts[3];
		OutNodeGuid = Parts[4];
		return true;
	}

	static FString TryFocusBlueprintNode(UBlueprint* Blueprint, const FBPQAGraphNode& Node)
	{
		if (!Blueprint)
		{
			return TEXT("未能打开蓝图资产。");
		}

		FString GraphName;
		FString NodeGuidString;
		if (!ParseNodeFlowNodeId(Node.Id, GraphName, NodeGuidString))
		{
			return FString::Printf(TEXT("已打开蓝图：%s"), *Blueprint->GetName());
		}

		FGuid NodeGuid;
		if (!FGuid::Parse(NodeGuidString, NodeGuid))
		{
			return FString::Printf(TEXT("已打开蓝图：%s"), *Blueprint->GetName());
		}

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for (UEdGraph* Graph : AllGraphs)
		{
			if (!Graph || Graph->GetName() != GraphName)
			{
				continue;
			}

			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (GraphNode && GraphNode->NodeGuid == NodeGuid)
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(GraphNode);
					return FString::Printf(TEXT("已定位到节点：%s / %s"), *Graph->GetName(), *GraphNode->GetName());
				}
			}
		}

		return FString::Printf(TEXT("已打开蓝图：%s"), *Blueprint->GetName());
	}
}

void SBlueprintQueryAnalysisGraphView::Construct(const FArguments& InArgs)
{
	Graph = InArgs._Graph;
	OnNodeSelected = InArgs._OnNodeSelected;
	OnNodeActivated = InArgs._OnNodeActivated;
	OnImageSaved = InArgs._OnImageSaved;
	bViewActive = InArgs._InitiallyActive;
	GraphViewerHtmlPath = BPQAWidget::GetGraphViewerAbsoluteHtmlPath();
	const FString GraphViewerFileUrl = BPQAWidget::GetGraphViewerFileUrl();
	const bool bHtmlExists = FPaths::FileExists(GraphViewerHtmlPath);
	const bool bWebBrowserModuleExists = FModuleManager::Get().ModuleExists(TEXT("WebBrowser"));

	UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis graph viewer path: %s"), *GraphViewerHtmlPath);
	UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis graph viewer url: %s"), *GraphViewerFileUrl);

	TSharedRef<SWidget> BrowserContent =
		SNew(SBorder)
		.BorderImage(BPQAWidget::WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.010f, 0.012f, 0.018f, 1.0f));

	if (!bHtmlExists)
	{
		SetBrowserError(TEXT("React Flow entry missing"), FString::Printf(TEXT("Local HTML file not found: %s"), *GraphViewerHtmlPath));
	}
	else if (!bWebBrowserModuleExists)
	{
		SetBrowserError(
			TEXT("WebBrowser module unavailable"),
			TEXT("BlueprintQueryAnalysis requires the Unreal WebBrowser module to render the React Flow view."));
	}
	else
	{
		BrowserContent = bViewActive ? MakeBrowserWidget(GraphViewerFileUrl) : MakeBrowserPlaceholder();
		bBrowserHtmlRequested = bViewActive;
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(BrowserHost, SBorder)
			.BorderImage(BPQAWidget::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.010f, 0.012f, 0.018f, 1.0f))
			[
				BrowserContent
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.Visibility(this, &SBlueprintQueryAnalysisGraphView::GetStateOverlayVisibility)
			.Padding(24.0f)
			.BorderImage(BPQAWidget::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.035f, 0.045f, 0.062f, 0.96f))
			[
	// Only show the Slate fallback for hard browser errors; React owns loading and empty states.
SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SBlueprintQueryAnalysisGraphView::GetStateTitleText)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FLinearColor(0.98f, 0.86f, 0.86f, 1.0f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &SBlueprintQueryAnalysisGraphView::GetStateDetailText)
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FLinearColor(0.74f, 0.78f, 0.86f, 1.0f))
				]
			]
		]
	];
}

void SBlueprintQueryAnalysisGraphView::SetActive(bool bInActive)
{
	if (bViewActive == bInActive)
	{
		return;
	}

	bViewActive = bInActive;
	if (!bViewActive)
	{
		return;
	}

	if (PendingGraphJson.IsEmpty())
	{
		PendingGraphJson = BuildGraphJson();
	}
	bPendingGraphPush = PendingGraphJson != LastPushedGraphJson;
	bPendingSelectionPush = true;
	bGraphPresentationPending = bPendingGraphPush && Graph.IsValid() && Graph->Nodes.Num() > 0;
	EnsureBrowserLoaded();
	QueueBrowserSync();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintQueryAnalysisGraphView::SetGraph(TSharedPtr<FBPQAGraphModel> InGraph)
{
	Graph = InGraph;
	bLoadingOverlayVisible = false;
	bHasPendingLoadingState = false;
	PendingGraphJson = BuildGraphJson();
	bPendingGraphPush = PendingGraphJson != LastPushedGraphJson;
	bGraphPresentationPending = bPendingGraphPush && Graph.IsValid() && Graph->Nodes.Num() > 0;
	if (bViewActive)
	{
		EnsureBrowserLoaded();
		PushGraphToBrowser();
		if (bPendingGraphPush)
		{
			QueueBrowserSync();
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintQueryAnalysisGraphView::SetSelectedNodeId(const FString& InSelectedNodeId)
{
	SelectedNodeId = InSelectedNodeId;
	bPendingSelectionPush = true;
	if (bViewActive)
	{
		EnsureBrowserLoaded();
		PushSelectionToBrowser();
		QueueBrowserSync();
	}
}

void SBlueprintQueryAnalysisGraphView::SetLoadingState(const FString& Message, float Progress)
{
	bHasPendingLoadingState = true;
	PendingLoadingMessage = Message;
	PendingLoadingProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	bLoadingOverlayVisible = true;
	if (bViewActive)
	{
		EnsureBrowserLoaded();
		PushLoadingStateToBrowser();
		QueueBrowserSync();
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintQueryAnalysisGraphView::HandleBrowserLoadStarted()
{
	bBrowserDocumentLoaded = false;
	bBrowserLoaded = false;
	bBrowserHtmlRequested = true;
	bBrowserErrorVisible = false;
	LastPushedGraphJson.Reset();
	BrowserErrorTitle.Reset();
	BrowserErrorDetail.Reset();
	UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis WebBrowser load started: %s"), Browser.IsValid() ? *Browser->GetUrl() : TEXT("<invalid browser>"));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintQueryAnalysisGraphView::HandleBrowserLoadCompleted()
{
	bBrowserDocumentLoaded = true;
	bBrowserHtmlRequested = true;
	UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis WebBrowser load completed: %s"), Browser.IsValid() ? *Browser->GetUrl() : TEXT("<invalid browser>"));
	ProbeBrowserReady();
	QueueBrowserSync();
}

void SBlueprintQueryAnalysisGraphView::HandleBrowserLoadError()
{
	SetBrowserError(
		TEXT("React Flow page failed to load"),
		FString::Printf(TEXT("Unable to load local entry: %s\nCurrent URL: %s"), *GraphViewerHtmlPath, Browser.IsValid() ? *Browser->GetUrl() : TEXT("<invalid browser>")));
}

void SBlueprintQueryAnalysisGraphView::HandleBrowserConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("BlueprintQueryAnalysis frontend console [%s] %s:%d %s"),
		BPQAWidget::ConsoleSeverityToString(Severity),
		*Source,
		Line,
		*Message);
}

bool SBlueprintQueryAnalysisGraphView::HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& /*Request*/)
{
	static const FString SelectPrefix = TEXT("bpqa://select/");
	static const FString OpenPrefix = TEXT("bpqa://open/");
	static const FString RenderedPrefix = TEXT("bpqa://rendered/");
	static const FString ErrorPrefix = TEXT("bpqa://error/");
	static const FString ImageBeginPrefix = TEXT("bpqa://image/begin/");
	static const FString ImageChunkPrefix = TEXT("bpqa://image/chunk/");
	static const FString ImageEndPrefix = TEXT("bpqa://image/end");

	if (Url == TEXT("bpqa://ready"))
	{
		bBrowserLoaded = true;
		bBrowserHtmlRequested = true;
		bBrowserErrorVisible = false;
		bPendingGraphPush = true;
		UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis frontend ready."));
		if (!bViewActive)
		{
			return true;
		}
		if (bHasPendingLoadingState)
		{
			PushLoadingStateToBrowser();
		}
		PushGraphToBrowser();
		PushSelectionToBrowser();
		Invalidate(EInvalidateWidgetReason::Paint);
		return true;
	}

	if (Url.StartsWith(RenderedPrefix))
	{
		if (!bViewActive)
		{
			return true;
		}
		if (!bBrowserLoaded)
		{
			UE_LOG(LogTemp, Warning, TEXT("BlueprintQueryAnalysis ignored frontend rendered before ready: %s"), *Url);
			return true;
		}

		bBrowserHtmlRequested = true;
		bBrowserErrorVisible = false;
		bGraphPresentationPending = false;
		bPendingGraphPush = false;
		UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis frontend rendered graph: %s"), *Url.RightChop(RenderedPrefix.Len()));
		Invalidate(EInvalidateWidgetReason::Paint);
		return true;
	}

	if (Url.StartsWith(ErrorPrefix))
	{
		const FString ErrorMessage = DecodeNodeIdPayload(Url.RightChop(ErrorPrefix.Len()));
		SetBrowserError(TEXT("React Flow 前端脚本错误"), ErrorMessage.IsEmpty() ? Url : ErrorMessage);
		return true;
	}

	// PNG export bridge: the offline CEF build cannot trigger a browser download,
	// so the frontend streams the rendered PNG here in url-safe base64 chunks and
	// the host writes it to disk alongside the JSON/Mermaid/DOT exports.
	if (Url.StartsWith(ImageBeginPrefix))
	{
		PendingImageName = DecodeNodeIdPayload(Url.RightChop(ImageBeginPrefix.Len()));
		PendingImageChunks.Reset();
		return true;
	}

	if (Url.StartsWith(ImageChunkPrefix))
	{
		const FString Rest = Url.RightChop(ImageChunkPrefix.Len());
		int32 SlashIndex = INDEX_NONE;
		if (Rest.FindChar(TEXT('/'), SlashIndex))
		{
			const int32 ChunkIndex = FCString::Atoi(*Rest.Left(SlashIndex));
			PendingImageChunks.Add(ChunkIndex, Rest.RightChop(SlashIndex + 1));
		}
		return true;
	}

	if (Url.StartsWith(ImageEndPrefix))
	{
		SaveImageFromChunks();
		return true;
	}

	if (Url.StartsWith(SelectPrefix))
	{
		const FString NodeId = DecodeNodeIdPayload(Url.RightChop(SelectPrefix.Len()));
		if (!NodeId.IsEmpty())
		{
			SelectedNodeId = NodeId;
			OnNodeSelected.ExecuteIfBound(NodeId);
			PushSelectionToBrowser();
		}
		return true;
	}

	if (Url.StartsWith(OpenPrefix))
	{
		const FString NodeId = DecodeNodeIdPayload(Url.RightChop(OpenPrefix.Len()));
		if (!NodeId.IsEmpty())
		{
			SelectedNodeId = NodeId;
			OnNodeSelected.ExecuteIfBound(NodeId);
			OnNodeActivated.ExecuteIfBound(NodeId);
			PushSelectionToBrowser();
		}
		return true;
	}

	if (Url.StartsWith(TEXT("http://")) || Url.StartsWith(TEXT("https://")))
	{
		return true;
	}

	return false;
}

FString SBlueprintQueryAnalysisGraphView::BuildGraphJson() const
{
	return Graph.IsValid()
		? FBPQAGraphExporter::ExportToJson(*Graph)
		: TEXT("{\"title\":\"Blueprint Insight\",\"mode\":\"StaticDependency\",\"nodes\":[],\"edges\":[]}");
}

TSharedRef<SWidget> SBlueprintQueryAnalysisGraphView::MakeBrowserPlaceholder() const
{
	return SNew(SBorder)
		.BorderImage(BPQAWidget::WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.010f, 0.012f, 0.018f, 1.0f));
}

TSharedRef<SWidget> SBlueprintQueryAnalysisGraphView::MakeBrowserWidget(const FString& InitialUrl)
{
	return SAssignNew(Browser, SWebBrowser)
		.InitialURL(InitialUrl)
		.ShowControls(false)
		.ShowAddressBar(false)
		.ShowErrorMessage(true)
		.SupportsTransparency(false)
		.ShowInitialThrobber(false)
		.BrowserFrameRate(30)
		.BackgroundColor(FColor(5, 6, 10, 255))
		.OnLoadStarted(this, &SBlueprintQueryAnalysisGraphView::HandleBrowserLoadStarted)
		.OnLoadCompleted(this, &SBlueprintQueryAnalysisGraphView::HandleBrowserLoadCompleted)
		.OnLoadError(this, &SBlueprintQueryAnalysisGraphView::HandleBrowserLoadError)
		.OnConsoleMessage(this, &SBlueprintQueryAnalysisGraphView::HandleBrowserConsoleMessage)
		.OnBeforeNavigation(this, &SBlueprintQueryAnalysisGraphView::HandleBeforeNavigation);
}

void SBlueprintQueryAnalysisGraphView::EnsureBrowserLoaded()
{
	if (!bViewActive || bBrowserErrorVisible)
	{
		return;
	}

	if (GraphViewerHtmlPath.IsEmpty())
	{
		GraphViewerHtmlPath = BPQAWidget::GetGraphViewerAbsoluteHtmlPath();
	}

	if (!FPaths::FileExists(GraphViewerHtmlPath))
	{
		SetBrowserError(TEXT("React Flow entry missing"), FString::Printf(TEXT("Local HTML file not found: %s"), *GraphViewerHtmlPath));
		return;
	}

	if (!FModuleManager::Get().ModuleExists(TEXT("WebBrowser")))
	{
		SetBrowserError(
			TEXT("WebBrowser module unavailable"),
			TEXT("BlueprintQueryAnalysis requires the Unreal WebBrowser module to render the React Flow view."));
		return;
	}

	if (!Browser.IsValid())
	{
		bBrowserHtmlRequested = true;
		if (BrowserHost.IsValid())
		{
			BrowserHost->SetContent(MakeBrowserWidget(BPQAWidget::GetGraphViewerFileUrl()));
		}
		return;
	}

	if (!bBrowserHtmlRequested)
	{
		LoadBrowserEntry();
	}
}

void SBlueprintQueryAnalysisGraphView::PushGraphToBrowser()
{
	if (!bViewActive || !Browser.IsValid() || !bBrowserLoaded)
	{
		return;
	}

	if (PendingGraphJson.IsEmpty())
	{
		PendingGraphJson = BuildGraphJson();
	}
	if (PendingGraphJson == LastPushedGraphJson)
	{
		Browser->ExecuteJavascript(TEXT("window.BPQAClearLoading && window.BPQAClearLoading();"));
		bPendingGraphPush = false;
		bHasPendingLoadingState = false;
		bGraphPresentationPending = false;
		bLoadingOverlayVisible = false;
		return;
	}

	Browser->ExecuteJavascript(FString::Printf(TEXT("window.BPQASetGraph && window.BPQASetGraph(%s);"), *PendingGraphJson));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("BlueprintQueryAnalysis pushed graph to frontend: %d nodes, %d edges."),
		Graph.IsValid() ? Graph->Nodes.Num() : 0,
		Graph.IsValid() ? Graph->Edges.Num() : 0);
	LastPushedGraphJson = PendingGraphJson;
	bPendingGraphPush = false;
	bHasPendingLoadingState = false;
	bGraphPresentationPending = false;
	bLoadingOverlayVisible = false;
}

void SBlueprintQueryAnalysisGraphView::PushSelectionToBrowser()
{
	if (!bViewActive || !Browser.IsValid() || !bBrowserLoaded)
	{
		return;
	}

	Browser->ExecuteJavascript(FString::Printf(
		TEXT("window.BPQASetSelected && window.BPQASetSelected('%s');"),
		*BPQAWidget::EscapeJavaScriptSingleQuotedString(SelectedNodeId)));
	bPendingSelectionPush = false;
}

void SBlueprintQueryAnalysisGraphView::PushLoadingStateToBrowser()
{
	if (!bViewActive || !Browser.IsValid() || !bBrowserLoaded || !bHasPendingLoadingState)
	{
		return;
	}

	Browser->ExecuteJavascript(FString::Printf(
		TEXT("window.BPQASetLoading && window.BPQASetLoading('%s', %s);"),
		*BPQAWidget::EscapeJavaScriptSingleQuotedString(PendingLoadingMessage),
		*FString::SanitizeFloat(PendingLoadingProgress)));
	bHasPendingLoadingState = false;
}

void SBlueprintQueryAnalysisGraphView::ProbeBrowserReady()
{
	if (!bViewActive || !Browser.IsValid() || bBrowserLoaded)
	{
		return;
	}

	// Deliver readiness through the iframe bridge (BPQANotifyHost). Navigating the
	// main frame's location.href to bpqa:// reloads the page in UE's CEF build,
	// which produced an endless load/ready/push/render/reload refresh loop.
	Browser->ExecuteJavascript(TEXT("if (window.BPQANotifyHost) { window.BPQANotifyHost('ready'); }"));
}

void SBlueprintQueryAnalysisGraphView::QueueBrowserSync()
{
	if (!bViewActive || !Browser.IsValid() || bBrowserSyncTimerActive || bBrowserErrorVisible)
	{
		return;
	}

	BrowserSyncAttempts = 0;
	bBrowserSyncTimerActive = true;
	RegisterActiveTimer(0.20f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintQueryAnalysisGraphView::RunBrowserSync));
}

EActiveTimerReturnType SBlueprintQueryAnalysisGraphView::RunBrowserSync(double InCurrentTime, float InDeltaTime)
{
	++BrowserSyncAttempts;

	if (!bViewActive)
	{
		bBrowserSyncTimerActive = false;
		return EActiveTimerReturnType::Stop;
	}

	if (bBrowserErrorVisible)
	{
		bBrowserSyncTimerActive = false;
		return EActiveTimerReturnType::Stop;
	}

	if (!bBrowserHtmlRequested)
	{
		LoadBrowserEntry();
	}
	else if (!bBrowserLoaded && (BrowserSyncAttempts == 1 || BrowserSyncAttempts % 5 == 0))
	{
		ProbeBrowserReady();
	}

	if (bHasPendingLoadingState)
	{
		PushLoadingStateToBrowser();
	}
	if (bPendingGraphPush)
	{
		// 只在确有待推送的新图时推送一次；bGraphPresentationPending 仅用于保持遮罩与超时检测，
	// Do not push the graph every tick; repeated fitView calls disturb cursor and zoom state.
		PushGraphToBrowser();
	}
	if (bPendingSelectionPush)
	{
		PushSelectionToBrowser();
	}

	const bool bNeedsMoreSync = !bBrowserLoaded || bPendingGraphPush || bPendingSelectionPush || bHasPendingLoadingState;
	const int32 TimeoutAttempts = !bBrowserLoaded ? BPQAWidget::BrowserReadyTimeoutAttempts : BPQAWidget::BrowserRenderedTimeoutAttempts;
	if (bNeedsMoreSync && BrowserSyncAttempts < TimeoutAttempts)
	{
		return EActiveTimerReturnType::Continue;
	}

	if (bNeedsMoreSync)
	{
		if (!bBrowserLoaded)
		{
			SetBrowserError(
				TEXT("React Flow frontend did not become ready"),
				FString::Printf(
					TEXT("No bpqa://ready within retry window.\nHTML: %s\nURL: %s\nRetry attempts: %d"),
					*GraphViewerHtmlPath,
					Browser.IsValid() ? *Browser->GetUrl() : TEXT("<invalid browser>"),
					BrowserSyncAttempts));
		}
		else if (bGraphPresentationPending)
		{
			SetBrowserError(
				TEXT("Graph data sent but frontend never acknowledged render"),
				FString::Printf(
					TEXT("No bpqa://rendered observed.\nNodes: %d\nEdges: %d\nRetry attempts: %d"),
					Graph.IsValid() ? Graph->Nodes.Num() : 0,
					Graph.IsValid() ? Graph->Edges.Num() : 0,
					BrowserSyncAttempts));
		}
	}

	bBrowserSyncTimerActive = false;
	return EActiveTimerReturnType::Stop;
}

void SBlueprintQueryAnalysisGraphView::LoadBrowserEntry()
{
	if (!bViewActive || !Browser.IsValid())
	{
		return;
	}

	if (GraphViewerHtmlPath.IsEmpty())
	{
		GraphViewerHtmlPath = BPQAWidget::GetGraphViewerAbsoluteHtmlPath();
	}

	if (!FPaths::FileExists(GraphViewerHtmlPath))
	{
		SetBrowserError(TEXT("React Flow 前端入口缺失"), FString::Printf(TEXT("找不到本地文件：%s"), *GraphViewerHtmlPath));
		return;
	}

	bBrowserHtmlRequested = true;
	const FString FileUrl = BPQAWidget::GetGraphViewerFileUrl();
	UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis loading frontend entry: %s"), *FileUrl);
	Browser->LoadURL(FileUrl);
}

void SBlueprintQueryAnalysisGraphView::SetBrowserError(const FString& Title, const FString& Detail)
{
	bBrowserErrorVisible = true;
	BrowserErrorTitle = Title;
	BrowserErrorDetail = Detail;
	bLoadingOverlayVisible = false;
	bGraphPresentationPending = false;
	bPendingGraphPush = false;
	bPendingSelectionPush = false;
	bBrowserSyncTimerActive = false;
	UE_LOG(LogTemp, Error, TEXT("BlueprintQueryAnalysis graph viewer error: %s - %s"), *Title, *Detail);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintQueryAnalysisGraphView::SaveImageFromChunks()
{
	if (PendingImageChunks.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlueprintQueryAnalysis PNG export received no data."));
		return;
	}

	// Reassemble chunks in index order; navigation delivery order is not guaranteed.
	TArray<int32> Indices;
	PendingImageChunks.GetKeys(Indices);
	Indices.Sort();

	FString Combined;
	for (const int32 Index : Indices)
	{
		Combined += PendingImageChunks[Index];
	}
	PendingImageChunks.Reset();

	// Convert url-safe base64 back to standard base64 (with padding) so the binary
	// decode overload accepts it.
	Combined.ReplaceInline(TEXT("-"), TEXT("+"));
	Combined.ReplaceInline(TEXT("_"), TEXT("/"));
	while (Combined.Len() % 4 != 0)
	{
		Combined.AppendChar(TEXT('='));
	}

	TArray<uint8> ImageBytes;
	if (!FBase64::Decode(Combined, ImageBytes) || ImageBytes.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("BlueprintQueryAnalysis failed to decode exported PNG data."));
		return;
	}

	FString FileName = FPaths::GetCleanFilename(PendingImageName);
	if (FileName.IsEmpty())
	{
		FileName = TEXT("BlueprintQueryAnalysis.png");
	}
	PendingImageName.Reset();

	const FString ExportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintQueryAnalysis"));
	IFileManager::Get().MakeDirectory(*ExportDirectory, true);
	const FString FullPath = FPaths::Combine(ExportDirectory, FileName);

	if (FFileHelper::SaveArrayToFile(ImageBytes, *FullPath))
	{
		UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis saved PNG export: %s (%d bytes)"), *FullPath, ImageBytes.Num());
		OnImageSaved.ExecuteIfBound(FullPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BlueprintQueryAnalysis failed to write PNG export: %s"), *FullPath);
	}
}

EVisibility SBlueprintQueryAnalysisGraphView::GetStateOverlayVisibility() const
{
	// Only browser-level hard errors use the Slate overlay; React handles the rest.
	return bBrowserErrorVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBlueprintQueryAnalysisGraphView::GetLoadingPanelVisibility() const
{
	return (bLoadingOverlayVisible && !bBrowserErrorVisible) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBlueprintQueryAnalysisGraphView::GetBusyVisibility() const
{
	// Loading and render-confirmation waiting are both busy states.
	const bool bBusy = (bLoadingOverlayVisible || bGraphPresentationPending) && !bBrowserErrorVisible;
	return bBusy ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBlueprintQueryAnalysisGraphView::GetIdlePanelVisibility() const
{
	return bLoadingOverlayVisible ? EVisibility::Collapsed : EVisibility::Visible;
}

FText SBlueprintQueryAnalysisGraphView::GetStateTitleText() const
{
	if (bBrowserErrorVisible)
	{
		return FText::FromString(BrowserErrorTitle.IsEmpty() ? TEXT("React Flow 视图错误") : BrowserErrorTitle);
	}

	if (bLoadingOverlayVisible)
	{
		return FText::FromString(PendingLoadingMessage.IsEmpty() ? TEXT("正在加载图数据") : PendingLoadingMessage);
	}

	if (bGraphPresentationPending)
	{
		return FText::FromString(TEXT("正在渲染图视图"));
	}

	if (Graph.IsValid() && Graph->Mode == EBPQAViewMode::RuntimeFlow)
	{
		return FText::FromString(TEXT("需要运行 PIE 或运行一次以获得运行流程数据"));
	}

	if (Graph.IsValid() && Graph->Mode == EBPQAViewMode::NodeFlow)
	{
		return FText::FromString(TEXT("需要选择蓝图并生成类内节点流程"));
	}

	return FText::FromString(TEXT("需要执行扫描静态依赖"));
}

FText SBlueprintQueryAnalysisGraphView::GetStateDetailText() const
{
	if (bBrowserErrorVisible)
	{
		return FText::FromString(BrowserErrorDetail.IsEmpty() ? TEXT("请查看 Output Log 中的 BlueprintQueryAnalysis WebBrowser 日志。") : BrowserErrorDetail);
	}

	if (bLoadingOverlayVisible)
	{
		return FText::FromString(TEXT("正在读取本地项目数据并更新图视图，不访问网络。"));
	}

	if (bGraphPresentationPending)
	{
		return FText::GetEmpty();
	}

	if (Graph.IsValid() && Graph->Mode == EBPQAViewMode::RuntimeFlow)
	{
		return FText::FromString(TEXT("点击左侧“开始运行采集”，进入 PIE 或触发加载流程，然后点击“停止运行采集”。"));
	}

	if (Graph.IsValid() && Graph->Mode == EBPQAViewMode::NodeFlow)
	{
		return FText::FromString(TEXT("双击运行流程节点会进入类内节点流程，显示该蓝图内可跟踪的事件、函数、宏和构造脚本节点。"));
	}

	return FText::FromString(TEXT("点击左侧“扫描静态依赖”，读取 /All/Game 对应的本地项目内容。"));
}

TOptional<float> SBlueprintQueryAnalysisGraphView::GetLoadingProgress() const
{
	// Loading uses real progress; render waiting uses indeterminate progress.
return bLoadingOverlayVisible ? TOptional<float>(PendingLoadingProgress) : TOptional<float>();
}

FString SBlueprintQueryAnalysisGraphView::DecodeNodeIdPayload(FString Payload) const
{
	Payload.TrimStartAndEndInline();
	FString NodeId;
	if (!FBase64::Decode(Payload, NodeId, EBase64Mode::UrlSafe))
	{
		return FString();
	}
	return NodeId;
}

void SBlueprintQueryAnalysisWidget::Construct(const FArguments& InArgs)
{
	RuntimeTracer = InArgs._RuntimeTracer;
	StaticAnalyzer = MakeShared<FBPQAStaticDependencyAnalyzer>();
	BlueprintNodeAnalyzer = MakeShared<FBPQABlueprintNodeAnalyzer>();

	StructureGraph = MakeShared<FBPQAGraphModel>();
	StructureGraph->Reset(TEXT("Blueprint Insight"), EBPQAViewMode::StaticDependency);
	RuntimeGraph = MakeShared<FBPQAGraphModel>();
	NodeFlowGraph = MakeShared<FBPQAGraphModel>();
    RuntimeGraph->Reset(TEXT("运行流程视图"), EBPQAViewMode::RuntimeFlow);

    // 默认激活结构视图。
    CurrentGraph = StructureGraph;
	ActiveViewIndex = StructureViewIndex;
    StatusMessage = TEXT("准备就绪。打开 Window -> Blueprint Insight 后，先点击“扫描静态依赖”。");

	ChildSlot
	[
		SNew(SSplitter)
		+ SSplitter::Slot()
		.Value(0.20f)
		[
			BuildLeftPanel()
		]
		+ SSplitter::Slot()
		.Value(0.56f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			.BorderImage(BPQAWidget::WhiteBrush())
			.BorderBackgroundColor(BPQAWidget::CenterPanelColor())
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				SAssignNew(ViewSwitcher, SWidgetSwitcher)
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(StructureView, SBlueprintQueryAnalysisGraphView)
					.Graph(StructureGraph)
					.InitiallyActive(true)
					.OnNodeSelected(this, &SBlueprintQueryAnalysisWidget::SelectNode)
					.OnNodeActivated(this, &SBlueprintQueryAnalysisWidget::OpenAssetForNode)
					.OnImageSaved(this, &SBlueprintQueryAnalysisWidget::HandleImageSaved)
				]
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(RuntimeView, SBlueprintQueryAnalysisGraphView)
					.Graph(RuntimeGraph)
					.InitiallyActive(false)
					.OnNodeSelected(this, &SBlueprintQueryAnalysisWidget::SelectNode)
					.OnNodeActivated(this, &SBlueprintQueryAnalysisWidget::OpenAssetForNode)
					.OnImageSaved(this, &SBlueprintQueryAnalysisWidget::HandleImageSaved)
				]
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(NodeFlowView, SBlueprintQueryAnalysisGraphView)
					.Graph(NodeFlowGraph)
					.InitiallyActive(false)
					.OnNodeSelected(this, &SBlueprintQueryAnalysisWidget::SelectNode)
					.OnNodeActivated(this, &SBlueprintQueryAnalysisWidget::OpenAssetForNode)
					.OnImageSaved(this, &SBlueprintQueryAnalysisWidget::HandleImageSaved)
				]
			]
		]
		+ SSplitter::Slot()
		.Value(0.24f)
		[
			BuildDetailsPanel()
		]
	];

	GraphView = StructureView;
	if (ViewSwitcher.IsValid())
	{
		ViewSwitcher->SetActiveWidgetIndex(StructureViewIndex);
	}

}

void SBlueprintQueryAnalysisWidget::SetActiveView(int32 ViewIndex)
{
	ActiveViewIndex = ViewIndex;
	if (ViewSwitcher.IsValid())
	{
		ViewSwitcher->SetActiveWidgetIndex(ViewIndex);
	}

	if (ViewIndex == RuntimeViewIndex)
	{
		CurrentGraph = RuntimeGraph;
		GraphView = RuntimeView;
	}
	else if (ViewIndex == NodeFlowViewIndex)
	{
		CurrentGraph = NodeFlowGraph;
		GraphView = NodeFlowView;
	}
	else
	{
		CurrentGraph = StructureGraph;
		GraphView = StructureView;
	}

	SelectedNodeId.Reset();
	if (StructureView.IsValid())
	{
		StructureView->SetActive(ViewIndex == StructureViewIndex);
	}
	if (RuntimeView.IsValid())
	{
		RuntimeView->SetActive(ViewIndex == RuntimeViewIndex);
	}
	if (NodeFlowView.IsValid())
	{
		NodeFlowView->SetActive(ViewIndex == NodeFlowViewIndex);
	}
	if (GraphView.IsValid())
	{
		GraphView->SetGraph(CurrentGraph);
		GraphView->SetSelectedNodeId(SelectedNodeId);
	}
}

TSharedRef<SWidget> SBlueprintQueryAnalysisWidget::BuildLeftPanel()
{
	return SNew(SBorder)
		.Padding(10.0f)
		.BorderImage(BPQAWidget::WhiteBrush())
		.BorderBackgroundColor(BPQAWidget::SidePanelColor())
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Blueprint Insight")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("扫描静态依赖")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::AnalyzeStaticGraph)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("返回文件夹总览")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::BackToFolderOverview)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
			SNew(SButton)
                .Text(FText::FromString(TEXT("运行流程视图")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::ShowRuntimeGraph)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("类内节点流程")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::ShowNodeFlowGraph)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("刷新当前视图")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::RefreshCurrentView)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("开始运行采集")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::StartRuntimeCapture)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("停止运行采集")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::StopRuntimeCapture)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 3.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SBlueprintQueryAnalysisWidget::GetAutoTrackPIECheckState)
				.OnCheckStateChanged(this, &SBlueprintQueryAnalysisWidget::HandleAutoTrackPIEChanged)
				[
					SNew(STextBlock)
                    .Text(FText::FromString(TEXT("PIE 运行时是否自动跟踪运行流程？")))
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("导出 JSON")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::ExportJson)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("导出 Mermaid")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::ExportMermaid)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("导出 DOT")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::ExportDot)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
                .Text(FText::FromString(TEXT("打开选中资产")))
				.OnClicked(this, &SBlueprintQueryAnalysisWidget::OpenSelectedAsset)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SBlueprintQueryAnalysisWidget::GetSummaryText)
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.72f, 0.76f, 0.82f, 1.0f))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
                .Text(FText::FromString(TEXT("左键选中；双击打开或进入文件夹；拖拽空白处平移；滚轮以鼠标位置缩放。")))
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f, 1.0f))
			]
		];
}

TSharedRef<SWidget> SBlueprintQueryAnalysisWidget::BuildDetailsPanel()
{
	return SNew(SBorder)
		.Padding(10.0f)
		.BorderImage(BPQAWidget::WhiteBrush())
		.BorderBackgroundColor(BPQAWidget::SidePanelColor())
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
                .Text(FText::FromString(TEXT("详情")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(this, &SBlueprintQueryAnalysisWidget::GetStatusText)
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.80f, 0.83f, 0.88f, 1.0f))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(STextBlock)
					.Text(this, &SBlueprintQueryAnalysisWidget::GetSelectedNodeText)
					.AutoWrapText(true)
					.ColorAndOpacity(FLinearColor(0.88f, 0.90f, 0.94f, 1.0f))
				]
			]
		];
}

FReply SBlueprintQueryAnalysisWidget::AnalyzeStaticGraph()
{
	SetActiveView(StructureViewIndex);
	if (GraphView.IsValid())
	{
        GraphView->SetLoadingState(TEXT("正在扫描 /All/Game 项目文件夹"), 0.18f);
	}
	QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeStatic);
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::BackToFolderOverview()
{
	return AnalyzeStaticGraph();
}

FReply SBlueprintQueryAnalysisWidget::ShowRuntimeGraph()
{
	if (!RuntimeTracer.IsValid())
	{
        StatusMessage = TEXT("运行时采集器不可用。");
		return FReply::Handled();
	}

	SetActiveView(RuntimeViewIndex);
	if (GraphView.IsValid())
	{
		GraphView->SetLoadingState(TEXT("正在整理运行流程事件"), 0.35f);
	}
	QueueDeferredAction(BPQAWidget::DeferredActionShowRuntime);
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::RefreshCurrentView()
{
	if (!GraphView.IsValid() || !CurrentGraph.IsValid())
	{
		StatusMessage = TEXT("当前没有可刷新的视图。");
		return FReply::Handled();
	}

	if (ActiveViewIndex == RuntimeViewIndex)
	{
		if (!RuntimeTracer.IsValid())
		{
			StatusMessage = TEXT("运行时采集器不可用。");
			return FReply::Handled();
		}

		GraphView->SetLoadingState(TEXT("正在刷新运行流程视图"), 0.35f);
		QueueDeferredAction(BPQAWidget::DeferredActionShowRuntime);
		return FReply::Handled();
	}

	if (ActiveViewIndex == NodeFlowViewIndex)
	{
		if (!CurrentNodeFlowAssetPath.IsEmpty())
		{
			PendingAssetPath = CurrentNodeFlowAssetPath;
			GraphView->SetLoadingState(TEXT("正在刷新类内节点流程"), 0.58f);
			QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeNodeFlow, FString(), SelectedNodeId);
			return FReply::Handled();
		}

		GraphView->SetGraph(NodeFlowGraph);
		GraphView->SetSelectedNodeId(SelectedNodeId);
		StatusMessage = TEXT("类内节点流程视图已刷新。");
		return FReply::Handled();
	}

	if (!CurrentFolderPath.IsEmpty())
	{
		GraphView->SetLoadingState(TEXT("正在刷新当前文件夹视图"), 0.28f);
		QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeFolder, CurrentFolderPath);
		return FReply::Handled();
	}

	GraphView->SetLoadingState(TEXT("正在刷新静态依赖视图"), 0.18f);
	QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeStatic);
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::StartRuntimeCapture()
{
	if (RuntimeTracer.IsValid())
	{
		SetActiveView(RuntimeViewIndex);
		RuntimeTracer->StartCapture();
		ResetRuntimePresentationState();
        CurrentGraph->Reset(TEXT("运行时真实加载/初始化顺序图"), EBPQAViewMode::RuntimeFlow);
		CurrentFolderPath.Reset();
		CurrentNodeFlowAssetPath.Reset();
		SelectedNodeId.Reset();
		if (GraphView.IsValid())
		{
			GraphView->SetGraph(CurrentGraph);
			GraphView->SetSelectedNodeId(SelectedNodeId);
		}
        StatusMessage = TEXT("运行采集已开始。现在进入 PIE 或触发加载流程即可记录事件；需要查看最新图时点击“刷新当前视图”。");
	}
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::StopRuntimeCapture()
{
	if (RuntimeTracer.IsValid())
	{
		SetActiveView(RuntimeViewIndex);
		if (GraphView.IsValid())
		{
            GraphView->SetLoadingState(TEXT("正在停止采集并生成运行流程视图"), 0.45f);
		}
		QueueDeferredAction(BPQAWidget::DeferredActionStopRuntime);
	}
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::ShowNodeFlowPlaceholder()
{
	SetActiveView(StructureViewIndex);
    CurrentGraph->Reset(TEXT("类内节点流程图"), EBPQAViewMode::NodeFlow);
	CurrentFolderPath.Reset();

	FBPQAGraphNode Node;
	Node.Id = TEXT("NodeFlowPlaceholder");
	Node.Label = TEXT("类内节点流程分析");
	Node.Type = TEXT("Planned");
    Node.Tooltip = TEXT("后续扩展：读取 UbergraphPages / FunctionGraphs / MacroGraphs，并解析 UK2Node 调用关系。");
	CurrentGraph->AddOrUpdateNode(Node);

    StatusMessage = TEXT("类内节点流程是 v0.5 扩展入口；第一版先完成静态依赖和运行加载流程。");
	SelectedNodeId.Reset();
	GraphView->SetGraph(CurrentGraph);
	GraphView->SetSelectedNodeId(SelectedNodeId);
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::ShowNodeFlowGraph()
{
	const FBPQAGraphNode* Node = GetSelectedNode();
	if (!Node)
	{
        StatusMessage = TEXT("请先选中一个可分析的蓝图节点。");
		return FReply::Handled();
	}

	if (Node->AssetPath.IsEmpty() && Node->PackageName.IsEmpty())
	{
        StatusMessage = TEXT("当前节点没有可用于蓝图分析的资产路径。");
		return FReply::Handled();
	}

	PendingAssetPath = !Node->AssetPath.IsEmpty() ? Node->AssetPath : Node->PackageName;
	PendingNodeId = Node->Id;
	SetActiveView(NodeFlowViewIndex);
	if (GraphView.IsValid())
	{
		GraphView->SetLoadingState(TEXT("正在整理类内节点流程"), 0.58f);
	}
	QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeNodeFlow, FString(), Node->Id);
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::ExportJson()
{
	if (!CurrentGraph.IsValid() || CurrentGraph->Nodes.Num() == 0)
	{
        StatusMessage = TEXT("当前没有可导出的图。");
		return FReply::Handled();
	}

	FString FullPath;
	FString ErrorMessage;
	const FString FileName = MakeExportBaseName() + TEXT(".json");
	if (FBPQAGraphExporter::SaveGraphAsJson(*CurrentGraph, FileName, FullPath, ErrorMessage))
	{
		StatusMessage = FString::Printf(TEXT("JSON 已导出：%s"), *FullPath);
	}
	else
	{
		StatusMessage = ErrorMessage;
	}
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::ExportMermaid()
{
	if (!CurrentGraph.IsValid() || CurrentGraph->Nodes.Num() == 0)
	{
        StatusMessage = TEXT("当前没有可导出的图。");
		return FReply::Handled();
	}

	FString FullPath;
	FString ErrorMessage;
	const FString FileName = MakeExportBaseName() + TEXT(".mmd");
	if (FBPQAGraphExporter::SaveGraphAsMermaid(*CurrentGraph, FileName, FullPath, ErrorMessage))
	{
		StatusMessage = FString::Printf(TEXT("Mermaid 已导出：%s"), *FullPath);
	}
	else
	{
		StatusMessage = ErrorMessage;
	}
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::ExportDot()
{
	if (!CurrentGraph.IsValid() || CurrentGraph->Nodes.Num() == 0)
	{
        StatusMessage = TEXT("当前没有可导出的图。");
		return FReply::Handled();
	}

	FString FullPath;
	FString ErrorMessage;
	const FString FileName = MakeExportBaseName() + TEXT(".dot");
	if (FBPQAGraphExporter::SaveGraphAsDot(*CurrentGraph, FileName, FullPath, ErrorMessage))
	{
		StatusMessage = FString::Printf(TEXT("DOT 已导出：%s"), *FullPath);
	}
	else
	{
		StatusMessage = ErrorMessage;
	}
	return FReply::Handled();
}

FReply SBlueprintQueryAnalysisWidget::OpenSelectedAsset()
{
	OpenAssetForNode(SelectedNodeId);
	return FReply::Handled();
}

ECheckBoxState SBlueprintQueryAnalysisWidget::GetAutoTrackPIECheckState() const
{
	return RuntimeTracer.IsValid() && RuntimeTracer->IsAutoCaptureOnPIE()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SBlueprintQueryAnalysisWidget::HandleAutoTrackPIEChanged(ECheckBoxState NewState)
{
	if (!RuntimeTracer.IsValid())
	{
        StatusMessage = TEXT("运行时采集器不可用。");
		return;
	}

	const bool bEnabled = NewState == ECheckBoxState::Checked;
	RuntimeTracer->SetAutoCaptureOnPIE(bEnabled);
	StatusMessage = bEnabled
        ? TEXT("PIE 自动跟踪已开启：每次进入 PIE 会覆盖生成新的运行流程数据。")
        : TEXT("PIE 自动跟踪已关闭：需要手动点击“开始运行采集”。");
}

void SBlueprintQueryAnalysisWidget::HandleImageSaved(const FString& FullPath)
{
	StatusMessage = FString::Printf(TEXT("PNG 已导出：%s"), *FullPath);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SBlueprintQueryAnalysisWidget::SelectNode(const FString& NodeId)
{
	SelectedNodeId = NodeId;
	if (GraphView.IsValid())
	{
		GraphView->SetSelectedNodeId(NodeId);
	}
}

void SBlueprintQueryAnalysisWidget::OpenAssetForNode(const FString& NodeId)
{
	if (!CurrentGraph.IsValid())
	{
		return;
	}

	const FBPQAGraphNode* Node = CurrentGraph->FindNodeById(NodeId);
	if (!Node)
	{
        StatusMessage = TEXT("没有选中的图节点。");
		return;
	}

	if (IsFolderNode(*Node))
	{
		const FString FolderPath = !Node->FolderPath.IsEmpty() ? Node->FolderPath : Node->AssetPath;
		QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeFolder, FolderPath);
		return;
	}

	if (CurrentGraph->Mode == EBPQAViewMode::RuntimeFlow)
	{
		PendingAssetPath = !Node->AssetPath.IsEmpty() ? Node->AssetPath : Node->PackageName;
		SetActiveView(NodeFlowViewIndex);
		if (GraphView.IsValid())
		{
			GraphView->SetLoadingState(TEXT("正在整理类内节点流程"), 0.58f);
		}
		QueueDeferredAction(BPQAWidget::DeferredActionAnalyzeNodeFlow, FString(), NodeId);
		return;
	}

	const FString PackageName = !Node->PackageName.IsEmpty() ? Node->PackageName : Node->Id;
	if (!PackageName.StartsWith(TEXT("/Game/")))
	{
        StatusMessage = TEXT("该节点不是项目资产，无法在资产编辑器中打开。");
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData;
	if (!Node->AssetPath.IsEmpty() && Node->AssetPath.Contains(TEXT(".")))
	{
		AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Node->AssetPath));
	}

	if (!AssetData.IsValid())
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(FName(*PackageName), Assets);
		if (Assets.Num() > 0)
		{
			AssetData = Assets[0];
		}
	}

	if (!AssetData.IsValid())
	{
		StatusMessage = FString::Printf(TEXT("找不到资产数据：%s"), *PackageName);
		return;
	}

	UObject* Asset = AssetData.GetAsset();
	if (!Asset)
	{
        StatusMessage = FString::Printf(TEXT("资产加载失败：%s"), *PackageName);
		return;
	}

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->OpenEditorForAsset(Asset);
		}
	}

	if (CurrentGraph->Mode == EBPQAViewMode::NodeFlow)
	{
		StatusMessage = BPQAWidget::TryFocusBlueprintNode(Cast<UBlueprint>(Asset), *Node);
		return;
	}

    StatusMessage = FString::Printf(TEXT("已打开资产：%s"), *AssetData.GetObjectPathString());
}

FText SBlueprintQueryAnalysisWidget::GetSummaryText() const
{
	const int32 NodeCount = CurrentGraph.IsValid() ? CurrentGraph->Nodes.Num() : 0;
	const int32 EdgeCount = CurrentGraph.IsValid() ? CurrentGraph->Edges.Num() : 0;
	const int32 RuntimeEventCount = RuntimeTracer.IsValid() ? RuntimeTracer->GetEvents().Num() : 0;
	const FString CaptureState = RuntimeTracer.IsValid() && RuntimeTracer->IsCapturing() ? TEXT("Capturing") : TEXT("Idle");
	FString ScopeText = CurrentFolderPath.IsEmpty() ? TEXT("/All/Game 文件夹总览") : CurrentFolderPath;
	if (CurrentGraph.IsValid() && CurrentGraph->Mode == EBPQAViewMode::RuntimeFlow)
	{
		ScopeText = TEXT("运行流程视图");
	}
	else if (CurrentGraph.IsValid() && CurrentGraph->Mode == EBPQAViewMode::NodeFlow)
	{
		ScopeText = TEXT("类内节点流程视图");
	}

	return FText::FromString(FString::Printf(
        TEXT("当前范围: %s\n节点: %d\n边: %d\n运行事件: %d\n采集状态: %s"),
		*ScopeText,
		NodeCount,
		EdgeCount,
		RuntimeEventCount,
		*CaptureState));
}

FText SBlueprintQueryAnalysisWidget::GetStatusText() const
{
	return FText::FromString(StatusMessage);
}

FText SBlueprintQueryAnalysisWidget::GetSelectedNodeText() const
{
	const FBPQAGraphNode* Node = GetSelectedNode();
	if (!Node)
	{
        return FText::FromString(TEXT("未选择节点。"));
	}

	FString Output;
	Output += FString::Printf(TEXT("Label: %s\n"), *Node->Label);
	Output += FString::Printf(TEXT("Type: %s\n"), *Node->Type);
	Output += FString::Printf(TEXT("AssetPath: %s\n"), *Node->AssetPath);
	Output += FString::Printf(TEXT("Package: %s\n"), *Node->PackageName);
	Output += FString::Printf(TEXT("Folder: %s\n"), *Node->FolderPath);
	Output += FString::Printf(TEXT("Class: %s\n"), *Node->ClassName);
	Output += FString::Printf(TEXT("ParentClass: %s\n"), *Node->ParentClass);
	Output += FString::Printf(TEXT("Time: %.3f\n"), Node->TimeSeconds);
	Output += FString::Printf(TEXT("Depth: %d\n"), Node->Depth);
	Output += FString::Printf(TEXT("Tooltip: %s\n"), *Node->Tooltip);
	Output += BPQAWidget::JoinArrayLines(TEXT("References"), Node->References);
	Output += BPQAWidget::JoinArrayLines(TEXT("ReferencedBy"), Node->ReferencedBy);
	Output += BPQAWidget::JoinArrayLines(TEXT("Interfaces"), Node->Interfaces);
	Output += BPQAWidget::JoinArrayLines(TEXT("Components / Notes"), Node->Components);
	return FText::FromString(Output);
}

const FBPQAGraphNode* SBlueprintQueryAnalysisWidget::GetSelectedNode() const
{
	return CurrentGraph.IsValid() && !SelectedNodeId.IsEmpty()
		? CurrentGraph->FindNodeById(SelectedNodeId)
		: nullptr;
}

FString SBlueprintQueryAnalysisWidget::MakeExportBaseName() const
{
	FString ModeName = TEXT("StaticDependency");
	if (CurrentGraph.IsValid())
	{
		switch (CurrentGraph->Mode)
		{
		case EBPQAViewMode::FolderDependency:
			ModeName = TEXT("FolderDependency");
			break;
		case EBPQAViewMode::FolderAssetDependency:
			ModeName = TEXT("FolderAssetDependency");
			break;
		case EBPQAViewMode::RuntimeFlow:
			ModeName = TEXT("RuntimeFlow");
			break;
		case EBPQAViewMode::NodeFlow:
			ModeName = TEXT("NodeFlow");
			break;
		case EBPQAViewMode::StaticDependency:
		default:
			ModeName = TEXT("StaticDependency");
			break;
		}
	}
	return FString::Printf(TEXT("BPInsight_%s_%s"), *ModeName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
}

bool SBlueprintQueryAnalysisWidget::IsFolderNode(const FBPQAGraphNode& Node) const
{
	return Node.Type == TEXT("Folder") || Node.Type == TEXT("ExternalFolder");
}

void SBlueprintQueryAnalysisWidget::QueueDeferredAction(uint8 Action, const FString& FolderPath, const FString& NodeId)
{
	PendingAction = Action;
	PendingFolderPath = FolderPath;
	PendingNodeId = NodeId;
	RegisterActiveTimer(0.05f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintQueryAnalysisWidget::RunDeferredAction));
}

EActiveTimerReturnType SBlueprintQueryAnalysisWidget::RunDeferredAction(double InCurrentTime, float InDeltaTime)
{
	const uint8 Action = PendingAction;
	const FString FolderPath = PendingFolderPath;
	const FString NodeId = PendingNodeId;
	PendingAction = BPQAWidget::DeferredActionNone;
	PendingFolderPath.Reset();
	PendingNodeId.Reset();

	switch (Action)
	{
	case BPQAWidget::DeferredActionAnalyzeStatic:
		ExecuteAnalyzeStaticGraph();
		break;
	case BPQAWidget::DeferredActionShowRuntime:
		ExecuteShowRuntimeGraph();
		break;
	case BPQAWidget::DeferredActionStopRuntime:
		ExecuteStopRuntimeCapture();
		break;
	case BPQAWidget::DeferredActionAnalyzeFolder:
		ExecuteAnalyzeFolder(FolderPath);
		break;
	case BPQAWidget::DeferredActionAnalyzeNodeFlow:
		ExecuteAnalyzeNodeFlow(NodeId);
		break;
	default:
		break;
	}

	PendingAssetPath.Reset();

	return EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType SBlueprintQueryAnalysisWidget::PollRuntimeGraph(double InCurrentTime, float InDeltaTime)
{
	if (!RuntimeTracer.IsValid() || !RuntimeView.IsValid())
	{
		return EActiveTimerReturnType::Continue;
	}

	const bool bIsCapturing = RuntimeTracer->IsCapturing();
	const int32 EventCount = RuntimeTracer->GetEvents().Num();
	const bool bNeedsRuntimeRefresh = EventCount != LastRuntimeEventCount
		|| bIsCapturing != bLastRuntimeCaptureState;

	if (!bNeedsRuntimeRefresh)
	{
		return EActiveTimerReturnType::Continue;
	}

	RuntimeTracer->RebuildRuntimeGraph();
	*RuntimeGraph = RuntimeTracer->GetRuntimeGraph();
	LastRuntimeEventCount = EventCount;
	bLastRuntimeCaptureState = bIsCapturing;

	RuntimeView->SetGraph(RuntimeGraph);
	if (ActiveViewIndex == RuntimeViewIndex)
	{
		CurrentGraph = RuntimeGraph;
		GraphView = RuntimeView;
		StatusMessage = bIsCapturing
            ? FString::Printf(TEXT("PIE 运行流程正在实时跟踪：已采集 %d 个原始事件，有新事件时更新树状图。"), EventCount)
            : FString::Printf(TEXT("PIE 运行流程跟踪已停止：已采集 %d 个原始事件。"), EventCount);
	}

	return EActiveTimerReturnType::Continue;
}

void SBlueprintQueryAnalysisWidget::ExecuteAnalyzeStaticGraph()
{
	FString ErrorMessage;
	if (StaticAnalyzer->AnalyzeProject(*StructureGraph, &ErrorMessage))
	{
		if (ActiveViewIndex == StructureViewIndex)
		{
			CurrentFolderPath.Reset();
			SelectedNodeId.Reset();
			CurrentGraph = StructureGraph;
			GraphView = StructureView;
		}
        StatusMessage = FString::Printf(TEXT("文件夹依赖扫描完成：%d 个文件夹节点，%d 条依赖边。双击 Project 下一级文件夹进入资产依赖图。"), StructureGraph->Nodes.Num(), StructureGraph->Edges.Num());
		UE_LOG(LogTemp, Display, TEXT("BlueprintQueryAnalysis static scan finished: %d nodes, %d edges."), StructureGraph->Nodes.Num(), StructureGraph->Edges.Num());
		StructureView->SetGraph(StructureGraph);
		StructureView->SetSelectedNodeId(FString());
	}
	else
	{
		StatusMessage = ErrorMessage;
		if (StructureView.IsValid())
		{
			StructureView->SetGraph(StructureGraph);
		}
	}
}

void SBlueprintQueryAnalysisWidget::ExecuteShowRuntimeGraph()
{
	if (!RuntimeTracer.IsValid())
	{
        StatusMessage = TEXT("运行时采集器不可用。");
		if (GraphView.IsValid())
		{
			GraphView->SetGraph(CurrentGraph);
		}
		return;
	}

	RuntimeTracer->RebuildRuntimeGraph();
	*RuntimeGraph = RuntimeTracer->GetRuntimeGraph();
	if (ActiveViewIndex == RuntimeViewIndex)
	{
		CurrentFolderPath.Reset();
		SelectedNodeId.Reset();
		CurrentGraph = RuntimeGraph;
		GraphView = RuntimeView;
	}
    StatusMessage = FString::Printf(TEXT("运行图已刷新：%d 个函数/事件节点。"), RuntimeGraph->Nodes.Num());
	RuntimeView->SetGraph(RuntimeGraph);
	RuntimeView->SetSelectedNodeId(FString());
}

void SBlueprintQueryAnalysisWidget::ExecuteStopRuntimeCapture()
{
	if (!RuntimeTracer.IsValid())
	{
        StatusMessage = TEXT("运行时采集器不可用。");
		if (GraphView.IsValid())
		{
			GraphView->SetGraph(CurrentGraph);
		}
		return;
	}

	RuntimeTracer->StopCapture(true);
	*RuntimeGraph = RuntimeTracer->GetRuntimeGraph();
	if (ActiveViewIndex == RuntimeViewIndex)
	{
		CurrentFolderPath.Reset();
		SelectedNodeId.Reset();
		CurrentGraph = RuntimeGraph;
		GraphView = RuntimeView;
	}
    StatusMessage = FString::Printf(TEXT("运行采集已停止：%d 个原始事件，已生成 %d 个函数/事件节点。"), RuntimeTracer->GetEvents().Num(), RuntimeGraph->Nodes.Num());
	RuntimeView->SetGraph(RuntimeGraph);
	RuntimeView->SetSelectedNodeId(FString());
}

void SBlueprintQueryAnalysisWidget::ResetRuntimePresentationState()
{
	LastRuntimeEventCount = 0;
	bLastRuntimeCaptureState = false;
}

void SBlueprintQueryAnalysisWidget::ExecuteAnalyzeFolder(const FString& FolderPath)
{
	FString ErrorMessage;
	if (StaticAnalyzer->AnalyzeFolder(FolderPath, *StructureGraph, &ErrorMessage))
	{
		if (ActiveViewIndex == StructureViewIndex)
		{
			CurrentFolderPath = FolderPath;
			SelectedNodeId.Reset();
			CurrentGraph = StructureGraph;
			GraphView = StructureView;
		}
		StructureView->SetGraph(StructureGraph);
		StructureView->SetSelectedNodeId(FString());
        StatusMessage = FString::Printf(TEXT("已进入文件夹：%s。显示该文件夹下所有资产类型节点和跨文件夹依赖详情。"), *FolderPath);
	}
	else
	{
		StatusMessage = ErrorMessage;
		if (StructureView.IsValid())
		{
			StructureView->SetGraph(StructureGraph);
		}
	}
}

void SBlueprintQueryAnalysisWidget::ExecuteAnalyzeNodeFlow(const FString& NodeId)
{
	FString BlueprintAssetPath = PendingAssetPath;
	if (BlueprintAssetPath.IsEmpty() && CurrentGraph.IsValid())
	{
		if (const FBPQAGraphNode* Node = CurrentGraph->FindNodeById(NodeId))
		{
			BlueprintAssetPath = !Node->AssetPath.IsEmpty() ? Node->AssetPath : Node->PackageName;
		}
	}

	if (BlueprintAssetPath.IsEmpty())
	{
        StatusMessage = TEXT("未找到可分析的蓝图资产路径。");
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData;
	if (BlueprintAssetPath.Contains(TEXT(".")))
	{
		AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(BlueprintAssetPath));
	}

	if (!AssetData.IsValid())
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(FName(*BlueprintAssetPath), Assets);
		if (Assets.Num() > 0)
		{
			AssetData = Assets[0];
		}
	}

	if (!AssetData.IsValid())
	{
		StatusMessage = FString::Printf(TEXT("找不到蓝图资产数据：%s"), *BlueprintAssetPath);
		return;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
	if (!Blueprint)
	{
        StatusMessage = FString::Printf(TEXT("资产不是蓝图：%s"), *AssetData.GetObjectPathString());
		return;
	}

	FString ErrorMessage;
	if (!BlueprintNodeAnalyzer.IsValid() || !BlueprintNodeAnalyzer->AnalyzeBlueprint(Blueprint, *NodeFlowGraph, &ErrorMessage))
	{
        StatusMessage = ErrorMessage.IsEmpty() ? TEXT("类内节点流程分析失败。") : ErrorMessage;
		return;
	}

	PendingAssetPath.Reset();
	PendingNodeId.Reset();
	CurrentFolderPath = Blueprint->GetOuter() ? Blueprint->GetOuter()->GetName() : FString();
	CurrentNodeFlowAssetPath = AssetData.GetObjectPathString();
	SelectedNodeId.Reset();
	if (ActiveViewIndex == NodeFlowViewIndex)
	{
		CurrentGraph = NodeFlowGraph;
		GraphView = NodeFlowView;
	}

	if (NodeFlowView.IsValid())
	{
		NodeFlowView->SetGraph(NodeFlowGraph);
		NodeFlowView->SetSelectedNodeId(FString());
	}

	StatusMessage = FString::Printf(
        TEXT("类内节点流程已生成：%d 个节点，%d 条连线。"),
		NodeFlowGraph->Nodes.Num(),
		NodeFlowGraph->Edges.Num());
}
