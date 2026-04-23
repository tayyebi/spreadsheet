// Objective-C++ with ARC
#import <Cocoa/Cocoa.h> // Cocoa
#include "platform.h" // IWindow
#include "app.h" // App
#include <functional> // function
class MacWin; // fwd
@interface SpV:NSView{MacWin*mw_;} // view
-(instancetype)initWithFrame:(NSRect)f mw:(MacWin*)m; // init
@end
class MacWin:public IWindow{ // Cocoa impl
    NSWindow*nsw=nil;SpV*v=nil;CGContextRef ctx=nullptr;int W,H; // state
    std::function<void(KeyEvent)>kcb;std::function<void()>rcb; // cbs
public:
    MacWin(int w,int h):W(w),H(h){ // ctor
        [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular]; // app
        NSRect f=NSMakeRect(100,100,w,h); // frame
        nsw=[[NSWindow alloc]initWithContentRect:f
            styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO]; // window
        [nsw setTitle:@"Spreadsheet"]; // title
        v=[[SpV alloc]initWithFrame:f mw:this];[v setWantsLayer:YES]; // view
        [nsw setContentView:v];[nsw makeFirstResponder:v];[nsw center];[nsw makeKeyAndOrderFront:nil]; // show
        [NSApp activateIgnoringOtherApps:YES];} // focus
    void drawText(int x,int y,const std::string&t,Color c)override{ // text
        if(!ctx)return; // guard
        CGContextSaveGState(ctx); // save
        CGContextSetRGBFillColor(ctx,c.r/255.f,c.g/255.f,c.b/255.f,1); // color
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGContextSelectFont(ctx,"Helvetica",12,kCGEncodingMacRoman); // font
        CGContextSetTextDrawingMode(ctx,kCGTextFill); // mode
        CGContextShowTextAtPoint(ctx,(CGFloat)x,(CGFloat)(H-y-12),t.c_str(),t.size()); // draw
#pragma clang diagnostic pop
        CGContextRestoreGState(ctx);} // restore
    void drawRect(int x,int y,int w,int h,Color c)override{ // outline
        if(!ctx)return; // guard
        CGContextSaveGState(ctx);CGContextSetRGBStrokeColor(ctx,c.r/255.f,c.g/255.f,c.b/255.f,1);CGContextSetLineWidth(ctx,1); // stroke
        CGContextStrokeRect(ctx,CGRectMake(x+.5f,H-y-h+.5f,w-1,h-1));CGContextRestoreGState(ctx);} // draw
    void fillRect(int x,int y,int w,int h,Color c)override{ // fill
        if(!ctx)return; // guard
        CGContextSaveGState(ctx);CGContextSetRGBFillColor(ctx,c.r/255.f,c.g/255.f,c.b/255.f,1); // color
        CGContextFillRect(ctx,CGRectMake(x,H-y-h,w,h));CGContextRestoreGState(ctx);} // draw
    void updateDisplay()override{[v setNeedsDisplay:YES];} // redraw
    void handleInput(std::function<void(KeyEvent)>f)override{kcb=std::move(f);} // bind
    void setRCB(std::function<void()>f){rcb=std::move(f);} // bind
    void run()override{[NSApp run];} // loop
    void onDraw(CGContextRef c){ctx=c;if(rcb)rcb();ctx=nullptr;} // render
    void onKey(NSEvent*ev){ // key
        if(!kcb)return; // guard
        KeyEvent ke{};NSUInteger m=[ev modifierFlags]; // mods
        ke.ctrl=(m&NSEventModifierFlagControl)!=0;ke.shift=(m&NSEventModifierFlagShift)!=0; // flags
        NSString*raw=[ev charactersIgnoringModifiers];if(![raw length])return; // chars
        unichar ch=[raw characterAtIndex:0];ke.ch=ch<128?char(ch):0; // char
        if(ke.ctrl&&ke.ch>=1&&ke.ch<=26)ke.ch=char(ke.ch+'a'-1); // norm
        switch(ch){ // map
            case NSUpArrowFunctionKey:ke.key=KEY_UP;ke.ch=0;break; // up
            case NSDownArrowFunctionKey:ke.key=KEY_DOWN;ke.ch=0;break; // down
            case NSLeftArrowFunctionKey:ke.key=KEY_LEFT;ke.ch=0;break; // left
            case NSRightArrowFunctionKey:ke.key=KEY_RIGHT;ke.ch=0;break; // right
            case'\r':case'\n':ke.key=KEY_ENTER;break; // enter
            case 0x1B:ke.key=KEY_ESC;break; // esc
            case NSDeleteCharacter:case NSBackspaceCharacter:ke.key=KEY_BACKSPACE;break; // bksp
            default:ke.key=ch<128?int(ch):0;break;} // char
        kcb(ke);}};// dispatch
@implementation SpV
-(instancetype)initWithFrame:(NSRect)f mw:(MacWin*)m{self=[super initWithFrame:f];if(self)mw_=m;return self;} // init
-(void)drawRect:(NSRect)__unused r{if(mw_)mw_->onDraw([[NSGraphicsContext currentContext]CGContext]);} // draw
-(void)keyDown:(NSEvent*)ev{if(mw_)mw_->onKey(ev);} // key
-(BOOL)acceptsFirstResponder{return YES;} // focus
@end
int main(){ // entry
    @autoreleasepool{ // ARC
        int w=40+Spreadsheet::COLS*100,h=20+Spreadsheet::ROWS*25; // size
        MacWin win(w,h);App app(win); // create
        win.setRCB([&]{app.render();}); // hook
        app.render();win.run();} // run
    return 0;} // exit
