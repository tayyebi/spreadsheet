// =============================================================================
// spreadsheet.cpp  —  Spreadsheet grid storage and formula evaluation
//
// Implements: cell storage (setCell / getCell), depth-first formula
// evaluation with cycle detection (evalCell), and the top-level pass
// over all cells (evaluateAll).
// =============================================================================

#include "spreadsheet.h"  // Spreadsheet, Cell, CellValue declarations
#include "formula.h"      // evaluateFormula() — called for '=' cells
#include <sstream>        // std::ostringstream — convert double to string

// ---------------------------------------------------------------------------
// setCell()  —  write raw text into a cell and reset its computed state
//
// We immediately clear display and value so that stale results from any
// previous content are not accidentally read before the next evaluateAll().
// The actual formula evaluation is deferred to evaluateAll() / evalCell().
// ---------------------------------------------------------------------------
void Spreadsheet::setCell(int r, int c, std::string raw) {
    auto& cell   = cells_[key(r, c)];  // creates entry if absent
    cell.raw     = std::move(raw);     // store exactly what the user typed
    cell.display = cell.raw;           // default display = raw (overwritten on eval)
    cell.value   = std::monostate{};   // mark as not-yet-evaluated
}

// ---------------------------------------------------------------------------
// getCell()  —  look up a cell by position
//
// Returns a pointer into the internal map, or nullptr if the cell was never
// written.  The two overloads keep the const-correctness contract.
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
//   3. Non-formula cells: try to parse as a number; fall back to text.
//   4. Formula cells:
//      a. Detect cycles via `vis` (current DFS stack).
//      b. Push to `vis`, call evaluateFormula() recursively, pop from `vis`.
//      c. Store the result as display and value.
//   5. Mark as done.
// ---------------------------------------------------------------------------
void Spreadsheet::evalCell(int r, int c,
                           std::set<uint64_t>& vis,
                           std::set<uint64_t>& done) {
    auto k = key(r, c);

    if (done.count(k)) return;  // already fully evaluated

    auto it = cells_.find(k);
    if (it == cells_.end()) return;  // cell was never written

    auto& cell = it->second;

    // ------------------------------------------------------------------
    // Plain value path: not a formula
    // ------------------------------------------------------------------
    if (cell.raw.empty() || cell.raw[0] != '=') {
        try {
            size_t consumed = 0;
            double v = std::stod(cell.raw, &consumed);
            if (consumed == cell.raw.size()) {
                cell.value   = v;
                cell.display = cell.raw;
            } else {
                cell.value   = cell.raw;
                cell.display = cell.raw;
            }
        } catch (...) {
            cell.value   = cell.raw;
            cell.display = cell.raw;
        }
        done.insert(k);
        return;
    }

    // ------------------------------------------------------------------
    // Cycle detection
    // ------------------------------------------------------------------
    if (vis.count(k)) {
        cell.display = "#CYCLE!";
        cell.value   = std::string("#CYCLE!");
        done.insert(k);
        return;
    }

    // ------------------------------------------------------------------
    // Formula evaluation path
    // ------------------------------------------------------------------
    vis.insert(k);

    auto res = evaluateFormula(cell.raw.substr(1), *this, vis, done);

    vis.erase(k);

    if (std::holds_alternative<std::string>(res)) {
        auto e       = std::get<std::string>(res);
        cell.display = e;
        cell.value   = e;
    } else {
        double v    = std::get<double>(res);
        cell.value  = v;
        std::ostringstream os;
        os << v;
        cell.display = os.str();
    }

    done.insert(k);
}

// ---------------------------------------------------------------------------
// evaluateAll()  —  (re-)evaluate every cell in the grid
//
// Creates fresh `vis` and `done` sets, then calls evalCell() for each cell
// in the sparse map.  Each cell is processed exactly once.
// ---------------------------------------------------------------------------
void Spreadsheet::evaluateAll() {
    std::set<uint64_t> vis, done;
    for (auto& [k, _] : cells_)
        evalCell(int(k >> 32), int(k & 0xFFFFFFFFu), vis, done);
}
