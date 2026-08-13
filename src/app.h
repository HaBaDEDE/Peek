#pragma once

#include "core/event_store.h"
#include "core/icon_cache.h"
#include "core/process_resolver.h"
#include "focus/focus_inspector.h"
#include "ghost/ghost_monitor.h"
#include "ui/main_window.h"
#include "unlock/unlock_service.h"

namespace peek {

class App {
public:
    App();
    int Run(HINSTANCE instance, int show_command);

private:
    EventStore events_{100};
    ProcessResolver resolver_;
    IconCache icons_{64};
    UnlockService unlock_;
    MainWindow window_;
    GhostMonitor ghost_;
    FocusInspector focus_;
};

} // namespace peek
