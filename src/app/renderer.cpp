// =============================================================================
// renderer.cpp  —  App::render(): paint the toolbar, formula bar, and grid
// =============================================================================

#include "renderer.h"  // paired header (includes app.h)
#include <string>      // std::string, std::to_string

// ---------------------------------------------------------------------------
// colLabel()  —  convert a zero-based column index to an Excel-style label
//
// Excel uses base-26 "bijective" numbering (no zero):
//   0 → "A", 1 → "B", …, 25 → "Z", 26 → "AA", 27 → "AB", …
// ---------------------------------------------------------------------------
static std::string colLabel(int c) {
    std::string s;
    int n = c + 1;
    while (n > 0) {
        s  = char('A' + (n - 1) % 26) + s;
        n  = (n - 1) / 26;
    }
    return s;
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
//
// The selected cell gets a thick 3-pixel blue border.
// In editing mode the selected cell has a yellow background and shows
// the in-progress edit buffer with a blinking cursor ('|').
// ---------------------------------------------------------------------------
void App::render() {
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

    int W = HW + Spreadsheet::COLS * CW;
    int H = TB + FB + HH + Spreadsheet::ROWS * CH;

    win_.fillRect(0, 0, W, H, bg);

    // Toolbar
    win_.fillRect(0, 0, W, TB, tbBg);
    win_.drawRect(0, 0, W, TB, gr);
    for (const auto& btn : buttons_) {
        win_.fillRect(btn.x, btn.y, btn.w, btn.h, btnBg);
        win_.drawRect(btn.x, btn.y, btn.w, btn.h, btnBd);
        win_.drawText(btn.x + 6, btn.y + 4, btn.label, tx);
    }

    // Formula bar
    win_.fillRect(0, TB, W, FB, fbBg);
    win_.drawRect(0, TB, W, FB, gr);
    std::string addr = colLabel(selCol_) + std::to_string(selRow_ + 1);
    win_.fillRect(0, TB, HW + 10, FB, fbRef);
    win_.drawRect(0, TB, HW + 10, FB, gr);
    win_.drawText(4, TB + 5, addr, tx);

    std::string rawContent;
    if (editing_) {
        rawContent = editBuf_;
    } else {
        const Cell* cell = sheet_.getCell(selRow_, selCol_);
        if (cell) rawContent = cell->raw;
    }
    win_.drawText(HW + 14, TB + 5, rawContent, tx);

    // Grid
    int gridY = TB + FB;

    win_.fillRect(0, gridY, HW, HH, hd);
    win_.drawRect(0, gridY, HW, HH, gr);

    for (int c = 0; c < Spreadsheet::COLS; ++c) {
        int x = HW + c * CW;
        win_.fillRect(x, gridY, CW, HH, hd);
        win_.drawRect(x, gridY, CW, HH, gr);
        win_.drawText(x + 4, gridY + 4, colLabel(c), tx);
    }

    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        int y = gridY + HH + r * CH;
        win_.fillRect(0, y, HW, CH, hd);
        win_.drawRect(0, y, HW, CH, gr);
        win_.drawText(4, y + 5, std::to_string(r + 1), tx);
    }

    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            int  x   = HW + c * CW;
            int  y   = gridY + HH + r * CH;
            bool sel = (r == selRow_ && c == selCol_);

            if (sel && editing_) win_.fillRect(x, y, CW, CH, ed);
            win_.drawRect(x, y, CW, CH, gr);

            std::string d;
            if (sel && editing_) {
                d = editBuf_ + '|';
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

    win_.updateDisplay();
}
