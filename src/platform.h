#pragma once             // include guard
#include <string>        // std::string
#include <functional>    // std::function
#include <cstdint>       // uint8_t

// Simple RGB color value
struct Color {
    uint8_t r{0}; // red channel
    uint8_t g{0}; // green channel
    uint8_t b{0}; // blue channel
};

// Keyboard event passed to the application
struct KeyEvent {
    int  key{0};   // virtual key code (use KEY_* constants or ASCII)
    char ch{0};    // printable character (0 if non-printable)
    bool ctrl{false};  // Ctrl modifier held
    bool shift{false}; // Shift modifier held
};

// Abstract window interface – each platform provides a concrete subclass
class IWindow {
public:
    virtual ~IWindow() = default; // virtual destructor

    // Draw a text string at pixel position (x,y) in the given color
    virtual void drawText(int x, int y, const std::string& text, Color c) = 0;

    // Draw the outline of a rectangle
    virtual void drawRect(int x, int y, int w, int h, Color c) = 0;

    // Fill a solid rectangle
    virtual void fillRect(int x, int y, int w, int h, Color c) = 0;

    // Flush the back buffer to the visible screen (double-buffer blit)
    virtual void updateDisplay() = 0;

    // Register the callback that the platform calls for every key event
    virtual void handleInput(std::function<void(KeyEvent)> cb) = 0;

    // Enter the platform event loop (blocks until the window is closed)
    virtual void run() = 0;

    // Well-known key codes (above ASCII range to avoid collisions)
    static constexpr int KEY_UP        = 0x100; // up arrow
    static constexpr int KEY_DOWN      = 0x101; // down arrow
    static constexpr int KEY_LEFT      = 0x102; // left arrow
    static constexpr int KEY_RIGHT     = 0x103; // right arrow
    static constexpr int KEY_ENTER     = '\r';  // Enter / Return
    static constexpr int KEY_ESC       = 0x1B;  // Escape
    static constexpr int KEY_BACKSPACE = 0x08;  // Backspace
};
