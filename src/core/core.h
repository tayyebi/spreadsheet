#pragma once // guard
#include <string> // string
#include <unordered_map> // map
#include <variant> // variant
#include <set> // set
#include <cstdint> // uint64_t
struct Coordinate{int row,col;}; // grid pos
using CellValue=std::variant<std::monostate,double,std::string>; // value union
struct Cell{std::string raw,display;CellValue value;}; // cell data
class Spreadsheet{ // data model
public:
    static constexpr int ROWS=20,COLS=10; // grid dims
    static uint64_t key(int r,int c){return(uint64_t(r)<<32)|uint32_t(c);} // encode key
    void setCell(int r,int c,std::string raw); // store raw
    Cell*getCell(int r,int c); // mutable lookup
    const Cell*getCell(int r,int c)const; // const lookup
    void evaluateAll(); // eval all cells
    void evalCell(int r,int c,std::set<uint64_t>&vis,std::set<uint64_t>&done); // DFS eval
    bool saveCSV(const std::string&p)const; // write CSV
    bool loadCSV(const std::string&p); // read CSV
private:
    std::unordered_map<uint64_t,Cell>cells_; // sparse storage
};
