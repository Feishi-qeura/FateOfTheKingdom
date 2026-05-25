#include "SquareGridSettings.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

USquareGridSettings::USquareGridSettings()
{
	bShowGrids = false;

	GridSize = 100.f;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DecalMat(TEXT("/GridSystem/Materials/Square/M_SquareGrid_Normal.M_SquareGrid_Normal"));
	DecalMaterial = DecalMat.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VisualizerMat(TEXT("/GridSystem/Materials/Square/M_SquareGrid_SensingVisualizer.M_SquareGrid_SensingVisualizer"));
	GridSensingVisualizerMaterial = VisualizerMat.Object;
}

USquareGridSettings::~USquareGridSettings()
{
}

void USquareGridSettings::SetParent(FEdModeGridEditor* GridEditorMode)
{
	ParentMode = GridEditorMode;
}
