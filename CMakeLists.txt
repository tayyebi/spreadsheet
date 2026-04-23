cmake_minimum_required(VERSION 3.20)
project(spreadsheet)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SOURCES
    src/core/core.cpp
    src/formula/formula.cpp
    src/formula/functions/function_base.cpp
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
    src/formula/functions/dispatch.cpp
    src/app/app.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND SOURCES src/platform/win32.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND SOURCES src/platform/macos.mm)
    enable_language(OBJCXX)
else()
    list(APPEND SOURCES src/platform/x11.cpp)
endif()
add_executable(spreadsheet ${SOURCES})
target_include_directories(spreadsheet PRIVATE src/core src/formula src/formula/functions src/app src/platform)
target_compile_features(spreadsheet PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(spreadsheet PRIVATE /W4)
else()
    target_compile_options(spreadsheet PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(spreadsheet PRIVATE user32 gdi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(spreadsheet PRIVATE "-framework Cocoa")
else()
    target_link_libraries(spreadsheet PRIVATE X11)
endif()
