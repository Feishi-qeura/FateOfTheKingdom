#pragma once

#include "BPQAGraphDataModel.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SCompoundWidget.h"

class FBPQAStaticDependencyAnalyzer;
class FBPQABlueprintNodeAnalyzer;
class FBPQARuntimeFlowTracer;
struct FWebNavigationRequest;
class SWebBrowser;
class SBorder;
class SWidgetSwitcher;
class SBlueprintQueryAnalysisGraphView;
enum class EWebBrowserConsoleLogSeverity;

DECLARE_DELEGATE_OneParam(FBPQANodeAction, const FString& /*NodeId*/);

class SBlueprintQueryAnalysisGraphView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintQueryAnalysisGraphView) {}
		SLATE_ARGUMENT(TSharedPtr<FBPQAGraphModel>, Graph)
		SLATE_ARGUMENT(bool, InitiallyActive)
		SLATE_EVENT(FBPQANodeAction, OnNodeSelected)
		SLATE_EVENT(FBPQANodeAction, OnNodeActivated)
		SLATE_EVENT(FBPQANodeAction, OnImageSaved)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetActive(bool bInActive);
	void SetGraph(TSharedPtr<FBPQAGraphModel> InGraph);
	void SetSelectedNodeId(const FString& InSelectedNodeId);
	void SetLoadingState(const FString& Message, float Progress);

private:
	void HandleBrowserLoadStarted();
	void HandleBrowserLoadCompleted();
	void HandleBrowserLoadError();
	void HandleBrowserConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity);
	bool HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);
	FString BuildGraphJson() const;
	TSharedRef<SWidget> MakeBrowserPlaceholder() const;
	TSharedRef<SWidget> MakeBrowserWidget(const FString& InitialUrl);
	void EnsureBrowserLoaded();
	void PushGraphToBrowser();
	void PushSelectionToBrowser();
	void PushLoadingStateToBrowser();
	void ProbeBrowserReady();
	void QueueBrowserSync();
	EActiveTimerReturnType RunBrowserSync(double InCurrentTime, float InDeltaTime);
	void LoadBrowserEntry();
	void SetBrowserError(const FString& Title, const FString& Detail);
	void SaveImageFromChunks();
	FString DecodeNodeIdPayload(FString Payload) const;
	EVisibility GetStateOverlayVisibility() const;
	EVisibility GetLoadingPanelVisibility() const;
	EVisibility GetBusyVisibility() const;
	EVisibility GetIdlePanelVisibility() const;
	FText GetStateTitleText() const;
	FText GetStateDetailText() const;
	TOptional<float> GetLoadingProgress() const;

private:
	TSharedPtr<FBPQAGraphModel> Graph;
	TSharedPtr<SBorder> BrowserHost;
	TSharedPtr<SWebBrowser> Browser;
	FBPQANodeAction OnNodeSelected;
	FBPQANodeAction OnNodeActivated;
	FBPQANodeAction OnImageSaved;
	FString SelectedNodeId;
	bool bViewActive = false;
	bool bBrowserLoaded = false;
	bool bBrowserDocumentLoaded = false;
	bool bBrowserHtmlRequested = false;
	bool bPendingGraphPush = false;
	bool bPendingSelectionPush = false;
	bool bGraphPresentationPending = false;
	bool bBrowserSyncTimerActive = false;
	int32 BrowserSyncAttempts = 0;
	bool bHasPendingLoadingState = false;
	FString PendingLoadingMessage;
	float PendingLoadingProgress = 0.0f;
	bool bLoadingOverlayVisible = false;
	bool bBrowserErrorVisible = false;
	FString BrowserErrorTitle;
	FString BrowserErrorDetail;
	FString GraphViewerHtmlPath;
	FString PendingGraphJson;
	FString LastPushedGraphJson;
	FString PendingImageName;
	TMap<int32, FString> PendingImageChunks;
};

class SBlueprintQueryAnalysisWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintQueryAnalysisWidget) {}
		SLATE_ARGUMENT(TSharedPtr<FBPQARuntimeFlowTracer>, RuntimeTracer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildLeftPanel();
	TSharedRef<SWidget> BuildDetailsPanel();

	FReply AnalyzeStaticGraph();
	FReply BackToFolderOverview();
	FReply ShowRuntimeGraph();
	FReply RefreshCurrentView();
	FReply StartRuntimeCapture();
	FReply StopRuntimeCapture();
	FReply ShowNodeFlowGraph();
	FReply ShowNodeFlowPlaceholder();
	FReply ExportJson();
	FReply ExportMermaid();
	FReply ExportDot();
	FReply OpenSelectedAsset();
	ECheckBoxState GetAutoTrackPIECheckState() const;
	void HandleAutoTrackPIEChanged(ECheckBoxState NewState);

	void SelectNode(const FString& NodeId);
	void OpenAssetForNode(const FString& NodeId);
	void HandleImageSaved(const FString& FullPath);

	// 在“结构视图（静态/文件夹/类内节点流程）”与“运行流程视图”之间切换。
	// 两个视图各自持有独立的 WebBrowser 与图数据，互不影响。
	static constexpr int32 StructureViewIndex = 0;
	static constexpr int32 RuntimeViewIndex = 1;
	static constexpr int32 NodeFlowViewIndex = 2;
	void SetActiveView(int32 ViewIndex);

	FText GetSummaryText() const;
	FText GetStatusText() const;
	FText GetSelectedNodeText() const;

	const FBPQAGraphNode* GetSelectedNode() const;
	FString MakeExportBaseName() const;
	bool IsFolderNode(const FBPQAGraphNode& Node) const;
	void QueueDeferredAction(uint8 Action, const FString& FolderPath = FString(), const FString& NodeId = FString());
	EActiveTimerReturnType RunDeferredAction(double InCurrentTime, float InDeltaTime);
	EActiveTimerReturnType PollRuntimeGraph(double InCurrentTime, float InDeltaTime);
	void ExecuteAnalyzeStaticGraph();
	void ExecuteShowRuntimeGraph();
	void ExecuteStopRuntimeCapture();
	void ExecuteAnalyzeFolder(const FString& FolderPath);
	void ExecuteAnalyzeNodeFlow(const FString& NodeId);
	void ResetRuntimePresentationState();

private:
	// CurrentGraph / GraphView 始终指向当前激活视图对应的图与控件（结构视图或运行视图）。
	TSharedPtr<FBPQAGraphModel> CurrentGraph;
	TSharedPtr<SBlueprintQueryAnalysisGraphView> GraphView;
	TSharedPtr<FBPQAStaticDependencyAnalyzer> StaticAnalyzer;
	TSharedPtr<FBPQABlueprintNodeAnalyzer> BlueprintNodeAnalyzer;
	TSharedPtr<FBPQARuntimeFlowTracer> RuntimeTracer;

	TSharedPtr<FBPQAGraphModel> StructureGraph;
	TSharedPtr<FBPQAGraphModel> RuntimeGraph;
	TSharedPtr<FBPQAGraphModel> NodeFlowGraph;
	TSharedPtr<SBlueprintQueryAnalysisGraphView> StructureView;
	TSharedPtr<SBlueprintQueryAnalysisGraphView> RuntimeView;
	TSharedPtr<SBlueprintQueryAnalysisGraphView> NodeFlowView;
	TSharedPtr<SWidgetSwitcher> ViewSwitcher;
	int32 ActiveViewIndex = 0;

	FString SelectedNodeId;
	FString CurrentFolderPath;
	FString CurrentNodeFlowAssetPath;
	FString StatusMessage;
	uint8 PendingAction = 0;
	FString PendingFolderPath;
	FString PendingNodeId;
	FString PendingAssetPath;
	int32 LastRuntimeEventCount = 0;
	bool bLastRuntimeCaptureState = false;
};
