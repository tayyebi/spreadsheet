#include "platform.h" // hdr
#include "app.h" // App
#include <X11/Xlib.h> // X11
#include <X11/Xutil.h> // lookup
#include <X11/keysym.h> // XK_*
#include <stdexcept> // error
class X11Win:public IWindow{ // X11 impl
    Display*dpy=nullptr;Window win=0;Pixmap buf=0;GC gc=nullptr;int W,H; // state
    std::function<void(KeyEvent)>kcb;std::function<void()>rcb; // cbs
    void fg(Color c){XColor x{};x.red=c.r*257u;x.green=c.g*257u;x.blue=c.b*257u;x.flags=DoRed|DoGreen|DoBlue;XAllocColor(dpy,DefaultColormap(dpy,DefaultScreen(dpy)),&x);XSetForeground(dpy,gc,x.pixel);} // fg color
public:
    X11Win(int w,int h):W(w),H(h){ // ctor
        dpy=XOpenDisplay(nullptr);if(!dpy)throw std::runtime_error("X"); // connect
        int s=DefaultScreen(dpy); // screen
        win=XCreateSimpleWindow(dpy,RootWindow(dpy,s),0,0,w,h,1,BlackPixel(dpy,s),WhitePixel(dpy,s)); // window
        XSelectInput(dpy,win,ExposureMask|KeyPressMask);XStoreName(dpy,win,"Spreadsheet");XMapWindow(dpy,win);XFlush(dpy); // setup
        gc=XCreateGC(dpy,win,0,nullptr);buf=XCreatePixmap(dpy,win,w,h,DefaultDepth(dpy,s));} // gc, pixmap
    ~X11Win(){if(buf){XFreePixmap(dpy,buf);}if(gc){XFreeGC(dpy,gc);}if(win){XDestroyWindow(dpy,win);}if(dpy){XCloseDisplay(dpy);}} // dtor
    void drawText(int x,int y,const std::string&t,Color c)override{fg(c);XDrawString(dpy,buf,gc,x,y+12,t.c_str(),(int)t.size());} // text
    void drawRect(int x,int y,int w,int h,Color c)override{fg(c);XDrawRectangle(dpy,buf,gc,x,y,(unsigned)(w-1),(unsigned)(h-1));} // outline
    void fillRect(int x,int y,int w,int h,Color c)override{fg(c);XFillRectangle(dpy,buf,gc,x,y,(unsigned)w,(unsigned)h);} // fill
    void updateDisplay()override{XCopyArea(dpy,buf,win,gc,0,0,(unsigned)W,(unsigned)H,0,0);XFlush(dpy);} // blit
    void handleInput(std::function<void(KeyEvent)>f)override{kcb=std::move(f);} // bind
    void setRedraw(std::function<void()>f){rcb=std::move(f);} // bind
    void run()override{ // loop
        XEvent ev; // event
        for(;;){XNextEvent(dpy,&ev); // next
            if(ev.type==Expose&&ev.xexpose.count==0&&rcb)rcb(); // expose
            else if(ev.type==KeyPress){ // key
                char buf[8]={};KeySym sym=0;XLookupString(&ev.xkey,buf,7,&sym,nullptr); // decode
                KeyEvent ke{};ke.ctrl=(ev.xkey.state&ControlMask)!=0;ke.shift=(ev.xkey.state&ShiftMask)!=0;ke.ch=buf[0]; // build
                if(ke.ctrl&&ke.ch>=1&&ke.ch<=26)ke.ch=char(ke.ch+'a'-1); // norm
                switch(sym){case XK_Up:ke.key=KEY_UP;ke.ch=0;break;case XK_Down:ke.key=KEY_DOWN;ke.ch=0;break;case XK_Left:ke.key=KEY_LEFT;ke.ch=0;break;case XK_Right:ke.key=KEY_RIGHT;ke.ch=0;break;case XK_Return:ke.key=KEY_ENTER;break;case XK_Escape:ke.key=KEY_ESC;break;case XK_BackSpace:ke.key=KEY_BACKSPACE;break;default:ke.key=int(buf[0]);break;} // map
                if(kcb)kcb(ke);}}}};// dispatch
int main(){ // entry
    int w=40+Spreadsheet::COLS*100,h=20+Spreadsheet::ROWS*25; // size
    X11Win win(w,h);App app(win);win.setRedraw([&]{app.render();});app.render();win.run();return 0;} // run
