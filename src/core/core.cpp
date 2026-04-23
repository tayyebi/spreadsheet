#include "core.h" // hdr
#include "formula.h" // formula
#include <fstream> // I/O
#include <sstream> // oss
static std::string E(const std::string&s){ // CSV escape
    if(s.find_first_of(",\"\n\r")==s.npos)return s; // plain
    std::string o="\"";for(char c:s){if(c=='"')o+='"';o+=c;}return o+'"';} // quoted
void Spreadsheet::setCell(int r,int c,std::string raw){ // store
    auto&cell=cells_[key(r,c)];cell.raw=std::move(raw);cell.display=cell.raw;cell.value=std::monostate{};} // init
Cell*Spreadsheet::getCell(int r,int c){auto it=cells_.find(key(r,c));return it!=cells_.end()?&it->second:nullptr;} // get
const Cell*Spreadsheet::getCell(int r,int c)const{auto it=cells_.find(key(r,c));return it!=cells_.end()?&it->second:nullptr;} // get c
void Spreadsheet::evalCell(int r,int c,std::set<uint64_t>&vis,std::set<uint64_t>&done){ // DFS
    auto k=key(r,c);if(done.count(k))return; // done
    auto it=cells_.find(k);if(it==cells_.end())return; // absent
    auto&cell=it->second; // ref
    if(cell.raw.empty()||cell.raw[0]!='='){ // plain
        try{size_t p=0;double v=std::stod(cell.raw,&p);if(p==cell.raw.size()){cell.value=v;cell.display=cell.raw;}else{cell.value=cell.raw;cell.display=cell.raw;}}
        catch(...){cell.value=cell.raw;cell.display=cell.raw;} // text
        done.insert(k);return;} // mark
    if(vis.count(k)){cell.display="#CYCLE!";cell.value=std::string("#CYCLE!");done.insert(k);return;} // cycle
    vis.insert(k); // push
    auto res=evaluateFormula(cell.raw.substr(1),*this,vis,done); // eval
    vis.erase(k); // pop
    if(std::holds_alternative<std::string>(res)){auto e=std::get<std::string>(res);cell.display=e;cell.value=e;} // err
    else{double v=std::get<double>(res);cell.value=v;std::ostringstream os;os<<v;cell.display=os.str();} // num
    done.insert(k);} // mark
void Spreadsheet::evaluateAll(){std::set<uint64_t>vis,done;for(auto&[k,_]:cells_)evalCell(int(k>>32),int(k&0xFFFFFFFFu),vis,done);} // all
bool Spreadsheet::saveCSV(const std::string&p)const{ // save
    std::ofstream f(p);if(!f)return false; // open
    for(int r=0;r<ROWS;++r){for(int c=0;c<COLS;++c){if(c>0)f<<',';auto it=cells_.find(key(r,c));if(it!=cells_.end())f<<E(it->second.raw);}f<<'\n';} // write
    return true;} // done
bool Spreadsheet::loadCSV(const std::string&p){ // load
    std::ifstream f(p);if(!f)return false; // open
    cells_.clear();std::string line;int r=0; // reset
    while(std::getline(f,line)&&r<ROWS){ // rows
        int c=0;size_t i=0;
        while(i<=line.size()&&c<COLS){ // fields
            std::string fld;
            if(i<line.size()&&line[i]=='"'){++i;while(i<line.size()){if(line[i]=='"'){if(++i<line.size()&&line[i]=='"'){fld+='"';++i;}else break;}else fld+=line[i++];}if(i<line.size()&&line[i]==',')++i;} // quoted
            else{auto e=line.find(',',i);if(e==line.npos)e=line.size();fld=line.substr(i,e-i);i=e+1;} // plain
            if(!fld.empty()){setCell(r,c,fld);}++c;} // store
        ++r;} // row
    return true;} // done
