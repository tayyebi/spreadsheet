// =============================================================================
// core.cpp  —  Spreadsheet method implementations
//
// Implements: cell storage, formula-driven DFS evaluation, cycle detection,
// and RFC-4180 CSV persistence.
// =============================================================================

#include "core.h"     // Spreadsheet, Cell, CellValue declarations
#include "formula.h"  // evaluateFormula() — called for cells whose raw starts with '='
#include <fstream>    // std::ifstream, std::ofstream — file I/O
#include <sstream>    // std::ostringstream — convert double to display string

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
