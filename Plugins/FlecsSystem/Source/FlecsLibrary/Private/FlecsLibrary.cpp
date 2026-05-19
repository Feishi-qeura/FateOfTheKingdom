#include "FlecsLibrary.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

//IMPLEMENT_GAME_MODULE(FFlcesLibraryModule, FlecsLibrary);
DEFINE_LOG_CATEGORY(FlecsLibrary);
#define LOCTEXT_NAMESPACE "FFlcesLibraryModule"

void FFlcesLibraryModule::StartupModule()
{
	UE_LOG(FlecsLibrary, Warning, TEXT("FlecsLibrary module has started!"));
}

void FFlcesLibraryModule::ShutdownModule()
{
	UE_LOG(FlecsLibrary, Warning, TEXT("FlecsLibrary module has shut down"));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FFlcesLibraryModule, FlcesLibrary)