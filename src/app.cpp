#include "app.h"      // own header
#include "core.h"     // Spreadsheet constants
#include <string>     // std::to_string
#include <cctype>     // std::isprint

// Build column letter label (A, B, …, Z, AA, …)
static std::string colLabel(int c) {
    std::string s;     // result string
    int n = c + 1;     // convert to 1-based
    while (n > 0) {                                 // decompose into base-26
        s  = static_cast<char>('A' + (n - 1) % 26) + s; // prepend letter
        n  = (n - 1) / 26;                               // shift
    }
    return s; // return label
}

// ── Constructor ──────────────────────────────────────────────────────────────

App::App(IWindow& win) : win_(win) {
    // Register our onKey method as the key-event handler
    win_.handleInput([this](KeyEvent e) { onKey(e); }); // bind handler
}

// ── render ───────────────────────────────────────────────────────────────────

void App::render() {
    // Common colours
    const Color bg     {255, 255, 255}; // white background
    const Color grid   {200, 200, 200}; // light-grey grid lines
    const Color hdrBg  {220, 220, 220}; // grey header fill
    const Color selClr {  0,   0, 200}; // blue selection border
    const Color txt    {  0,   0,   0}; // black text
    const Color editBg {255, 255, 200}; // light-yellow editing fill

    int W = HDR_W + Spreadsheet::COLS * CELL_W; // total grid width
    int H = HDR_H + Spreadsheet::ROWS * CELL_H; // total grid height

    win_.fillRect(0, 0, W, H, bg); // clear entire canvas

    // ── column header row ────────────────────────────────────────────────────
    win_.fillRect(0, 0, HDR_W, HDR_H, hdrBg); // corner cell fill
    for (int c = 0; c < Spreadsheet::COLS; ++c) {        // each column
        int x = HDR_W + c * CELL_W;                      // left edge
        win_.fillRect(x, 0, CELL_W, HDR_H, hdrBg);       // header fill
        win_.drawRect(x, 0, CELL_W, HDR_H, grid);         // header border
        win_.drawText(x + 4, 4, colLabel(c), txt);        // column letter
    }

    // ── row header column ────────────────────────────────────────────────────
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {         // each row
        int y = HDR_H + r * CELL_H;                       // top edge
        win_.fillRect(0, y, HDR_W, CELL_H, hdrBg);        // header fill
        win_.drawRect(0, y, HDR_W, CELL_H, grid);          // header border
        win_.drawText(4, y + 5, std::to_string(r + 1), txt); // row number
    }

    // ── data cells ──────────────────────────────────────────────────────────
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {         // each row
        for (int c = 0; c < Spreadsheet::COLS; ++c) {     // each column
            int x = HDR_W + c * CELL_W; // left edge of cell
            int y = HDR_H + r * CELL_H; // top  edge of cell

            bool isSel = (r == selRow_ && c == selCol_); // is this selected?
            if (isSel && editing_)
                win_.fillRect(x, y, CELL_W, CELL_H, editBg); // editing highlight

            win_.drawRect(x, y, CELL_W, CELL_H, grid); // cell border

            std::string disp; // text to display
            if (isSel && editing_) {
                disp = editBuf_ + '|'; // show edit buffer with cursor
            } else {
                const Cell* cell = sheet_.getCell(r, c); // fetch cell
                if (cell) disp = cell->display;           // use computed display
            }
            if (!disp.empty())
                win_.drawText(x + 3, y + 5, disp, txt); // draw cell text
        }
    }

    // ── selection highlight (thick border, 3 px) ─────────────────────────────
    int sx = HDR_W + selCol_ * CELL_W; // selection X
    int sy = HDR_H + selRow_ * CELL_H; // selection Y
    for (int i = 0; i < 3; ++i) {                              // three rings
        win_.drawRect(sx - i, sy - i,
                      CELL_W + 2 * i, CELL_H + 2 * i, selClr); // ring border
    }

    win_.updateDisplay(); // blit back buffer to screen
}

// ── onKey ────────────────────────────────────────────────────────────────────

void App::onKey(KeyEvent e) {
    if (editing_) { // ── editing mode ──────────────────────────────────────
        if (e.key == IWindow::KEY_ENTER) {          // commit edit
            sheet_.setCell(selRow_, selCol_, editBuf_); // store raw input
            sheet_.evaluateAll();                       // re-evaluate formulas
            editing_ = false;                           // leave edit mode
        } else if (e.key == IWindow::KEY_ESC) {     // cancel edit
            editing_ = false;                       // discard buffer
            editBuf_.clear();                       // clear buffer
        } else if (e.key == IWindow::KEY_BACKSPACE) { // delete character
            if (!editBuf_.empty()) editBuf_.pop_back(); // remove last char
        } else if (e.ch != 0 &&
                   std::isprint(static_cast<unsigned char>(e.ch)) &&
                   !e.ctrl) {                       // printable character
            editBuf_ += e.ch;                       // append to buffer
        }
    } else { // ── navigation mode ──────────────────────────────────────────
        if (e.key == IWindow::KEY_UP   && selRow_ > 0)
            --selRow_; // move selection up
        else if (e.key == IWindow::KEY_DOWN  && selRow_ < Spreadsheet::ROWS - 1)
            ++selRow_; // move selection down
        else if (e.key == IWindow::KEY_LEFT  && selCol_ > 0)
            --selCol_; // move selection left
        else if (e.key == IWindow::KEY_RIGHT && selCol_ < Spreadsheet::COLS - 1)
            ++selCol_; // move selection right
        else if (e.key == IWindow::KEY_ENTER) {          // begin editing
            editing_ = true;                             // enter edit mode
            const Cell* cell = sheet_.getCell(selRow_, selCol_); // get current
            editBuf_ = cell ? cell->raw : "";            // seed buffer with raw
        } else if (e.ctrl && (e.ch == 's' || e.ch == 19)) { // Ctrl+S = save
            sheet_.saveCSV("spreadsheet.csv");           // write CSV file
        } else if (e.ctrl && (e.ch == 'o' || e.ch == 15)) { // Ctrl+O = open
            sheet_.loadCSV("spreadsheet.csv");           // read CSV file
            sheet_.evaluateAll();                        // re-evaluate
        }
    }
    render(); // redraw after every key event
}
