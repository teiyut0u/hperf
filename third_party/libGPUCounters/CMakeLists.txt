#
# Copyright (c) 2021-2025 Arm Limited.
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.13.5)

project(hwcpipe VERSION 2.4)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    HWCPIPE_WALL
    "Enable all warnings."
    ON
)
option(
    HWCPIPE_WERROR
    "Treat compile and link warning as error."
    ON
)
option(
    HWCPIPE_PIC
    "Generate position independent code."
    ON
)

if(CMAKE_BUILD_TYPE
    AND (NOT
        CMAKE_BUILD_TYPE
        STREQUAL
        "Debug"
        )
    )
    set(IS_RELEASE_BUILD YES)
endif()

option(
    HWCPIPE_ENABLE_LTO
    "Enable link time optimization. Enabled by default for release builds."
    ${IS_RELEASE_BUILD}
)

if (HWCPIPE_ENABLE_EXCEPTIONS)
    add_compile_options(-fexceptions)
else()
    add_compile_options(-fno-exceptions)
endif()

if (HWCPIPE_ENABLE_RTTI)
    add_compile_options(-frtti)
else()
    add_compile_options(-fno-rtti)
endif()

if(HWCPIPE_WERROR)
    add_compile_options(
        -Werror
        -pedantic-errors
        -Wno-abi # Silence GCC 7.1 ABI compatibility warning
    )
    add_link_options(-Werror)
endif()

add_subdirectory(backend/device)

if(HWCPIPE_PIC)
    set_target_properties(device PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

add_subdirectory(hwcpipe)
