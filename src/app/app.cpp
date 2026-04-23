#include "app.h" // hdr
#include <string> // string
#include <cctype> // isprint
static std::string CL(int c){std::string s;int n=c+1;while(n>0){s=char('A'+(n-1)%26)+s;n=(n-1)/26;}return s;} // col label
App::App(IWindow&w):win_(w){win_.handleInput([this](KeyEvent e){onKey(e);});} // ctor
void App::render(){ // draw
    Color bg{255,255,255},gr{200,200,200},hd{220,220,220},sl{0,0,200},tx{0,0,0},ed{255,255,200}; // colors
    int W=HW+Spreadsheet::COLS*CW,H=HH+Spreadsheet::ROWS*CH; // dims
    win_.fillRect(0,0,W,H,bg);win_.fillRect(0,0,HW,HH,hd); // clear, corner
    for(int c=0;c<Spreadsheet::COLS;++c){ // col headers
        int x=HW+c*CW;win_.fillRect(x,0,CW,HH,hd);win_.drawRect(x,0,CW,HH,gr);win_.drawText(x+4,4,CL(c),tx);} // fill,border,label
    for(int r=0;r<Spreadsheet::ROWS;++r){ // row headers
        int y=HH+r*CH;win_.fillRect(0,y,HW,CH,hd);win_.drawRect(0,y,HW,CH,gr);win_.drawText(4,y+5,std::to_string(r+1),tx);} // fill,border,label
    for(int r=0;r<Spreadsheet::ROWS;++r)for(int c=0;c<Spreadsheet::COLS;++c){ // cells
        int x=HW+c*CW,y=HH+r*CH;bool sel=(r==selRow_&&c==selCol_); // pos, sel?
        if(sel&&editing_)win_.fillRect(x,y,CW,CH,ed); // edit bg
        win_.drawRect(x,y,CW,CH,gr); // border
        std::string d;if(sel&&editing_)d=editBuf_+'|';else{const Cell*cell=sheet_.getCell(r,c);if(cell)d=cell->display;} // content
        if(!d.empty())win_.drawText(x+3,y+5,d,tx);} // text
    int sx=HW+selCol_*CW,sy=HH+selRow_*CH; // sel pos
    for(int i=0;i<3;++i)win_.drawRect(sx-i,sy-i,CW+2*i,CH+2*i,sl); // thick border
    win_.updateDisplay();} // blit
void App::onKey(KeyEvent e){ // key
    if(editing_){ // edit mode
        if(e.key==IWindow::KEY_ENTER){sheet_.setCell(selRow_,selCol_,editBuf_);sheet_.evaluateAll();editing_=false;} // commit
        else if(e.key==IWindow::KEY_ESC){editing_=false;editBuf_.clear();} // cancel
        else if(e.key==IWindow::KEY_BACKSPACE){if(!editBuf_.empty())editBuf_.pop_back();} // bksp
        else if(e.ch&&std::isprint((unsigned)e.ch)&&!e.ctrl)editBuf_+=e.ch;} // append
    else{ // nav mode
        if(e.key==IWindow::KEY_UP&&selRow_>0)--selRow_; // up
        else if(e.key==IWindow::KEY_DOWN&&selRow_<Spreadsheet::ROWS-1)++selRow_; // down
        else if(e.key==IWindow::KEY_LEFT&&selCol_>0)--selCol_; // left
        else if(e.key==IWindow::KEY_RIGHT&&selCol_<Spreadsheet::COLS-1)++selCol_; // right
        else if(e.key==IWindow::KEY_ENTER){editing_=true;const Cell*c=sheet_.getCell(selRow_,selCol_);editBuf_=c?c->raw:"";} // edit
        else if(e.ctrl&&(e.ch=='s'||e.ch==19))sheet_.saveCSV("spreadsheet.csv"); // Ctrl+S
        else if(e.ctrl&&(e.ch=='o'||e.ch==15)){sheet_.loadCSV("spreadsheet.csv");sheet_.evaluateAll();}} // Ctrl+O
    render();} // redraw
