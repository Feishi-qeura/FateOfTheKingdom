#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TestGameMode.generated.h"

class UFlecsSubsystem;
class ASquareGridManager;

UCLASS()
class FATEOFTHEKINGDOM_API ATestGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
	UDataTable* GridDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
	UFlecsSubsystem* FlecsSubsystem;
	ATestGameMode();
	//ALevelLifecycleGameMode();

	
protected:
	// 1. 系统初始化阶段 (关卡开始)
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    
	// 2. 逻辑开始阶段
	virtual void BeginPlay() override;

	// 3. 每一帧更新 (如果需要协调多个系统)
	virtual void Tick(float DeltaTime) override;

	// 4. 清理阶段 (关卡结束)
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitGridSystem();
	void GetLandscape();
	void GetStaticMesh();
};
