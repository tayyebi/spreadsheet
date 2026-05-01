// =============================================================================
// app.cpp  —  App method implementations: rendering and key/mouse handling
// =============================================================================

#include "app.h"    // App declaration
#include <string>   // std::string, std::to_string
#include <cctype>   // std::isprint

// ---------------------------------------------------------------------------
// CL()  —  convert a zero-based column index to an Excel-style column label
//
// Excel uses base-26 "bijective" numbering (no zero):
//   0 → "A", 1 → "B", …, 25 → "Z", 26 → "AA", 27 → "AB", …
// ---------------------------------------------------------------------------
static std::string CL(int c) {
    std::string s;
    int n = c + 1;  // convert to 1-based for the bijective encoding
    while (n > 0) {
        s  = char('A' + (n - 1) % 26) + s;  // prepend the next letter
        n  = (n - 1) / 26;                   // move to the next more-significant position
    }
    return s;
}

// ---------------------------------------------------------------------------
// App::App()  —  constructor
//
// Stores the window reference, registers onKey() and onMouse() as event
// handlers, and initialises the fixed toolbar button descriptors.
// ---------------------------------------------------------------------------
App::App(IWindow& w) : win_(w) {
    win_.handleInput([this](KeyEvent  e) { onKey(e);   });
    win_.handleMouse([this](MouseEvent e) { onMouse(e); });

    // Toolbar buttons: y=4, height=TB-8=24, width=60, gap=4 between buttons.
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

// ---------------------------------------------------------------------------
// App::render()  —  paint the toolbar, formula bar, and spreadsheet grid
//
// Layout (all dimensions in pixels):
//
//   y=0        ┌──────────────────────────────────────────────────────────────┐
//              │  [Save] [Load] [Clear] [Sum] [SvODS] [LdODS]               │ ← toolbar (TB=32)
//   y=TB       ├──────┬───────────────────────────────────────────────────────┤
//              │  A1  │  =SUM(A1:A3)                                         │ ← formula bar (FB=25)
//   y=TB+FB    ├──────┬──────┬──────┬────┬──────┐                           │
//              │      │  A   │  B   │ …  │  J   │                           │ ← column headers (HH=20)
//   y=TB+FB+HH ├──────┼──────┼──────┼────┼──────┤                           │
//              │  1   │      │      │    │      │                           │ ← data rows (CH=25 each)
//              │  2   │      │      │    │      │                           │
//              │  …   │      │      │    │      │                           │
//
// The selected cell gets a thick 3-pixel blue border.
// In editing mode the selected cell has a yellow background and shows
// the in-progress edit buffer with a blinking cursor ('|').
// ---------------------------------------------------------------------------
void App::render() {
    // Define the colour palette for the UI.
    Color bg   {255, 255, 255};  // white  — default cell background
    Color gr   {200, 200, 200};  // grey   — cell border / grid lines
    Color hd   {220, 220, 220};  // light grey — header background
    Color sl   {  0,   0, 200};  // blue   — selection highlight border
    Color tx   {  0,   0,   0};  // black  — text
    Color ed   {255, 255, 200};  // yellow — edit-mode cell background
    Color tbBg {230, 230, 230};  // light grey — toolbar background
    Color btnBg{245, 245, 245};  // near-white — button face
    Color btnBd{160, 160, 160};  // medium grey — button border
    Color fbBg {255, 255, 255};  // white — formula bar background
    Color fbRef{235, 235, 235};  // light grey — cell-reference name box

    // Compute total window dimensions from grid dimensions.
    int W = HW + Spreadsheet::COLS * CW;
    int H = TB + FB + HH + Spreadsheet::ROWS * CH;

    // -----------------------------------------------------------------------
    // Clear the entire window.
    // -----------------------------------------------------------------------
    win_.fillRect(0, 0, W, H, bg);

    // -----------------------------------------------------------------------
    // Toolbar strip (y = 0, height = TB)
    // -----------------------------------------------------------------------
    win_.fillRect(0, 0, W, TB, tbBg);
    win_.drawRect(0, 0, W, TB, gr);

    for (const auto& btn : buttons_) {
        win_.fillRect(btn.x, btn.y, btn.w, btn.h, btnBg);
        win_.drawRect(btn.x, btn.y, btn.w, btn.h, btnBd);
        win_.drawText(btn.x + 6, btn.y + 4, btn.label, tx);
    }

    // -----------------------------------------------------------------------
    // Formula bar (y = TB, height = FB)
    // -----------------------------------------------------------------------
    win_.fillRect(0, TB, W, FB, fbBg);
    win_.drawRect(0, TB, W, FB, gr);

    // Cell-reference name box (left portion, same width as row header).
    std::string addr = CL(selCol_) + std::to_string(selRow_ + 1);
    win_.fillRect(0, TB, HW + 10, FB, fbRef);
    win_.drawRect(0, TB, HW + 10, FB, gr);
    win_.drawText(4, TB + 5, addr, tx);

    // Raw formula / value for the selected cell (right portion).
    std::string rawContent;
    if (editing_) {
        rawContent = editBuf_;
    } else {
        const Cell* cell = sheet_.getCell(selRow_, selCol_);
        if (cell) rawContent = cell->raw;
    }
    win_.drawText(HW + 14, TB + 5, rawContent, tx);

    // -----------------------------------------------------------------------
    // Grid: column headers, row headers, and data cells
    //   The grid starts at y = TB + FB.
    // -----------------------------------------------------------------------
    int gridY = TB + FB;  // y pixel where the grid area begins

    // Top-left corner square.
    win_.fillRect(0, gridY, HW, HH, hd);
    win_.drawRect(0, gridY, HW, HH, gr);

    // Column header row: A, B, C, …
    for (int c = 0; c < Spreadsheet::COLS; ++c) {
        int x = HW + c * CW;
        win_.fillRect(x, gridY, CW, HH, hd);
        win_.drawRect(x, gridY, CW, HH, gr);
        win_.drawText(x + 4, gridY + 4, CL(c), tx);
    }

    // Row header column: 1, 2, 3, …
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        int y = gridY + HH + r * CH;
        win_.fillRect(0, y, HW, CH, hd);
        win_.drawRect(0, y, HW, CH, gr);
        win_.drawText(4, y + 5, std::to_string(r + 1), tx);
    }

    // Data cells.
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            int  x   = HW + c * CW;
            int  y   = gridY + HH + r * CH;
            bool sel = (r == selRow_ && c == selCol_);

            // In edit mode, highlight the selected cell in yellow.
            if (sel && editing_) win_.fillRect(x, y, CW, CH, ed);

            win_.drawRect(x, y, CW, CH, gr);  // cell border

            // Determine display text.
            std::string d;
            if (sel && editing_) {
                d = editBuf_ + '|';            // show typing cursor
            } else {
                const Cell* cell = sheet_.getCell(r, c);
                if (cell) d = cell->display;
            }

            if (!d.empty()) win_.drawText(x + 3, y + 5, d, tx);
        }
    }

    // Selection highlight: three concentric blue rectangles (thick border).
    int sx = HW + selCol_ * CW;
    int sy = gridY + HH + selRow_ * CH;
    for (int i = 0; i < 3; ++i)
        win_.drawRect(sx - i, sy - i, CW + 2 * i, CH + 2 * i, sl);

    win_.updateDisplay();  // blit the back-buffer to the screen
}

// ---------------------------------------------------------------------------
// App::onKey()  —  handle one keyboard event
//
//   NAVIGATION mode (editing_ == false)
//     Arrow keys    → move the selection cursor
//     Tab           → move one column right
//     Home          → jump to column A
//     End           → jump to last column
//     Enter / F2    → enter edit mode for the selected cell
//     Delete        → clear the selected cell
//     Ctrl+S        → save the grid to "spreadsheet.csv"
//     Ctrl+O        → load the grid from "spreadsheet.csv" and re-evaluate
//     Ctrl+Shift+S  → save the grid to "spreadsheet.ods" (OpenDocument)
//     Ctrl+Shift+O  → load "spreadsheet.ods" and re-evaluate
//
//   EDIT mode (editing_ == true)
//     Printable chars → append to editBuf_
//     Backspace       → delete the last character from editBuf_
//     Enter           → commit: store the buffer to the cell, re-evaluate all
//     Tab             → commit and move one column right
//     Escape          → cancel: discard editBuf_, return to navigation
// ---------------------------------------------------------------------------
void App::onKey(KeyEvent e) {
    if (editing_) {
        // ----- EDIT mode -----
        if (e.key == IWindow::KEY_ENTER) {
            // Commit the edit buffer to the data model and re-evaluate all formulas.
            sheet_.setCell(selRow_, selCol_, editBuf_);
            sheet_.evaluateAll();
            editing_ = false;
            editBuf_.clear();
        } else if (e.key == IWindow::KEY_TAB) {
            // Commit and move one column right.
            sheet_.setCell(selRow_, selCol_, editBuf_);
            sheet_.evaluateAll();
            editing_ = false;
            editBuf_.clear();
            if (selCol_ < Spreadsheet::COLS - 1) ++selCol_;
        } else if (e.key == IWindow::KEY_ESC) {
            // Cancel without saving.
            editing_ = false;
            editBuf_.clear();
        } else if (e.key == IWindow::KEY_BACKSPACE) {
            // Delete the last character (if any).
            if (!editBuf_.empty()) editBuf_.pop_back();
        } else if (e.ch && std::isprint(static_cast<unsigned char>(e.ch)) && !e.ctrl) {
            // Append any printable non-control character to the buffer.
            editBuf_ += e.ch;
        }
    } else {
        // ----- NAVIGATION mode -----
        if      (e.key == IWindow::KEY_UP    && selRow_ > 0)
            --selRow_;
        else if (e.key == IWindow::KEY_DOWN  && selRow_ < Spreadsheet::ROWS - 1)
            ++selRow_;
        else if (e.key == IWindow::KEY_LEFT  && selCol_ > 0)
            --selCol_;
        else if (e.key == IWindow::KEY_RIGHT && selCol_ < Spreadsheet::COLS - 1)
            ++selCol_;
        else if (e.key == IWindow::KEY_TAB && selCol_ < Spreadsheet::COLS - 1)
            ++selCol_;
        else if (e.key == IWindow::KEY_HOME)
            selCol_ = 0;
        else if (e.key == IWindow::KEY_END)
            selCol_ = Spreadsheet::COLS - 1;
        else if (e.key == IWindow::KEY_DELETE) {
            // Clear the selected cell and re-evaluate.
            sheet_.setCell(selRow_, selCol_, "");
            sheet_.evaluateAll();
        }
        else if (e.key == IWindow::KEY_ENTER || e.key == IWindow::KEY_F2) {
            // Start editing: pre-populate the buffer with the cell's raw content.
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";
        }
        else if (e.ctrl && (e.ch == 's' || e.ch == 19))  // 19 = Ctrl+S raw code
            sheet_.saveCSV("spreadsheet.csv");
        else if (e.ctrl && (e.ch == 'o' || e.ch == 15)) {  // 15 = Ctrl+O raw code
            sheet_.loadCSV("spreadsheet.csv");
            sheet_.evaluateAll();
        }
        // Ctrl+Shift+S/O — ODS save/load.
        // Ctrl+Shift+S may deliver 's' (normalised) with shift=true, or 'S'.
        else if (e.ctrl && e.shift && (e.ch == 's' || e.ch == 'S'))
            sheet_.saveODS("spreadsheet.ods");
        else if (e.ctrl && e.shift && (e.ch == 'o' || e.ch == 'O')) {
            sheet_.loadODS("spreadsheet.ods");
            sheet_.evaluateAll();
        }
    }

    render();  // repaint after every event regardless of what changed
}

// ---------------------------------------------------------------------------
// App::onMouse()  —  handle one mouse button press event
//
// Handles two kinds of clicks:
//   1. Toolbar button clicks — trigger the corresponding action (Save, Load,
//      Clear, or Sum).
//   2. Grid cell clicks — move the selection to the clicked cell; if currently
//      in edit mode the pending edit is committed first.
// ---------------------------------------------------------------------------
void App::onMouse(MouseEvent e) {
    if (e.button != 1) return;  // respond to left-button clicks only

    // Check toolbar button hits.
    for (const auto& btn : buttons_) {
        if (e.x >= btn.x && e.x < btn.x + btn.w &&
            e.y >= btn.y && e.y < btn.y + btn.h) {

            if (btn.label == "Save") {
                sheet_.saveCSV("spreadsheet.csv");

            } else if (btn.label == "Load") {
                sheet_.loadCSV("spreadsheet.csv");
                sheet_.evaluateAll();
                editing_ = false;
                editBuf_.clear();

            } else if (btn.label == "Clear") {
                editing_ = false;
                editBuf_.clear();
                sheet_.setCell(selRow_, selCol_, "");
                sheet_.evaluateAll();

            } else if (btn.label == "Sum") {
                // Pre-fill a SUM formula that sums the column above the cursor.
                editing_ = true;
                if (selRow_ > 0)
                    editBuf_ = "=SUM(" + CL(selCol_) + "1:" +
                               CL(selCol_) + std::to_string(selRow_) + ")";
                else
                    editBuf_ = "=SUM()";

            } else if (btn.label == "SvODS") {
                // Save to OpenDocument Spreadsheet format (.ods).
                sheet_.saveODS("spreadsheet.ods");

            } else if (btn.label == "LdODS") {
                // Load from OpenDocument Spreadsheet format (.ods).
                sheet_.loadODS("spreadsheet.ods");
                sheet_.evaluateAll();
                editing_ = false;
                editBuf_.clear();
            }

            render();
            return;
        }
    }

    // Check grid cell clicks (below the toolbar + formula bar + column headers).
    int gridY = TB + FB + HH;
    if (e.x >= HW && e.y >= gridY) {
        int c = (e.x - HW) / CW;
        int r = (e.y - gridY) / CH;
        if (r >= 0 && r < Spreadsheet::ROWS && c >= 0 && c < Spreadsheet::COLS) {
            // Commit any in-progress edit before changing selection.
            if (editing_) {
                sheet_.setCell(selRow_, selCol_, editBuf_);
                sheet_.evaluateAll();
                editing_ = false;
                editBuf_.clear();
            }
            selRow_ = r;
            selCol_ = c;
            render();
        }
    }
}
