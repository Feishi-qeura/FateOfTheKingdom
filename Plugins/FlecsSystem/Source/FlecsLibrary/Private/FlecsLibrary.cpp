#include "FlecsLibrary.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

//IMPLEMENT_GAME_MODULE(FFlcesLibraryModule, FlecsLibrary);
DEFINE_LOG_CATEGORY(FlecsLibrary);
#define LOCTEXT_NAMESPACE "FFlecsLibraryModule"

void FFlecsLibraryModule::StartupModule()
{
	UE_LOG(FlecsLibrary, Warning, TEXT("FlecsLibrary module has started!"));
}

void FFlecsLibraryModule::ShutdownModule()
{
	UE_LOG(FlecsLibrary, Warning, TEXT("FlecsLibrary module has shut down"));
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FFlecsLibraryModule, FlecsLibrary)