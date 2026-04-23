cmake_minimum_required(VERSION 3.20) # require cmake 3.20+
project(spreadsheet)                  # project name

set(CMAKE_CXX_STANDARD 20)            # use C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # enforce C++20

# Common source files shared across all platforms
set(SOURCES
    src/core.cpp     # spreadsheet data model
    src/formula.cpp  # formula evaluator
    src/app.cpp      # UI application logic
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")   # Windows platform
    list(APPEND SOURCES src/platform_win32.cpp) # Win32/GDI backend
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin") # macOS platform
    list(APPEND SOURCES src/platform_macos.mm)  # Cocoa backend
    enable_language(OBJCXX)                     # enable Objective-C++
else()                                     # Linux/X11 platform
    list(APPEND SOURCES src/platform_x11.cpp)   # X11 backend
endif()

add_executable(spreadsheet ${SOURCES}) # build the executable

target_include_directories(spreadsheet PRIVATE src) # include src headers

target_compile_features(spreadsheet PRIVATE cxx_std_20) # require C++20

if(MSVC)  # MSVC compiler warnings
    target_compile_options(spreadsheet PRIVATE /W4) # strict MSVC warnings
else()    # GCC/Clang warnings
    target_compile_options(spreadsheet PRIVATE -Wall -Wextra -Wpedantic) # strict GCC/Clang
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")   # Windows libraries
    target_link_libraries(spreadsheet PRIVATE user32 gdi32) # Win32 libs
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin") # macOS framework
    target_link_libraries(spreadsheet PRIVATE "-framework Cocoa") # Cocoa framework
else()                                     # Linux library
    target_link_libraries(spreadsheet PRIVATE X11) # X11 library
endif()
