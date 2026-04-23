#pragma once // guard
#include "platform.h" // IWindow
#include "core.h" // Spreadsheet
class App{ // application
public:
    explicit App(IWindow&w); // ctor
    void render(); // draw grid
    void onKey(KeyEvent e); // handle key
private:
    IWindow&win_; // window ref
    Spreadsheet sheet_; // data model
    int selRow_=0,selCol_=0; // selection
    bool editing_=false; // edit mode
    std::string editBuf_; // edit buffer
    static constexpr int CW=100,CH=25,HW=40,HH=20; // cell/header dims
};
