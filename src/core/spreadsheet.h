// =============================================================================
// spreadsheet.h  —  Spreadsheet class: grid storage, evaluation, and I/O
//
// The Spreadsheet is the central data model (the "M" in MVC).
// Responsibilities:
//   • Store cells sparsely — only written cells occupy memory
//   • Expose setCell / getCell for reading and writing
//   • Evaluate all formulas in dependency order (DFS, cycle detection)
//   • Serialize to / from CSV and ODS file formats
//
// Method implementations are split across three single-purpose files:
//   spreadsheet.cpp — grid storage and formula evaluation
//   csv.cpp         — CSV (RFC-4180) persistence
//   ods.cpp         — OpenDocument Spreadsheet (.ods) persistence
// =============================================================================
#pragma once

#include "cell.h"        // CellValue, Cell, Coordinate
#include <string>        // std::string — file paths, raw cell content
#include <unordered_map> // std::unordered_map — sparse O(1) cell storage
#include <set>           // std::set — cycle-detection sets
#include <cstdint>       // uint64_t, uint32_t — key packing

class Spreadsheet {
public:
    // Grid dimensions are compile-time constants shared across all layers.
    static constexpr int ROWS = 20;  // rows  0..19
    static constexpr int COLS = 10;  // columns 0..9

    // -----------------------------------------------------------------------
    // key()  —  pack (row, col) into one 64-bit integer
    //   key = (row << 32) | col
    // -----------------------------------------------------------------------
    static uint64_t key(int r, int c) {
        return (uint64_t(r) << 32) | uint32_t(c);
    }

    // -- Grid storage --------------------------------------------------------

    // Store raw text in a cell and reset its computed state.
    // The cell is NOT re-evaluated here; call evaluateAll() to recompute.
    void setCell(int r, int c, std::string raw);

    // Look up a cell by (row, col).  Returns nullptr if never set.
    // The non-const overload lets the evaluator write display/value back.
          Cell* getCell(int r, int c);
    const Cell* getCell(int r, int c) const;

    // -- Evaluation ----------------------------------------------------------

    // Evaluate every cell in the grid.
    // Starts a DFS from each unevaluated cell, propagating computed values
    // along dependency edges before evaluating dependents.
    void evaluateAll();

    // Evaluate a single cell, first recursively evaluating its dependencies.
    //   vis  – DFS stack (cells currently being visited): used for cycle detect
    //   done – set of already-fully-evaluated cells (memoisation)
    void evalCell(int r, int c, std::set<uint64_t>& vis, std::set<uint64_t>& done);

    // -- CSV persistence (implemented in csv.cpp) ----------------------------

    // Write every cell's raw string to a CSV file (RFC-4180 quoting rules).
    // Returns true on success, false if the file could not be opened.
    bool saveCSV(const std::string& path) const;

    // Read a CSV file and populate the grid; clears existing data first.
    // Returns true on success, false if the file could not be opened.
    bool loadCSV(const std::string& path);

    // -- ODS persistence (implemented in ods.cpp) ----------------------------

    // Write the grid to an OpenDocument Spreadsheet (.ods) file.
    // Uses ZIP STORE (no compression) so no external library is required.
    // LibreOffice and compatible suites can open the resulting file.
    // Returns true on success, false if the file could not be written.
    bool saveODS(const std::string& path) const;

    // Read an ODS file and populate the grid (ZIP STORE entries only).
    // Returns true on success, false on error.
    bool loadODS(const std::string& path);

private:
    // Sparse map from encoded key → Cell.
    // Only cells that have been written to exist as entries.
    std::unordered_map<uint64_t, Cell> cells_;
};
