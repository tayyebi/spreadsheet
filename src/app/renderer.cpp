// =============================================================================
// renderer.cpp  —  App::renderToolbar(), renderFormulaBar(), renderGrid(),
//                  App::render()
//
// Terminal UI renderer.  All coordinates are character positions (col, row)
// where (0,0) is the top-left of the terminal.
//
// Layout:
//   Row 0        : toolbar — key-shortcut help bar
//   Row 1        : formula bar — "[cellname] | raw content"
//   Row 2        : column-header row
//   Rows 3..22   : spreadsheet rows 1..20
// =============================================================================

#include "renderer.h"  // paired header (includes app.h)
#include <string>
#include <algorithm>   // std::min / std::max

// ---------------------------------------------------------------------------
// Colour palette  (same RGB values as the old pixel renderer)
// ---------------------------------------------------------------------------

// CURSOR_CHAR is appended to the edit buffer to indicate the insertion point.
// A vertical bar matches the conventional text-cursor appearance.
static constexpr char CURSOR_CHAR = '|';
// ---------------------------------------------------------------------------
static constexpr Color C_BG      = {255, 255, 255};  // cell background
static constexpr Color C_HDR_BG  = {220, 220, 220};  // header background
static constexpr Color C_SEL_BG  = {198, 224, 255};  // selected-range fill
static constexpr Color C_EDIT_BG = {255, 255, 200};  // editing cell fill
static constexpr Color C_TB_BG   = {235, 235, 235};  // toolbar background
static constexpr Color C_TXT     = {  0,   0,   0};  // normal text
static constexpr Color C_SEL_TXT = {  0,   0, 180};  // active-cursor text
static constexpr Color C_GR      = {140, 140, 140};  // grid separator colour
static constexpr Color C_HDR_HL  = {180, 200, 230};  // header in-selection highlight

// ---------------------------------------------------------------------------
// CL()  —  zero-based column index to Excel-style label ("A", "B", …, "AA")
// ---------------------------------------------------------------------------
static std::string CL(int c) {
    std::string s;
    int n = c + 1;
    while (n > 0) {
        s = char('A' + (n - 1) % 26) + s;
        n = (n - 1) / 26;
    }
    return s;
}

static std::string cellName(int r, int c) {
    return CL(c) + std::to_string(r + 1);
}

// Fit or pad `s` into exactly `w` characters, left-aligned.
static std::string fitL(const std::string& s, int w) {
    if (w <= 0) return {};
    if ((int)s.size() >= w) return s.substr(0, w);
    return s + std::string(w - (int)s.size(), ' ');
}

// Right-align `s` in exactly `w` characters.
static std::string fitR(const std::string& s, int w) {
    if (w <= 0) return {};
    if ((int)s.size() >= w) return s.substr(0, w);
    return std::string(w - (int)s.size(), ' ') + s;
}

// ---------------------------------------------------------------------------
// App::renderToolbar()  —  row 0: key-shortcut help line
// ---------------------------------------------------------------------------
void App::renderToolbar() {
    int W = HW + Spreadsheet::COLS * CW;
    win_.fillRect(0, 0, W, TB, C_TB_BG);

    static const char HELP[] =
        " ^N:New  ^O:Open  ^S:Save | "
        "^Z:Undo  ^Y:Redo | "
        "^X:Cut  ^C:Copy  ^V:Paste | "
        "^D:FillDown  ^R:FillRight | "
        "F2:Edit  Del:Clear  ^Q:Quit";

    win_.drawText(0, 0, fitL(HELP, W), C_TXT);
}

// ---------------------------------------------------------------------------
// App::renderFormulaBar()  —  row TB (=1): cell-name | formula content
// ---------------------------------------------------------------------------
void App::renderFormulaBar() {
    int W = HW + Spreadsheet::COLS * CW;
    int Y = TB;  // row index of formula bar

    win_.fillRect(0, Y, W, FB, C_BG);

    // Left panel: cell name right-aligned in HW-1 chars, then '|'
    std::string name = fitR(cellName(selRow_, selCol_), HW - 1);
    win_.drawText(0, Y, name, C_TXT);
    win_.drawText(HW - 1, Y, "|", C_GR);

    // Right panel: raw cell content (or edit buffer while editing)
    std::string content;
    if (editing_) {
        content = editBuf_ + CURSOR_CHAR;  // append cursor marker
    } else {
        const Cell* cell = sheet_.getCell(selRow_, selCol_);
        if (cell) content = cell->raw;
    }
    win_.drawText(HW, Y, fitL(content, W - HW), C_TXT);
}

// ---------------------------------------------------------------------------
// App::renderGrid()  —  column headers + all data rows
// ---------------------------------------------------------------------------
void App::renderGrid() {
    constexpr int GY = TB + FB;  // row index of column-header line

    int sr0, sr1, sc0, sc1;
    selRect(sr0, sr1, sc0, sc1);
    bool multiSel = (sr0 != sr1 || sc0 != sc1);

    // ---- Column-header row ------------------------------------------------
    // Corner cell (top-left blank area)
    win_.fillRect(0, GY, HW, HH, C_HDR_BG);
    win_.drawText(HW - 1, GY, "|", C_GR);

    for (int c = 0; c < Spreadsheet::COLS; ++c) {
        int    x    = HW + c * CW;
        bool   inR  = (c >= sc0 && c <= sc1);
        Color  hbg  = inR ? C_HDR_HL : C_HDR_BG;

        win_.fillRect(x, GY, CW, HH, hbg);

        // Centre the column label in CW-1 chars, append '|' separator.
        std::string lbl = CL(c);
        int pad = ((CW - 1) - (int)lbl.size()) / 2;
        std::string cell = fitL(std::string(pad, ' ') + lbl, CW - 1) + '|';
        win_.drawText(x, GY, cell, C_TXT);
    }

    // ---- Data rows --------------------------------------------------------
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        int  y       = GY + HH + r * CH;
        bool rowInR  = (r >= sr0 && r <= sr1);

        // Row-number header
        win_.fillRect(0, y, HW, CH, rowInR ? C_HDR_HL : C_HDR_BG);
        win_.drawText(0, y, fitR(std::to_string(r + 1), HW - 1) + '|', C_TXT);

        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            int  x        = HW + c * CW;
            bool isCursor = (r == selRow_ && c == selCol_);
            bool inRange  = multiSel && rowInR && (c >= sc0 && c <= sc1);

            // Background colour
            Color cellbg;
            if      (isCursor && editing_) cellbg = C_EDIT_BG;
            else if (isCursor)             cellbg = C_SEL_BG;
            else if (inRange)              cellbg = C_SEL_BG;
            else                           cellbg = C_BG;

            win_.fillRect(x, y, CW, CH, cellbg);

            // Content text
            std::string d;
            if (isCursor && editing_) {
                d = editBuf_ + CURSOR_CHAR;
            } else {
                const Cell* cell = sheet_.getCell(r, c);
                if (cell) d = cell->display;
            }

            Color txtc = isCursor ? C_SEL_TXT : C_TXT;
            // CW-1 chars of content, then '|' separator.
            win_.drawText(x, y, fitL(d, CW - 1) + '|', txtc);
        }
    }
}

// ---------------------------------------------------------------------------
// App::render()
// ---------------------------------------------------------------------------
void App::render() {
    renderToolbar();
    renderFormulaBar();
    renderGrid();
    win_.updateDisplay();
}
