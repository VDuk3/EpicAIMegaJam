// GBPixelVfxBaker module: no startup work. The bake game mode only runs when a -Pvfx... switch
// is on the command line; the cook's Editor process can load this module and must see no side effects.
// The log category is defined again here (same name as the runtime module's): log categories are
// matched by name, and this keeps both DLLs free of cross-module data imports.
#include "GBPixelVfxLog.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGBPixelVfx);

IMPLEMENT_MODULE(FDefaultGameModuleImpl, GBPixelVfxBaker);
