# ==============================================================================
# CMakeLists.txt  —  Build configuration for the cross-platform spreadsheet app
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
#   platform/    — OS-specific window implementation (selected below)
# ------------------------------------------------------------------------------
set(SOURCES
    # Core data model — one source file per responsibility:
    src/core/spreadsheet.cpp   # grid storage and formula evaluation
    src/core/csv.cpp           # CSV (RFC-4180) persistence
    src/core/ods.cpp           # OpenDocument Spreadsheet (.ods) persistence
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
    # Dispatcher: implements FunctionNode::eval() via an unordered_map lookup:
    src/formula/functions/dispatch.cpp
    # App layer — one source file per responsibility:
    src/app/app.cpp            # constructor and toolbar layout
    src/app/renderer.cpp       # App::render()
    src/app/keyboard.cpp       # App::onKey()
    src/app/mouse.cpp          # App::onMouse()
)

# ------------------------------------------------------------------------------
# Platform selection
#
# We compile exactly one platform backend based on the target OS.
# Each backend implements the IWindow interface declared in platform.h.
# ------------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND SOURCES src/platform/win32.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND SOURCES src/platform/macos.mm)
    # macos.mm is Objective-C++; we must enable the OBJCXX language so CMake
    # uses the Objective-C++ compiler (clang++ with -x objective-c++) for it.
    enable_language(OBJCXX)
else()
    # Assume Linux / other Unix with X11.
    list(APPEND SOURCES src/platform/x11.cpp)
endif()

# ------------------------------------------------------------------------------
# Executable target
# ------------------------------------------------------------------------------
add_executable(spreadsheet ${SOURCES})

# On Windows the entry point is WinMain (a GUI subsystem executable), not main.
# Without WIN32_EXECUTABLE the linker generates a console application and looks
# for main(), causing LNK2019 "unresolved external symbol main".
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(spreadsheet PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

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

# ------------------------------------------------------------------------------
# Platform-specific linker dependencies
# ------------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # user32 — window management (CreateWindowEx, MessageLoop, …)
    # gdi32  — graphics device interface (BitBlt, CreatePen, …)
    target_link_libraries(spreadsheet PRIVATE user32 gdi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # The Cocoa framework provides NSApplication, NSWindow, NSView, etc.
    target_link_libraries(spreadsheet PRIVATE "-framework Cocoa")
else()
    # libX11 — the client-side X11 library (XOpenDisplay, XCreateWindow, …)
    target_link_libraries(spreadsheet PRIVATE X11)
    # Statically bundle the C++ runtime so the binary does not depend on the
    # host system's libstdc++/libgcc version (avoids GLIBCXX_x.y.z not found).
    target_link_options(spreadsheet PRIVATE -static-libstdc++ -static-libgcc)
endif()
