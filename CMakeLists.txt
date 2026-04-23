cmake_minimum_required(VERSION 3.20) # min ver
project(spreadsheet) # project
set(CMAKE_CXX_STANDARD 20) # c++20
set(CMAKE_CXX_STANDARD_REQUIRED ON) # enforce
set(SOURCES src/core.cpp src/formula.cpp src/app.cpp) # common
if(CMAKE_SYSTEM_NAME STREQUAL "Windows") # win
    list(APPEND SOURCES src/platform_win32.cpp) # win backend
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin") # mac
    list(APPEND SOURCES src/platform_macos.mm) # mac backend
    enable_language(OBJCXX) # obj-c++
else() # linux
    list(APPEND SOURCES src/platform_x11.cpp) # x11 backend
endif()
add_executable(spreadsheet ${SOURCES}) # target
target_include_directories(spreadsheet PRIVATE src) # includes
target_compile_features(spreadsheet PRIVATE cxx_std_20) # c++20
if(MSVC) # msvc
    target_compile_options(spreadsheet PRIVATE /W4) # warnings
else() # gcc/clang
    target_compile_options(spreadsheet PRIVATE -Wall -Wextra -Wpedantic) # warnings
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows") # win libs
    target_link_libraries(spreadsheet PRIVATE user32 gdi32) # gdi
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin") # mac libs
    target_link_libraries(spreadsheet PRIVATE "-framework Cocoa") # cocoa
else() # linux libs
    target_link_libraries(spreadsheet PRIVATE X11) # x11
endif()
