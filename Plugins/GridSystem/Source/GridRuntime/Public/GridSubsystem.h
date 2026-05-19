#pragma once

//#include "GridLibrary/GridRuntime/Public/Square/SquareGridManager.h"
#include "GridSubsystem.generated.h"
class UGrid;
class ASquareGridManager;
class USquarePathFinder;
UCLASS()
class GRIDRUNTIME_API UGridSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "Debug")
	ASquareGridManager* GridManager;
	UPROPERTY(VisibleAnywhere, Category = "Debug")
	USquarePathFinder* SquarePathFinder;
	
	UFUNCTION(BlueprintCallable, Category = "GridSubsytem")
	void InitGridManager(UDataTable* gridDataTable, float gridSize = 100.0f, FVector originPos = FVector(0, 0, 0));
	UFUNCTION(BlueprintCallable, Category = "GridSubsytem")
	void InitMapGrid(FBox MapBound);
	TArray<FVector> DoPathFind(AActor* actor, FVector endPos);
	FIntVector GetCoordByPosition(const FVector& Position);
	// ---- USubsystem 生命周期 ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Tick（每帧推进 Flecs World）----
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	void         Tick(float DeltaTime);
	
private:
	void TestPathFinder();
	void DrawGridBorders(UGrid* Grid, float GridSize, float ZOffset);
};
