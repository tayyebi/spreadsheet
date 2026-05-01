// =============================================================================
// renderer.cpp  —  App::renderToolbar(), renderFormulaBar(), renderGrid(), render()
// =============================================================================

#include "renderer.h"  // paired header (includes app.h)
#include <string>      // std::string, std::to_string

// ---------------------------------------------------------------------------
// CL()  —  zero-based column index to Excel-style label
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

// ---------------------------------------------------------------------------
// cellName()  —  e.g. row=0, col=1  →  "B1"
// ---------------------------------------------------------------------------
static std::string cellName(int r, int c) {
    return CL(c) + std::to_string(r + 1);
}

// ---------------------------------------------------------------------------
// App::renderToolbar()
// ---------------------------------------------------------------------------
void App::renderToolbar() {
    Color tb_bg {235, 235, 235};
    Color tb_bd {180, 180, 180};
    Color btn_h {255, 255, 255};
    Color tx    {  0,   0,   0};
    Color sep_c {160, 160, 160};

    int W = HW + Spreadsheet::COLS * CW;

    win_.fillRect(0, 0, W, TB, tb_bg);
    win_.drawRect(0, TB - 1, W, 1, sep_c);

    for (const auto& btn : toolBtns_) {
        if (btn.label == "|") {
            int mx = btn.x + btn.w / 2;
            for (int y = btn.y + 2; y < btn.y + btn.h - 2; ++y)
                win_.fillRect(mx, y, 1, 1, sep_c);
        } else {
            win_.fillRect(btn.x, btn.y, btn.w, btn.h, btn_h);
            win_.drawRect(btn.x, btn.y, btn.w, btn.h, tb_bd);
            int tw  = (int)btn.label.size() * 6;
            int tx_ = btn.x + (btn.w - tw) / 2;
            win_.drawText(tx_, btn.y + 4, btn.label, tx);
        }
    }
}

// ---------------------------------------------------------------------------
// App::renderFormulaBar()
// ---------------------------------------------------------------------------
void App::renderFormulaBar() {
    Color fb_bg {255, 255, 255};
    Color fb_bd {180, 180, 180};
    Color hd    {220, 220, 220};
    Color tx    {  0,   0,   0};

    int W = HW + Spreadsheet::COLS * CW;
    int Y = TB;

    win_.fillRect(0, Y, W, FB, fb_bg);
    win_.drawRect(0, Y + FB - 1, W, 1, fb_bd);

    constexpr int NW = 52;
    win_.fillRect(2, Y + 2, NW, FB - 4, hd);
    win_.drawRect(2, Y + 2, NW, FB - 4, fb_bd);
    win_.drawText(6, Y + 5, cellName(selRow_, selCol_), tx);

    win_.fillRect(NW + 4, Y + 3, 1, FB - 6, fb_bd);

    int fx = NW + 8;
    std::string content;
    if (editing_) {
        content = editBuf_ + '|';
    } else {
        const Cell* cell = sheet_.getCell(selRow_, selCol_);
        if (cell) content = cell->raw;
    }
    if (!content.empty())
        win_.drawText(fx, Y + 5, content, tx);
}

// ---------------------------------------------------------------------------
// App::renderGrid()
// ---------------------------------------------------------------------------
void App::renderGrid() {
    Color bg     {255, 255, 255};
    Color gr     {200, 200, 200};
    Color hd     {220, 220, 220};
    Color sl     {  0,   0, 200};
    Color rng_bg {198, 224, 255};
    Color tx     {  0,   0,   0};
    Color ed     {255, 255, 200};

    constexpr int GY = TB + FB;

    int W = HW + Spreadsheet::COLS * CW;
    int H = GY + HH + Spreadsheet::ROWS * CH;

    win_.fillRect(0, GY, W, H - GY, bg);
    win_.fillRect(0, GY, HW, HH, hd);

    int sr0, sr1, sc0, sc1;
    selRect(sr0, sr1, sc0, sc1);
    bool multiSel = (sr0 != sr1 || sc0 != sc1);

    for (int c = 0; c < Spreadsheet::COLS; ++c) {
        int x = HW + c * CW;
        bool inRange = (c >= sc0 && c <= sc1);
        win_.fillRect(x, GY, CW, HH, inRange ? Color{180, 200, 230} : hd);
        win_.drawRect(x, GY, CW, HH, gr);
        win_.drawText(x + 4, GY + 4, CL(c), tx);
    }

    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        int y = GY + HH + r * CH;
        bool inRange = (r >= sr0 && r <= sr1);
        win_.fillRect(0, y, HW, CH, inRange ? Color{180, 200, 230} : hd);
        win_.drawRect(0, y, HW, CH, gr);
        win_.drawText(4, y + 5, std::to_string(r + 1), tx);
    }

    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            int  x       = HW + c * CW;
            int  y       = GY + HH + r * CH;
            bool sel     = (r == selRow_ && c == selCol_);
            bool inRange = multiSel && (r >= sr0 && r <= sr1 && c >= sc0 && c <= sc1);

            if (sel && editing_) {
                win_.fillRect(x, y, CW, CH, ed);
            } else if (inRange) {
                win_.fillRect(x, y, CW, CH, rng_bg);
            }

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

    int sx = HW + selCol_ * CW;
    int sy = GY + HH + selRow_ * CH;
    for (int i = 0; i < 3; ++i)
        win_.drawRect(sx - i, sy - i, CW + 2 * i, CH + 2 * i, sl);
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
