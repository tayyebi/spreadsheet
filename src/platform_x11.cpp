#include "platform.h" // IWindow, Color, KeyEvent
#include "app.h"      // App class
#include <X11/Xlib.h>   // X11 core API
#include <X11/Xutil.h>  // XLookupString
#include <X11/keysym.h> // XK_* key symbol constants
#include <stdexcept>    // std::runtime_error
#include <cstring>      // memset
#include <functional>   // std::function

class X11Window : public IWindow { // X11/Xlib implementation of IWindow
    Display*  dpy_{nullptr};  // connection to X server
    Window    win_{0};        // X11 window ID
    Pixmap    buf_{0};        // off-screen double-buffer pixmap
    GC        gc_{nullptr};   // reusable graphics context
    int       width_{0};      // window width in pixels
    int       height_{0};     // window height in pixels
    std::function<void(KeyEvent)> keyCb_; // registered key-event callback
    std::function<void()>         redrawCb_; // called on Expose events

    // Allocate an X11 color and set it as the GC foreground
    void setFg(Color c) {
        XColor xc{};                                  // initialize color struct
        xc.red   = static_cast<unsigned short>(c.r * 257u); // 8-bit -> 16-bit
        xc.green = static_cast<unsigned short>(c.g * 257u); // scale green
        xc.blue  = static_cast<unsigned short>(c.b * 257u); // scale blue
        xc.flags = DoRed | DoGreen | DoBlue;          // use all channels
        Colormap cm = DefaultColormap(dpy_, DefaultScreen(dpy_)); // get colormap
        XAllocColor(dpy_, cm, &xc);                   // allocate closest color
        XSetForeground(dpy_, gc_, xc.pixel);           // apply to GC
    }

public:
    X11Window(int w, int h) : width_(w), height_(h) { // constructor
        dpy_ = XOpenDisplay(nullptr);                 // open default display
        if (!dpy_) throw std::runtime_error("XOpenDisplay failed"); // check
        int scr = DefaultScreen(dpy_);                // default screen index
        win_ = XCreateSimpleWindow(                   // create top-level window
            dpy_, RootWindow(dpy_, scr),              // parent = root
            0, 0, static_cast<unsigned>(w), static_cast<unsigned>(h), // geometry
            1,                                        // border width
            BlackPixel(dpy_, scr),                    // border color
            WhitePixel(dpy_, scr));                   // background color
        XSelectInput(dpy_, win_,                      // subscribe to events
            ExposureMask | KeyPressMask);              // expose + key input
        XStoreName(dpy_, win_, "Spreadsheet");         // window title
        XMapWindow(dpy_, win_);                       // make visible
        XFlush(dpy_);                                 // flush to server
        gc_  = XCreateGC(dpy_, win_, 0, nullptr);     // create graphics context
        buf_ = XCreatePixmap(                         // create back-buffer pixmap
            dpy_, win_,
            static_cast<unsigned>(w), static_cast<unsigned>(h),
            static_cast<unsigned>(DefaultDepth(dpy_, scr))); // same depth as window
    }

    ~X11Window() { // destructor – release all X resources
        if (buf_) XFreePixmap(dpy_, buf_); // free back-buffer
        if (gc_)  XFreeGC(dpy_, gc_);      // free GC
        if (win_) XDestroyWindow(dpy_, win_); // destroy window
        if (dpy_) XCloseDisplay(dpy_);      // close connection
    }

    void drawText(int x, int y, const std::string& text, Color c) override {
        setFg(c);                                              // set text color
        XDrawString(dpy_, buf_, gc_,                          // draw to back buffer
            x, y + 12,                                        // baseline offset +12px
            text.c_str(), static_cast<int>(text.size()));     // text data
    }

    void drawRect(int x, int y, int w, int h, Color c) override {
        setFg(c); // set outline color
        XDrawRectangle(dpy_, buf_, gc_,              // draw hollow rectangle
            x, y,                                    // top-left corner
            static_cast<unsigned>(w - 1),            // width (exclusive)
            static_cast<unsigned>(h - 1));            // height (exclusive)
    }

    void fillRect(int x, int y, int w, int h, Color c) override {
        setFg(c); // set fill color
        XFillRectangle(dpy_, buf_, gc_,              // fill solid rectangle
            x, y,
            static_cast<unsigned>(w),
            static_cast<unsigned>(h));
    }

    void updateDisplay() override {
        XCopyArea(dpy_, buf_, win_, gc_,             // blit back-buffer to window
            0, 0, static_cast<unsigned>(width_),
            static_cast<unsigned>(height_), 0, 0);   // source and dest origins
        XFlush(dpy_);                                // flush to X server
    }

    void handleInput(std::function<void(KeyEvent)> cb) override {
        keyCb_ = std::move(cb); // store key callback
    }

    // Not part of IWindow; called by main() to register the redraw callback
    void setRedrawCallback(std::function<void()> cb) {
        redrawCb_ = std::move(cb); // store redraw callback
    }

    void run() override { // X11 event loop
        XEvent ev;              // event storage
        bool   running = true;  // loop flag
        while (running) {       // process events until window closes
            XNextEvent(dpy_, &ev); // block until next event
            if (ev.type == Expose && ev.xexpose.count == 0) { // final expose
                if (redrawCb_) redrawCb_(); // re-render on expose
            } else if (ev.type == KeyPress) {           // keyboard event
                char   buf[8]  = {};                    // character buffer
                KeySym sym     = 0;                     // key symbol
                XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, nullptr); // decode

                KeyEvent ke{};                          // build event
                ke.ctrl  = (ev.xkey.state & ControlMask) != 0; // ctrl modifier
                ke.shift = (ev.xkey.state & ShiftMask)  != 0; // shift modifier
                ke.ch    = buf[0];                      // decoded character

                // Normalise ctrl characters to lowercase letters
                if (ke.ctrl && ke.ch >= 1 && ke.ch <= 26)
                    ke.ch = static_cast<char>(ke.ch + 'a' - 1); // e.g. ^S -> 's'

                switch (sym) { // map X11 keysym to IWindow key codes
                    case XK_Up:        ke.key = KEY_UP;        ke.ch = 0; break;
                    case XK_Down:      ke.key = KEY_DOWN;      ke.ch = 0; break;
                    case XK_Left:      ke.key = KEY_LEFT;      ke.ch = 0; break;
                    case XK_Right:     ke.key = KEY_RIGHT;     ke.ch = 0; break;
                    case XK_Return:    ke.key = KEY_ENTER;               break;
                    case XK_Escape:    ke.key = KEY_ESC;                 break;
                    case XK_BackSpace: ke.key = KEY_BACKSPACE;           break;
                    default:           ke.key = static_cast<int>(buf[0]); break;
                }
                if (keyCb_) keyCb_(ke); // dispatch to application
            }
        }
    }
};

int main() { // program entry point
    int w = 40  + Spreadsheet::COLS * 100; // total window width
    int h = 20  + Spreadsheet::ROWS * 25;  // total window height
    X11Window win(w, h);                   // create X11 window
    App       app(win);                    // create application
    win.setRedrawCallback([&app]() { app.render(); }); // hook render
    app.render();                          // draw initial frame
    win.run();                             // start event loop
    return 0;                              // exit cleanly
}
