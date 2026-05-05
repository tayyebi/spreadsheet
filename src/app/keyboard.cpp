// =============================================================================
// keyboard.cpp  —  App::onKey(): handle one keyboard event
// =============================================================================

#include "keyboard.h"  // paired header (includes app.h)
#include "csv.h"       // saveCSV, loadCSV
#include <cctype>      // std::isprint

static bool isPrintableCh(char ch) {
    return std::isprint(static_cast<unsigned char>(ch));
}

void App::onKey(KeyEvent e) {
    if (editing_) {
        // ===== EDIT MODE =====
        if (e.key == IWindow::KEY_ENTER && !e.shift) {
            commitEdit(+1, 0);
        } else if (e.key == IWindow::KEY_ENTER && e.shift) {
            commitEdit(-1, 0);
        } else if (e.key == IWindow::KEY_TAB && !e.shift) {
            commitEdit(0, +1);
        } else if (e.key == IWindow::KEY_TAB && e.shift) {
            commitEdit(0, -1);
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
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";

        } else if (e.key == IWindow::KEY_ENTER && e.shift) {
            moveSel(-1, 0, false);

        } else if (e.key == IWindow::KEY_F2) {
            editing_ = true;
            const Cell* c = sheet_.getCell(selRow_, selCol_);
            editBuf_ = c ? c->raw : "";

        } else if (e.key == IWindow::KEY_DELETE || e.key == IWindow::KEY_BACKSPACE) {
            int r0, r1, c0, c1;
            selRect(r0, r1, c0, c1);
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c)
                    setCellWithUndo(r, c, "");
            sheet_.evaluateAll();

        } else if (e.key == IWindow::KEY_HOME && !e.ctrl) {
            selCol_ = 0;
            if (!e.shift) anchorCol_ = 0;
            scrollToSel();

        } else if (e.key == IWindow::KEY_END && !e.ctrl) {
            int lastC = 0;
            for (int c = 0; c < cols_; ++c)
                if (!rawOf(selRow_, c).empty()) lastC = c;
            selCol_ = lastC;
            if (!e.shift) anchorCol_ = lastC;
            scrollToSel();

        } else if (e.key == IWindow::KEY_HOME && e.ctrl) {
            selRow_ = 0; selCol_ = 0;
            if (!e.shift) { anchorRow_ = 0; anchorCol_ = 0; }
            scrollToSel();

        } else if (e.key == IWindow::KEY_END && e.ctrl) {
            int lastR = 0, lastC = 0;
            sheet_.forEachCell([&](int r, int c, const Cell& cell) {
                if (!cell.raw.empty()) {
                    if (r > lastR || (r == lastR && c > lastC)) { lastR = r; lastC = c; }
                }
            });
            selRow_ = lastR; selCol_ = lastC;
            if (!e.shift) { anchorRow_ = lastR; anchorCol_ = lastC; }
            scrollToSel();

        } else if (e.key == IWindow::KEY_PGUP) {
            constexpr int PAGE = 10;
            moveSel(-PAGE, 0, e.shift);

        } else if (e.key == IWindow::KEY_PGDN) {
            constexpr int PAGE = 10;
            moveSel(+PAGE, 0, e.shift);

        } else if (e.ctrl && e.ch == 'a') {
            anchorRow_ = 0; anchorCol_ = 0;
            selRow_ = rows_ - 1;
            selCol_ = cols_ - 1;
            scrollToSel();

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
            saveCSV(sheet_, "spreadsheet.csv");

        } else if (e.ctrl && e.ch == 'o') {
            loadCSV(sheet_, "spreadsheet.csv");
            sheet_.evaluateAll();

        } else if (e.ctrl && e.ch == 'n') {
            sheet_.forEachCell([&](int r, int c, const Cell& cell) {
                if (!cell.raw.empty())
                    setCellWithUndo(r, c, "");
            });
            sheet_.evaluateAll();
            selRow_ = 0; selCol_ = 0;
            anchorRow_ = 0; anchorCol_ = 0;
            viewRow_ = 0; viewCol_ = 0;

        } else if (!e.ctrl && !e.alt && e.ch && isPrintableCh(e.ch)) {
            editing_ = true;
            editBuf_ = std::string(1, e.ch);
        }
    }

    render();
}
