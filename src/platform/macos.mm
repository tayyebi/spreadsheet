// =============================================================================
// macos.mm  —  macOS platform implementation of IWindow (Objective-C++)
//
// Uses Cocoa (AppKit) via Objective-C++ to create an NSWindow, handle key
// events, and draw with Core Graphics (Quartz 2D) into an NSView.
//
// Drawing model:
//   drawText / drawRect / fillRect render into the Core Graphics context
//   provided by Cocoa during a drawRect: call.  The context pointer is stored
//   in `ctx` only while a drawRect: is in progress; it is nullptr at all other
//   times (guarded by null checks at the start of each draw method).
//
//   Unlike X11 (which uses an explicit off-screen Pixmap), Cocoa handles
//   double-buffering automatically when NSBackingStoreBuffered is used.
//   updateDisplay() just calls [view setNeedsDisplay:YES] to schedule a
//   drawRect: which will composite the backing store to the screen.
//
// Coordinate system note:
//   Core Graphics uses a bottom-left origin (y increases upward), while our
//   IWindow API uses a top-left origin (y increases downward, like screen pixels).
//   We flip by computing: cg_y = H - y - h  (for fills) and  H - y - 12 (for text).
//
// Entry point: main() at the bottom of this file.
// =============================================================================

// Objective-C++ with ARC (Automatic Reference Counting) — Cocoa objects are
// managed by the runtime; we do not need to call retain/release explicitly.
#import <Cocoa/Cocoa.h>      // NSApplication, NSWindow, NSView, NSEvent, …
#include "platform.h"         // IWindow, Color, KeyEvent
#include "app.h"              // App
#include <functional>         // std::function

class MacWin;  // forward declaration so SpV (an Obj-C class) can reference it

// ---------------------------------------------------------------------------
// SpV  —  the custom NSView that owns drawing and key handling
//
// We subclass NSView so we can override drawRect: (for painting) and
// keyDown: (for keyboard events).  The view holds a pointer back to MacWin
// so it can forward events to the C++ layer.
// ---------------------------------------------------------------------------
@interface SpV : NSView {
    MacWin* mw_;  // pointer to the owning MacWin (not reference-counted)
}
- (instancetype)initWithFrame:(NSRect)f mw:(MacWin*)m;  // designated initialiser
@end

// ---------------------------------------------------------------------------
// MacWin  —  concrete IWindow implementation backed by an NSWindow
// ---------------------------------------------------------------------------
class MacWin : public IWindow {
    NSWindow* nsw = nil;          // the Cocoa window object
    SpV*      v   = nil;          // our custom drawing view
    CGContextRef ctx = nullptr;   // active Core Graphics context (set during drawRect:)
    int W, H;                     // window dimensions in pixels

    // Callbacks registered by the App layer.
    std::function<void(KeyEvent)> kcb;  // keyboard event callback
    std::function<void()>         rcb;  // redraw callback

public:
    // -----------------------------------------------------------------------
    // Constructor: initialise the Cocoa application, create the window and view
    // -----------------------------------------------------------------------
    MacWin(int w, int h) : W(w), H(h) {
        // Initialise the shared NSApplication singleton (required before any
        // Cocoa calls).  NSApplicationActivationPolicyRegular makes our app
        // appear in the Dock and the menu bar.
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect f = NSMakeRect(100, 100, w, h);  // initial window origin and size

        // Create the window.  NSBackingStoreBuffered means Cocoa maintains a
        // composited backing store (automatic double buffering).
        nsw = [[NSWindow alloc]
            initWithContentRect:f
                      styleMask:NSWindowStyleMaskTitled
                               | NSWindowStyleMaskClosable
                               | NSWindowStyleMaskMiniaturizable
                               | NSWindowStyleMaskResizable
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [nsw setTitle:@"Spreadsheet"];

        // Create our custom view and install it as the window's content view.
        v = [[SpV alloc] initWithFrame:f mw:this];
        [v setWantsLayer:YES];  // enable layer-backed rendering for smooth drawing

        [nsw setContentView:v];
        [nsw makeFirstResponder:v];  // route keyboard events to our view
        [nsw center];
        [nsw makeKeyAndOrderFront:nil];  // show and bring to front

        [NSApp activateIgnoringOtherApps:YES];  // give us keyboard focus
    }

    // -----------------------------------------------------------------------
    // Drawing methods — all check `ctx` first because they are only valid
    // while a Core Graphics context is active (inside drawRect:).
    // -----------------------------------------------------------------------

    // Draw a text string at pixel (x, y) with the given colour.
    // CGContextShowTextAtPoint uses legacy QuickDraw font encoding, but is
    // simpler than the CTLine/CTFont path for a minimal implementation.
    void drawText(int x, int y, const std::string& t, Color c) override {
        if (!ctx) return;
        CGContextSaveGState(ctx);
        CGContextSetRGBFillColor(ctx, c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        // Select the Helvetica bitmap font at 12 pt.
        CGContextSelectFont(ctx, "Helvetica", 12, kCGEncodingMacRoman);
        CGContextSetTextDrawingMode(ctx, kCGTextFill);
        // Flip y: CG origin is bottom-left, our API is top-left.
        CGContextShowTextAtPoint(ctx, (CGFloat)x, (CGFloat)(H - y - 12),
                                 t.c_str(), t.size());
#pragma clang diagnostic pop
        CGContextRestoreGState(ctx);
    }

    // Draw a rectangle outline using a 1-pixel stroke.
    // We offset by 0.5 to align with pixel boundaries (anti-aliasing artefact
    // in Quartz: strokes are centred on the coordinate, so a 1-pt stroke on
    // an integer coordinate straddles two pixels).
    void drawRect(int x, int y, int w, int h, Color c) override {
        if (!ctx) return;
        CGContextSaveGState(ctx);
        CGContextSetRGBStrokeColor(ctx, c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f);
        CGContextSetLineWidth(ctx, 1.f);
        CGContextStrokeRect(ctx,
            CGRectMake(x + .5f, H - y - h + .5f, w - 1, h - 1));
        CGContextRestoreGState(ctx);
    }

    // Fill a rectangle with a solid colour.
    void fillRect(int x, int y, int w, int h, Color c) override {
        if (!ctx) return;
        CGContextSaveGState(ctx);
        CGContextSetRGBFillColor(ctx, c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f);
        CGContextFillRect(ctx, CGRectMake(x, H - y - h, w, h));
        CGContextRestoreGState(ctx);
    }

    // Schedule a redraw.  Cocoa will call drawRect: on the view on the next
    // run-loop pass, which will invoke rcb() → App::render().
    void updateDisplay() override { [v setNeedsDisplay:YES]; }

    void handleInput(std::function<void(KeyEvent)> f) override { kcb = std::move(f); }
    void setRCB(std::function<void()> f)                       { rcb = std::move(f); }

    // Start the Cocoa run loop.  Blocks until the application quits.
    void run() override { [NSApp run]; }

    // Called by SpV::drawRect: with the active Core Graphics context.
    void onDraw(CGContextRef c) {
        ctx = c;       // make the context available to draw methods
        if (rcb) rcb();
        ctx = nullptr; // clear so draw methods are no-ops outside drawRect:
    }

    // -----------------------------------------------------------------------
    // onKey()  —  translate an NSEvent into a normalised KeyEvent
    // -----------------------------------------------------------------------
    void onKey(NSEvent* ev) {
        if (!kcb) return;

        KeyEvent     ke{};
        NSUInteger   m = [ev modifierFlags];
        ke.ctrl  = (m & NSEventModifierFlagControl) != 0;
        ke.shift = (m & NSEventModifierFlagShift)   != 0;

        // Get the character ignoring modifiers (so Shift+A gives 'a', not 'A').
        NSString* raw = [ev charactersIgnoringModifiers];
        if (![raw length]) return;

        unichar ch = [raw characterAtIndex:0];
        ke.ch = (ch < 128) ? char(ch) : 0;  // only store 7-bit ASCII chars

        // Normalise Ctrl codes 1–26 back to lowercase letters.
        if (ke.ctrl && ke.ch >= 1 && ke.ch <= 26)
            ke.ch = char(ke.ch + 'a' - 1);

        // Map Cocoa function-key symbols to our portable KEY_* codes.
        switch (ch) {
            case NSUpArrowFunctionKey:    ke.key = KEY_UP;        ke.ch = 0; break;
            case NSDownArrowFunctionKey:  ke.key = KEY_DOWN;      ke.ch = 0; break;
            case NSLeftArrowFunctionKey:  ke.key = KEY_LEFT;      ke.ch = 0; break;
            case NSRightArrowFunctionKey: ke.key = KEY_RIGHT;     ke.ch = 0; break;
            case '\r': case '\n':         ke.key = KEY_ENTER;                break;
            case 0x1B:                    ke.key = KEY_ESC;                  break;
            case NSDeleteCharacter:
            case NSBackspaceCharacter:    ke.key = KEY_BACKSPACE;            break;
            default:                      ke.key = (ch < 128) ? int(ch) : 0; break;
        }

        kcb(ke);
    }
};

// ---------------------------------------------------------------------------
// SpV  —  NSView implementation (Objective-C++, must be after MacWin definition)
// ---------------------------------------------------------------------------
@implementation SpV

- (instancetype)initWithFrame:(NSRect)f mw:(MacWin*)m {
    self = [super initWithFrame:f];
    if (self) mw_ = m;
    return self;
}

// drawRect: is called by Cocoa whenever the view needs repainting.
// We forward the Core Graphics context to MacWin::onDraw().
- (void)drawRect:(NSRect)__unused r {
    if (mw_) mw_->onDraw([[NSGraphicsContext currentContext] CGContext]);
}

// Forward keyboard events to MacWin::onKey().
- (void)keyDown:(NSEvent*)ev {
    if (mw_) mw_->onKey(ev);
}

// Must return YES so that the view can become the first responder
// (required to receive keyboard events).
- (BOOL)acceptsFirstResponder { return YES; }

@end

// ---------------------------------------------------------------------------
// main()  —  application entry point for macOS
// ---------------------------------------------------------------------------
int main() {
    @autoreleasepool {  // ARC autorelease pool: frees Obj-C temporaries on scope exit
        int w = 40 + Spreadsheet::COLS * 100;
        int h = 20 + Spreadsheet::ROWS * 25;

        MacWin win(w, h);
        App    app(win);

        win.setRCB([&] { app.render(); });  // connect drawRect: → App::render()
        app.render();                        // draw the initial grid
        win.run();                           // enter the Cocoa run loop
    }
    return 0;
}
