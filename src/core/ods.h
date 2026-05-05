// =============================================================================
// ods.h  —  Custom binary spreadsheet persistence (no XML, no ZIP)
//
// Declares two free functions that read and write a Spreadsheet to/from a
// compact custom binary format.  Implementation is in ods.cpp.
//
// File layout (all multi-byte integers are little-endian):
//   Bytes  0– 5  Magic: "SSHEET"
//   Bytes  6– 7  Version: 0x01 0x00
//   Bytes  8–11  Number of non-empty cells (uint32_t)
//   For each cell:
//     4 bytes  Row index (uint32_t)
//     4 bytes  Column index (uint32_t)
//     4 bytes  Content length in bytes (uint32_t)
//     N bytes  Raw cell content (UTF-8, no null terminator)
//
// No external libraries are required.  The format is a simple, direct
// binary serialisation of the spreadsheet's non-empty cells.
// =============================================================================
#pragma once

#include "spreadsheet.h"  // Spreadsheet
#include <string>         // std::string

// Write the spreadsheet to a binary file using the SSHEET format.
// Returns true on success, false if the file could not be written.
bool saveODS(const Spreadsheet& sheet, const std::string& path);

// Read a SSHEET binary file and populate the grid.
// Existing cell data is cleared before loading.
// Returns true on success, false on any I/O or format error.
bool loadODS(Spreadsheet& sheet, const std::string& path);
