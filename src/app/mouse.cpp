// =============================================================================
// mouse.cpp  —  App::onMouse(): handle one mouse button press event
// =============================================================================

#include "mouse.h"  // paired header (includes app.h)

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
        return;
    }

    int row = (y - GY - HH) / CH;
    int col = (x - HW)       / CW;

    if (row < 0 || row >= Spreadsheet::ROWS) { render(); return; }
    if (col < 0 || col >= Spreadsheet::COLS) { render(); return; }

    if (editing_) commitEdit();

    bool shift = false;
    selRow_ = row;
    selCol_ = col;
    if (!shift) { anchorRow_ = row; anchorCol_ = col; }

    render();
}
