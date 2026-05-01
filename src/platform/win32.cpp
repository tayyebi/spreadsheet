// =============================================================================
// win32.cpp  —  Windows platform implementation of IWindow
//
// Uses the Win32 API (user32.dll + gdi32.dll) to create a window, handle
// keyboard messages, and draw into an off-screen Device Context (double buffer).
//
// Double-buffering strategy:
//   `mdc`  — a memory Device Context associated with `mbmp` (a compatible bitmap).
//   All draw calls write to mdc.  updateDisplay() copies mdc → the window DC
//   using BitBlt(), which flips the buffer atomically to prevent flicker.
//
// Win32 messages handled:
//   WM_PAINT   — trigger a redraw via the rcb callback
//   WM_KEYDOWN — handle non-character keys (arrows, Enter, Esc, Backspace)
//   WM_CHAR    — handle printable characters and Ctrl combos
//   WM_DESTROY — post WM_QUIT to exit the message loop
//
// Entry point: WinMain() at the bottom of this file.
// =============================================================================

#ifndef UNICODE
#define UNICODE    // Use UTF-16 wide-character Win32 APIs (WCHAR instead of char)
#endif
#ifndef _UNICODE
#define _UNICODE   // Also enable wide-character CRT functions (_wfopen etc.)
#endif

#include <windows.h>  // Win32 API — HWND, HDC, CreateWindowExW, BitBlt, …
#include "platform.h" // IWindow, Color, KeyEvent
#include "app.h"       // App
#include <stdexcept>  // std::runtime_error

// ---------------------------------------------------------------------------
// W32Win  —  concrete IWindow implementation backed by a Win32 window
// ---------------------------------------------------------------------------
class W32Win : public IWindow {
    HWND    hwnd = nullptr;   // handle to our top-level window
    HDC     mdc  = nullptr;   // memory (off-screen) Device Context
    HBITMAP mbmp = nullptr;   // the bitmap associated with mdc (the back-buffer)
    HBITMAP obmp = nullptr;   // the original bitmap replaced by mbmp (saved for cleanup)
    int W, H;                 // window client area dimensions in pixels

    // Callbacks registered by the App layer.
    std::function<void(KeyEvent)>   kcb;  // keyboard event callback
    std::function<void(MouseEvent)> mcb;  // mouse event callback
    std::function<void()>           rcb;  // WM_PAINT / redraw callback

    // Singleton pointer: Win32 window procedures (WP) are global C callbacks
    // with no user-data pointer, so we keep one static instance pointer so
    // WP can reach the W32Win object.
    static W32Win* inst;

    // -----------------------------------------------------------------------
    // CR()  —  convert our Color struct to a Win32 COLORREF (0x00BBGGRR)
    // -----------------------------------------------------------------------
    static COLORREF CR(Color c) { return RGB(c.r, c.g, c.b); }

public:
    // -----------------------------------------------------------------------
    // Constructor: register the window class, create the window, set up the
    // double-buffer memory DC.
    // -----------------------------------------------------------------------
    W32Win(int w, int h) : W(w), H(h) {
        inst = this;  // install singleton so WP() can call our methods

        // Register the window class.  Every Win32 window needs a named class
        // that describes its icon, cursor, background, and window procedure.
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WP;                          // our message handler
        wc.hInstance     = GetModuleHandleW(nullptr);    // this .exe
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = HBRUSH(COLOR_WINDOW + 1);    // system default window colour
        wc.lpszClassName = L"SW";                        // class name (must match CreateWindowExW)
        RegisterClassExW(&wc);

        // AdjustWindowRect inflates the requested client-area size to account
        // for the title bar, borders, etc., so the usable interior is exactly w×h.
        RECT r{0, 0, w, h};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd = CreateWindowExW(0, L"SW", L"Spreadsheet",
                               WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               r.right - r.left, r.bottom - r.top,
                               nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!hwnd) throw std::runtime_error("CreateWindowEx failed");

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);  // send the first WM_PAINT immediately

        // Create the off-screen (memory) DC that mirrors the window DC.
        HDC dc = GetDC(hwnd);
        mdc  = CreateCompatibleDC(dc);                  // memory DC
        mbmp = CreateCompatibleBitmap(dc, w, h);         // backing bitmap
        obmp = HBITMAP(SelectObject(mdc, mbmp));         // activate bitmap; save old
        ReleaseDC(hwnd, dc);
    }

    // -----------------------------------------------------------------------
    // Destructor: deselect and delete GDI objects, destroy the window.
    // -----------------------------------------------------------------------
    ~W32Win() {
        if (mdc) {
            SelectObject(mdc, obmp);  // restore original bitmap before deleting
            DeleteObject(mbmp);
            DeleteDC(mdc);
        }
        if (hwnd) DestroyWindow(hwnd);
    }

    // Draw text using the current DC font (default system font).
    // TRANSPARENT background mode prevents a white box behind each string.
    void drawText(int x, int y, const std::string& t, Color c) override {
        SetTextColor(mdc, CR(c));
        SetBkMode(mdc, TRANSPARENT);
        TextOutA(mdc, x, y, t.c_str(), (int)t.size());
    }

    // Draw a rectangle outline using a 1-pixel solid pen.
    // We select the pen into the DC, draw, then restore the original pen and
    // set a NULL brush so the interior is not filled.
    void drawRect(int x, int y, int w, int h, Color c) override {
        HPEN  p  = CreatePen(PS_SOLID, 1, CR(c));
        HPEN  op = HPEN(SelectObject(mdc, p));
        HBRUSH ob = HBRUSH(SelectObject(mdc, GetStockObject(NULL_BRUSH)));
        Rectangle(mdc, x, y, x + w, y + h);
        SelectObject(mdc, op);
        SelectObject(mdc, ob);
        DeleteObject(p);
    }

    // Flood-fill a rectangle with a solid colour using a temporary brush.
    void fillRect(int x, int y, int w, int h, Color c) override {
        HBRUSH b = CreateSolidBrush(CR(c));
        RECT   r{x, y, x + w, y + h};
        FillRect(mdc, &r, b);
        DeleteObject(b);
    }

    // Blit (copy) the off-screen back-buffer to the window's Device Context.
    // SRCCOPY copies pixels verbatim; no stretching or colour transformation.
    void updateDisplay() override {
        HDC dc = GetDC(hwnd);
        BitBlt(dc, 0, 0, W, H, mdc, 0, 0, SRCCOPY);
        ReleaseDC(hwnd, dc);
    }

    void handleInput(std::function<void(KeyEvent)> f) override { kcb = std::move(f); }
    void handleMouse(std::function<void(MouseEvent)> f) override { mcb = std::move(f); }
    void setRCB(std::function<void()> f)                       { rcb = std::move(f); }

    // Standard Win32 message loop: GetMessageW blocks until a message arrives,
    // TranslateMessage generates WM_CHAR from WM_KEYDOWN for printable keys,
    // DispatchMessageW routes the message to WP().
    void run() override {
        MSG m{};
        while (GetMessageW(&m, nullptr, 0, 0)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    // -----------------------------------------------------------------------
    // WP()  —  Win32 Window Procedure (message handler)
    //
    // A global callback required by the Win32 API.  We route every message
    // through the singleton `inst` pointer so it feels like a member function.
    // -----------------------------------------------------------------------
    static LRESULT CALLBACK WP(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
        if (!inst) return DefWindowProcW(hw, msg, wp, lp);

        switch (msg) {
        case WM_PAINT: {
            // Windows sends WM_PAINT when the window needs repainting.
            // We call rcb() (which calls App::render()) inside Begin/EndPaint.
            PAINTSTRUCT ps;
            BeginPaint(hw, &ps);
            if (inst->rcb) inst->rcb();
            EndPaint(hw, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            // WM_KEYDOWN fires for non-character keys (arrows, Escape, etc.)
            // and for character keys before WM_CHAR is generated.
            // We only handle the special keys here; printable chars come via WM_CHAR.
            if (!inst->kcb) break;
            KeyEvent ke{};
            ke.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            ke.shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
            ke.alt   = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            switch (wp) {
                case VK_UP:     ke.key = KEY_UP;        break;
                case VK_DOWN:   ke.key = KEY_DOWN;      break;
                case VK_LEFT:   ke.key = KEY_LEFT;      break;
                case VK_RIGHT:  ke.key = KEY_RIGHT;     break;
                case VK_RETURN: ke.key = KEY_ENTER;     break;
                case VK_ESCAPE: ke.key = KEY_ESC;       break;
                case VK_BACK:   ke.key = KEY_BACKSPACE; break;
                case VK_TAB:    ke.key = KEY_TAB;  ke.ch = '\t'; break;
                case VK_DELETE: ke.key = KEY_DELETE;    break;
                case VK_F2:     ke.key = KEY_F2;        break;
                case VK_F5:     ke.key = KEY_F5;        break;
                case VK_HOME:   ke.key = KEY_HOME;      break;
                case VK_END:    ke.key = KEY_END;       break;
                case VK_PRIOR:  ke.key = KEY_PGUP;      break;
                case VK_NEXT:   ke.key = KEY_PGDN;      break;
                // All other keys are handled by WM_CHAR instead.
                default: return DefWindowProcW(hw, msg, wp, lp);
            }
            inst->kcb(ke);
            return 0;
        }
        case WM_CHAR: {
            // WM_CHAR carries the translated character code (including Ctrl codes).
            if (!inst->kcb) break;
            KeyEvent ke{};
            ke.key  = int(wp);
            ke.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            ke.alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            ke.ch   = char(wp);
            // Normalise Ctrl codes 1–26 back to lowercase letters (Ctrl+S = 19 → 's').
            if (ke.ctrl && ke.ch >= 1 && ke.ch <= 26)
                ke.ch = char(ke.ch + 'a' - 1);
            inst->kcb(ke);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            // Left mouse button clicked: forward position to the App layer.
            if (inst->mcb) {
                MouseEvent me{};
                me.x      = (int)(short)LOWORD(lp);
                me.y      = (int)(short)HIWORD(lp);
                me.button = 1;
                me.pressed = true;
                inst->mcb(me);
            }
            return 0;
        }
        case WM_DESTROY:
            // User closed the window: post WM_QUIT to exit GetMessageW loop.
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hw, msg, wp, lp);
    }
};

// Static singleton initialised to null; set in the constructor.
W32Win* W32Win::inst = nullptr;

// ---------------------------------------------------------------------------
// WinMain()  —  application entry point for Windows
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int w = App::HW + Spreadsheet::COLS * App::CW;
    int h = App::TB + App::FB + App::HH + Spreadsheet::ROWS * App::CH;

    W32Win win(w, h);
    App    app(win);

    win.setRCB([&] { app.render(); });               // connect WM_PAINT → App::render()
    win.handleMouse([&](MouseEvent e) { app.onMouse(e); });
    app.render();                                     // draw the initial grid
    win.run();                                        // enter the message loop
    return 0;
}
