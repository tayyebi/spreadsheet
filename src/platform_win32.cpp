#ifndef UNICODE
#define UNICODE // wide
#endif
#ifndef _UNICODE
#define _UNICODE // wide CRT
#endif
#include <windows.h> // Win32
#include "platform.h" // IWindow
#include "app.h" // App
#include <stdexcept> // error
class W32Win:public IWindow{ // Win32 impl
    HWND hwnd=nullptr;HDC mdc=nullptr;HBITMAP mbmp=nullptr;HBITMAP obmp=nullptr;int W,H; // state
    std::function<void(KeyEvent)>kcb;std::function<void()>rcb; // cbs
    static W32Win*inst; // singleton
    static COLORREF CR(Color c){return RGB(c.r,c.g,c.b);} // color
public:
    W32Win(int w,int h):W(w),H(h){ // ctor
        inst=this;WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=WP;wc.hInstance=GetModuleHandleW(nullptr); // class
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=HBRUSH(COLOR_WINDOW+1);wc.lpszClassName=L"SW";RegisterClassExW(&wc); // style
        RECT r{0,0,w,h};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE); // adjust
        hwnd=CreateWindowExW(0,L"SW",L"Spreadsheet",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr); // window
        if(!hwnd)throw std::runtime_error("CreateWindowEx"); // check
        ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd); // show
        HDC dc=GetDC(hwnd);mdc=CreateCompatibleDC(dc);mbmp=CreateCompatibleBitmap(dc,w,h);obmp=HBITMAP(SelectObject(mdc,mbmp));ReleaseDC(hwnd,dc);} // buffer
    ~W32Win(){if(mdc){SelectObject(mdc,obmp);DeleteObject(mbmp);DeleteDC(mdc);}if(hwnd){DestroyWindow(hwnd);}} // dtor
    void drawText(int x,int y,const std::string&t,Color c)override{SetTextColor(mdc,CR(c));SetBkMode(mdc,TRANSPARENT);TextOutA(mdc,x,y,t.c_str(),(int)t.size());} // text
    void drawRect(int x,int y,int w,int h,Color c)override{ // outline
        HPEN p=CreatePen(PS_SOLID,1,CR(c));HPEN op=HPEN(SelectObject(mdc,p)); // pen
        HBRUSH ob=HBRUSH(SelectObject(mdc,GetStockObject(NULL_BRUSH))); // no fill
        Rectangle(mdc,x,y,x+w,y+h);SelectObject(mdc,op);SelectObject(mdc,ob);DeleteObject(p);} // draw
    void fillRect(int x,int y,int w,int h,Color c)override{HBRUSH b=CreateSolidBrush(CR(c));RECT r{x,y,x+w,y+h};FillRect(mdc,&r,b);DeleteObject(b);} // fill
    void updateDisplay()override{HDC dc=GetDC(hwnd);BitBlt(dc,0,0,W,H,mdc,0,0,SRCCOPY);ReleaseDC(hwnd,dc);} // blit
    void handleInput(std::function<void(KeyEvent)>f)override{kcb=std::move(f);} // bind
    void setRCB(std::function<void()>f){rcb=std::move(f);} // bind
    void run()override{MSG m{};while(GetMessageW(&m,nullptr,0,0)){TranslateMessage(&m);DispatchMessageW(&m);}} // loop
    static LRESULT CALLBACK WP(HWND hw,UINT msg,WPARAM wp,LPARAM lp){ // proc
        if(!inst)return DefWindowProcW(hw,msg,wp,lp); // guard
        switch(msg){ // dispatch
        case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hw,&ps);if(inst->rcb)inst->rcb();EndPaint(hw,&ps);return 0;} // paint
        case WM_KEYDOWN:{if(!inst->kcb)break; // key
            KeyEvent ke{};ke.ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;ke.shift=(GetKeyState(VK_SHIFT)&0x8000)!=0; // mods
            switch(wp){case VK_UP:ke.key=KEY_UP;break;case VK_DOWN:ke.key=KEY_DOWN;break;case VK_LEFT:ke.key=KEY_LEFT;break;case VK_RIGHT:ke.key=KEY_RIGHT;break;case VK_RETURN:ke.key=KEY_ENTER;break;case VK_ESCAPE:ke.key=KEY_ESC;break;case VK_BACK:ke.key=KEY_BACKSPACE;break;default:return DefWindowProcW(hw,msg,wp,lp);} // vkey
            inst->kcb(ke);return 0;} // dispatch
        case WM_CHAR:{if(!inst->kcb)break; // char
            KeyEvent ke{};ke.key=int(wp);ke.ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;ke.ch=char(wp); // build
            if(ke.ctrl&&ke.ch>=1&&ke.ch<=26)ke.ch=char(ke.ch+'a'-1); // norm
            inst->kcb(ke);return 0;} // dispatch
        case WM_DESTROY:PostQuitMessage(0);return 0;} // quit
        return DefWindowProcW(hw,msg,wp,lp);}};// default
W32Win*W32Win::inst=nullptr; // static
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){ // entry
    int w=40+Spreadsheet::COLS*100,h=20+Spreadsheet::ROWS*25; // size
    W32Win win(w,h);App app(win);win.setRCB([&]{app.render();});app.render();win.run();return 0;} // run
