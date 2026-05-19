#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
class FComponentVisualizer;
DECLARE_LOG_CATEGORY_EXTERN(GridEditor, Log, All);
class FGridEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    /**
     * Singleton-like access to this module's interface.  This is just for convenience!
     * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
     *
     * @return Returns singleton instance, loading the module on demand if needed
     */
    static inline FGridEditorModule& Get()
    {
        return FModuleManager::LoadModuleChecked< FGridEditorModule >( "GridEditor" );
    }

    /**
     * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
     *
     * @return True if the module is loaded and ready to use
     */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded( "GridEditor" );
    }
    /**
    * Registers a custom class
    *
    * @param ClassName				The class name to register for property customization
    * @param DetailLayoutDelegate	The delegate to call to get the custom detail layout instance
    */
    void RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate);

    /**
    * Registers a custom struct
    *
    * @param StructName				The name of the struct to register for property customization
    * @param StructLayoutDelegate	The delegate to call to get the custom detail layout instance
    */
    void RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate);

    void RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer);
    void RegisterComponentVisualizer();
    void UnregisterComponentVisualizer();

private:
    /** List of registered class that we must unregister when the module shuts down */
    TSet< FName > RegisteredClassNames;
    TSet< FName > RegisteredPropertyTypes;
    TArray<FName> RegisteredComponentClassNames;
};
