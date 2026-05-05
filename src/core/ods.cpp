// =============================================================================
// ods.cpp  —  Custom binary spreadsheet persistence (no XML, no ZIP)
//
// Implements saveODS() and loadODS() using a simple dependency-free binary
// format called SSHEET.
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
// No external libraries, no XML, no ZIP — only standard C++ I/O.
// =============================================================================

#include "ods.h"    // saveODS, loadODS declarations
#include <fstream>  // std::ifstream, std::ofstream
#include <vector>   // std::vector
#include <cstring>  // std::memcmp

// ---------------------------------------------------------------------------
// Helper: write a uint32_t in little-endian byte order
// ---------------------------------------------------------------------------
static void writeLE32(std::ofstream& f, uint32_t v) {
    const char bytes[4] = {
        char(v & 0xFF),
        char((v >>  8) & 0xFF),
        char((v >> 16) & 0xFF),
        char((v >> 24) & 0xFF)
    };
    f.write(bytes, 4);
}

// ---------------------------------------------------------------------------
// Helper: read a uint32_t in little-endian byte order.
// Returns false if fewer than 4 bytes are available.
// ---------------------------------------------------------------------------
static bool readLE32(std::ifstream& f, uint32_t& v) {
    unsigned char bytes[4];
    if (!f.read(reinterpret_cast<char*>(bytes), 4)) return false;
    v = uint32_t(bytes[0])
      | uint32_t(uint32_t(bytes[1]) <<  8)
      | uint32_t(uint32_t(bytes[2]) << 16)
      | uint32_t(uint32_t(bytes[3]) << 24);
    return true;
}

// ---------------------------------------------------------------------------
// saveODS()  —  write non-empty cells to a SSHEET binary file
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// File magic: 6 ASCII bytes + 1-byte major version + 1-byte minor version.
// Using an explicit array avoids any ambiguity with string-literal null
// terminators when writing/comparing exactly 8 bytes.
// ---------------------------------------------------------------------------
static constexpr char kMagic[8] = {'S','S','H','E','E','T', 0x01, 0x00};

bool saveODS(const Spreadsheet& sheet, const std::string& path) {
    // Collect non-empty cells first so we know the count before writing.
    struct CellRecord {
        uint32_t    row;
        uint32_t    col;
        std::string content;
    };
    std::vector<CellRecord> cells;
    cells.reserve(64);

    sheet.forEachCell([&](int r, int c, const Cell& cell) {
        if (!cell.raw.empty())
            cells.push_back({
                static_cast<uint32_t>(r),
                static_cast<uint32_t>(c),
                cell.raw
            });
    });

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // Magic + version (8 bytes, explicit array — no reliance on null terminator)
    f.write(kMagic, 8);

    // Cell count
    writeLE32(f, static_cast<uint32_t>(cells.size()));

    // Cell records
    for (const auto& rec : cells) {
        writeLE32(f, rec.row);
        writeLE32(f, rec.col);
        writeLE32(f, static_cast<uint32_t>(rec.content.size()));
        f.write(rec.content.data(), static_cast<std::streamsize>(rec.content.size()));
    }

    return f.good();
}

// ---------------------------------------------------------------------------
// loadODS()  —  read a SSHEET binary file and populate the spreadsheet
// ---------------------------------------------------------------------------
bool loadODS(Spreadsheet& sheet, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Verify magic + version (8 bytes, explicit comparison against kMagic)
    char header[8];
    if (!f.read(header, 8)) return false;
    if (std::memcmp(header, kMagic, 8) != 0) return false;

    // Read cell count
    uint32_t count = 0;
    if (!readLE32(f, count)) return false;

    // Guard against malformed/huge counts
    if (count > 16000000u) return false;  // 16 M cells is a reasonable sanity limit

    sheet.clear();

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t row = 0, col = 0, len = 0;
        if (!readLE32(f, row)) return false;
        if (!readLE32(f, col)) return false;
        if (!readLE32(f, len)) return false;

        // Bounds check against the maximum addressable grid
        if (row >= static_cast<uint32_t>(Spreadsheet::MAX_ROWS) ||
            col >= static_cast<uint32_t>(Spreadsheet::MAX_COLS))
            return false;

        // Guard against absurdly large content lengths (e.g. corrupted file)
        if (len > 65536) return false;

        std::string content(len, '\0');
        if (len > 0 && !f.read(content.data(), static_cast<std::streamsize>(len)))
            return false;

        sheet.setCell(static_cast<int>(row), static_cast<int>(col), content);
    }

    return true;
}
