# spreadsheet

A terminal-based spreadsheet application with no GUI library dependencies.

## Features
- 20 × 10 grid with formula evaluation (including SUM/AVERAGE/MIN/MAX/COUNT/IF, math + trig, logical, and statistical functions)
- Keyboard navigation and cell editing
- Undo / redo, copy / cut / paste, fill-down / fill-right
- CSV save (`Ctrl+S`) and load (`Ctrl+O`)

## Build

```sh
cmake -B build
cmake --build build
```

Requires: C++20 compiler, POSIX system (Linux/macOS terminal). No external libraries needed.

## Usage

```sh
./build/spreadsheet
```

| Key              | Action                        |
|------------------|-------------------------------|
| Arrow keys       | Navigate cells                |
| Enter / F2       | Enter edit mode               |
| Esc              | Cancel edit                   |
| Tab / Shift+Tab  | Move right / left             |
| Delete / Backspace | Clear cell(s)               |
| Ctrl+Arrow       | Jump to edge of data block    |
| Shift+Arrow      | Extend selection              |
| Ctrl+A           | Select all                    |
| Ctrl+C / X / V   | Copy / Cut / Paste            |
| Ctrl+Z / Y       | Undo / Redo                   |
| Ctrl+D / R       | Fill Down / Fill Right        |
| Ctrl+S / O / N   | Save / Open / New (CSV)       |
| Ctrl+Q           | Quit                          |
