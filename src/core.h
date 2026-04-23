#pragma once              // include guard
#include <string>         // std::string
#include <unordered_map>  // sparse storage
#include <variant>        // cell value variant
#include <set>            // DFS sets
#include <cstdint>        // uint64_t

// A position in the spreadsheet grid
struct Coordinate {
    int row{0}; // row index, 0-based
    int col{0}; // column index, 0-based
};

// A cell value: empty, numeric, or string (text/formula display)
using CellValue = std::variant<std::monostate, double, std::string>;

// One cell's data
struct Cell {
    std::string raw;     // raw user input (may start with =)
    CellValue   value;   // evaluated value
    std::string display; // formatted string shown in UI
};

class Spreadsheet { // main spreadsheet data model
public:
    static constexpr int ROWS = 20; // total rows in grid
    static constexpr int COLS = 10; // total columns in grid

    // Encode (row,col) into a 64-bit key
    static uint64_t key(int r, int c) {
        return (static_cast<uint64_t>(r) << 32) | static_cast<uint32_t>(c); // pack row/col
    }

    void        setCell(int r, int c, std::string raw); // store raw content
    Cell*       getCell(int r, int c);                  // mutable cell pointer (nullptr if empty)
    const Cell* getCell(int r, int c) const;            // const cell pointer

    void evaluateAll(); // evaluate all formula cells (DFS cycle detection)

    // DFS helper called from formula engine when resolving cell refs
    void evalCell(int r, int c, std::set<uint64_t>& vis, std::set<uint64_t>& done);

    bool saveCSV(const std::string& path) const; // serialise grid to CSV file
    bool loadCSV(const std::string& path);       // deserialise CSV into grid

private:
    std::unordered_map<uint64_t, Cell> cells_; // sparse cell storage
};
