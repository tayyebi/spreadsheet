// =============================================================================
// cell.h  —  Cell data types: CellValue, Cell, and Coordinate
//
// This header defines the three data types that represent a single cell's
// state.  It has no dependencies on any other application header and can be
// included by any layer (core, formula, app) without pulling in the full
// Spreadsheet class.
// =============================================================================
#pragma once

#include <string>   // std::string — raw input, display text, error tokens
#include <variant>  // std::variant — type-safe discriminated union (C++17)

// ---------------------------------------------------------------------------
// Coordinate  —  a named pair of zero-based (row, col) indices
//
// Used by callers that prefer passing a single struct rather than two ints.
// ---------------------------------------------------------------------------
struct Coordinate { int row, col; };

// ---------------------------------------------------------------------------
// CellValue  —  the three things a cell can hold after evaluation
//
// std::variant holds exactly ONE of:
//   • std::monostate  – empty / not yet evaluated
//   • double          – a computed numeric result, e.g. 42.0 or 3.14
//   • std::string     – user-typed text or an error token like "#ERR!"
// ---------------------------------------------------------------------------
using CellValue = std::variant<std::monostate, double, std::string>;

// ---------------------------------------------------------------------------
// Cell  —  one cell in the spreadsheet grid
//
//   raw     – exactly what the user typed (e.g. "=SUM(A1:A3)", "42", "Hello")
//   display – the human-readable string shown in the UI after evaluation
//   value   – the machine-readable result consumed by dependent formulas
// ---------------------------------------------------------------------------
struct Cell {
    std::string raw;      // original user input, never transformed
    std::string display;  // rendered string painted in the UI cell
    CellValue   value;    // evaluated result used during formula computation
};
