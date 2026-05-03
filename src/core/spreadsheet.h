// =============================================================================
// spreadsheet.h  —  Spreadsheet class: grid storage and formula evaluation
//
// The Spreadsheet is the central data model (the "M" in MVC).
// Responsibilities:
//   • Store cells sparsely — only written cells occupy memory
//   • Expose setCell / getCell for reading and writing
//   • Evaluate all formulas in dependency order (DFS, cycle detection)
//
// Persistence is handled by separate single-purpose modules:
//   csv.h / csv.cpp — CSV (RFC-4180) read/write (free functions)
//   ods.h / ods.cpp — Custom binary SSHEET format read/write (free functions)
// =============================================================================
#pragma once

#include "cell.h"        // CellValue, Cell, Coordinate
#include <string>        // std::string — raw cell content
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

    // Remove all cell data (used by persistence loaders before populating).
    void clear();

    // -- Evaluation ----------------------------------------------------------

    // Evaluate every cell in the grid.
    // Starts a DFS from each unevaluated cell, propagating computed values
    // along dependency edges before evaluating dependents.
    void evaluateAll();

    // Evaluate a single cell, first recursively evaluating its dependencies.
    //   vis  – DFS stack (cells currently being visited): used for cycle detect
    //   done – set of already-fully-evaluated cells (memoisation)
    void evalCell(int r, int c, std::set<uint64_t>& vis, std::set<uint64_t>& done);

private:
    // Sparse map from encoded key → Cell.
    // Only cells that have been written to exist as entries.
    std::unordered_map<uint64_t, Cell> cells_;
};
