#pragma once

#include "BPQAGraphDataModel.h"
#include "Engine/World.h"

class AActor;
class UObject;
class UPackage;
struct FEndLoadPackageContext;

class FBPQARuntimeFlowTracer
{
public:
	FBPQARuntimeFlowTracer();
	~FBPQARuntimeFlowTracer();

	void RegisterDelegates();
	void UnregisterDelegates();

	void StartCapture();
	void StopCapture(bool bRebuildGraph = true);
	void Clear();

	bool IsCapturing() const { return bIsCapturing; }
	bool IsAutoCaptureOnPIE() const { return bAutoCaptureOnPIE; }
	void SetAutoCaptureOnPIE(bool bEnabled) { bAutoCaptureOnPIE = bEnabled; }
	const TArray<FBPQAFlowEvent>& GetEvents() const { return Events; }
	const FBPQAGraphModel& GetRuntimeGraph() const { return RuntimeGraph; }

	void RebuildRuntimeGraph();

private:
	void RecordEvent(EBPQAFlowEventType Type, UObject* Object, const FString& OverrideName = FString(), UWorld* OverrideWorld = nullptr);
	void RecordPackageLoaded(UPackage* Package);

	void RegisterWorldDelegates(UWorld* World);
	void RemoveWorldDelegates();

	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleAssetLoaded(UObject* Asset);
	void HandleObjectConstructed(UObject* Object);
	void HandleEndLoadPackage(const FEndLoadPackageContext& Context);
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);
	void HandleActorSpawned(AActor* Actor);
	void HandleWorldBeginPlay(UWorld* World);
	void HandleBeginPIE(bool bIsSimulating);
	void HandleEndPIE(bool bIsSimulating);

	bool ShouldCaptureWorld(UWorld* World);
	bool ShouldRecordObject(UObject* Object) const;
	FString MakeObjectKey(UObject* Object) const;
	void EnsureBlueprintFlowCache();

private:
	bool bDelegatesRegistered = false;
	bool bIsCapturing = false;
	bool bAutoCaptureOnPIE = false;
	double CaptureStartSeconds = 0.0;
	int32 MaxEventCount = 800;
	int32 RuntimeGraphRevision = 0;

	TArray<FBPQAFlowEvent> Events;
	FBPQAGraphModel RuntimeGraph;
	FBPQAGraphModel BlueprintFlowCache;
	bool bBlueprintFlowCacheValid = false;
	TSet<FString> SeenConstructedObjects;
	TWeakObjectPtr<UWorld> CaptureWorld;
	FString CaptureWorldName;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle AssetLoadedHandle;
	FDelegateHandle ObjectConstructedHandle;
	FDelegateHandle EndLoadPackageHandle;
	FDelegateHandle PostWorldInitializationHandle;
	FDelegateHandle BeginPIEHandle;
	FDelegateHandle EndPIEHandle;

	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> ActorSpawnedHandles;
	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> WorldBeginPlayHandles;
};
