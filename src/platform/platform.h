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
// ---------------------------------------------------------------------------
struct KeyEvent {
    int  key   = 0;
    char ch    = 0;
    bool ctrl  = false;
    bool shift = false;
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

    // Start the platform event loop.  Blocks until the window is closed.
    virtual void run() = 0;

    // -----------------------------------------------------------------------
    // Virtual key code constants
    //
    // Arrow keys are assigned values above the 8-bit ASCII range (0x100+)
    // so they can coexist with regular character codes in the same int field.
    // Control characters (Enter, Escape, Backspace) use their ASCII values.
    // -----------------------------------------------------------------------
    static constexpr int KEY_UP        = 0x100;   // ↑ arrow
    static constexpr int KEY_DOWN      = 0x101;   // ↓ arrow
    static constexpr int KEY_LEFT      = 0x102;   // ← arrow
    static constexpr int KEY_RIGHT     = 0x103;   // → arrow
    static constexpr int KEY_ENTER     = '\r';    // Carriage Return (13)
    static constexpr int KEY_ESC       = 0x1B;    // Escape (27)
    static constexpr int KEY_BACKSPACE = 0x08;    // Backspace (8)
};
