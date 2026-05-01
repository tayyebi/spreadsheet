// =============================================================================
// app.h  —  Application controller (the "Controller" in MVC)
//
// App sits between the platform window (IWindow) and the data model
// (Spreadsheet).  It:
//   • owns the Spreadsheet and the selection / editing state
//   • renders the grid onto the window each frame
//   • translates keyboard events into model mutations and navigations
// =============================================================================
#pragma once
#include "platform.h"   // IWindow, KeyEvent, MouseEvent, Color
#include "spreadsheet.h" // Spreadsheet, Cell
#include <string>
#include <vector>

class App {
public:
    // Construct the App, binding the window's input callback to onKey().
    // The window reference is stored non-owning; the window must outlive App.
    explicit App(IWindow& w);

    // Repaint the entire grid to the window's back-buffer and blit it.
    // Called after every state change and on initial display.
    void render();

    // Handle one keyboard event.  Delegates to edit-mode or navigation-mode
    // logic depending on the current state of `editing_`.
    void onKey(KeyEvent e);

    // Handle one mouse button press event.  Used for toolbar button clicks
    // and grid cell selection.
    void onMouse(MouseEvent e);

    // -----------------------------------------------------------------------
    // Layout constants (pixels) — public so platform main() can compute the
    // window size without hard-coding the values.
    //
    //   TB, FB — toolbar and formula-bar heights (above the column headers)
    //   CW, CH — cell width and height
    //   HW, HH — header column width and header row height
    // -----------------------------------------------------------------------
    static constexpr int TB = 32;            // toolbar height
    static constexpr int FB = 25;            // formula bar height
    static constexpr int CW = 100, CH = 25;  // cell dimensions
    static constexpr int HW =  40, HH = 20;  // header dimensions

private:
    IWindow&    win_;          // non-owning reference to the platform window
    Spreadsheet sheet_;        // the data model (cells, formulas, values)

    // Selection: the cell currently highlighted by the cursor.
    int selRow_ = 0;
    int selCol_ = 0;

    // Edit mode: when true the user is typing a new value for the selected cell.
    bool editing_ = false;

    // The characters the user has typed so far during an edit session.
    // Committed to the sheet on Enter, discarded on Escape.
    std::string editBuf_;

    // -----------------------------------------------------------------------
    // Toolbar button descriptor
    //
    // Each entry names a rectangular click target and the action it triggers.
    // Positions are in window pixels (origin top-left, before any grid offset).
    // -----------------------------------------------------------------------
    struct ToolButton { int x, y, w, h; std::string label; };
    std::vector<ToolButton> buttons_;
};
