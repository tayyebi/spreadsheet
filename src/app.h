#pragma once           // include guard
#include "platform.h"  // IWindow, KeyEvent, Color
#include "core.h"      // Spreadsheet

// Application controller: renders the grid and processes keyboard input
class App {
public:
    explicit App(IWindow& win); // bind to an IWindow implementation

    void render();           // draw the full grid to the back buffer
    void onKey(KeyEvent e);  // handle one keyboard event

private:
    IWindow&    win_;         // reference to the platform window
    Spreadsheet sheet_;       // the spreadsheet data model

    int  selRow_{0};          // currently selected cell row
    int  selCol_{0};          // currently selected cell column
    bool editing_{false};     // true while the user is typing into a cell
    std::string editBuf_;     // accumulates keystrokes during editing

    // Fixed cell dimensions in pixels
    static constexpr int CELL_W = 100; // cell width
    static constexpr int CELL_H = 25;  // cell height
    // Space reserved for row/column header labels
    static constexpr int HDR_W = 40;   // row-header width
    static constexpr int HDR_H = 20;   // column-header height
};
