cmake_minimum_required(VERSION 3.16)
project(hperf VERSION 1.0.0 LANGUAGES CXX)

# To generate compile_commands.json for clangd code hinting
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Users can specify the target CPU type by command-line option, e.g., -DCPU_TYPE=N1
# Available CPU type:
# N1: Arm Neoverse-N1, Ampere Altra / Altra MAX (default)
# TAISHAN: Kunpeng 920
# ICX: Intel Ice Lake
# CLX: Intel Cascade Lake
set(CPU_TYPE "ORYON" CACHE STRING "Specify the target CPU type (e.g., N1, TAISHAN, ...)")
string(TOUPPER "${CPU_TYPE}" CPU_TYPE_UPPER)

# ==========================================
# Target: executable file hperf
# ==========================================
# Search for source files
file(GLOB_RECURSE HPERF_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/hperf/*.cpp"
)

set(HPERF_LIB_SOURCES ${HPERF_SOURCES})
list(REMOVE_ITEM HPERF_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")

add_library(hperf_lib STATIC ${HPERF_LIB_SOURCES})
target_include_directories(hperf_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(hperf_lib PRIVATE -Wall -fexceptions)

# Add CPU_TYPE macro
if(CPU_TYPE_UPPER STREQUAL "N1")
    target_compile_definitions(hperf_lib PRIVATE CPU_N1)
    message(STATUS "Configuring for CPU type: Neoverse-N1")
elseif(CPU_TYPE_UPPER STREQUAL "TAISHAN")
    target_compile_definitions(hperf_lib PRIVATE CPU_TAISHAN)
    message(STATUS "Configuring for CPU type: Hislicon Taishan")
elseif(CPU_TYPE_UPPER STREQUAL "ICX")
    target_compile_definitions(hperf_lib PRIVATE CPU_ICX)
    message(STATUS "Configuring for CPU type: Intel Ice Lake")
elseif(CPU_TYPE_UPPER STREQUAL "CLX")
    target_compile_definitions(hperf_lib PRIVATE CPU_CLX)
    message(STATUS "Configuring for CPU type: Intel Cascade Lake")
else()
    message(FATAL_ERROR "Unsupported CPU_TYPE: ${CPU_TYPE}.")
endif()

add_executable(hperf "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")
target_link_libraries(hperf PRIVATE hperf_lib)
target_include_directories(hperf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(hperf PRIVATE -Wall -fexceptions)

# ==========================================
# Test
# ==========================================
add_subdirectory(test)