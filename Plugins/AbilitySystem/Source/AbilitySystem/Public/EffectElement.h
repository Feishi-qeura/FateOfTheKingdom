#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameFramework/Character.h"
#include "Math/UnrealMathUtility.h" 
#include "EffectElement.generated.h"

/**
 * 
 */
UCLASS(Abstract , Blueprintable, BlueprintType,Category = "Ability System")

class ABILITYSYSTEM_API UEffectElement : public UObject
{
	GENERATED_BODY()

public:
	UEffectElement();


	virtual void PostInitProperties() override;



	UFUNCTION(BlueprintImplementableEvent, Category = "Ability System" , meta = (DisplayName = "开始时"))
	void OnBeginPlay();
	//定义效果的逻辑
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability System" , meta = (DisplayName = "效果激活时"))
	void OnActivate();

	//类外调用，激活效果
	UFUNCTION(BlueprintCallable , Category = "Ability System" , meta = (DisplayName = "激活效果"))
	void ActivateEffect();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability System" , meta = (DisplayName = "效果销毁时"))
	void OnDestroy();

	UFUNCTION(BlueprintCallable, Category = "Ability System|LifeCycle" , meta = (DisplayName = "销毁效果"))
	void DestroyEffectElement();
	//默认钳位在0-最大效果层数之间，可根据实际需要重载
	//常态请勿重载
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent , Category = "Ability System" , meta = (DisplayName = "设置效果层数"))
	void SetEffectStacks(int32 TargetStacks);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability System" , meta = (displayName = "效果层数"))
	int32 EffectStacks;
	
	UPROPERTY(EditDefaultsOnly , BlueprintReadOnly , Category = "Ability System" , meta = (displayName = "最大效果层数"))
	int32 MaxEffectStacks;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ability System",Meta = (ExposeOnSpawn=true))
	ACharacter* Owner;

	//可直接传入材质或纹理
	UPROPERTY(EditAnywhere, BlueprintReadOnly , Category = "Ability System", meta = (AllowedClasses = "Texture2D,MaterialInterface" , displayname = "效果图标"))
	TObjectPtr<UObject> Image;

private:

	virtual UWorld* GetWorld() const override;
	UWorld* World;

};
