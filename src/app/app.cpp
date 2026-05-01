// =============================================================================
// app.cpp  —  App method implementations: toolbar, formula bar, grid rendering,
//             mouse handling, and comprehensive keyboard shortcuts.
// =============================================================================

#include "app.h"
#include <string>
#include <cctype>
#include <algorithm>   // std::min / max
#include <sstream>

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

// ---------------------------------------------------------------------------
// isPrintableCh()  —  safe wrapper for std::isprint with char argument
//
// std::isprint(int) has undefined behaviour when passed a negative char value
// (e.g. non-ASCII characters on platforms where char is signed).  Casting to
// unsigned char first is the standard-conforming fix (see C++17 §[cctype]).
// ---------------------------------------------------------------------------
static bool isPrintableCh(char ch) {
    return std::isprint(static_cast<unsigned char>(ch));
}

// ---------------------------------------------------------------------------
// SIGMA_LABEL  —  UTF-8 encoding of the Greek capital letter Σ (U+03A3)
//
// Used as the toolbar label for the "insert SUM formula" button.
// ---------------------------------------------------------------------------
static constexpr const char* SIGMA_LABEL = "\xCE\xA3";

// ---------------------------------------------------------------------------
// cellName()  —  e.g. row=0, col=1  →  "B1"
// ---------------------------------------------------------------------------
static std::string cellName(int r, int c) {
    return CL(c) + std::to_string(r + 1);
}

// ---------------------------------------------------------------------------
// App::App()
// ---------------------------------------------------------------------------
App::App(IWindow& w) : win_(w) {
    win_.handleInput([this](KeyEvent e) { onKey(e); });
    win_.handleMouse([this](MouseEvent e) { onMouse(e); });
    initToolbar();
}

// ---------------------------------------------------------------------------
// App::initToolbar()  —  build the toolbar button list
//
// Buttons are laid out left-to-right at y=5 inside the TB=32 px toolbar strip.
// A label of "|" is a visual separator (drawn as a thin vertical line, not a
// clickable button).
// ---------------------------------------------------------------------------
void App::initToolbar() {
    // Button dimensions
    constexpr int BH = 22;  // button height
    constexpr int BY =  5;  // top y inside toolbar

    // Helper lambda to append a button and return the next x position.
    int x = 4;
    auto add = [&](const std::string& lbl, int w) {
        toolBtns_.push_back({x, BY, w, BH, lbl, true});
        x += w + 3;
    };
    auto sep = [&]() {
        // Separator: zero-width marker; rendered as a vertical line.
        toolBtns_.push_back({x, BY, 1, BH, "|", false});
        x += 7;
    };

    add("New",   36);
    add("Open",  42);
    add("Save",  38);
    sep();
    add("Undo",  38);
    add("Redo",  38);
    sep();
    add("Cut",   32);
    add("Copy",  42);
    add("Paste", 46);
    sep();
    add("B",     22);   // Bold (visual; font support not available)
    add("I",     22);   // Italic (visual)
    add("U",     22);   // Underline (visual)
    sep();
    add(SIGMA_LABEL, 28); // Σ — insert SUM formula
}

// ---------------------------------------------------------------------------
// App::selRect()  —  normalised selection rectangle
// ---------------------------------------------------------------------------
void App::selRect(int& r0, int& r1, int& c0, int& c1) const {
    r0 = std::min(selRow_, anchorRow_);
    r1 = std::max(selRow_, anchorRow_);
    c0 = std::min(selCol_, anchorCol_);
    c1 = std::max(selCol_, anchorCol_);
}

// ---------------------------------------------------------------------------
// App::rawOf()  —  raw string of a cell, "" if absent
// ---------------------------------------------------------------------------
std::string App::rawOf(int r, int c) const {
    const Cell* cell = sheet_.getCell(r, c);
    return cell ? cell->raw : "";
}

// ---------------------------------------------------------------------------
// App::renderToolbar()
// ---------------------------------------------------------------------------
void App::renderToolbar() {
    Color tb_bg {235, 235, 235};  // toolbar background
    Color tb_bd {180, 180, 180};  // button border
    Color btn_h {255, 255, 255};  // button highlight (face)
    Color tx    {  0,   0,   0};  // text
    Color sep_c {160, 160, 160};  // separator line colour

    int W = HW + Spreadsheet::COLS * CW;

    // Toolbar background
    win_.fillRect(0, 0, W, TB, tb_bg);
    // Bottom border line
    win_.drawRect(0, TB - 1, W, 1, sep_c);

    for (const auto& btn : toolBtns_) {
        if (btn.label == "|") {
            // Draw separator as a vertical line in the middle of the button area.
            int mx = btn.x + btn.w / 2;
            for (int y = btn.y + 2; y < btn.y + btn.h - 2; ++y)
                win_.fillRect(mx, y, 1, 1, sep_c);
        } else {
            // Draw button face
            win_.fillRect(btn.x, btn.y, btn.w, btn.h, btn_h);
            win_.drawRect(btn.x, btn.y, btn.w, btn.h, tb_bd);
            // Centre text horizontally (approximate: 6 px per character)
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
    int Y = TB;  // formula bar starts just below toolbar

    // Background
    win_.fillRect(0, Y, W, FB, fb_bg);
    // Bottom border
    win_.drawRect(0, Y + FB - 1, W, 1, fb_bd);

    // Cell-name box (e.g. "B3")
    constexpr int NW = 52;  // width of the name box
    win_.fillRect(2, Y + 2, NW, FB - 4, hd);
    win_.drawRect(2, Y + 2, NW, FB - 4, fb_bd);
    win_.drawText(6, Y + 5, cellName(selRow_, selCol_), tx);

    // Separator line between name box and formula content
    win_.fillRect(NW + 4, Y + 3, 1, FB - 6, fb_bd);

    // Formula / value area
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
    Color bg  {255, 255, 255};
    Color gr  {200, 200, 200};
    Color hd  {220, 220, 220};
    Color sl  {  0,   0, 200};  // selection border
    Color rng_bg {198, 224, 255}; // light-blue range highlight
    Color tx  {  0,   0,   0};
    Color ed  {255, 255, 200};

    // Y offset for the entire grid (below toolbar + formula bar)
    constexpr int GY = TB + FB;

    int W = HW + Spreadsheet::COLS * CW;
    int H = GY + HH + Spreadsheet::ROWS * CH;

    // Clear grid area
    win_.fillRect(0, GY, W, H - GY, bg);

    // Corner header
    win_.fillRect(0, GY, HW, HH, hd);

    // Determine selection range
    int sr0, sr1, sc0, sc1;
    selRect(sr0, sr1, sc0, sc1);
    bool multiSel = (sr0 != sr1 || sc0 != sc1);

    // Column headers
    for (int c = 0; c < Spreadsheet::COLS; ++c) {
        int x = HW + c * CW;
        bool inRange = (c >= sc0 && c <= sc1);
        win_.fillRect(x, GY, CW, HH, inRange ? Color{180,200,230} : hd);
        win_.drawRect(x, GY, CW, HH, gr);
        win_.drawText(x + 4, GY + 4, CL(c), tx);
    }

    // Row headers
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        int y = GY + HH + r * CH;
        bool inRange = (r >= sr0 && r <= sr1);
        win_.fillRect(0, y, HW, CH, inRange ? Color{180,200,230} : hd);
        win_.drawRect(0, y, HW, CH, gr);
        win_.drawText(4, y + 5, std::to_string(r + 1), tx);
    }

    // Data cells
    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            int  x   = HW + c * CW;
            int  y   = GY + HH + r * CH;
            bool sel = (r == selRow_ && c == selCol_);
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

    // Selection highlight: thick blue border around the active cell
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

// ---------------------------------------------------------------------------
// App::commitEdit()  —  save editBuf_ to the model, move cursor, re-evaluate
//   dr / dc — row/col delta to apply after commit (0 = stay put)
// ---------------------------------------------------------------------------
void App::commitEdit(int dr, int dc) {
    if (!editing_) return;
    std::string oldRaw = rawOf(selRow_, selCol_);
    sheet_.setCell(selRow_, selCol_, editBuf_);
    sheet_.evaluateAll();
    pushUndo(selRow_, selCol_, oldRaw, editBuf_);
    editing_ = false;
    editBuf_.clear();
    moveSel(dr, dc, false);
}

// ---------------------------------------------------------------------------
// App::cancelEdit()
// ---------------------------------------------------------------------------
void App::cancelEdit() {
    editing_ = false;
    editBuf_.clear();
}

// ---------------------------------------------------------------------------
// App::moveSel()  —  move the active cell, optionally extending the range
// ---------------------------------------------------------------------------
void App::moveSel(int dr, int dc, bool shift) {
    int nr = clampRow(selRow_ + dr);
    int nc = clampCol(selCol_ + dc);
    selRow_ = nr;
    selCol_ = nc;
    if (!shift) {
        anchorRow_ = nr;
        anchorCol_ = nc;
    }
}

// ---------------------------------------------------------------------------
// App::jumpEdge()  —  Ctrl+Arrow: jump to the edge of a data block
// ---------------------------------------------------------------------------
void App::jumpEdge(int dr, int dc, bool shift) {
    int r = selRow_, c = selCol_;
    bool curEmpty = rawOf(r, c).empty();

    if (curEmpty) {
        // Current cell is empty: scan for the first non-empty cell in direction
        while (true) {
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= Spreadsheet::ROWS || nc < 0 || nc >= Spreadsheet::COLS) break;
            r = nr; c = nc;
            if (!rawOf(r, c).empty()) break;
        }
    } else {
        // Current cell is non-empty: scan to the last non-empty before a gap
        while (true) {
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= Spreadsheet::ROWS || nc < 0 || nc >= Spreadsheet::COLS) break;
            if (rawOf(nr, nc).empty()) break;
            r = nr; c = nc;
        }
    }

    selRow_ = clampRow(r);
    selCol_ = clampCol(c);
    if (!shift) { anchorRow_ = selRow_; anchorCol_ = selCol_; }
}

// ---------------------------------------------------------------------------
// App::pushUndo()
// ---------------------------------------------------------------------------
void App::pushUndo(int r, int c, const std::string& oldRaw, const std::string& newRaw) {
    if (oldRaw == newRaw) return;
    undoStack_.push_back({r, c, oldRaw, newRaw});
    redoStack_.clear();  // any new edit invalidates the redo stack
}

// ---------------------------------------------------------------------------
// App::setCellWithUndo()
// ---------------------------------------------------------------------------
void App::setCellWithUndo(int r, int c, const std::string& newRaw) {
    std::string oldRaw = rawOf(r, c);
    sheet_.setCell(r, c, newRaw);
    pushUndo(r, c, oldRaw, newRaw);
}

// ---------------------------------------------------------------------------
// App::doUndo()
// ---------------------------------------------------------------------------
void App::doUndo() {
    if (undoStack_.empty()) return;
    auto& e = undoStack_.back();
    sheet_.setCell(e.r, e.c, e.oldRaw);
    redoStack_.push_back(e);
    undoStack_.pop_back();
    sheet_.evaluateAll();
}

// ---------------------------------------------------------------------------
// App::doRedo()
// ---------------------------------------------------------------------------
void App::doRedo() {
    if (redoStack_.empty()) return;
    auto& e = redoStack_.back();
    sheet_.setCell(e.r, e.c, e.newRaw);
    undoStack_.push_back(e);
    redoStack_.pop_back();
    sheet_.evaluateAll();
}

// ---------------------------------------------------------------------------
// App::doCopy()
// ---------------------------------------------------------------------------
void App::doCopy() {
    clipboard_ = rawOf(selRow_, selCol_);
}

// ---------------------------------------------------------------------------
// App::doCut()
// ---------------------------------------------------------------------------
void App::doCut() {
    clipboard_ = rawOf(selRow_, selCol_);
    setCellWithUndo(selRow_, selCol_, "");
    sheet_.evaluateAll();
}

// ---------------------------------------------------------------------------
// App::doPaste()
// ---------------------------------------------------------------------------
void App::doPaste() {
    if (clipboard_.empty()) return;
    setCellWithUndo(selRow_, selCol_, clipboard_);
    sheet_.evaluateAll();
}

// ---------------------------------------------------------------------------
// App::doFillDown()  —  Ctrl+D: fill selection downward with the top-row value
// ---------------------------------------------------------------------------
void App::doFillDown() {
    int r0, r1, c0, c1;
    selRect(r0, r1, c0, c1);
    for (int c = c0; c <= c1; ++c) {
        std::string src = rawOf(r0, c);
        for (int r = r0 + 1; r <= r1; ++r)
            setCellWithUndo(r, c, src);
    }
    sheet_.evaluateAll();
}

// ---------------------------------------------------------------------------
// App::doFillRight()  —  Ctrl+R: fill selection rightward with the left-col value
// ---------------------------------------------------------------------------
void App::doFillRight() {
    int r0, r1, c0, c1;
    selRect(r0, r1, c0, c1);
    for (int r = r0; r <= r1; ++r) {
        std::string src = rawOf(r, c0);
        for (int c = c0 + 1; c <= c1; ++c)
            setCellWithUndo(r, c, src);
    }
    sheet_.evaluateAll();
}

// ---------------------------------------------------------------------------
// App::toolbarAction()  —  dispatch a toolbar button click
// ---------------------------------------------------------------------------
void App::toolbarAction(int idx) {
    if (idx < 0 || idx >= (int)toolBtns_.size()) return;
    const std::string& lbl = toolBtns_[idx].label;

    if (lbl == "New") {
        // Clear the sheet (each non-empty cell becomes an undo entry)
        for (int r = 0; r < Spreadsheet::ROWS; ++r)
            for (int c = 0; c < Spreadsheet::COLS; ++c)
                if (!rawOf(r, c).empty())
                    setCellWithUndo(r, c, "");
        sheet_.evaluateAll();
    } else if (lbl == "Open") {
        sheet_.loadCSV("spreadsheet.csv");
        sheet_.evaluateAll();
    } else if (lbl == "Save") {
        sheet_.saveCSV("spreadsheet.csv");
    } else if (lbl == "Undo") {
        doUndo();
    } else if (lbl == "Redo") {
        doRedo();
    } else if (lbl == "Cut") {
        doCut();
    } else if (lbl == "Copy") {
        doCopy();
    } else if (lbl == "Paste") {
        doPaste();
    } else if (lbl == SIGMA_LABEL) {
        // Insert a SUM formula for the column above the selected cell.
        // Build =SUM(A1:A<selRow_>) if there are rows above; else just =SUM().
        std::string formula = "=SUM(";
        if (selRow_ > 0) {
            formula += CL(selCol_) + "1:" + CL(selCol_) + std::to_string(selRow_);
        }
        formula += ")";
        editing_ = true;
        editBuf_ = formula;
    }
    // Bold / Italic / Underline: font attribute support not available in the
    // current drawing layer — buttons are displayed but have no effect yet.
}

// ---------------------------------------------------------------------------
// App::onMouse()
// ---------------------------------------------------------------------------
void App::onMouse(MouseEvent e) {
    if (!e.pressed) return;  // ignore release events

    int x = e.x, y = e.y;

    // --- Toolbar click ---
    if (y < TB) {
        for (int i = 0; i < (int)toolBtns_.size(); ++i) {
            const auto& btn = toolBtns_[i];
            if (btn.label == "|") continue;
            if (x >= btn.x && x < btn.x + btn.w &&
                y >= btn.y && y < btn.y + btn.h) {
                if (editing_) commitEdit();
                toolbarAction(i);
                render();
                return;
            }
        }
        return;
    }

    // --- Formula bar click → enter edit mode for selected cell ---
    if (y >= TB && y < TB + FB) {
        if (!editing_) {
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";
        }
        render();
        return;
    }

    // --- Grid click ---
    constexpr int GY = TB + FB;
    if (y < GY + HH) {
        render();
        return;  // click in column-header row — ignore for now
    }

    int row = (y - GY - HH) / CH;
    int col = (x - HW)       / CW;

    if (row < 0 || row >= Spreadsheet::ROWS) { render(); return; }
    if (col < 0 || col >= Spreadsheet::COLS) { render(); return; }

    if (editing_) commitEdit();

    // Shift-click extends the selection range.
    bool shift = false;  // mouse events don't carry modifiers here; use plain click
    selRow_ = row;
    selCol_ = col;
    if (!shift) { anchorRow_ = row; anchorCol_ = col; }

    render();
}

// ---------------------------------------------------------------------------
// App::onKey()
// ---------------------------------------------------------------------------
void App::onKey(KeyEvent e) {
    if (editing_) {
        // ===== EDIT MODE =====
        if (e.key == IWindow::KEY_ENTER && !e.shift) {
            commitEdit(+1, 0);  // commit and move down
        } else if (e.key == IWindow::KEY_ENTER && e.shift) {
            commitEdit(-1, 0);  // Shift+Enter → move up
        } else if (e.key == IWindow::KEY_TAB && !e.shift) {
            commitEdit(0, +1);  // Tab → move right
        } else if (e.key == IWindow::KEY_TAB && e.shift) {
            commitEdit(0, -1);  // Shift+Tab → move left
        } else if (e.key == IWindow::KEY_ESC) {
            cancelEdit();
        } else if (e.key == IWindow::KEY_BACKSPACE) {
            if (!editBuf_.empty()) editBuf_.pop_back();
        } else if (e.key == IWindow::KEY_UP) {
            commitEdit(-1, 0);
        } else if (e.key == IWindow::KEY_DOWN) {
            commitEdit(+1, 0);
        } else if (e.key == IWindow::KEY_LEFT) {
            commitEdit(0, -1);
        } else if (e.key == IWindow::KEY_RIGHT) {
            commitEdit(0, +1);
        } else if (e.ctrl && (e.ch == 'z')) {
            cancelEdit();
            doUndo();
        } else if (e.ctrl && (e.ch == 'y' || (e.ch == 'z' && e.shift))) {
            cancelEdit();
            doRedo();
        } else if (e.ch && isPrintableCh(e.ch) && !e.ctrl && !e.alt) {
            editBuf_ += e.ch;
        }
    } else {
        // ===== NAVIGATION MODE =====

        if (e.key == IWindow::KEY_UP) {
            if (e.ctrl) jumpEdge(-1, 0, e.shift);
            else        moveSel(-1, 0, e.shift);

        } else if (e.key == IWindow::KEY_DOWN) {
            if (e.ctrl) jumpEdge(+1, 0, e.shift);
            else        moveSel(+1, 0, e.shift);

        } else if (e.key == IWindow::KEY_LEFT) {
            if (e.ctrl) jumpEdge(0, -1, e.shift);
            else        moveSel(0, -1, e.shift);

        } else if (e.key == IWindow::KEY_RIGHT) {
            if (e.ctrl) jumpEdge(0, +1, e.shift);
            else        moveSel(0, +1, e.shift);

        } else if (e.key == IWindow::KEY_TAB && !e.shift) {
            moveSel(0, +1, false);

        } else if (e.key == IWindow::KEY_TAB && e.shift) {
            moveSel(0, -1, false);

        } else if (e.key == IWindow::KEY_ENTER && !e.shift) {
            // Enter in navigation: start editing the selected cell.
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";

        } else if (e.key == IWindow::KEY_ENTER && e.shift) {
            moveSel(-1, 0, false);  // Shift+Enter in nav mode → move up

        } else if (e.key == IWindow::KEY_F2) {
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";

        } else if (e.key == IWindow::KEY_DELETE || e.key == IWindow::KEY_BACKSPACE) {
            // Delete / Backspace in nav mode: clear the selection
            int r0, r1, c0, c1;
            selRect(r0, r1, c0, c1);
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c)
                    setCellWithUndo(r, c, "");
            sheet_.evaluateAll();

        } else if (e.key == IWindow::KEY_HOME && !e.ctrl) {
            // Home: go to column A in the same row
            selCol_ = 0;
            if (!e.shift) anchorCol_ = 0;

        } else if (e.key == IWindow::KEY_END && !e.ctrl) {
            // End: go to last non-empty column in the row (or last column)
            int lastC = 0;
            for (int c = 0; c < Spreadsheet::COLS; ++c)
                if (!rawOf(selRow_, c).empty()) lastC = c;
            selCol_ = lastC;
            if (!e.shift) anchorCol_ = lastC;

        } else if (e.key == IWindow::KEY_HOME && e.ctrl) {
            // Ctrl+Home: go to A1
            selRow_ = 0; selCol_ = 0;
            if (!e.shift) { anchorRow_ = 0; anchorCol_ = 0; }

        } else if (e.key == IWindow::KEY_END && e.ctrl) {
            // Ctrl+End: go to the last used cell in the sheet
            int lastR = 0, lastC = 0;
            for (int r = 0; r < Spreadsheet::ROWS; ++r)
                for (int c = 0; c < Spreadsheet::COLS; ++c)
                    if (!rawOf(r, c).empty()) { lastR = r; lastC = c; }
            selRow_ = lastR; selCol_ = lastC;
            if (!e.shift) { anchorRow_ = lastR; anchorCol_ = lastC; }

        } else if (e.key == IWindow::KEY_PGUP) {
            // Page Up: move selection up by the visible number of rows (≈ grid rows)
            constexpr int PAGE = 10;
            moveSel(-PAGE, 0, e.shift);

        } else if (e.key == IWindow::KEY_PGDN) {
            // Page Down: move selection down by a page
            constexpr int PAGE = 10;
            moveSel(+PAGE, 0, e.shift);

        } else if (e.ctrl && e.ch == 'a') {
            // Ctrl+A: select all
            anchorRow_ = 0; anchorCol_ = 0;
            selRow_ = Spreadsheet::ROWS - 1;
            selCol_ = Spreadsheet::COLS - 1;

        } else if (e.ctrl && e.ch == 'c') {
            doCopy();

        } else if (e.ctrl && e.ch == 'x') {
            doCut();

        } else if (e.ctrl && e.ch == 'v') {
            doPaste();

        } else if (e.ctrl && e.ch == 'z' && !e.shift) {
            doUndo();

        } else if (e.ctrl && (e.ch == 'y' || (e.ch == 'z' && e.shift))) {
            doRedo();

        } else if (e.ctrl && e.ch == 'd') {
            doFillDown();

        } else if (e.ctrl && e.ch == 'r') {
            doFillRight();

        } else if (e.ctrl && e.ch == 's') {
            sheet_.saveCSV("spreadsheet.csv");

        } else if (e.ctrl && e.ch == 'o') {
            sheet_.loadCSV("spreadsheet.csv");
            sheet_.evaluateAll();

        } else if (e.ctrl && e.ch == 'n') {
            // Ctrl+N: new (clear sheet)
            for (int r = 0; r < Spreadsheet::ROWS; ++r)
                for (int c = 0; c < Spreadsheet::COLS; ++c)
                    if (!rawOf(r, c).empty())
                        setCellWithUndo(r, c, "");
            sheet_.evaluateAll();
            selRow_ = 0; selCol_ = 0;
            anchorRow_ = 0; anchorCol_ = 0;

        } else if (!e.ctrl && !e.alt && e.ch && isPrintableCh(e.ch)) {
            // Any printable character starts editing and is the first char.
            editing_ = true;
            editBuf_ = std::string(1, e.ch);
        }
    }

    render();
}

