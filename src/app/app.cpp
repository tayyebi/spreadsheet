// =============================================================================
// app.cpp  —  App method implementations: rendering and key handling
// =============================================================================

#include "app.h"    // App declaration
#include <string>   // std::string, std::to_string
#include <cctype>   // std::isprint

// ---------------------------------------------------------------------------
// CL()  —  convert a zero-based column index to an Excel-style column label
//
// Excel uses base-26 "bijective" numbering (no zero):
//   0 → "A", 1 → "B", …, 25 → "Z", 26 → "AA", 27 → "AB", …
//
// Algorithm: repeatedly extract the least-significant "digit" (1–26) by
// computing (n-1) % 26 + 'A', then divide n by 26 (rounded down), prepending
// each character to build the string from least to most significant.
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
// Stores the window reference and immediately registers onKey() as the
// window's input handler.  The lambda captures `this` so that key events
// are forwarded to our method rather than a free function.
// ---------------------------------------------------------------------------
App::App(IWindow& w) : win_(w) {
    win_.handleInput([this](KeyEvent e) { onKey(e); });
}

// ---------------------------------------------------------------------------
// App::render()  —  paint the entire spreadsheet grid
//
// Layout (all dimensions in pixels):
//
//   (0,0) ┌────────────────────────────────────────────┐
//         │  corner  │   A    │   B    │  …  │   J    │  ← header row (HH=20)
//         ├──────────┼────────┼────────┼─────┼────────┤
//         │    1     │  cell  │  cell  │     │  cell  │  ← data rows (CH=25 each)
//         │    2     │        │        │     │        │
//         │   …      │        │        │     │        │
//
//   Column header width: HW=40 px
//   Cell width: CW=100 px, cell height: CH=25 px
//
// The selected cell gets a thick 3-pixel blue border drawn by three
// concentric drawRect() calls, each 1 px inset from the last.
// In editing mode the selected cell also gets a yellow background and shows
// the in-progress edit buffer with a blinking cursor ('|').
// ---------------------------------------------------------------------------
void App::render() {
    // Define the colour palette for the UI.
    Color bg  {255, 255, 255};  // white  — default cell background
    Color gr  {200, 200, 200};  // grey   — cell border / grid lines
    Color hd  {220, 220, 220};  // light grey — header background
    Color sl  {  0,   0, 200};  // blue   — selection highlight border
    Color tx  {  0,   0,   0};  // black  — text
    Color ed  {255, 255, 200};  // yellow — edit-mode cell background

    // Compute total window dimensions from grid dimensions.
    int W = HW + Spreadsheet::COLS * CW;
    int H = HH + Spreadsheet::ROWS * CH;

    // Clear the whole window and paint the top-left corner header square.
    win_.fillRect(0, 0, W, H, bg);
    win_.fillRect(0, 0, HW, HH, hd);

    // Paint the column header row: A, B, C, …
    for (int c = 0; c < Spreadsheet::COLS; ++c) {
        int x = HW + c * CW;
        win_.fillRect(x, 0, CW, HH, hd);    // header background
        win_.drawRect(x, 0, CW, HH, gr);    // header border
        win_.drawText(x + 4, 4, CL(c), tx); // column label
    }

    // Paint the row header column: 1, 2, 3, …
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        int y = HH + r * CH;
        win_.fillRect(0, y, HW, CH, hd);                       // header background
        win_.drawRect(0, y, HW, CH, gr);                       // header border
        win_.drawText(4, y + 5, std::to_string(r + 1), tx);   // row number
    }

    // Paint all data cells.
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            int  x   = HW + c * CW;
            int  y   = HH + r * CH;
            bool sel = (r == selRow_ && c == selCol_);

            // In edit mode, highlight the selected cell in yellow.
            if (sel && editing_) win_.fillRect(x, y, CW, CH, ed);

            win_.drawRect(x, y, CW, CH, gr);  // cell border

            // Determine display text: show the edit buffer (with cursor '|')
            // for the cell being edited, or the cell's computed display value.
            std::string d;
            if (sel && editing_) {
                d = editBuf_ + '|';            // show typing cursor
            } else {
                const Cell* cell = sheet_.getCell(r, c);
                if (cell) d = cell->display;   // evaluated display string
            }

            if (!d.empty()) win_.drawText(x + 3, y + 5, d, tx);
        }
    }

    // Draw the selection highlight: three concentric rectangles in blue
    // (each 1 px larger than the one inside it) to produce a thick border.
    int sx = HW + selCol_ * CW;
    int sy = HH + selRow_ * CH;
    for (int i = 0; i < 3; ++i)
        win_.drawRect(sx - i, sy - i, CW + 2 * i, CH + 2 * i, sl);

    win_.updateDisplay();  // blit the back-buffer to the screen
}

// ---------------------------------------------------------------------------
// App::onKey()  —  handle one keyboard event
//
// The app operates as a simple two-state machine:
//
//   NAVIGATION mode (editing_ == false)
//     Arrow keys  → move the selection cursor
//     Enter       → enter edit mode for the selected cell
//     Ctrl+S      → save the grid to "spreadsheet.csv"
//     Ctrl+O      → load the grid from "spreadsheet.csv" and re-evaluate
//
//   EDIT mode (editing_ == true)
//     Printable chars → append to editBuf_
//     Backspace       → delete the last character from editBuf_
//     Enter           → commit: store the buffer to the cell, re-evaluate all
//     Escape          → cancel: discard editBuf_, return to navigation
//
// After any state change we always call render() to keep the display in sync.
// ---------------------------------------------------------------------------
void App::onKey(KeyEvent e) {
    if (editing_) {
        // ----- EDIT mode -----
        if (e.key == IWindow::KEY_ENTER) {
            // Commit the edit buffer to the data model and re-evaluate all formulas.
            sheet_.setCell(selRow_, selCol_, editBuf_);
            sheet_.evaluateAll();
            editing_ = false;
        } else if (e.key == IWindow::KEY_ESC) {
            // Cancel without saving: just clear the buffer and exit edit mode.
            editing_ = false;
            editBuf_.clear();
        } else if (e.key == IWindow::KEY_BACKSPACE) {
            // Delete the last character (if any).
            if (!editBuf_.empty()) editBuf_.pop_back();
        } else if (e.ch && std::isprint((unsigned)e.ch) && !e.ctrl) {
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
        else if (e.key == IWindow::KEY_ENTER) {
            // Start editing: pre-populate the buffer with the cell's raw content
            // so the user can see and modify the existing formula / value.
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";
        }
        else if (e.ctrl && (e.ch == 's' || e.ch == 19))  // 19 = Ctrl+S raw code
            sheet_.saveCSV("spreadsheet.csv");
        else if (e.ctrl && (e.ch == 'o' || e.ch == 15)) {  // 15 = Ctrl+O raw code
            sheet_.loadCSV("spreadsheet.csv");
            sheet_.evaluateAll();  // re-evaluate after loading new data
        }
    }

    render();  // repaint after every event regardless of what changed
}
