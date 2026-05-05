# ==============================================================================
# CMakeLists.txt  —  Build configuration for the terminal-UI spreadsheet app
#
# Requires CMake 3.20+ for modern target-based API support.
# Build steps:
#   cmake -B build        (configure — detect compiler, generate build files)
#   cmake --build build   (compile and link)
# ==============================================================================

cmake_minimum_required(VERSION 3.20)
project(spreadsheet)

# Use C++20 throughout.  "REQUIRED" makes it a hard error if the compiler
# does not support C++20 rather than silently falling back to an older standard.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# Source files
#
# Each directory corresponds to one architectural layer:
#   core/        — data model (Spreadsheet, Cell, CellValue)
#   formula/     — lexer, parser, AST
#   formula/functions/ — one file per spreadsheet function (SUM, AVERAGE, …)
#   app/         — application controller (rendering, key handling)
#   platform/    — terminal UI implementation (tui.cpp, no external libraries)
# ------------------------------------------------------------------------------
set(SOURCES
    # Core data model — one source file per responsibility:
    src/core/spreadsheet.cpp   # grid storage and formula evaluation
    src/core/csv.cpp           # CSV (RFC-4180) persistence
    src/core/ods.cpp           # Custom binary SSHEET persistence
    src/formula/formula.cpp
    # Shared helpers used by every formula function:
    src/formula/functions/function_base.cpp
    # Individual formula function implementations (one per file):
    src/formula/functions/sum.cpp
    src/formula/functions/avg.cpp
    src/formula/functions/min.cpp
    src/formula/functions/max.cpp
    src/formula/functions/count.cpp
    src/formula/functions/if.cpp
    src/formula/functions/abs.cpp
    src/formula/functions/round.cpp
    src/formula/functions/sqrt.cpp
    src/formula/functions/power.cpp
    src/formula/functions/extended.cpp
    # Dispatcher: implements FunctionNode::eval() via an unordered_map lookup:
    src/formula/functions/dispatch.cpp
    # App layer — one source file per responsibility:
    src/app/app.cpp            # constructor and toolbar layout
    src/app/renderer.cpp       # App::render()
    src/app/keyboard.cpp       # App::onKey()
    src/app/mouse.cpp          # App::onMouse()
    # Platform: single terminal-UI backend (ANSI + POSIX termios, no GUI libs)
    src/platform/tui.cpp
)

# ------------------------------------------------------------------------------
# Executable target
# ------------------------------------------------------------------------------
add_executable(spreadsheet ${SOURCES})

# Each sub-directory is added as a separate include path so that files within
# a layer can include their own headers without path prefixes, e.g.:
#   #include "core.h"     (from any file in src/core/)
#   #include "formula.h"  (from any file that has src/formula/ on its path)
target_include_directories(spreadsheet PRIVATE
    src/core
    src/formula
    src/formula/functions   # needed by function_base.h, sum.h, avg.h, …
    src/app
    src/platform)

# Pin the standard at the target level (redundant with the global setting but
# makes the requirement explicit and portable if this target is ever extracted).
target_compile_features(spreadsheet PRIVATE cxx_std_20)

# Enable strict warnings to catch potential bugs at compile time.
if(MSVC)
    target_compile_options(spreadsheet PRIVATE /W4)
else()
    target_compile_options(spreadsheet PRIVATE -Wall -Wextra -Wpedantic)
endif()
