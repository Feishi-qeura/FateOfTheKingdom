#include "BPQAGraphDataModel.h"

const TCHAR* LexToString(EBPQAFlowEventType Type)
{
	switch (Type)
	{
	case EBPQAFlowEventType::PreLoadMap:
		return TEXT("PreLoadMap");
	case EBPQAFlowEventType::PostLoadMap:
		return TEXT("PostLoadMap");
	case EBPQAFlowEventType::PackageLoaded:
		return TEXT("PackageLoaded");
	case EBPQAFlowEventType::AssetLoaded:
		return TEXT("AssetLoaded");
	case EBPQAFlowEventType::ObjectConstructed:
		return TEXT("ObjectConstructed");
	case EBPQAFlowEventType::WorldInitialized:
		return TEXT("WorldInitialized");
	case EBPQAFlowEventType::ActorAdded:
		return TEXT("ActorAdded");
	case EBPQAFlowEventType::WorldBeginPlay:
		return TEXT("WorldBeginPlay");
	case EBPQAFlowEventType::BeginPIE:
		return TEXT("BeginPIE");
	case EBPQAFlowEventType::EndPIE:
		return TEXT("EndPIE");
	default:
		return TEXT("Unknown");
	}
}
