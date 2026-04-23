#ifndef UNICODE
#define UNICODE        // use wide-character Win32 API
#endif
#ifndef _UNICODE
#define _UNICODE       // match CRT wide-character mode
#endif
#include <windows.h>   // Win32 API (HWND, HDC, MSG, …)
#include "platform.h"  // IWindow, Color, KeyEvent
#include "app.h"       // App class
#include <string>      // std::string
#include <functional>  // std::function
#include <stdexcept>   // std::runtime_error

class Win32Window : public IWindow { // Win32/GDI implementation of IWindow
    HWND   hwnd_{nullptr};    // window handle
    HDC    memDC_{nullptr};   // back-buffer device context
    HBITMAP memBmp_{nullptr}; // back-buffer bitmap
    HBITMAP oldBmp_{nullptr}; // previously selected bitmap (saved for cleanup)
    int    width_{0};         // client area width
    int    height_{0};        // client area height
    std::function<void(KeyEvent)> keyCb_;   // key-event callback
    std::function<void()>         redrawCb_; // paint/expose callback

    static Win32Window* inst_; // singleton pointer for WndProc dispatch

    // Convert our Color to a Win32 COLORREF
    static COLORREF toCR(Color c) {
        return RGB(c.r, c.g, c.b); // pack RGB
    }

public:
    Win32Window(int w, int h) : width_(w), height_(h) { // constructor
        inst_ = this; // register singleton for WndProc
        WNDCLASSEXW wc{};                        // window class descriptor
        wc.cbSize        = sizeof(wc);           // structure size
        wc.lpfnWndProc   = WndProc;              // message handler
        wc.hInstance     = GetModuleHandleW(nullptr); // current module
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW); // arrow cursor
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); // background
        wc.lpszClassName = L"SpreadsheetWnd";    // class name
        RegisterClassExW(&wc);                   // register class

        // Account for non-client area when sizing
        RECT rc{0, 0, w, h};                     // desired client rect
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE); // adjust for borders
        hwnd_ = CreateWindowExW(                  // create window
            0, L"SpreadsheetWnd", L"Spreadsheet", // class and title
            WS_OVERLAPPEDWINDOW,                  // style flags
            CW_USEDEFAULT, CW_USEDEFAULT,         // default position
            rc.right - rc.left, rc.bottom - rc.top, // adjusted size
            nullptr, nullptr,                     // no parent / menu
            GetModuleHandleW(nullptr), nullptr);  // instance and param
        if (!hwnd_) throw std::runtime_error("CreateWindowEx failed"); // check

        ShowWindow(hwnd_, SW_SHOW);  // make visible
        UpdateWindow(hwnd_);         // send initial WM_PAINT

        HDC dc = GetDC(hwnd_);                         // get window DC
        memDC_  = CreateCompatibleDC(dc);              // create back-buffer DC
        memBmp_ = CreateCompatibleBitmap(dc, w, h);    // create back-buffer bitmap
        oldBmp_ = static_cast<HBITMAP>(               // select bitmap into DC
            SelectObject(memDC_, memBmp_));
        ReleaseDC(hwnd_, dc);                          // release window DC
    }

    ~Win32Window() { // destructor – free GDI objects
        if (memDC_) {
            SelectObject(memDC_, oldBmp_);   // deselect our bitmap
            DeleteObject(memBmp_);           // delete bitmap
            DeleteDC(memDC_);                // delete back-buffer DC
        }
        if (hwnd_) DestroyWindow(hwnd_); // destroy window
    }

    void drawText(int x, int y, const std::string& text, Color c) override {
        SetTextColor(memDC_, toCR(c));               // set text color
        SetBkMode(memDC_, TRANSPARENT);              // no background fill
        TextOutA(memDC_, x, y,                       // draw to back buffer
            text.c_str(), static_cast<int>(text.size())); // text data
    }

    void drawRect(int x, int y, int w, int h, Color c) override {
        HPEN pen   = CreatePen(PS_SOLID, 1, toCR(c));     // 1-px solid pen
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC_, pen)); // select
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH)); // no fill
        HBRUSH oldBr  = static_cast<HBRUSH>(SelectObject(memDC_, nullBr)); // select
        Rectangle(memDC_, x, y, x + w, y + h);            // draw outline
        SelectObject(memDC_, oldPen);                      // restore pen
        SelectObject(memDC_, oldBr);                       // restore brush
        DeleteObject(pen);                                 // free pen
    }

    void fillRect(int x, int y, int w, int h, Color c) override {
        HBRUSH br = CreateSolidBrush(toCR(c));   // solid fill brush
        RECT   rc{x, y, x + w, y + h};           // RECT structure
        FillRect(memDC_, &rc, br);                // fill rectangle
        DeleteObject(br);                         // free brush
    }

    void updateDisplay() override {
        HDC dc = GetDC(hwnd_);                         // get window DC
        BitBlt(dc, 0, 0, width_, height_,             // blit back buffer
            memDC_, 0, 0, SRCCOPY);                    // source copy
        ReleaseDC(hwnd_, dc);                          // release DC
    }

    void handleInput(std::function<void(KeyEvent)> cb) override {
        keyCb_ = std::move(cb); // store key callback
    }

    void setRedrawCallback(std::function<void()> cb) {
        redrawCb_ = std::move(cb); // store paint callback
    }

    void run() override { // Win32 message pump
        MSG msg{};                              // message buffer
        while (GetMessageW(&msg, nullptr, 0, 0)) { // fetch messages
            TranslateMessage(&msg);             // generate WM_CHAR from WM_KEYDOWN
            DispatchMessageW(&msg);             // route to WndProc
        }
    }

    // WndProc is static so we access the instance via inst_
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (!inst_) return DefWindowProcW(hwnd, msg, wp, lp); // safety check
        switch (msg) { // dispatch messages
            case WM_PAINT: {                        // paint request
                PAINTSTRUCT ps;                     // paint struct
                BeginPaint(hwnd, &ps);              // begin paint
                if (inst_->redrawCb_) inst_->redrawCb_(); // re-render
                EndPaint(hwnd, &ps);                // end paint
                return 0;                           // handled
            }
            case WM_KEYDOWN: {                      // non-character key pressed
                if (!inst_->keyCb_) break;          // no handler registered
                KeyEvent ke{};                      // build event
                ke.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0; // check ctrl
                ke.shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0; // check shift
                switch (wp) { // virtual-key to IWindow key code
                    case VK_UP:     ke.key = KEY_UP;        break;
                    case VK_DOWN:   ke.key = KEY_DOWN;      break;
                    case VK_LEFT:   ke.key = KEY_LEFT;      break;
                    case VK_RIGHT:  ke.key = KEY_RIGHT;     break;
                    case VK_RETURN: ke.key = KEY_ENTER;     break;
                    case VK_ESCAPE: ke.key = KEY_ESC;       break;
                    case VK_BACK:   ke.key = KEY_BACKSPACE; break;
                    default: return DefWindowProcW(hwnd, msg, wp, lp); // pass on
                }
                inst_->keyCb_(ke); // dispatch event
                return 0;           // handled
            }
            case WM_CHAR: {                         // printable character
                if (!inst_->keyCb_) break;          // no handler registered
                KeyEvent ke{};                      // build event
                ke.key  = static_cast<int>(wp);     // raw character code
                ke.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0; // check ctrl
                ke.ch   = static_cast<char>(wp);    // character value
                // WM_CHAR delivers control characters (1-26) when Ctrl is held
                if (ke.ctrl && ke.ch >= 1 && ke.ch <= 26)
                    ke.ch = static_cast<char>(ke.ch + 'a' - 1); // normalise
                inst_->keyCb_(ke); // dispatch event
                return 0;          // handled
            }
            case WM_DESTROY:              // window closing
                PostQuitMessage(0);       // signal message loop to exit
                return 0;                 // handled
        }
        return DefWindowProcW(hwnd, msg, wp, lp); // default handling
    }
};

Win32Window* Win32Window::inst_ = nullptr; // static instance pointer

// Windows entry point replaces main()
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int w = 40  + Spreadsheet::COLS * 100; // window client width
    int h = 20  + Spreadsheet::ROWS * 25;  // window client height
    Win32Window win(w, h);                 // create window
    App         app(win);                  // create application
    win.setRedrawCallback([&app]() { app.render(); }); // hook paint
    app.render();                          // draw initial frame
    win.run();                             // run message loop
    return 0;                              // exit
}
