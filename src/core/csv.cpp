// =============================================================================
// csv.cpp  —  CSV (RFC-4180) persistence: saveCSV() and loadCSV()
//
// Writes and reads the raw cell strings so that formulas (e.g. "=SUM(A1:A3)")
// are preserved verbatim and re-evaluated on load.  The grid is accessed
// entirely through the public Spreadsheet API (getCell / setCell / clear).
// =============================================================================

#include "csv.h"   // saveCSV, loadCSV declarations
#include <fstream> // std::ifstream, std::ofstream

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
// saveCSV()  —  write non-empty cells to a CSV file
//
// Finds the bounding box of non-empty cells, then writes that rectangle.
// Empty cells produce an empty field; the file always contains a proper
// rectangular grid up to (maxRow × maxCol) so it can be re-loaded cleanly.
// ---------------------------------------------------------------------------
bool saveCSV(const Spreadsheet& sheet, const std::string& path) {
    // Find bounding box of non-empty cells.
    int maxR = -1, maxC = -1;
    sheet.forEachCell([&](int r, int c, const Cell& cell) {
        if (!cell.raw.empty()) {
            if (r > maxR) maxR = r;
            if (c > maxC) maxC = c;
        }
    });

    if (maxR < 0) return true;  // nothing to write

    std::ofstream f(path);
    if (!f) return false;

    for (int r = 0; r <= maxR; ++r) {
        for (int c = 0; c <= maxC; ++c) {
            if (c > 0) f << ',';
            const Cell* cell = sheet.getCell(r, c);
            if (cell) f << E(cell->raw);
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
bool loadCSV(Spreadsheet& sheet, const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;

    sheet.clear();

    std::string line;
    int r = 0;
    while (std::getline(f, line) && r < Spreadsheet::MAX_ROWS) {
        int    c = 0;
        size_t i = 0;

        while (i <= line.size() && c < Spreadsheet::MAX_COLS) {
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

            if (!fld.empty()) sheet.setCell(r, c, fld);
            ++c;
        }
        ++r;
    }
    return true;
}

