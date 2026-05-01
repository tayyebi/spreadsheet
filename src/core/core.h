// =============================================================================
// core.h  —  backward-compatibility header
//
// This file previously defined CellValue, Cell, Coordinate, and Spreadsheet.
// Those definitions have been split into single-purpose headers:
//   cell.h        — CellValue, Cell, Coordinate
//   spreadsheet.h — Spreadsheet class
//
// Including core.h still pulls in everything as before.
// =============================================================================
#pragma once
#include "cell.h"
#include "spreadsheet.h"

