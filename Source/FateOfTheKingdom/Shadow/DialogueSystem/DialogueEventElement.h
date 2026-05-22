#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NativeGameplayTags.h"
#include "DialogueEventElement.generated.h"

UCLASS(Abstract , Blueprintable, BlueprintType, Category = "Dialogue System")
class FATEOFTHEKINGDOM_API UDialogueEventElement : public UObject
{

    GENERATED_BODY()

public:

    UDialogueEventElement();

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Dialogue System", meta = (DisplayName = "执行对话事件"))
    void ExecuteDialogueEvent();

    UFUNCTION(BlueprintImplementableEvent, Category = "Dialogue System", meta = (DisplayName = "开始运行"))
    void OnBeginplay();

    UPROPERTY(VisibleAnywhere , BlueprintReadOnly, Category = "Dialouge System",Meta = (ExposeOnSpawn=true))
	AActor* Owner;

    
private:
    
    virtual UWorld* GetWorld() const override;
    UWorld* World;

};