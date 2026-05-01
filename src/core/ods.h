// =============================================================================
// ods.h  —  OpenDocument Spreadsheet (.ods) persistence for Spreadsheet
//
// Declares two free functions that read and write a Spreadsheet to/from the
// ODS container format.  Implementation is in ods.cpp.
//
// The ODS file is a ZIP STORE (uncompressed) archive — no external libraries
// are required.  LibreOffice and compatible suites can open the saved file.
//
// Limitation: loadODS() only supports STORE-compressed entries.  Files saved
// by LibreOffice use DEFLATE and cannot be loaded here.  Use saveODS() and
// loadODS() as a paired round-trip format.
// =============================================================================
#pragma once

#include "spreadsheet.h"  // Spreadsheet
#include <string>         // std::string

// Write the spreadsheet to an ODS file (ZIP STORE, ODF 1.2 XML).
// Returns true on success, false if the file could not be written.
bool saveODS(const Spreadsheet& sheet, const std::string& path);

// Read an ODS file (ZIP STORE entries only) and populate the grid.
// Existing cell data is cleared before loading.
// Returns true on success, false on any I/O or format error.
bool loadODS(Spreadsheet& sheet, const std::string& path);
