// =============================================================================
// mouse.cpp  —  App::onMouse(): handle one mouse button press event
// =============================================================================

#include "mouse.h"  // paired header (includes app.h)

void App::onMouse(MouseEvent e) {
    // --- Toolbar click ---
    if (e.pressed && e.y < TB) {
        for (int i = 0; i < (int)toolBtns_.size(); ++i) {
            const auto& btn = toolBtns_[i];
            if (btn.label == "|") continue;
            if (e.x >= btn.x && e.x < btn.x + btn.w &&
                e.y >= btn.y && e.y < btn.y + btn.h) {
                if (editing_) commitEdit();
                toolbarAction(i);
                render();
                return;
            }
        }
        return;
    }

    // --- Wheel scroll (button 4 = up, 5 = down) ---
    if (e.button == 4 || e.button == 5) {
        int delta = (e.button == 4) ? -3 : 3;
        viewRow_ += delta;
        if (viewRow_ < 0) viewRow_ = 0;
        render();
        return;
    }

    if (!e.pressed) return;  // ignore non-scroll release events

    // --- Formula bar click → enter edit mode for selected cell ---
    if (e.y >= TB && e.y < TB + FB) {
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
    if (e.y < GY + HH) {
        render();
        return;
    }

    // Convert screen coordinates to grid cell coordinates (accounting for viewport).
    int row = viewRow_ + (e.y - GY - HH) / CH;
    int col = viewCol_ + (e.x - HW)       / CW;

    if (row < 0 || row >= rows_) { render(); return; }
    if (col < 0 || col >= cols_) { render(); return; }

    if (editing_) commitEdit();

    if (e.shift) {
        // Shift+click: extend the selection keeping the existing anchor.
        selRow_ = clampRow(row);
        selCol_ = clampCol(col);
    } else {
        // Plain click: move cursor and reset anchor.
        selRow_    = clampRow(row);
        selCol_    = clampCol(col);
        anchorRow_ = selRow_;
        anchorCol_ = selCol_;
    }

    render();
}
