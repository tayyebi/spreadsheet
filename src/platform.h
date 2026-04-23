#pragma once // guard
#include <string> // string
#include <functional> // function
#include <cstdint> // uint8_t
struct Color{uint8_t r,g,b;}; // RGB
struct KeyEvent{int key=0;char ch=0;bool ctrl=false,shift=false;}; // key event
class IWindow{ // window interface
public:
    virtual~IWindow()=default; // dtor
    virtual void drawText(int x,int y,const std::string&t,Color c)=0; // text
    virtual void drawRect(int x,int y,int w,int h,Color c)=0; // outline
    virtual void fillRect(int x,int y,int w,int h,Color c)=0; // fill
    virtual void updateDisplay()=0; // blit
    virtual void handleInput(std::function<void(KeyEvent)>cb)=0; // input cb
    virtual void run()=0; // event loop
    static constexpr int KEY_UP=0x100,KEY_DOWN=0x101,KEY_LEFT=0x102,KEY_RIGHT=0x103; // arrows
    static constexpr int KEY_ENTER='\r',KEY_ESC=0x1B,KEY_BACKSPACE=0x08; // special keys
};
