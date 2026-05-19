#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "GridInfo.generated.h"

class UGrid;

UENUM(BlueprintType)
enum class EGridTileType : uint8
{
	None     UMETA(DisplayName = "空"),
	Road    UMETA(DisplayName = "道路"),
	Water    UMETA(DisplayName = "水面"),
	Desert   UMETA(DisplayName = "沙漠")
};

UCLASS(Blueprintable)
class GRIDRUNTIME_API UGridInfo : public UObject
{
	GENERATED_BODY()
	
public:
	UGridInfo();
	virtual ~UGridInfo();	

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GridInfo")
	void Clear();
	virtual void Clear_Implementation();

	/**
	property has changed, notify GridPainter refresh grid state
	*/
	UFUNCTION(BlueprintCallable, Category = "GridInfo")
	void Dirty();
	UPROPERTY(BlueprintReadWrite, Category = "GridInfo")
	FHitResult HitResult;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GridInfo")
	FGameplayTagContainer GameplayTags;

	// 格子类型ID，对应数据表的行名
	UPROPERTY(BlueprintReadWrite)
	FName TileID = NAME_None;
    
	// 运行时状态（不属于地形定义，所以放这里）
	UPROPERTY(BlueprintReadWrite)
	AActor* OccupyingUnit = nullptr;

	UGrid* ParentGrid;
};
