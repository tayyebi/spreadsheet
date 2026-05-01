// =============================================================================
// csv.h  —  CSV (RFC-4180) persistence for Spreadsheet
//
// Declares two free functions that read and write a Spreadsheet to/from a
// comma-separated values file.  Implementation is in csv.cpp.
//
// The raw cell strings are preserved verbatim (including formula text such as
// "=SUM(A1:A3)") so the file is a faithful snapshot of user input.
// Re-evaluate the sheet after loadCSV() to recompute all formula values.
// =============================================================================
#pragma once

#include "spreadsheet.h"  // Spreadsheet
#include <string>         // std::string

// Write every cell's raw string to a CSV file following RFC-4180 rules.
// Fields containing commas, quotes, or newlines are quoted automatically.
// Returns true on success, false if the file could not be opened for writing.
bool saveCSV(const Spreadsheet& sheet, const std::string& path);

// Read a CSV file and populate the grid; existing cell data is cleared first.
// Parses quoted fields (with "" for embedded quotes) and plain fields.
// Returns true on success, false if the file could not be opened for reading.
bool loadCSV(Spreadsheet& sheet, const std::string& path);
