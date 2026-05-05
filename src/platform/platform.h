// =============================================================================
// platform.h  —  Platform abstraction layer (the "View" interface)
//
// All platform-specific code (X11, Win32, Cocoa) implements the IWindow
// abstract class defined here.  App only calls IWindow methods, so adding
// a new platform means writing one new .cpp file without touching App.
//
// This is the "Bridge" design pattern: the abstraction (IWindow) and the
// implementations (X11Win, W32Win, MacWin) vary independently.
// =============================================================================
#pragma once

#include <string>      // std::string — used for drawText
#include <functional>  // std::function — used for the input callback
#include <cstdint>     // uint8_t — for Color channel values

// ---------------------------------------------------------------------------
// Color  —  a 24-bit RGB colour value
//
// Each channel is an 8-bit unsigned integer in the range 0–255.
// Examples: {255,255,255} = white, {0,0,0} = black, {0,0,200} = blue.
// ---------------------------------------------------------------------------
struct Color { uint8_t r, g, b; };

// ---------------------------------------------------------------------------
// KeyEvent  —  a normalised keyboard event
//
// The platform translates its native key representation into this struct
// before calling the registered input callback.
//
//   key   – virtual key code for non-character keys (arrows, Enter, Esc, …)
//             uses the KEY_* constants defined in IWindow
//   ch    – the ASCII character for printable keys (0 for special keys)
//   ctrl  – true if the Control modifier was held
//   shift – true if the Shift modifier was held
//   alt   – true if the Alt/Option modifier was held
// ---------------------------------------------------------------------------
struct KeyEvent {
    int  key   = 0;
    char ch    = 0;
    bool ctrl  = false;
    bool shift = false;
    bool alt   = false;
};

// ---------------------------------------------------------------------------
// MouseEvent  —  a normalised mouse event
//
//   x, y    – cursor position in terminal character cells (top-left origin)
//   button  – 1=left, 2=middle, 3=right, 4=wheel-up, 5=wheel-down
//   pressed – true on button press, false on button release
//   shift   – true if the Shift modifier was held
// ---------------------------------------------------------------------------
struct MouseEvent {
    int  x       = 0;
    int  y       = 0;
    int  button  = 0;
    bool pressed = true;
    bool shift   = false;
};

// ---------------------------------------------------------------------------
// IWindow  —  the abstract window interface
//
// Responsibilities:
//   • Provide 2D drawing primitives (text, rectangles, fills)
//   • Accept and fire input callbacks for keyboard events
//   • Run the platform event loop
//
// All drawing is double-buffered: drawText / drawRect / fillRect render into
// an off-screen buffer, and updateDisplay() blits that buffer to the screen.
// This prevents flickering during multi-step redraws.
// ---------------------------------------------------------------------------
class IWindow {
public:
    virtual ~IWindow() = default;

    // Draw a string at pixel position (x, y) using the given colour.
    // The y coordinate is measured from the top of the window.
    virtual void drawText(int x, int y, const std::string& t, Color c) = 0;

    // Draw the outline of a rectangle (width w, height h) at (x, y).
    // Does not fill the interior.
    virtual void drawRect(int x, int y, int w, int h, Color c) = 0;

    // Fill a rectangle (width w, height h) at (x, y) with the given colour.
    virtual void fillRect(int x, int y, int w, int h, Color c) = 0;

    // Copy the off-screen back-buffer to the visible screen (blit / flip).
    // Call this once at the end of a complete render() pass.
    virtual void updateDisplay() = 0;

    // Register the callback that will be called for every keyboard event.
    // Only one callback is active at a time (the last one registered wins).
    virtual void handleInput(std::function<void(KeyEvent)> cb) = 0;

    // Register the callback that will be called for every mouse button event.
    // Default is a no-op so platforms that do not yet implement mouse events
    // still compile without changes.
    virtual void handleMouse(std::function<void(MouseEvent)> cb) { (void)cb; }

    // Return the current terminal / window dimensions in character cells.
    // The default returns a common safe fallback (80×24).
    virtual void getTermSize(int& rows, int& cols) const { rows = 24; cols = 80; }

    // Start the platform event loop.  Blocks until the window is closed.
    virtual void run() = 0;

    // -----------------------------------------------------------------------
    // Virtual key code constants
    //
    // Arrow keys and function keys are assigned values above the 8-bit ASCII
    // range (0x100+) so they can coexist with regular character codes.
    // Control characters (Enter, Escape, Backspace, Tab) use their ASCII values.
    // -----------------------------------------------------------------------
    static constexpr int KEY_UP        = 0x100;   // ↑ arrow
    static constexpr int KEY_DOWN      = 0x101;   // ↓ arrow
    static constexpr int KEY_LEFT      = 0x102;   // ← arrow
    static constexpr int KEY_RIGHT     = 0x103;   // → arrow
    static constexpr int KEY_F2        = 0x104;   // F2  — enter edit mode
    static constexpr int KEY_HOME      = 0x105;   // Home
    static constexpr int KEY_END       = 0x106;   // End
    static constexpr int KEY_PGUP      = 0x107;   // Page Up
    static constexpr int KEY_PGDN      = 0x108;   // Page Down
    static constexpr int KEY_F5        = 0x109;   // F5  — go-to cell
    static constexpr int KEY_ENTER     = '\r';    // Carriage Return (13)
    static constexpr int KEY_ESC       = 0x1B;    // Escape (27)
    static constexpr int KEY_BACKSPACE = 0x08;    // Backspace (8)
    static constexpr int KEY_TAB       = '\t';    // Tab (9)
    static constexpr int KEY_DELETE    = 0x7F;    // Forward-Delete / Del key
};
