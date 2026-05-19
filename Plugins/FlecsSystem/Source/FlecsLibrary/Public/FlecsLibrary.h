#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
DECLARE_LOG_CATEGORY_EXTERN(FlecsLibrary, All, All);
class FFlecsLibraryModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
