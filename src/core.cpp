#include "core.h"     // own header
#include "formula.h"  // evaluateFormula
#include <fstream>    // file I/O
#include <sstream>    // std::ostringstream
#include <stdexcept>  // std::stod exception

// ── setCell ─────────────────────────────────────────────────────────────────

void Spreadsheet::setCell(int r, int c, std::string raw) {
    auto  k    = key(r, c);        // compute sparse-map key
    auto& cell = cells_[k];        // insert or retrieve cell
    cell.raw     = std::move(raw); // store raw input
    cell.display = cell.raw;       // default display = raw text
    cell.value   = std::monostate{}; // clear any previous value
}

// ── getCell ──────────────────────────────────────────────────────────────────

Cell* Spreadsheet::getCell(int r, int c) {
    auto it = cells_.find(key(r, c)); // search map
    return it != cells_.end() ? &it->second : nullptr; // null if absent
}

const Cell* Spreadsheet::getCell(int r, int c) const {
    auto it = cells_.find(key(r, c)); // search map (const)
    return it != cells_.end() ? &it->second : nullptr; // null if absent
}

// ── evalCell (DFS) ───────────────────────────────────────────────────────────

void Spreadsheet::evalCell(int r, int c,
                            std::set<uint64_t>& vis,
                            std::set<uint64_t>& done) {
    auto k  = key(r, c);                  // key for this cell
    if (done.count(k)) return;            // already evaluated, skip
    auto it = cells_.find(k);             // locate cell in map
    if (it == cells_.end()) return;       // absent cells are empty
    auto& cell = it->second;              // reference to cell data

    if (cell.raw.empty() || cell.raw[0] != '=') { // plain text or number
        try {
            size_t pos = 0;                           // parse position
            double v   = std::stod(cell.raw, &pos);  // attempt numeric parse
            if (pos == cell.raw.size()) {             // entire string consumed
                cell.value   = v;        // store as double
                cell.display = cell.raw; // show as typed
            } else {                                  // not a pure number
                cell.value   = cell.raw; // store as string
                cell.display = cell.raw; // show as text
            }
        } catch (...) {            // stod threw (e.g. empty or letters)
            cell.value   = cell.raw; // treat as string
            cell.display = cell.raw; // display verbatim
        }
        done.insert(k); // mark complete
        return;         // nothing more to do
    }

    // ── Formula cell ────────────────────────────────────────────────────────
    if (vis.count(k)) {                        // already on the DFS stack
        cell.display = "#CYCLE!";              // report cycle error
        cell.value   = std::string("#CYCLE!"); // store error string
        done.insert(k);                        // mark to avoid re-entry
        return;                                // abort evaluation
    }

    vis.insert(k); // push onto visiting stack

    auto result = evaluateFormula(cell.raw.substr(1), *this, vis, done); // evaluate

    vis.erase(k); // pop from visiting stack

    if (std::holds_alternative<std::string>(result)) { // error result
        auto errStr  = std::get<std::string>(result); // extract error string
        cell.display = errStr;                         // show error
        cell.value   = errStr;                         // store as string value
    } else {                                           // numeric result
        double v     = std::get<double>(result);       // extract double
        cell.value   = v;                              // store numeric value
        std::ostringstream oss;                        // format buffer
        oss << v;                                      // convert to string
        cell.display = oss.str();                      // store display string
    }
    done.insert(k); // mark evaluation complete
}

// ── evaluateAll ──────────────────────────────────────────────────────────────

void Spreadsheet::evaluateAll() {
    std::set<uint64_t> vis;  // cells currently on the DFS stack
    std::set<uint64_t> done; // cells whose evaluation finished
    for (auto& [k, _] : cells_) { // iterate every stored cell
        int r = static_cast<int>(k >> 32);          // decode row
        int c = static_cast<int>(k & 0xFFFFFFFFu);  // decode column
        evalCell(r, c, vis, done);                   // ensure evaluated
    }
}

// ── CSV helpers ──────────────────────────────────────────────────────────────

static std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s; // no escaping needed
    std::string out = "\""; // opening quote
    for (char ch : s) {     // scan every character
        if (ch == '"') out += '"'; // double up embedded quotes
        out += ch;                 // append character
    }
    out += '"'; // closing quote
    return out; // return escaped field
}

// ── saveCSV ──────────────────────────────────────────────────────────────────

bool Spreadsheet::saveCSV(const std::string& path) const {
    std::ofstream f(path); // open output file
    if (!f)  return false; // bail on open failure
    for (int r = 0; r < ROWS; ++r) {          // each row
        for (int c = 0; c < COLS; ++c) {      // each column
            if (c > 0) f << ',';              // field separator
            auto it = cells_.find(key(r, c)); // find cell
            if (it != cells_.end())           // cell exists
                f << csvEscape(it->second.raw); // write escaped raw content
        }
        f << '\n'; // end of row
    }
    return true; // success
}

// ── loadCSV ──────────────────────────────────────────────────────────────────

bool Spreadsheet::loadCSV(const std::string& path) {
    std::ifstream f(path); // open input file
    if (!f) return false;  // bail on open failure
    cells_.clear();        // discard current data
    std::string line;      // current CSV line
    int r = 0;             // row counter
    while (std::getline(f, line) && r < ROWS) { // read lines
        int    c = 0;  // column counter
        size_t i = 0;  // character position
        while (i <= line.size() && c < COLS) { // parse fields
            std::string field; // accumulate field text
            if (i < line.size() && line[i] == '"') { // quoted field
                ++i; // skip opening quote
                while (i < line.size()) { // read quoted content
                    if (line[i] == '"') {            // quote character
                        ++i;                         // consume it
                        if (i < line.size() && line[i] == '"') { // escaped quote
                            field += '"'; // add literal quote
                            ++i;          // skip second quote
                        } else {
                            break; // closing quote reached
                        }
                    } else {
                        field += line[i++]; // ordinary character
                    }
                }
                if (i < line.size() && line[i] == ',') ++i; // skip trailing comma
            } else {                                    // unquoted field
                auto end = line.find(',', i);           // find next comma
                if (end == std::string::npos) end = line.size(); // or end of line
                field = line.substr(i, end - i);        // extract field text
                i     = end + 1;                        // advance past comma
            }
            if (!field.empty()) setCell(r, c, field); // store non-empty field
            ++c;                                       // next column
        }
        ++r; // next row
    }
    return true; // success
}
