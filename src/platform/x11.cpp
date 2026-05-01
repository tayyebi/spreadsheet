// =============================================================================
// x11.cpp  —  Linux/X11 platform implementation of IWindow
//
// Uses the Xlib C library to create a window, handle keyboard events, and
// draw text and rectangles into an off-screen Pixmap (double buffer).
//
// Double-buffering strategy:
//   All draw calls write into `buf` (an off-screen X Pixmap).
//   updateDisplay() copies `buf` → the visible window with XCopyArea(),
//   then flushes the X command queue.  This eliminates visible tearing.
//
// Entry point: main() at the bottom of this file creates X11Win and App,
// then starts the X event loop.
// =============================================================================

#include "platform.h"  // IWindow, Color, KeyEvent
#include "app.h"        // App
#include <X11/Xlib.h>   // core X11 types and functions
#include <X11/Xutil.h>  // XLookupString — convert XKeyEvent to character
#include <X11/keysym.h> // XK_Up, XK_Down, … — symbolic key constants
#include <stdexcept>    // std::runtime_error — thrown if X can't open

// ---------------------------------------------------------------------------
// X11Win  —  concrete IWindow implementation backed by an X11 window
// ---------------------------------------------------------------------------
class X11Win : public IWindow {
    // X11 resources — all default to null/zero so the destructor can safely
    // check which ones were successfully created before freeing them.
    Display* dpy = nullptr;   // connection to the X server
    Window   win = 0;         // our top-level X window
    Pixmap   buf = 0;         // off-screen drawing buffer (double buffer)
    GC       gc  = nullptr;   // Graphics Context — holds drawing attributes
    int      W, H;            // window dimensions in pixels

    // Callbacks registered by the App layer.
    std::function<void(KeyEvent)>   kcb;  // keyboard event callback
    std::function<void(MouseEvent)> mcb;  // mouse event callback
    std::function<void()>           rcb;  // redraw/expose callback

    // -----------------------------------------------------------------------
    // fg()  —  set the X foreground (drawing) colour
    //
    // X11 does not accept RGB triples directly; we must allocate a colour
    // entry in the server's colourmap first.  XColor.red/green/blue are
    // 16-bit values (0–65535), so we multiply our 8-bit channels by 257
    // (= 65535 / 255, approximately) to scale them up.
    // -----------------------------------------------------------------------
    void fg(Color c) {
        XColor x{};
        x.red   = c.r * 257u;
        x.green = c.g * 257u;
        x.blue  = c.b * 257u;
        x.flags = DoRed | DoGreen | DoBlue;
        XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &x);
        XSetForeground(dpy, gc, x.pixel);
    }

public:
    // -----------------------------------------------------------------------
    // Constructor: open an X display, create the window and off-screen buffer
    // -----------------------------------------------------------------------
    X11Win(int w, int h) : W(w), H(h) {
        // Connect to the X server identified by the DISPLAY environment variable.
        dpy = XOpenDisplay(nullptr);
        if (!dpy) throw std::runtime_error("Cannot open X display");

        int s = DefaultScreen(dpy);

        // Create a simple window with a 1-pixel black border on a white background.
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, s),
                                  0, 0, w, h, 1,
                                  BlackPixel(dpy, s), WhitePixel(dpy, s));

        // Subscribe to Expose (repaint), KeyPress, and mouse button events.
        XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask);
        XStoreName(dpy, win, "Spreadsheet");   // title bar text
        XMapWindow(dpy, win);                  // make the window visible
        XFlush(dpy);                           // send all pending X commands

        // Create the Graphics Context used for all drawing.
        gc = XCreateGC(dpy, win, 0, nullptr);

        // Create the off-screen Pixmap (same depth as the window).
        buf = XCreatePixmap(dpy, win, w, h, DefaultDepth(dpy, s));
    }

    // -----------------------------------------------------------------------
    // Destructor: release all X resources in reverse creation order
    // -----------------------------------------------------------------------
    ~X11Win() {
        if (buf) XFreePixmap(dpy, buf);
        if (gc)  XFreeGC(dpy, gc);
        if (win) XDestroyWindow(dpy, win);
        if (dpy) XCloseDisplay(dpy);
    }

    // Draw text string `t` at (x, y) using XDrawString (bitmap font).
    // y+12 shifts the baseline so the text sits neatly inside a CH=25 cell.
    void drawText(int x, int y, const std::string& t, Color c) override {
        fg(c);
        XDrawString(dpy, buf, gc, x, y + 12, t.c_str(), (int)t.size());
    }

    // Draw the outline of a rectangle.  We subtract 1 from w and h because
    // XDrawRectangle draws to (x+w, y+h) inclusive, which is 1 pixel too large.
    void drawRect(int x, int y, int w, int h, Color c) override {
        fg(c);
        XDrawRectangle(dpy, buf, gc, x, y, (unsigned)(w - 1), (unsigned)(h - 1));
    }

    // Flood-fill a rectangle with the given colour.
    void fillRect(int x, int y, int w, int h, Color c) override {
        fg(c);
        XFillRectangle(dpy, buf, gc, x, y, (unsigned)w, (unsigned)h);
    }

    // Blit the back-buffer to the visible window.
    // XCopyArea copies the entire Pixmap, then XFlush sends the command.
    void updateDisplay() override {
        XCopyArea(dpy, buf, win, gc, 0, 0, (unsigned)W, (unsigned)H, 0, 0);
        XFlush(dpy);
    }

    // Register the keyboard callback (replaces any previous registration).
    void handleInput(std::function<void(KeyEvent)> f) override {
        kcb = std::move(f);
    }

    // Register the mouse button callback.
    void handleMouse(std::function<void(MouseEvent)> f) override {
        mcb = std::move(f);
    }

    // Register the redraw callback (called on Expose events).
    void setRedraw(std::function<void()> f) { rcb = std::move(f); }

    // -----------------------------------------------------------------------
    // run()  —  the X11 event loop
    //
    // Blocks forever, dispatching event types:
    //   Expose      — the window was uncovered and needs repainting
    //   KeyPress    — the user pressed a key
    //   ButtonPress — the user clicked a mouse button
    // -----------------------------------------------------------------------
    void run() override {
        XEvent ev;
        for (;;) {
            XNextEvent(dpy, &ev);

            if (ev.type == Expose && ev.xexpose.count == 0 && rcb) {
                // count == 0 means this is the last pending Expose event;
                // we only repaint on the last one to avoid redundant redraws.
                rcb();
            } else if (ev.type == ButtonPress) {
                // Forward mouse button press to the App layer.
                if (mcb) {
                    MouseEvent me{};
                    me.x       = ev.xbutton.x;
                    me.y       = ev.xbutton.y;
                    me.button  = (int)ev.xbutton.button;  // 1=left,2=mid,3=right
                    me.pressed = true;
                    mcb(me);
                }
            } else if (ev.type == KeyPress) {
                char    buf8[8] = {};
                KeySym  sym     = 0;
                // XLookupString translates the raw X key event into an ASCII
                // character (buf8) and a portable KeySym symbol (sym).
                XLookupString(&ev.xkey, buf8, 7, &sym, nullptr);

                KeyEvent ke{};
                ke.ctrl  = (ev.xkey.state & ControlMask) != 0;
                ke.shift = (ev.xkey.state & ShiftMask)   != 0;
                ke.alt   = (ev.xkey.state & Mod1Mask)    != 0;
                ke.ch    = buf8[0];

                // When Ctrl is held, XLookupString returns control codes 1–26
                // (Ctrl+A = 1, Ctrl+B = 2, …).  Map them back to 'a'–'z' so
                // App can check  `e.ctrl && e.ch == 's'`  uniformly.
                if (ke.ctrl && ke.ch >= 1 && ke.ch <= 26)
                    ke.ch = char(ke.ch + 'a' - 1);

                // Map X KeySyms to our platform-independent KEY_* codes.
                switch (sym) {
                    case XK_Up:        ke.key = KEY_UP;        ke.ch = 0; break;
                    case XK_Down:      ke.key = KEY_DOWN;      ke.ch = 0; break;
                    case XK_Left:      ke.key = KEY_LEFT;      ke.ch = 0; break;
                    case XK_Right:     ke.key = KEY_RIGHT;     ke.ch = 0; break;
                    case XK_Delete:    ke.key = KEY_DELETE;    ke.ch = 0; break;
                    case XK_Tab:       ke.key = KEY_TAB;       ke.ch = '\t'; break;
                    case XK_Home:      ke.key = KEY_HOME;      ke.ch = 0; break;
                    case XK_End:       ke.key = KEY_END;       ke.ch = 0; break;
                    case XK_F2:        ke.key = KEY_F2;        ke.ch = 0; break;
                    case XK_F5:        ke.key = KEY_F5;        ke.ch = 0; break;
                    case XK_Page_Up:   ke.key = KEY_PGUP;      ke.ch = 0; break;
                    case XK_Page_Down: ke.key = KEY_PGDN;      ke.ch = 0; break;
                    case XK_Return:    ke.key = KEY_ENTER;                break;
                    case XK_Escape:    ke.key = KEY_ESC;                  break;
                    case XK_BackSpace: ke.key = KEY_BACKSPACE;            break;
                    default:           ke.key = int(buf8[0]);             break;
                }

                if (kcb) kcb(ke);
            } else if (ev.type == ButtonPress) {
                // Translate X11 button event to a platform-independent MouseEvent.
                // X button numbers: 1=left, 2=middle, 3=right.
                MouseEvent me{};
                me.x      = ev.xbutton.x;
                me.y      = ev.xbutton.y;
                me.button = (int)ev.xbutton.button;
                if (mcb) mcb(me);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// main()  —  application entry point for Linux/X11
// ---------------------------------------------------------------------------
int main() {
    // Compute window size from App layout constants so it stays in sync.
    int w = App::HW + Spreadsheet::COLS * App::CW;
    int h = App::TB + App::FB + App::HH + Spreadsheet::ROWS * App::CH;

    X11Win win(w, h);
    App    app(win);

    // Wire callbacks so X events reach the App layer.
    win.setRedraw([&] { app.render(); });
    win.handleMouse([&](MouseEvent e) { app.onMouse(e); });

    app.render();  // draw the initial (empty) grid before entering the loop
    win.run();     // blocks until the window is closed
    return 0;
}
