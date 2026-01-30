cmake_minimum_required(VERSION 3.16)
project(hperf VERSION 0.1.0 LANGUAGES CXX)

# compile setup >>>
# To generate compile_commands.json for clangd code hinting
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# arm64
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# <<< compile setup

# hperf lib >>>
file(GLOB_RECURSE HPERF_LIB_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/hperf/*/*.cpp"
)

add_library(lib${PROJECT_NAME} STATIC ${HPERF_LIB_SOURCES})

target_include_directories(lib${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third/CLI11/include>"
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
# <<< hperf lib

# hperf exe >>>
add_executable(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE lib${PROJECT_NAME})
# <<< hperf exe

# # Users can specify the target CPU type by command-line option, e.g., -DCPU_TYPE=N1
# # Available CPU type:
# # N1: Arm Neoverse-N1, Ampere Altra / Altra MAX (default)
# # TAISHAN: Kunpeng 920
# # ICX: Intel Ice Lake
# # CLX: Intel Cascade Lakeadd_executable(${PROJECT_NAME} main.cpp)
# set(CPU_TYPE "ORYON" CACHE STRING "Specify the target CPU type (e.g., N1, TAISHAN, ...)")
# string(TOUPPER "${CPU_TYPE}" CPU_TYPE_UPPER)

# ==========================================
# Target: executable file hperf
# ==========================================
# Search for source files
# file(GLOB_RECURSE HPERF_SOURCES 
#     "${CMAKE_SOURCE_DIR}/src/**/*.cpp"
# )

# set(HPERF_LIB_SOURCES ${HPERF_SOURCES})
# list(REMOVE_ITEM HPERF_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")
#
# add_library(hperf_lib STATIC ${HPERF_LIB_SOURCES})
# target_include_directories(hperf_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_compile_options(hperf_lib PRIVATE -Wall -fexceptions)
#
# # Add CPU_TYPE macro
# if(CPU_TYPE_UPPER STREQUAL "N1")
#     target_compile_definitions(hperf_lib PRIVATE CPU_N1)
#     message(STATUS "Configuring for CPU type: Neoverse-N1")
# elseif(CPU_TYPE_UPPER STREQUAL "TAISHAN")
#     target_compile_definitions(hperf_lib PRIVATE CPU_TAISHAN)
#     message(STATUS "Configuring for CPU type: Hislicon Taishan")
# elseif(CPU_TYPE_UPPER STREQUAL "ICX")
#     target_compile_definitions(hperf_lib PRIVATE CPU_ICX)
#     message(STATUS "Configuring for CPU type: Intel Ice Lake")
# elseif(CPU_TYPE_UPPER STREQUAL "CLX")
#     target_compile_definitions(hperf_lib PRIVATE CPU_CLX)
#     message(STATUS "Configuring for CPU type: Intel Cascade Lake")
# else()
#     message(FATAL_ERROR "Unsupported CPU_TYPE: ${CPU_TYPE}.")
# endif()
#
# add_executable(hperf "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")
# target_link_libraries(hperf PRIVATE hperf_lib)
# target_include_directories(hperf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# target_compile_options(hperf PRIVATE -Wall -fexceptions)
#
# # ==========================================
# # Test
# # ==========================================
# add_subdirectory(test)
