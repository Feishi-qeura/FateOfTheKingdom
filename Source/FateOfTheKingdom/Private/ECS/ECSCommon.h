#pragma once

#include "CoreMinimal.h"
#include "ECSCommon.generated.h"

// 实体句柄
USTRUCT(BlueprintType)
struct FATEOFTHEKINGDOM_API FEntityHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 EntityID = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	int32 Generation = 0;

	bool IsValid() const { return EntityID != INDEX_NONE; }

	bool operator==(const FEntityHandle& Other) const
	{
		return EntityID == Other.EntityID && Generation == Other.Generation;
	}
};