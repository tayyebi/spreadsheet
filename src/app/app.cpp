// =============================================================================
// app.cpp  —  App constructor: window binding and toolbar button layout
// =============================================================================

#include "app.h"  // App declaration

// ---------------------------------------------------------------------------
// App::App()  —  constructor
//
// Stores the window reference, registers onKey() and onMouse() as event
// handlers, and initialises the fixed toolbar button descriptors.
// ---------------------------------------------------------------------------
App::App(IWindow& w) : win_(w) {
    win_.handleInput([this](KeyEvent  e) { onKey(e);   });
    win_.handleMouse([this](MouseEvent e) { onMouse(e); });

    // Toolbar buttons: y=4, height=TB-8=24, gap=4 between buttons.
    int bh = TB - 8;
    buttons_ = {
        {   4, 4, 60, bh, "Save"   },  // Ctrl+S: save CSV
        {  68, 4, 60, bh, "Load"   },  // Ctrl+O: load CSV
        { 132, 4, 60, bh, "Clear"  },  // Delete cell contents
        { 196, 4, 60, bh, "Sum"    },  // Insert SUM formula
        { 264, 4, 68, bh, "SvODS"  },  // Ctrl+Shift+S: save ODS
        { 336, 4, 68, bh, "LdODS"  },  // Ctrl+Shift+O: load ODS
    };
}

