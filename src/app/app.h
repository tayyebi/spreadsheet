// =============================================================================
// app.h  —  Application controller (the "Controller" in MVC)
//
// App sits between the platform window (IWindow) and the data model
// (Spreadsheet).  It:
//   • owns the Spreadsheet and the selection / editing state
//   • renders the toolbar, formula bar, and grid onto the window each frame
//   • translates keyboard and mouse events into model mutations and navigation
// =============================================================================
#pragma once
#include "platform.h"  // IWindow, KeyEvent, MouseEvent, Color
#include "spreadsheet.h" // Spreadsheet, Cell

#include <string>
#include <vector>

class App {
public:
    // Construct the App, binding the window's input callbacks to onKey()/onMouse().
    explicit App(IWindow& w);

    // Repaint the toolbar, formula bar, and grid.
    // Called after every state change and on initial display.
    void render();

    // Handle one keyboard event.
    void onKey(KeyEvent e);

    // Handle one mouse button event.
    void onMouse(MouseEvent e);

    // -----------------------------------------------------------------------
    // Layout constants (characters) — all values are in terminal character
    // units (columns / lines) rather than pixels.
    // -----------------------------------------------------------------------
    static constexpr int CW = 10, CH = 1;   // cell width (chars) / height (lines)
    static constexpr int HW =  5, HH = 1;   // row-header width / col-header height
    static constexpr int TB =  1;            // toolbar strip height (lines)
    static constexpr int FB =  1;            // formula-bar strip height (lines)

private:
    IWindow&    win_;          // non-owning reference to the platform window
    Spreadsheet sheet_;        // the data model (cells, formulas, values)

    // -----------------------------------------------------------------------
    // Selection state
    //
    // The "active cell" is (selRow_, selCol_).
    // When the user holds Shift the anchor stays at (anchorRow_, anchorCol_)
    // and the cursor end moves, creating a rectangular selection range.
    // In single-cell mode anchor == cursor.
    // -----------------------------------------------------------------------
    int selRow_    = 0, selCol_    = 0;  // active cell (cursor end of selection)
    int anchorRow_ = 0, anchorCol_ = 0; // anchor end of selection

    // Edit mode: when true the user is typing a new value for the selected cell.
    bool editing_ = false;

    // The characters the user has typed so far during an edit session.
    std::string editBuf_;

    // -----------------------------------------------------------------------
    // Clipboard — single-cell text copy/cut/paste
    // -----------------------------------------------------------------------
    std::string clipboard_;

    // -----------------------------------------------------------------------
    // Undo / redo
    //
    // Each entry records the row/col and the before/after raw strings of one
    // cell change.  Multi-cell operations (fill down/right) push one entry
    // per cell.  Committing an edit clears the redo stack.
    // -----------------------------------------------------------------------
    struct UndoEntry { int r, c; std::string oldRaw, newRaw; };
    std::vector<UndoEntry> undoStack_;
    std::vector<UndoEntry> redoStack_;

    // -----------------------------------------------------------------------
    // Toolbar buttons
    // -----------------------------------------------------------------------
    struct ToolBtn {
        int         x, y, w, h;   // bounding box
        std::string label;         // displayed text
        bool        enabled = true;
    };
    std::vector<ToolBtn> toolBtns_;

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    // Commit the current edit buffer to the model; optionally move the cursor.
    //   dr / dc — delta applied to (selRow_, selCol_) after commit (0 = stay)
    void commitEdit(int dr = 0, int dc = 0);

    // Cancel the current edit without saving.
    void cancelEdit();

    // Build the toolBtns_ vector (called once in the constructor).
    void initToolbar();

    // Rendering sub-passes.
    void renderToolbar();
    void renderFormulaBar();
    void renderGrid();

    // Move selection by (dr, dc).  If shift is held, extend the range;
    // otherwise collapse to a single cell at the new position.
    void moveSel(int dr, int dc, bool shift);

    // Clamp r/c to valid grid coordinates.
    static int clampRow(int r) { return r < 0 ? 0 : (r >= Spreadsheet::ROWS ? Spreadsheet::ROWS - 1 : r); }
    static int clampCol(int c) { return c < 0 ? 0 : (c >= Spreadsheet::COLS ? Spreadsheet::COLS - 1 : c); }

    // Jump to the edge of a data block in direction (dr, dc) (Ctrl+Arrow).
    void jumpEdge(int dr, int dc, bool shift);

    // Undo / redo helpers.
    void pushUndo(int r, int c, const std::string& oldRaw, const std::string& newRaw);
    void doUndo();
    void doRedo();

    // Clipboard operations.
    void doCopy();
    void doCut();
    void doPaste();

    // Fill operations.
    void doFillDown();
    void doFillRight();

    // Toolbar button action dispatch.
    void toolbarAction(int btnIdx);

    // Set a cell raw value, recording an undo entry.
    void setCellWithUndo(int r, int c, const std::string& newRaw);

    // Return the raw string of a cell, or "" if empty.
    std::string rawOf(int r, int c) const;

    // Return the normalised selection rectangle [r0,r1] x [c0,c1] (inclusive).
    void selRect(int& r0, int& r1, int& c0, int& c1) const;
};
