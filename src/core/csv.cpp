// =============================================================================
// csv.cpp  —  CSV (RFC-4180) persistence for Spreadsheet
//
// Implements: saveCSV() and loadCSV().
// Writes and reads the raw cell strings so that formulas (e.g. "=SUM(A1:A3)")
// are preserved verbatim and re-evaluated on load.
// =============================================================================

#include "spreadsheet.h"  // Spreadsheet, Cell
#include <fstream>        // std::ifstream, std::ofstream

// ---------------------------------------------------------------------------
// E()  —  CSV field escaper (RFC-4180)
//
// Wraps fields containing comma, double-quote, newline, or carriage-return
// in double-quotes and doubles any embedded double-quotes.
//   hello        → hello
//   hello,world  → "hello,world"
//   say "hi"     → "say ""hi"""
// ---------------------------------------------------------------------------
static std::string E(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == s.npos) return s;
    std::string o = "\"";
    for (char c : s) {
        if (c == '"') o += '"';
        o += c;
    }
    return o + '"';
}

// ---------------------------------------------------------------------------
// saveCSV()  —  write the grid to a CSV file
//
// Writes ROWS lines, each containing COLS comma-separated fields holding
// the raw cell strings.  Empty cells produce an empty field.
// ---------------------------------------------------------------------------
bool Spreadsheet::saveCSV(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (c > 0) f << ',';
            auto it = cells_.find(key(r, c));
            if (it != cells_.end())
                f << E(it->second.raw);
        }
        f << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// loadCSV()  —  read a CSV file and populate the grid
//
// Parses RFC-4180 format: quoted fields (with "" for embedded quotes) and
// plain (unquoted) fields.  Only non-empty cells are stored.
// ---------------------------------------------------------------------------
bool Spreadsheet::loadCSV(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;

    cells_.clear();

    std::string line;
    int r = 0;
    while (std::getline(f, line) && r < ROWS) {
        int    c = 0;
        size_t i = 0;

        while (i <= line.size() && c < COLS) {
            std::string fld;

            if (i < line.size() && line[i] == '"') {
                ++i;
                while (i < line.size()) {
                    if (line[i] == '"') {
                        ++i;
                        if (i < line.size() && line[i] == '"') {
                            fld += '"';
                            ++i;
                        } else {
                            break;
                        }
                    } else {
                        fld += line[i++];
                    }
                }
                if (i < line.size() && line[i] == ',') ++i;
            } else {
                auto e = line.find(',', i);
                if (e == line.npos) e = line.size();
                fld = line.substr(i, e - i);
                i   = e + 1;
            }

            if (!fld.empty()) setCell(r, c, fld);
            ++c;
        }
        ++r;
    }
    return true;
}
