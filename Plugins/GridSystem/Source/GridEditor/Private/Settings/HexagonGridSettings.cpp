#include "HexagonGridSettings.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UHexagonGridSettings::UHexagonGridSettings()
{
	bShowGrids = false;
	bGridSnap = false;

	GridSize = 100.f;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DecalMat(TEXT("/GridSystem/Materials/Hexagon/M_HexagonGrid_Normal.M_HexagonGrid_Normal"));
	DecalMaterial = DecalMat.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VisualizerMat(TEXT("/GridSystem/Materials/Hexagon/M_HexagonGrid_SensingVisualizer.M_HexagonGrid_SensingVisualizer"));
	GridSensingVisualizerMaterial = VisualizerMat.Object;
}

UHexagonGridSettings::~UHexagonGridSettings()
{
}

void UHexagonGridSettings::SetParent(FEdModeGridEditor* GridEditorMode)
{
	ParentMode = GridEditorMode;
}
