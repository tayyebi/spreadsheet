// =============================================================================
// mouse.cpp  —  App::onMouse(): handle one mouse button press event
//
// Handles two kinds of left-button clicks:
//   1. Toolbar button clicks — trigger the corresponding action
//   2. Grid cell clicks — move the selection (committing any in-progress edit)
// =============================================================================

#include "app.h"   // App declaration
#include <string>  // std::string, std::to_string

// ---------------------------------------------------------------------------
// colLabel()  —  convert a zero-based column index to an Excel-style label
//   0 → "A", 1 → "B", …, 25 → "Z", 26 → "AA", …
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

void App::onMouse(MouseEvent e) {
    if (e.button != 1) return;  // left-button clicks only

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
                editing_ = true;
                if (selRow_ > 0)
                    editBuf_ = "=SUM(" + colLabel(selCol_) + "1:" +
                               colLabel(selCol_) + std::to_string(selRow_) + ")";
                else
                    editBuf_ = "=SUM()";

            } else if (btn.label == "SvODS") {
                sheet_.saveODS("spreadsheet.ods");

            } else if (btn.label == "LdODS") {
                sheet_.loadODS("spreadsheet.ods");
                sheet_.evaluateAll();
                editing_ = false;
                editBuf_.clear();
            }

            render();
            return;
        }
    }

    // Check grid cell clicks (below toolbar + formula bar + column headers).
    int gridY = TB + FB + HH;
    if (e.x >= HW && e.y >= gridY) {
        int c = (e.x - HW) / CW;
        int r = (e.y - gridY) / CH;
        if (r >= 0 && r < Spreadsheet::ROWS && c >= 0 && c < Spreadsheet::COLS) {
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
