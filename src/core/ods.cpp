// =============================================================================
// ods.cpp  —  OpenDocument Spreadsheet (.ods) persistence for Spreadsheet
//
// Implements: saveODS() and loadODS().
//
// The ODS file format is a ZIP archive containing XML files.  This file
// generates and parses the three entries required by the ODF 1.2 spec:
//   mimetype              — MIME type string, must be the first ZIP entry
//   META-INF/manifest.xml — lists all entries and their MIME types
//   content.xml           — the actual spreadsheet data in ODF XML
//
// ZIP STORE (compression method 0) is used throughout so no external library
// is required.  LibreOffice and compatible suites accept uncompressed ODS.
//
// Limitation: only STORE-compressed entries can be loaded.  ODS files saved
// by LibreOffice use DEFLATE and cannot be read back by loadODS().
// Use saveODS() / loadODS() as a self-contained round-trip format.
// =============================================================================

#include "spreadsheet.h"  // Spreadsheet, Cell
#include <fstream>        // std::ifstream, std::ofstream
#include <vector>         // std::vector<uint8_t>  — ZIP byte buffer
#include <array>          // std::array<uint32_t,256> — CRC-32 lookup table

// ---------------------------------------------------------------------------
// crc32zip()  —  CRC-32 checksum (IEEE 802.3 polynomial, as used by ZIP)
//
// Builds the 256-entry lookup table once (function-local static, thread-safe
// in C++11) and then folds each byte into the running CRC.
// ---------------------------------------------------------------------------
static uint32_t crc32zip(const uint8_t* data, size_t len) {
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
// wLE16 / wLE32  —  append a 16- or 32-bit integer in little-endian order
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
// Per the ODF 1.2 specification the `mimetype` entry MUST be the first entry,
// stored without compression, and without a data descriptor.  All conditions
// are satisfied here for every entry.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> buildZip(
        const std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries) {

    std::vector<uint8_t>  zip;
    std::vector<uint32_t> localOffsets;

    // Local file headers + data.
    for (const auto& [name, data] : entries) {
        localOffsets.push_back(static_cast<uint32_t>(zip.size()));

        uint32_t crc = crc32zip(data.data(), data.size());
        uint32_t sz  = static_cast<uint32_t>(data.size());

        wLE32(zip, 0x04034B50u);
        wLE16(zip, 20);   // version needed
        wLE16(zip, 0);    // flags
        wLE16(zip, 0);    // compression: STORE
        wLE16(zip, 0);    // last mod time
        wLE16(zip, 0);    // last mod date
        wLE32(zip, crc);
        wLE32(zip, sz);   // compressed size
        wLE32(zip, sz);   // uncompressed size
        wLE16(zip, static_cast<uint16_t>(name.size()));
        wLE16(zip, 0);    // extra field length
        for (char c : name) zip.push_back(static_cast<uint8_t>(c));
        zip.insert(zip.end(), data.begin(), data.end());
    }

    // Central directory.
    uint32_t cdStart = static_cast<uint32_t>(zip.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& [name, data] = entries[i];
        uint32_t crc = crc32zip(data.data(), data.size());
        uint32_t sz  = static_cast<uint32_t>(data.size());

        wLE32(zip, 0x02014B50u);
        wLE16(zip, 20);   // version made by
        wLE16(zip, 20);   // version needed
        wLE16(zip, 0);    // flags
        wLE16(zip, 0);    // compression: STORE
        wLE16(zip, 0);    // last mod time
        wLE16(zip, 0);    // last mod date
        wLE32(zip, crc);
        wLE32(zip, sz);   // compressed size
        wLE32(zip, sz);   // uncompressed size
        wLE16(zip, static_cast<uint16_t>(name.size()));
        wLE16(zip, 0);    // extra field length
        wLE16(zip, 0);    // file comment length
        wLE16(zip, 0);    // disk number start
        wLE16(zip, 0);    // internal attributes
        wLE32(zip, 0);    // external attributes
        wLE32(zip, localOffsets[i]);
        for (char c : name) zip.push_back(static_cast<uint8_t>(c));
    }

    // End of central directory record.
    uint32_t cdSize   = static_cast<uint32_t>(zip.size()) - cdStart;
    uint16_t nEntries = static_cast<uint16_t>(entries.size());

    wLE32(zip, 0x06054B50u);  // EOCD signature
    wLE16(zip, 0);
    wLE16(zip, 0);
    wLE16(zip, nEntries);
    wLE16(zip, nEntries);
    wLE32(zip, cdSize);
    wLE32(zip, cdStart);
    wLE16(zip, 0);  // comment length

    return zip;
}

// ---------------------------------------------------------------------------
// xmlEsc()  —  escape characters that must be encoded in XML text content
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
// xmlUnesc()  —  reverse the standard XML character-entity references
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
// Cell encoding rules:
//   • Numeric literals  → office:value-type="float" with office:value attr
//   • Formulas/strings  → office:value-type="string"
//   • In both cases the raw string is stored in <text:p> for round-trip fidelity
//   • Empty cells use a self-closing <table:table-cell/> tag
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
// Only STORE (compression method 0) entries are supported.
// Returns an empty string on any error.
// ---------------------------------------------------------------------------
static std::string extractZipEntry(const std::vector<uint8_t>& zip,
                                   const std::string& target) {
    if (zip.size() < 22) return {};

    auto r16 = [&](size_t off) -> uint16_t {
        return uint16_t(zip[off]) | uint16_t(uint16_t(zip[off + 1]) << 8);
    };
    auto r32 = [&](size_t off) -> uint32_t {
        return uint32_t(zip[off])
             | uint32_t(uint32_t(zip[off + 1]) <<  8)
             | uint32_t(uint32_t(zip[off + 2]) << 16)
             | uint32_t(uint32_t(zip[off + 3]) << 24);
    };

    // Locate End of Central Directory signature by scanning backwards.
    int64_t eocd = -1;
    for (int64_t i = static_cast<int64_t>(zip.size()) - 22; i >= 0; --i) {
        if (r32(static_cast<size_t>(i)) == 0x06054B50u) { eocd = i; break; }
    }
    if (eocd < 0) return {};

    uint32_t cdOff    = r32(static_cast<size_t>(eocd) + 16);
    uint16_t nEntries = r16(static_cast<size_t>(eocd) +  8);

    size_t pos = cdOff;
    for (uint16_t i = 0; i < nEntries; ++i) {
        if (pos + 46 > zip.size() || r32(pos) != 0x02014B50u) break;

        uint16_t method = r16(pos + 10);
        uint32_t sz     = r32(pos + 24);
        uint16_t nlen   = r16(pos + 28);
        uint16_t elen   = r16(pos + 30);
        uint16_t clen   = r16(pos + 32);
        uint32_t loff   = r32(pos + 42);

        std::string name(reinterpret_cast<const char*>(zip.data() + pos + 46), nlen);
        pos += 46 + nlen + elen + clen;

        if (name != target) continue;
        if (method != 0) return {};  // only STORE is supported

        if (loff + 30 > zip.size()) return {};
        uint16_t lnlen = r16(loff + 26);
        uint16_t lelen = r16(loff + 28);
        size_t   doff  = loff + 30 + lnlen + lelen;

        if (doff + sz > zip.size()) return {};
        return std::string(reinterpret_cast<const char*>(zip.data() + doff), sz);
    }
    return {};
}

// ---------------------------------------------------------------------------
// parseODSContent()  —  minimal ODS content.xml parser
//
// Reads cell data from the first <table:table> element.  Handles:
//   table:number-columns-repeated — advance col by N for repeated cells
//   table:number-rows-repeated    — advance row by N for repeated rows
// ---------------------------------------------------------------------------
static bool parseODSContent(const std::string& xml, Spreadsheet& sheet) {
    auto attrVal = [](const std::string& tag, const std::string& attr) -> std::string {
        std::string needle = attr + "=\"";
        auto pos = tag.find(needle);
        if (pos == std::string::npos) return {};
        pos += needle.size();
        auto end = tag.find('"', pos);
        if (end == std::string::npos) return {};
        return tag.substr(pos, end - pos);
    };

    size_t tablePos = xml.find("<table:table ");
    if (tablePos == std::string::npos) tablePos = xml.find("<table:table>");
    if (tablePos == std::string::npos) return false;

    int row = 0;
    size_t pos = tablePos;

    while (row < Spreadsheet::ROWS) {
        auto rowStart = xml.find("<table:table-row", pos);
        auto tableEnd = xml.find("</table:table>", pos);
        if (rowStart == std::string::npos || rowStart > tableEnd) break;

        auto rowTagEnd = xml.find('>', rowStart);
        if (rowTagEnd == std::string::npos) break;
        std::string rowTag = xml.substr(rowStart, rowTagEnd - rowStart + 1);

        int rowRepeat = 1;
        std::string rr = attrVal(rowTag, "table:number-rows-repeated");
        if (!rr.empty()) {
            try { rowRepeat = std::stoi(rr); } catch (...) { rowRepeat = 1; }
        }

        bool rowSelfClose = rowTag.size() >= 2 && rowTag[rowTag.size() - 2] == '/';
        if (rowSelfClose) {
            row += rowRepeat;
            pos = rowTagEnd + 1;
            continue;
        }

        auto rowEnd = xml.find("</table:table-row>", rowTagEnd);
        if (rowEnd == std::string::npos) break;

        int col = 0;
        size_t cpos = rowTagEnd + 1;

        while (col < Spreadsheet::COLS) {
            auto cellStart = xml.find("<table:table-cell", cpos);
            if (cellStart == std::string::npos || cellStart >= rowEnd) break;

            auto cellTagEnd = xml.find('>', cellStart);
            if (cellTagEnd == std::string::npos || cellTagEnd > rowEnd) break;

            std::string cellTag = xml.substr(cellStart, cellTagEnd - cellStart + 1);

            int colRepeat = 1;
            std::string cr = attrVal(cellTag, "table:number-columns-repeated");
            if (!cr.empty()) {
                try { colRepeat = std::stoi(cr); } catch (...) { colRepeat = 1; }
            }

            bool selfClose = cellTag.size() >= 2 && cellTag[cellTag.size() - 2] == '/';
            if (selfClose) {
                col  += colRepeat;
                cpos  = cellTagEnd + 1;
                continue;
            }

            auto cellEnd = xml.find("</table:table-cell>", cellTagEnd);
            if (cellEnd == std::string::npos || cellEnd > rowEnd) break;

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
            cpos  = cellEnd + 19;  // strlen("</table:table-cell>")
        }

        row += rowRepeat;
        pos  = rowEnd + 18;  // strlen("</table:table-row>")
    }
    return true;
}

// ---------------------------------------------------------------------------
// Spreadsheet::saveODS()  —  write the grid to an ODS file
// ---------------------------------------------------------------------------
bool Spreadsheet::saveODS(const std::string& path) const {
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
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>());
    f.close();

    std::string content = extractZipEntry(data, "content.xml");
    if (content.empty()) return false;

    cells_.clear();
    return parseODSContent(content, *this);
}
