// =============================================================================
// keyboard.cpp  —  App::onKey(): handle one keyboard event
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
// =============================================================================

#include "app.h"   // App declaration
#include <cctype>  // std::isprint

void App::onKey(KeyEvent e) {
    if (editing_) {
        // ----- EDIT mode -----
        if (e.key == IWindow::KEY_ENTER) {
            sheet_.setCell(selRow_, selCol_, editBuf_);
            sheet_.evaluateAll();
            editing_ = false;
            editBuf_.clear();
        } else if (e.key == IWindow::KEY_TAB) {
            sheet_.setCell(selRow_, selCol_, editBuf_);
            sheet_.evaluateAll();
            editing_ = false;
            editBuf_.clear();
            if (selCol_ < Spreadsheet::COLS - 1) ++selCol_;
        } else if (e.key == IWindow::KEY_ESC) {
            editing_ = false;
            editBuf_.clear();
        } else if (e.key == IWindow::KEY_BACKSPACE) {
            if (!editBuf_.empty()) editBuf_.pop_back();
        } else if (e.ch && std::isprint(static_cast<unsigned char>(e.ch)) && !e.ctrl) {
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
            sheet_.setCell(selRow_, selCol_, "");
            sheet_.evaluateAll();
        }
        else if (e.key == IWindow::KEY_ENTER || e.key == IWindow::KEY_F2) {
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
        else if (e.ctrl && e.shift && (e.ch == 's' || e.ch == 'S'))
            sheet_.saveODS("spreadsheet.ods");
        else if (e.ctrl && e.shift && (e.ch == 'o' || e.ch == 'O')) {
            sheet_.loadODS("spreadsheet.ods");
            sheet_.evaluateAll();
        }
    }

    render();
}
