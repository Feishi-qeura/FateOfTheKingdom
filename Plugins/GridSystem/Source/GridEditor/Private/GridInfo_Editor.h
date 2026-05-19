#pragma once

#include "CoreMinimal.h"
#include "GridInfo.h"
#include "GridInfo_Editor.generated.h"

UCLASS()
class GRIDEDITOR_API UGridInfo_Editor : public UGridInfo
{
	GENERATED_BODY()

public:
	UGridInfo_Editor();

	bool GetVisibility() const;

	bool GetShowNormal() const;
	void SetShowNormal(bool bNewShowNormal);

	bool GetSensing() const;
	void SetSensing(bool bNewSensing);

private:
	bool bShowNormal;

	bool bSensing;
};
