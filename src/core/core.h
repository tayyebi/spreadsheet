// =============================================================================
// core.h  —  Spreadsheet data model: cells, grid dimensions, and persistence
//
// This header defines the three fundamental building blocks:
//   1. CellValue  – a type-safe union describing what a cell may hold
//   2. Cell       – one grid square: raw user input, display string, computed value
//   3. Spreadsheet – the 20×10 grid with evaluation, CSV I/O, and cell access
// =============================================================================

// #pragma once tells the compiler to include this file at most once per
// translation unit, preventing duplicate-definition errors without requiring
// the traditional #ifndef / #define / #endif include-guard pattern.
#pragma once

#include <string>          // std::string — for raw text, display strings, errors
#include <unordered_map>   // std::unordered_map — sparse O(1) cell storage
#include <variant>         // std::variant — type-safe union (C++17)
#include <set>             // std::set — ordered sets used for cycle detection
#include <cstdint>         // uint64_t, uint32_t — exact-width integers for key packing

// ---------------------------------------------------------------------------
// Coordinate  —  a named pair of zero-based (row, col) indices
//
// Used by callers that prefer passing a single struct rather than two ints.
// Not used internally by Spreadsheet itself.
// ---------------------------------------------------------------------------
struct Coordinate { int row, col; };

// ---------------------------------------------------------------------------
// CellValue  —  the three things a cell can hold after evaluation
//
// std::variant is C++17's discriminated union.  It holds exactly ONE of:
//   • std::monostate  – a no-value sentinel; means the cell is empty or
//                       has not been evaluated yet (similar to null/None)
//   • double          – a computed numeric result, e.g. 42.0 or 3.14
//   • std::string     – user-typed text or an error token like "#ERR!"
//
// Using variant instead of separate bool/double/string fields means the
// compiler enforces exhaustive case-handling via std::holds_alternative or
// std::visit, making it impossible to accidentally read the wrong field.
// ---------------------------------------------------------------------------
using CellValue = std::variant<std::monostate, double, std::string>;

// ---------------------------------------------------------------------------
// Cell  —  one cell in the spreadsheet grid
//
// Three fields cover the full cell lifecycle:
//   raw     – exactly what the user typed, preserved verbatim so it can be
//             re-edited and saved to CSV.  Examples: "=SUM(A1:A3)", "42", "Hello"
//   display – the human-readable string shown in the grid after evaluation.
//             For a formula "=1+2" this would be "3"; for an error it's "#ERR!".
//   value   – the machine-readable result consumed by other formulas that
//             reference this cell.  monostate means not-yet-evaluated.
// ---------------------------------------------------------------------------
struct Cell {
    std::string raw;      // original user input, never transformed
    std::string display;  // rendered string painted in the UI cell
    CellValue   value;    // evaluated result used during formula computation
};

// ---------------------------------------------------------------------------
// Spreadsheet  —  the central data model (the "M" in MVC)
//
// Responsibilities:
//   • Store cells sparsely: only written cells occupy memory
//   • Expose setCell / getCell for reading and writing
//   • Evaluate all formulas in dependency order using depth-first search (DFS)
//   • Detect and report circular references as "#CYCLE!"
//   • Serialize the raw cell data to / from RFC-4180 CSV files
// ---------------------------------------------------------------------------
class Spreadsheet {
public:
    // Grid dimensions are compile-time constants.  Every module that includes
    // this header agrees on the same values without a runtime call.
    static constexpr int ROWS = 20;  // rows  0..19
    static constexpr int COLS = 10;  // columns 0..9

    // -----------------------------------------------------------------------
    // key()  —  pack (row, col) into one 64-bit integer
    //
    // We place row in the upper 32 bits and col in the lower 32 bits:
    //   key = (row << 32) | col
    // This gives a unique integer for every grid position and lets us use
    // an unordered_map<uint64_t, Cell> as sparse storage.
    //
    // Example: row=1, col=3  →  0x0000000100000003
    // -----------------------------------------------------------------------
    static uint64_t key(int r, int c) {
        return (uint64_t(r) << 32) | uint32_t(c);
    }

    // Store raw text in a cell and reset its computed state.
    // The cell is NOT re-evaluated here; call evaluateAll() to recompute.
    void setCell(int r, int c, std::string raw);

    // Look up a cell by (row, col).  Returns nullptr if the cell was never set.
    // The non-const overload lets the evaluator write display/value back.
          Cell* getCell(int r, int c);
    const Cell* getCell(int r, int c) const;

    // Evaluate every cell in the grid.
    // Internally starts a DFS from each unevaluated cell, propagating
    // computed values along dependency edges before evaluating dependents.
    void evaluateAll();

    // Evaluate a single cell, first recursively evaluating its dependencies.
    //   vis  – the DFS stack (cells currently being visited): used to detect
    //          cycles (if a cell appears in vis while being evaluated, it's a cycle)
    //   done – the set of already-fully-evaluated cells, to skip re-work
    void evalCell(int r, int c, std::set<uint64_t>& vis, std::set<uint64_t>& done);

    // Write every cell's raw string to a CSV file (RFC-4180 quoting rules).
    // Returns true on success, false if the file could not be opened.
    bool saveCSV(const std::string& path) const;

    // Read a CSV file and populate the grid; clears existing data first.
    // Returns true on success, false if the file could not be opened.
    bool loadCSV(const std::string& path);

    // Write the grid to an OpenDocument Spreadsheet (.ods) file.
    // The ODS archive is written using ZIP STORE (no compression) so that no
    // external library is required.  LibreOffice and compatible suites can
    // open the resulting file directly.
    // Returns true on success, false if the file could not be written.
    bool saveODS(const std::string& path) const;

    // Read an OpenDocument Spreadsheet (.ods) file and populate the grid.
    // Only ZIP STORE (uncompressed) entries are supported; ODS files saved
    // by LibreOffice use DEFLATE and cannot be loaded with this method.
    // Use saveODS() to create files that loadODS() can later read back.
    // Returns true on success, false on error.
    bool loadODS(const std::string& path);

private:
    // Sparse map from encoded key → Cell.
    // Only cells that have been written to exist as entries.
    // An empty 20×10 grid uses zero heap memory for cells.
    std::unordered_map<uint64_t, Cell> cells_;
};
