// platform_macos.mm – Cocoa + Core Graphics back-end
// Compiled as Objective-C++ (*.mm) with ARC enabled

#import <Cocoa/Cocoa.h>          // NSApplication, NSWindow, NSView, NSEvent
#import <CoreGraphics/CoreGraphics.h> // CGBitmapContext, CGImageRef, …
#include "platform.h"            // IWindow, Color, KeyEvent
#include "app.h"                 // App
#include <functional>            // std::function
#include <stdexcept>             // std::runtime_error

// Forward-declare C++ wrapper so the Objective-C view can reference it
class MacWindow;

// ── Custom NSView ─────────────────────────────────────────────────────────────

@interface SpreadsheetView : NSView {
    MacWindow* macWin_; // pointer to the C++ MacWindow
}
- (instancetype)initWithFrame:(NSRect)f macWin:(MacWindow*)mw; // designated init
@end

// ── MacWindow (IWindow implementation) ───────────────────────────────────────

class MacWindow : public IWindow {
public:
    NSWindow*         nswin_{nil};       // Cocoa window
    SpreadsheetView*  view_{nil};        // custom content view
    CGContextRef      cgCtx_{nullptr};   // off-screen CG bitmap (back buffer)
    int               width_{0};         // pixel width
    int               height_{0};        // pixel height
    std::function<void(KeyEvent)> keyCb_;    // key-event handler
    std::function<void()>         redrawCb_; // called before blitting buffer

    MacWindow(int w, int h) : width_(w), height_(h) {
        NSApp = [NSApplication sharedApplication]; // obtain shared application
        [NSApp setActivationPolicy:             // configure as regular app
            NSApplicationActivationPolicyRegular];
        NSRect frame = NSMakeRect(100, 100,     // initial window position
            static_cast<CGFloat>(w),            // width
            static_cast<CGFloat>(h));           // height
        nswin_ = [[NSWindow alloc]              // allocate window
            initWithContentRect:frame
            styleMask:(NSWindowStyleMaskTitled  // title bar
                | NSWindowStyleMaskClosable     // close button
                | NSWindowStyleMaskMiniaturizable // minimise button
                | NSWindowStyleMaskResizable)   // resize handles
            backing:NSBackingStoreBuffered      // buffered backing store
            defer:NO];                          // create immediately
        [nswin_ setTitle:@"Spreadsheet"];       // window title
        view_ = [[SpreadsheetView alloc]        // create custom content view
            initWithFrame:frame macWin:this];
        [view_ setWantsLayer:YES];              // layer-backed (double-buffered)
        [nswin_ setContentView:view_];          // install as content view
        [nswin_ makeFirstResponder:view_];      // give view keyboard focus
        [nswin_ center];                        // centre on screen
        [nswin_ makeKeyAndOrderFront:nil];      // show and focus window
        [NSApp activateIgnoringOtherApps:YES];  // bring to front
        createBackBuffer(w, h);                 // allocate CG back buffer
    }

    ~MacWindow() {
        if (cgCtx_) CGContextRelease(cgCtx_); // release CG context
    }

    // Allocate (or reallocate) the off-screen CG bitmap context
    void createBackBuffer(int w, int h) {
        if (cgCtx_) CGContextRelease(cgCtx_); // release old context
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB(); // RGB color space
        cgCtx_ = CGBitmapContextCreate(       // allocate bitmap memory
            nullptr,                          // let CG manage memory
            static_cast<size_t>(w),           // width in pixels
            static_cast<size_t>(h),           // height in pixels
            8,                                // bits per channel
            0,                                // bytes per row (auto)
            cs,                               // color space
            kCGImageAlphaPremultipliedLast);  // RGBA premultiplied
        CGColorSpaceRelease(cs);              // balance retain
    }

    // ── IWindow virtual methods ───────────────────────────────────────────────

    void drawText(int x, int y, const std::string& text, Color c) override {
        if (!cgCtx_) return;                    // guard
        // Build a Core Text attributed string for correct glyph rendering
        CFStringRef cfStr = CFStringCreateWithCString( // create CF string
            nullptr, text.c_str(), kCFStringEncodingUTF8); // UTF-8 input
        CFMutableAttributedStringRef as =           // mutable attributed string
            CFAttributedStringCreateMutable(nullptr, 0);
        CFAttributedStringReplaceString(as,         // set string content
            CFRangeMake(0, 0), cfStr);
        CTFontRef font = CTFontCreateWithName(       // create 12pt Helvetica
            CFSTR("Helvetica"), 12.0, nullptr);
        CFAttributedStringSetAttribute(as,           // apply font
            CFRangeMake(0, CFStringGetLength(cfStr)),
            kCTFontAttributeName, font);
        CGFloat r = c.r / 255.0f;                    // red component
        CGFloat g = c.g / 255.0f;                    // green component
        CGFloat b = c.b / 255.0f;                    // blue component
        CGColorRef col = CGColorCreateGenericRGB(r, g, b, 1.0); // text color
        CFAttributedStringSetAttribute(as,           // apply color
            CFRangeMake(0, CFStringGetLength(cfStr)),
            kCTForegroundColorAttributeName, col);
        CTLineRef line = CTLineCreateWithAttributedString(as); // create line
        // CG origin is bottom-left; flip y so caller uses top-left origin
        CGContextSetTextPosition(cgCtx_,
            static_cast<CGFloat>(x),
            static_cast<CGFloat>(height_ - y - 13)); // baseline offset
        CTLineDraw(line, cgCtx_);                    // render glyphs
        CFRelease(line);                             // release line
        CGColorRelease(col);                         // release color
        CFRelease(font);                             // release font
        CFRelease(as);                               // release attributed string
        CFRelease(cfStr);                            // release CF string
    }

    void drawRect(int x, int y, int w, int h, Color c) override {
        if (!cgCtx_) return;                         // guard
        CGContextSaveGState(cgCtx_);                 // save state
        CGContextSetRGBStrokeColor(cgCtx_,           // set stroke color
            c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
        CGContextSetLineWidth(cgCtx_, 1.0f);         // 1-pixel line
        CGRect r = CGRectMake(                       // flipped rect (CG Y up)
            static_cast<CGFloat>(x) + 0.5f,
            static_cast<CGFloat>(height_ - y - h) + 0.5f,
            static_cast<CGFloat>(w - 1),
            static_cast<CGFloat>(h - 1));
        CGContextStrokeRect(cgCtx_, r);              // stroke outline
        CGContextRestoreGState(cgCtx_);              // restore state
    }

    void fillRect(int x, int y, int w, int h, Color c) override {
        if (!cgCtx_) return;                         // guard
        CGContextSaveGState(cgCtx_);                 // save state
        CGContextSetRGBFillColor(cgCtx_,             // set fill color
            c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
        CGRect r = CGRectMake(                       // flipped rect
            static_cast<CGFloat>(x),
            static_cast<CGFloat>(height_ - y - h),
            static_cast<CGFloat>(w),
            static_cast<CGFloat>(h));
        CGContextFillRect(cgCtx_, r);                // fill rectangle
        CGContextRestoreGState(cgCtx_);              // restore state
    }

    void updateDisplay() override {
        [view_ setNeedsDisplay:YES]; // schedule next drawRect: call
    }

    void handleInput(std::function<void(KeyEvent)> cb) override {
        keyCb_ = std::move(cb); // register key callback
    }

    void run() override {
        [NSApp run]; // enter Cocoa event loop (blocks)
    }

    // ── Called by SpreadsheetView::drawRect: ─────────────────────────────────

    void onDraw() {
        if (redrawCb_) redrawCb_();                  // let app render to buffer
        CGImageRef img = CGBitmapContextCreateImage(cgCtx_); // snapshot buffer
        CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext]; // screen ctx
        CGContextDrawImage(ctx,                      // blit to screen context
            CGRectMake(0, 0,                         // destination origin
                static_cast<CGFloat>(width_),        // destination width
                static_cast<CGFloat>(height_)),      // destination height
            img);                                    // source image
        CGImageRelease(img);                         // release snapshot
    }

    // ── Called by SpreadsheetView::keyDown: ──────────────────────────────────

    void onKey(NSEvent* event) {
        KeyEvent ke{};                              // build key event
        NSUInteger mods = [event modifierFlags];    // modifier flags
        ke.ctrl  = (mods & NSEventModifierFlagControl)  != 0; // Ctrl held
        ke.shift = (mods & NSEventModifierFlagShift)    != 0; // Shift held
        NSString* raw = [event charactersIgnoringModifiers]; // key chars
        if ([raw length] > 0) {                     // has a character
            unichar ch = [raw characterAtIndex:0];  // first Unicode code point
            ke.ch = (ch < 128) ? static_cast<char>(ch) : 0; // ASCII or none
            if (ke.ctrl && ke.ch >= 1 && ke.ch <= 26)        // control char
                ke.ch = static_cast<char>(ke.ch + 'a' - 1);  // normalise
            switch (ch) { // map special keys to IWindow codes
                case NSUpArrowFunctionKey:    ke.key = KEY_UP;        ke.ch=0; break;
                case NSDownArrowFunctionKey:  ke.key = KEY_DOWN;      ke.ch=0; break;
                case NSLeftArrowFunctionKey:  ke.key = KEY_LEFT;      ke.ch=0; break;
                case NSRightArrowFunctionKey: ke.key = KEY_RIGHT;     ke.ch=0; break;
                case '\r': case '\n':         ke.key = KEY_ENTER;             break;
                case 0x1B:                    ke.key = KEY_ESC;               break;
                case NSDeleteCharacter:
                case NSBackspaceCharacter:    ke.key = KEY_BACKSPACE;         break;
                default: ke.key = (ch < 128) ? static_cast<int>(ch) : 0;     break;
            }
        }
        if (keyCb_) keyCb_(ke); // dispatch to application
    }
};

// ── SpreadsheetView implementation ───────────────────────────────────────────

@implementation SpreadsheetView

- (instancetype)initWithFrame:(NSRect)f macWin:(MacWindow*)mw {
    self = [super initWithFrame:f]; // call NSView init
    if (self) macWin_ = mw;         // store C++ pointer
    return self;                    // return initialized view
}

- (void)drawRect:(NSRect)__unused dirtyRect { // render callback from Cocoa
    if (macWin_) macWin_->onDraw(); // delegate to C++
}

- (void)keyDown:(NSEvent*)event { // keyboard event from Cocoa
    if (macWin_) macWin_->onKey(event); // delegate to C++
}

- (BOOL)acceptsFirstResponder { return YES; } // allow keyboard focus

@end

// ── Entry point ──────────────────────────────────────────────────────────────

int main() {
    @autoreleasepool {                                // ARC top-level pool
        int w = 40  + Spreadsheet::COLS * 100;       // window width
        int h = 20  + Spreadsheet::ROWS * 25;        // window height
        MacWindow win(w, h);                         // create Cocoa window
        App       app(win);                          // create application
        win.redrawCb_ = [&app]() { app.render(); };  // hook render callback
        app.render();                                // draw initial frame
        win.run();                                   // start Cocoa loop
    }                                                // drain pool
    return 0;                                        // exit
}
