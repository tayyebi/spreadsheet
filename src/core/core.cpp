// =============================================================================
// core.cpp  —  Spreadsheet method implementations
//
// Implements: cell storage, formula-driven DFS evaluation, cycle detection,
// RFC-4180 CSV persistence, and OpenDocument Spreadsheet (.ods) I/O.
// =============================================================================

#include "core.h"     // Spreadsheet, Cell, CellValue declarations
#include "formula.h"  // evaluateFormula() — called for cells whose raw starts with '='
#include <fstream>    // std::ifstream, std::ofstream — file I/O
#include <sstream>    // std::ostringstream — convert double to display string
#include <vector>     // std::vector<uint8_t>  — ZIP byte buffer
#include <array>      // std::array<uint32_t,256> — CRC-32 lookup table

// ---------------------------------------------------------------------------
// E()  —  CSV field escaper (RFC-4180)
//
// RFC-4180 requires that any field containing a comma, double-quote, newline,
// or carriage-return must be wrapped in double-quotes, and any literal
// double-quote inside such a field must be doubled: " → "".
//
// If the string contains none of those characters we return it unchanged
// (the "plain" fast path).  Otherwise we wrap it:
//   hello        → hello
//   3.14         → 3.14
//   hello,world  → "hello,world"
//   say "hi"     → "say ""hi"""
// ---------------------------------------------------------------------------
static std::string E(const std::string& s) {
    // Fast path: no special characters, return as-is
    if (s.find_first_of(",\"\n\r") == s.npos) return s;
    // Slow path: wrap in double-quotes, escaping internal double-quotes
    std::string o = "\"";
    for (char c : s) {
        if (c == '"') o += '"';  // double every quote inside
        o += c;
    }
    return o + '"';
}

// ---------------------------------------------------------------------------
// setCell()  —  write raw text into a cell and reset its computed state
//
// We immediately clear display and value so that stale results from any
// previous content are not accidentally read before the next evaluateAll().
// The actual formula evaluation is deferred to evaluateAll() / evalCell().
// ---------------------------------------------------------------------------
void Spreadsheet::setCell(int r, int c, std::string raw) {
    auto& cell = cells_[key(r, c)];  // creates entry if absent (default Cell{})
    cell.raw     = std::move(raw);   // store exactly what the user typed
    cell.display = cell.raw;         // default display = raw (overwritten on eval)
    cell.value   = std::monostate{}; // mark as not-yet-evaluated
}

// ---------------------------------------------------------------------------
// getCell()  —  look up a cell by position
//
// Returns a pointer into the internal map, or nullptr if the cell was never
// written.  The two overloads (mutable and const) keep the const-correctness
// contract: a const Spreadsheet can only yield const Cell pointers.
// ---------------------------------------------------------------------------
Cell* Spreadsheet::getCell(int r, int c) {
    auto it = cells_.find(key(r, c));
    return it != cells_.end() ? &it->second : nullptr;
}
const Cell* Spreadsheet::getCell(int r, int c) const {
    auto it = cells_.find(key(r, c));
    return it != cells_.end() ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// evalCell()  —  evaluate one cell using depth-first search
//
// Algorithm overview:
//   1. If the cell is already in `done`, skip it (memoisation).
//   2. If the cell has no raw content, skip it.
//   3. If the raw content is not a formula (doesn't start with '='),
//      try to parse it as a number; otherwise treat it as text.
//   4. If the raw content IS a formula:
//      a. If the cell is already in `vis` (on the current DFS stack),
//         we have a circular reference → mark as "#CYCLE!" and return.
//      b. Push the cell onto `vis`, call evaluateFormula() which will
//         recursively call evalCell() for any cells the formula references,
//         then pop the cell from `vis`.
//      c. Store the formula result as display and value.
//   5. Mark the cell as done to prevent redundant re-evaluation.
// ---------------------------------------------------------------------------
void Spreadsheet::evalCell(int r, int c,
                           std::set<uint64_t>& vis,
                           std::set<uint64_t>& done) {
    auto k = key(r, c);

    // Memoisation: if this cell was already fully evaluated, do nothing.
    if (done.count(k)) return;

    // Absent cell (never written): nothing to evaluate.
    auto it = cells_.find(k);
    if (it == cells_.end()) return;

    auto& cell = it->second;

    // ------------------------------------------------------------------
    // Plain value path: not a formula
    // ------------------------------------------------------------------
    // Cells whose raw string is empty or does not begin with '=' hold
    // either a numeric literal or arbitrary text.
    if (cell.raw.empty() || cell.raw[0] != '=') {
        try {
            size_t consumed = 0;
            double v = std::stod(cell.raw, &consumed);
            if (consumed == cell.raw.size()) {
                // The entire string parsed as a number — store as double.
                cell.value   = v;
                cell.display = cell.raw;
            } else {
                // Partial parse or non-numeric text — store as string.
                cell.value   = cell.raw;
                cell.display = cell.raw;
            }
        } catch (...) {
            // stod threw (e.g. empty string, letters) — treat as plain text.
            cell.value   = cell.raw;
            cell.display = cell.raw;
        }
        done.insert(k);  // mark fully evaluated
        return;
    }

    // ------------------------------------------------------------------
    // Cycle detection
    // ------------------------------------------------------------------
    // `vis` is the set of cells on the current DFS call stack.  If we are
    // asked to evaluate a cell that is already in `vis`, we have found a
    // circular reference (A→B→A or longer chain).
    if (vis.count(k)) {
        cell.display = "#CYCLE!";
        cell.value   = std::string("#CYCLE!");
        done.insert(k);
        return;
    }

    // ------------------------------------------------------------------
    // Formula evaluation path
    // ------------------------------------------------------------------
    vis.insert(k);  // push: mark this cell as currently being evaluated

    // Strip the leading '=' and pass the expression to the formula engine.
    // evaluateFormula() may call back into evalCell() for referenced cells.
    auto res = evaluateFormula(cell.raw.substr(1), *this, vis, done);

    vis.erase(k);   // pop: we are done descending from this cell

    // Store the result depending on whether it is a number or an error string.
    if (std::holds_alternative<std::string>(res)) {
        auto e       = std::get<std::string>(res);
        cell.display = e;
        cell.value   = e;
    } else {
        double v    = std::get<double>(res);
        cell.value  = v;
        std::ostringstream os;
        os << v;
        cell.display = os.str();  // e.g. "42", "3.14159"
    }

    done.insert(k);  // mark fully evaluated
}

// ---------------------------------------------------------------------------
// evaluateAll()  —  (re-)evaluate every cell in the grid
//
// Creates fresh `vis` and `done` sets for the entire pass, then calls
// evalCell() for each cell that exists in the sparse map.  Because evalCell()
// checks `done` first, each cell is processed exactly once regardless of the
// iteration order.
// ---------------------------------------------------------------------------
void Spreadsheet::evaluateAll() {
    std::set<uint64_t> vis, done;
    for (auto& [k, _] : cells_)
        evalCell(int(k >> 32), int(k & 0xFFFFFFFFu), vis, done);
}

// ---------------------------------------------------------------------------
// saveCSV()  —  write the grid to a CSV file
//
// Writes ROWS lines, each containing COLS comma-separated fields holding
// the raw cell strings (not the evaluated display values).  This means
// formulas like "=SUM(A1:A3)" are saved as-is and re-evaluated on load.
// Empty cells produce an empty field ("") in the CSV.
// ---------------------------------------------------------------------------
bool Spreadsheet::saveCSV(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;  // could not create / open file

    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            if (c > 0) f << ',';  // separate fields with commas
            auto it = cells_.find(key(r, c));
            if (it != cells_.end())
                f << E(it->second.raw);  // RFC-4180 escape
            // empty cells produce an empty field (nothing is written)
        }
        f << '\n';  // one row per line
    }
    return true;
}

// ---------------------------------------------------------------------------
// loadCSV()  —  read a CSV file and populate the grid
//
// Parses RFC-4180 format: quoted fields (with "" for embedded quotes) and
// plain (unquoted) fields.  Only cells with non-empty content are stored.
// ---------------------------------------------------------------------------
bool Spreadsheet::loadCSV(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;  // file not found or not readable

    cells_.clear();  // discard all existing cell data before loading

    std::string line;
    int r = 0;
    while (std::getline(f, line) && r < ROWS) {
        int    c = 0;
        size_t i = 0;

        // Parse each comma-separated field on this line.
        while (i <= line.size() && c < COLS) {
            std::string fld;

            if (i < line.size() && line[i] == '"') {
                // Quoted field: skip opening quote, then consume until the
                // matching closing quote, doubling "" into a single ".
                ++i;
                while (i < line.size()) {
                    if (line[i] == '"') {
                        ++i;
                        if (i < line.size() && line[i] == '"') {
                            fld += '"';  // escaped quote pair "" → "
                            ++i;
                        } else {
                            break;       // closing quote
                        }
                    } else {
                        fld += line[i++];
                    }
                }
                if (i < line.size() && line[i] == ',') ++i;  // skip separator
            } else {
                // Plain (unquoted) field: read up to the next comma.
                auto e = line.find(',', i);
                if (e == line.npos) e = line.size();
                fld = line.substr(i, e - i);
                i   = e + 1;
            }

            if (!fld.empty()) setCell(r, c, fld);  // only store non-empty cells
            ++c;
        }
        ++r;
    }
    return true;
}

// =============================================================================
// ODS I/O — ZIP-STORE based OpenDocument Spreadsheet read/write
//
// The ODS file format is a ZIP archive containing XML files.  We generate
// and consume the three files required by the ODF 1.2 specification:
//   mimetype            — MIME type string, must be the first ZIP entry
//   META-INF/manifest.xml — lists all entries and their MIME types
//   content.xml         — the actual spreadsheet data
//
// We use the ZIP STORE method (compression method 0) throughout, which
// requires no external libraries.  LibreOffice and compatible suites accept
// uncompressed ODS files without issues.
//
// For loading, only STORE-compressed entries are supported.  ODS files saved
// by LibreOffice use DEFLATE (method 8) and cannot be loaded with this
// implementation — use saveODS() / loadODS() as a round-trip format.
// =============================================================================

// ---------------------------------------------------------------------------
// crc32zip()  —  CRC-32 checksum (IEEE 802.3 polynomial, as used by ZIP)
//
// Builds the 256-entry lookup table once (function-local static, initialised
// thread-safely in C++11 and later) and then folds each byte into the CRC.
// ---------------------------------------------------------------------------
static uint32_t crc32zip(const uint8_t* data, size_t len) {
    // C++11 guarantees that function-local static initialisation is performed
    // exactly once even in the presence of multiple threads.
    static const auto tbl = []() {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = tbl[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// ZIP-STORE writer helpers
//
// wLE16 / wLE32  —  append a 16- or 32-bit integer in little-endian order
//                   to the given byte vector.
// ---------------------------------------------------------------------------
static void wLE16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(uint8_t(x));
    v.push_back(uint8_t(x >> 8));
}
static void wLE32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x));
    v.push_back(uint8_t(x >>  8));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 24));
}

// ---------------------------------------------------------------------------
// buildZip()  —  assemble a valid ZIP archive using STORE (no compression)
//
// Each entry is written as a local file header followed immediately by the
// raw file data.  After all entries, the central directory and the end-of-
// central-directory record are appended.
//
// Per the ODF 1.2 specification the `mimetype` entry MUST be:
//   • the first entry in the archive
//   • stored without compression (STORE / method 0)
//   • not use a data descriptor (sizes and CRC are in the local header)
// All three conditions are satisfied by this implementation for every entry.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> buildZip(
        const std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries) {

    std::vector<uint8_t>  zip;
    std::vector<uint32_t> localOffsets;  // byte offset of each local file header

    // Write local file headers + data.
    for (const auto& [name, data] : entries) {
        localOffsets.push_back(static_cast<uint32_t>(zip.size()));

        uint32_t crc = crc32zip(data.data(), data.size());
        uint32_t sz  = static_cast<uint32_t>(data.size());

        wLE32(zip, 0x04034B50u);                          // local file header signature
        wLE16(zip, 20);                                   // version needed (2.0)
        wLE16(zip, 0);                                    // general purpose bit flags
        wLE16(zip, 0);                                    // compression: STORE
        wLE16(zip, 0);                                    // last mod file time
        wLE16(zip, 0);                                    // last mod file date
        wLE32(zip, crc);                                  // CRC-32
        wLE32(zip, sz);                                   // compressed size
        wLE32(zip, sz);                                   // uncompressed size
        wLE16(zip, static_cast<uint16_t>(name.size()));   // file name length
        wLE16(zip, 0);                                    // extra field length
        for (char c : name) zip.push_back(static_cast<uint8_t>(c));
        zip.insert(zip.end(), data.begin(), data.end());
    }

    // Central directory.
    uint32_t cdStart = static_cast<uint32_t>(zip.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& [name, data] = entries[i];
        uint32_t crc = crc32zip(data.data(), data.size());
        uint32_t sz  = static_cast<uint32_t>(data.size());

        wLE32(zip, 0x02014B50u);                          // central dir file header sig
        wLE16(zip, 20);                                   // version made by
        wLE16(zip, 20);                                   // version needed
        wLE16(zip, 0);                                    // general purpose bit flags
        wLE16(zip, 0);                                    // compression: STORE
        wLE16(zip, 0);                                    // last mod file time
        wLE16(zip, 0);                                    // last mod file date
        wLE32(zip, crc);                                  // CRC-32
        wLE32(zip, sz);                                   // compressed size
        wLE32(zip, sz);                                   // uncompressed size
        wLE16(zip, static_cast<uint16_t>(name.size()));   // file name length
        wLE16(zip, 0);                                    // extra field length
        wLE16(zip, 0);                                    // file comment length
        wLE16(zip, 0);                                    // disk number start
        wLE16(zip, 0);                                    // internal file attributes
        wLE32(zip, 0);                                    // external file attributes
        wLE32(zip, localOffsets[i]);                      // relative offset of local hdr
        for (char c : name) zip.push_back(static_cast<uint8_t>(c));
    }

    // End of central directory record.
    uint32_t cdSize   = static_cast<uint32_t>(zip.size()) - cdStart;
    uint16_t nEntries = static_cast<uint16_t>(entries.size());

    wLE32(zip, 0x06054B50u);  // end of central directory signature
    wLE16(zip, 0);            // disk number
    wLE16(zip, 0);            // disk with start of central directory
    wLE16(zip, nEntries);     // entries on this disk
    wLE16(zip, nEntries);     // total entries
    wLE32(zip, cdSize);       // size of central directory
    wLE32(zip, cdStart);      // offset of start of central directory
    wLE16(zip, 0);            // ZIP file comment length

    return zip;
}

// ---------------------------------------------------------------------------
// xmlEsc()  —  escape the XML characters that must be encoded in text content
//              and double-quoted attribute values
//   &  →  &amp;   <  →  &lt;   >  →  &gt;   "  →  &quot;
// ---------------------------------------------------------------------------
static std::string xmlEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;";  break;
            case '<': out += "&lt;";   break;
            case '>': out += "&gt;";   break;
            case '"': out += "&quot;"; break;
            default:  out += c;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// xmlUnesc()  —  reverse the five standard XML character-entity references
// ---------------------------------------------------------------------------
static std::string xmlUnesc(std::string s) {
    struct Rep { const char* from; size_t flen; const char* to; };
    static const Rep reps[] = {
        {"&amp;",  5, "&"},
        {"&lt;",   4, "<"},
        {"&gt;",   4, ">"},
        {"&quot;", 6, "\""},
        {"&apos;", 6, "'"},
    };
    for (const auto& rep : reps) {
        std::string out;
        size_t prev = 0, pos;
        while ((pos = s.find(rep.from, prev)) != std::string::npos) {
            out.append(s, prev, pos - prev);
            out += rep.to;
            prev = pos + rep.flen;
        }
        out.append(s, prev);
        s = std::move(out);
    }
    return s;
}

// ---------------------------------------------------------------------------
// makeContentXML()  —  generate ODS content.xml for the given spreadsheet
//
// Cell content rules:
//   • Numeric literals → office:value-type="float" with office:value attribute
//   • All other text (formulas, strings) → office:value-type="string"
//   • In both cases the raw cell string is stored verbatim in <text:p> so
//     our own loadODS() can restore it exactly, including the leading '='
//     for formula cells.
//   • Empty cells use a self-closing <table:table-cell/> tag.
// ---------------------------------------------------------------------------
static std::string makeContentXML(const Spreadsheet& sheet) {
    std::string x;
    x += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    x += "<office:document-content"
         " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
         " xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\""
         " xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\""
         " office:version=\"1.2\">\n";
    x += "<office:body><office:spreadsheet>\n";
    x += "<table:table table:name=\"Sheet1\">\n";

    for (int r = 0; r < Spreadsheet::ROWS; ++r) {
        x += "<table:table-row>\n";
        for (int c = 0; c < Spreadsheet::COLS; ++c) {
            const Cell* cell = sheet.getCell(r, c);
            if (!cell || cell->raw.empty()) {
                x += "<table:table-cell/>\n";
                continue;
            }
            // Determine whether the raw string is a plain numeric literal.
            bool isNumeric = false;
            if (cell->raw[0] != '=') {
                try {
                    size_t consumed = 0;
                    std::stod(cell->raw, &consumed);
                    isNumeric = (consumed == cell->raw.size());
                } catch (...) {}
            }
            if (isNumeric) {
                x += "<table:table-cell"
                     " office:value-type=\"float\""
                     " office:value=\"" + xmlEsc(cell->raw) + "\">"
                     "<text:p>" + xmlEsc(cell->raw) + "</text:p>"
                     "</table:table-cell>\n";
            } else {
                x += "<table:table-cell"
                     " office:value-type=\"string\">"
                     "<text:p>" + xmlEsc(cell->raw) + "</text:p>"
                     "</table:table-cell>\n";
            }
        }
        x += "</table:table-row>\n";
    }

    x += "</table:table>\n"
         "</office:spreadsheet></office:body>\n"
         "</office:document-content>\n";
    return x;
}

// ---------------------------------------------------------------------------
// extractZipEntry()  —  find and return the raw bytes of a named ZIP entry
//
// Algorithm:
//   1. Locate the End of Central Directory (EOCD) record by scanning
//      backwards from the end of the file for the 4-byte EOCD signature.
//   2. Walk the central directory to find the entry whose name matches
//      `target`.
//   3. From the matching central directory record, read the offset of the
//      corresponding local file header.
//   4. Skip over the local file header and variable-length fields, then
//      read `sz` bytes of file data.
//
// Only STORE (compression method 0) entries are supported.  Returns an
// empty string on any error (file not found, wrong compression, etc.).
// ---------------------------------------------------------------------------
static std::string extractZipEntry(const std::vector<uint8_t>& zip,
                                   const std::string& target) {
    if (zip.size() < 22) return {};

    // Little-endian read helpers (local lambdas).
    auto r16 = [&](size_t off) -> uint16_t {
        return uint16_t(zip[off]) | uint16_t(uint16_t(zip[off + 1]) << 8);
    };
    auto r32 = [&](size_t off) -> uint32_t {
        return uint32_t(zip[off])
             | uint32_t(uint32_t(zip[off + 1]) <<  8)
             | uint32_t(uint32_t(zip[off + 2]) << 16)
             | uint32_t(uint32_t(zip[off + 3]) << 24);
    };

    // Locate the EOCD signature (0x06054B50) by scanning backwards.
    // The EOCD is at minimum 22 bytes, so start at size-22.
    int64_t eocd = -1;
    for (int64_t i = static_cast<int64_t>(zip.size()) - 22; i >= 0; --i) {
        if (r32(static_cast<size_t>(i)) == 0x06054B50u) { eocd = i; break; }
    }
    if (eocd < 0) return {};

    uint32_t cdOff    = r32(static_cast<size_t>(eocd) + 16);
    uint16_t nEntries = r16(static_cast<size_t>(eocd) +  8);

    // Walk the central directory.
    size_t pos = cdOff;
    for (uint16_t i = 0; i < nEntries; ++i) {
        if (pos + 46 > zip.size() || r32(pos) != 0x02014B50u) break;

        uint16_t method = r16(pos + 10);  // compression method
        uint32_t sz     = r32(pos + 24);  // compressed size
        uint16_t nlen   = r16(pos + 28);  // file name length
        uint16_t elen   = r16(pos + 30);  // extra field length
        uint16_t clen   = r16(pos + 32);  // file comment length
        uint32_t loff   = r32(pos + 42);  // local header offset

        std::string name(reinterpret_cast<const char*>(zip.data() + pos + 46), nlen);
        pos += 46 + nlen + elen + clen;

        if (name != target) continue;
        if (method != 0) return {};  // only STORE is supported

        // Read data from the local file header.
        if (loff + 30 > zip.size()) return {};
        uint16_t lnlen = r16(loff + 26);  // local file name length
        uint16_t lelen = r16(loff + 28);  // local extra field length
        size_t   doff  = loff + 30 + lnlen + lelen;

        if (doff + sz > zip.size()) return {};
        return std::string(reinterpret_cast<const char*>(zip.data() + doff), sz);
    }
    return {};
}

// ---------------------------------------------------------------------------
// parseODSContent()  —  minimal ODS content.xml parser
//
// Reads cell data from the first <table:table> element.  For each
// <table:table-row>, iterates over <table:table-cell> children and calls
// setCell() with the text found inside <text:p>.
//
// Handled attributes:
//   table:number-columns-repeated — advance col by N for repeated cells
//   table:number-rows-repeated    — advance row by N for repeated rows
//
// Only STORE-level ODS XML is expected here (already extracted from ZIP).
// ---------------------------------------------------------------------------
static bool parseODSContent(const std::string& xml, Spreadsheet& sheet) {
    // Helper: extract the value of a named XML attribute from a tag string.
    auto attrVal = [](const std::string& tag, const std::string& attr) -> std::string {
        std::string needle = attr + "=\"";
        auto pos = tag.find(needle);
        if (pos == std::string::npos) return {};
        pos += needle.size();
        auto end = tag.find('"', pos);
        if (end == std::string::npos) return {};
        return tag.substr(pos, end - pos);
    };

    // Advance past the opening <table:table ...> tag.
    size_t tablePos = xml.find("<table:table ");
    if (tablePos == std::string::npos) tablePos = xml.find("<table:table>");
    if (tablePos == std::string::npos) return false;

    int row = 0;
    size_t pos = tablePos;

    while (row < Spreadsheet::ROWS) {
        // Find next <table:table-row or </table:table (end of table).
        auto rowStart = xml.find("<table:table-row", pos);
        auto tableEnd = xml.find("</table:table>", pos);
        if (rowStart == std::string::npos || rowStart > tableEnd) break;

        // Grab the row opening tag to check for number-rows-repeated.
        auto rowTagEnd = xml.find('>', rowStart);
        if (rowTagEnd == std::string::npos) break;
        std::string rowTag = xml.substr(rowStart, rowTagEnd - rowStart + 1);

        // Determine row repeat count.
        int rowRepeat = 1;
        std::string rr = attrVal(rowTag, "table:number-rows-repeated");
        if (!rr.empty()) {
            try { rowRepeat = std::stoi(rr); } catch (...) { rowRepeat = 1; }
        }

        // Self-closing row tag (<table:table-row .../>) — skip entirely.
        bool rowSelfClose = rowTag.size() >= 2 && rowTag[rowTag.size() - 2] == '/';
        if (rowSelfClose) {
            row += rowRepeat;
            pos = rowTagEnd + 1;
            continue;
        }

        auto rowEnd = xml.find("</table:table-row>", rowTagEnd);
        if (rowEnd == std::string::npos) break;

        // Parse cells within this row.
        int col = 0;
        size_t cpos = rowTagEnd + 1;

        while (col < Spreadsheet::COLS) {
            auto cellStart = xml.find("<table:table-cell", cpos);
            if (cellStart == std::string::npos || cellStart >= rowEnd) break;

            auto cellTagEnd = xml.find('>', cellStart);
            if (cellTagEnd == std::string::npos || cellTagEnd > rowEnd) break;

            std::string cellTag = xml.substr(cellStart, cellTagEnd - cellStart + 1);

            // Column repeat count.
            int colRepeat = 1;
            std::string cr = attrVal(cellTag, "table:number-columns-repeated");
            if (!cr.empty()) {
                try { colRepeat = std::stoi(cr); } catch (...) { colRepeat = 1; }
            }

            bool selfClose = cellTag.size() >= 2 && cellTag[cellTag.size() - 2] == '/';
            if (selfClose) {
                // Empty cell (or run of empty cells) — skip without storing.
                col  += colRepeat;
                cpos  = cellTagEnd + 1;
                continue;
            }

            // Find closing tag for this cell.
            auto cellEnd = xml.find("</table:table-cell>", cellTagEnd);
            if (cellEnd == std::string::npos || cellEnd > rowEnd) break;

            // Extract the first <text:p> content.
            auto tpOpen = xml.find("<text:p>", cellTagEnd + 1);
            if (tpOpen != std::string::npos && tpOpen < cellEnd) {
                auto tpClose = xml.find("</text:p>", tpOpen + 8);
                if (tpClose != std::string::npos && tpClose <= cellEnd) {
                    std::string text = xml.substr(tpOpen + 8, tpClose - tpOpen - 8);
                    text = xmlUnesc(text);
                    if (!text.empty() && row < Spreadsheet::ROWS && col < Spreadsheet::COLS)
                        sheet.setCell(row, col, text);
                }
            }

            col  += colRepeat;
            cpos  = cellEnd + 19;  // 19 == strlen("</table:table-cell>")
        }

        row += rowRepeat;
        pos  = rowEnd + 18;  // 18 == strlen("</table:table-row>")
    }
    return true;
}

// ---------------------------------------------------------------------------
// Spreadsheet::saveODS()  —  write the grid to an ODS file
// ---------------------------------------------------------------------------
bool Spreadsheet::saveODS(const std::string& path) const {
    // Per ODF 1.2 §17.7 the mimetype file must be stored without compression
    // and without an extra field, and must be the FIRST entry in the archive.
    static const std::string kMime =
        "application/vnd.oasis.opendocument.spreadsheet";

    static const std::string kManifest =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest:manifest"
        " xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\""
        " manifest:version=\"1.2\">\n"
        " <manifest:file-entry"
        "  manifest:full-path=\"/\""
        "  manifest:version=\"1.2\""
        "  manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>\n"
        " <manifest:file-entry"
        "  manifest:full-path=\"content.xml\""
        "  manifest:media-type=\"text/xml\"/>\n"
        "</manifest:manifest>\n";

    std::string content = makeContentXML(*this);

    // Convert string → byte vector helper.
    auto toBytes = [](const std::string& s) {
        return std::vector<uint8_t>(s.begin(), s.end());
    };

    std::vector<std::pair<std::string, std::vector<uint8_t>>> entries = {
        {"mimetype",              toBytes(kMime)},
        {"META-INF/manifest.xml", toBytes(kManifest)},
        {"content.xml",           toBytes(content)},
    };

    auto zip = buildZip(entries);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(zip.data()),
            static_cast<std::streamsize>(zip.size()));
    return f.good();
}

// ---------------------------------------------------------------------------
// Spreadsheet::loadODS()  —  read the grid from an ODS file
// ---------------------------------------------------------------------------
bool Spreadsheet::loadODS(const std::string& path) {
    // Read the whole file into memory.
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>());
    f.close();

    // Extract the content.xml entry from the ZIP.
    std::string content = extractZipEntry(data, "content.xml");
    if (content.empty()) return false;

    // Populate cells from the XML.
    cells_.clear();
    return parseODSContent(content, *this);
}

