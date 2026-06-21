#pragma once

#include "CoreMinimal.h"
#include "ASEnum.generated.h"

UENUM(BlueprintType) 
enum class UDE_Faction : uint8
{
    Faction_None          UMETA(DisplayName = "None"),      
    Faction_Player        UMETA(DisplayName = "玩家"),
    Faction_Enemy         UMETA(DisplayName = "敌人"),
    Faction_Neutral       UMETA(DisplayName = "中立")
};