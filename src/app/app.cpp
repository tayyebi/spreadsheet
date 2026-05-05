// =============================================================================
// app.cpp  —  App constructor, toolbar init, and shared helper implementations
// =============================================================================

#include "app.h"     // App declaration
#include "csv.h"     // saveCSV, loadCSV
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
// SIGMA_LABEL  —  UTF-8 encoding of the Greek capital letter Σ (U+03A3)
//
// Used as the toolbar label for the "insert SUM formula" button.
// ---------------------------------------------------------------------------
static constexpr const char* SIGMA_LABEL = "\xCE\xA3";

// ---------------------------------------------------------------------------
// App::App()
// ---------------------------------------------------------------------------
App::App(IWindow& w) : win_(w) {
    win_.handleInput([this](KeyEvent e) { onKey(e); });
    win_.handleMouse([this](MouseEvent e) { onMouse(e); });
    initToolbar();
}

// ---------------------------------------------------------------------------
// App::initToolbar()  —  TUI: no pixel-positioned buttons needed.
// The toolbar is rendered as a static key-shortcut help line.
// ---------------------------------------------------------------------------
void App::initToolbar() {}

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
// App::commitEdit()
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
//
// Auto-expands the grid when the cursor tries to move past the last row/col,
// then scrolls the viewport so the cursor is always visible.
// ---------------------------------------------------------------------------
void App::moveSel(int dr, int dc, bool shift) {
    // Attempt the raw move first so we know if we're hitting the boundary.
    int nr = selRow_ + dr;
    int nc = selCol_ + dc;

    // Auto-expand rows when the cursor steps past the last row.
    if (dr > 0 && nr >= rows_) {
        int newRows = rows_ + GROW_ROWS;
        if (newRows > Spreadsheet::MAX_ROWS) newRows = Spreadsheet::MAX_ROWS;
        rows_ = newRows;
    }
    // Auto-expand cols when the cursor steps past the last column.
    if (dc > 0 && nc >= cols_) {
        int newCols = cols_ + GROW_COLS;
        if (newCols > Spreadsheet::MAX_COLS) newCols = Spreadsheet::MAX_COLS;
        cols_ = newCols;
    }

    selRow_ = clampRow(nr);
    selCol_ = clampCol(nc);
    if (!shift) {
        anchorRow_ = selRow_;
        anchorCol_ = selCol_;
    }
    scrollToSel();
}

// ---------------------------------------------------------------------------
// App::scrollToSel()  —  adjust the viewport so the cursor cell is visible
// ---------------------------------------------------------------------------
void App::scrollToSel() {
    int vr = visibleRows();
    int vc = visibleCols();

    if (selRow_ < viewRow_)
        viewRow_ = selRow_;
    else if (selRow_ >= viewRow_ + vr)
        viewRow_ = selRow_ - vr + 1;

    if (selCol_ < viewCol_)
        viewCol_ = selCol_;
    else if (selCol_ >= viewCol_ + vc)
        viewCol_ = selCol_ - vc + 1;

    if (viewRow_ < 0) viewRow_ = 0;
    if (viewCol_ < 0) viewCol_ = 0;
}

// ---------------------------------------------------------------------------
// App::jumpEdge()  —  Ctrl+Arrow: jump to the edge of a data block
// ---------------------------------------------------------------------------
void App::jumpEdge(int dr, int dc, bool shift) {
    int r = selRow_, c = selCol_;
    bool curEmpty = rawOf(r, c).empty();

    if (curEmpty) {
        while (true) {
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= rows_ || nc < 0 || nc >= cols_) break;
            r = nr; c = nc;
            if (!rawOf(r, c).empty()) break;
        }
    } else {
        while (true) {
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= rows_ || nc < 0 || nc >= cols_) break;
            if (rawOf(nr, nc).empty()) break;
            r = nr; c = nc;
        }
    }

    selRow_ = clampRow(r);
    selCol_ = clampCol(c);
    if (!shift) { anchorRow_ = selRow_; anchorCol_ = selCol_; }
    scrollToSel();
}

// ---------------------------------------------------------------------------
// App::pushUndo()
// ---------------------------------------------------------------------------
void App::pushUndo(int r, int c, const std::string& oldRaw, const std::string& newRaw) {
    if (oldRaw == newRaw) return;
    undoStack_.push_back({r, c, oldRaw, newRaw});
    redoStack_.clear();
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
// App::doFillDown()  —  Ctrl+D
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
// App::doFillRight()  —  Ctrl+R
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
        // forEachCell visits sparse-map entries, including cells cleared to "".
        // Only push undo entries for cells that currently hold real content.
        sheet_.forEachCell([&](int r, int c, const Cell& cell) {
            if (!cell.raw.empty())
                setCellWithUndo(r, c, "");
        });
        sheet_.evaluateAll();
    } else if (lbl == "Open") {
        loadCSV(sheet_, "spreadsheet.csv");
        sheet_.evaluateAll();
    } else if (lbl == "Save") {
        saveCSV(sheet_, "spreadsheet.csv");
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
        std::string formula = "=SUM(";
        if (selRow_ > 0) {
            formula += CL(selCol_) + "1:" + CL(selCol_) + std::to_string(selRow_);
        }
        formula += ")";
        editing_ = true;
        editBuf_ = formula;
    }
}
