#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TestPlayerControl.generated.h"
class ATestUnit;
class AThirdCameraManager;
class UGridSubsystem;
UCLASS()
class ATestPlayerControl : public APlayerController
{
	GENERATED_BODY()
public:
	ATestUnit* Pawn;
protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;
private:
	AThirdCameraManager* MyCam = nullptr;
	UGridSubsystem* GridSubsystem;
	
	void OnMouseClick();
	void SetupCamera();
	void OnZoomIn();
	void OnZoomOut();
};
