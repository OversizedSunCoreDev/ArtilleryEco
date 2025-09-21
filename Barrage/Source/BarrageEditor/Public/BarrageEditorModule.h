#pragma once

#include "Engine.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UnrealEd.h"

DECLARE_LOG_CATEGORY_EXTERN(BarrageEditorLog, All, All)

/**
* The public interface to this module
*/
class IBarrageEditorModule : public IModuleInterface
{

public:
    /**
    * Singleton-like access to this module's interface.  This is just for convenience!
    * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
    *
    * @return Returns singleton instance, loading the module on demand if needed
    */
    static inline IBarrageEditorModule& Get()
    {
        return FModuleManager::LoadModuleChecked<IBarrageEditorModule>("BarrageEditor");
    }

    /**
    * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
    *
    * @return True if the module is loaded and ready to use
    */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("BarrageEditor");
    }
};