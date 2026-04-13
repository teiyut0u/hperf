cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

project(hperf VERSION 1.0.0 LANGUAGES CXX)

# To generate compile_commands.json for clangd code hinting.
# Create a symlink at the source root so clangd always picks up the most
# recently configured preset without any manual VSCode settings changes.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(CREATE_LINK
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    SYMBOLIC
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Android NDK config: enable RTTI/exceptions, use static libc++
if(ANDROID)
    set(ANDROID_CPP_FEATURES "rtti exceptions")
    set(ANDROID_STL c++_static)
endif()

# ==========================================
# Target: tomlplusplus (header-only library)
# ==========================================
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tomlplusplus)

# ==========================================
# Target: execuatable file hperf
# ==========================================
# Search for source files (non-recursive: monitor/ is added conditionally below)
file(GLOB HPERF_SOURCES
    "${CMAKE_SOURCE_DIR}/src/hperf/*.cpp"
)

set(HPERF_LIB_SOURCES ${HPERF_SOURCES})
list(REMOVE_ITEM HPERF_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")

add_library(hperf_lib STATIC ${HPERF_LIB_SOURCES})
target_compile_features(hperf_lib PRIVATE cxx_std_17)
target_include_directories(hperf_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(hperf_lib PRIVATE -Wall -fexceptions)
target_link_libraries(hperf_lib PUBLIC tomlplusplus)

# Monitor module: CMN bandwidth monitoring (Linux only — CMN PMU is server-class hardware)
if(NOT ANDROID)
    file(GLOB_RECURSE MONITOR_SOURCES "${CMAKE_SOURCE_DIR}/src/hperf/monitor/*.cpp")
    target_sources(hperf_lib PRIVATE ${MONITOR_SOURCES})
endif()

add_executable(hperf "${CMAKE_SOURCE_DIR}/src/hperf/main.cpp")
target_link_libraries(hperf PRIVATE hperf_lib)
target_include_directories(hperf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(hperf PRIVATE -Wall -fexceptions)

# ==========================================
# Target: execuatable file gperf (Android only)
# ==========================================
if(ANDROID)
    add_subdirectory(third_party/libGPUCounters)

    add_executable(gperf
        "${CMAKE_SOURCE_DIR}/src/gperf/main.cpp"
        "${CMAKE_SOURCE_DIR}/src/gperf/visualizer.cpp"
        "${CMAKE_SOURCE_DIR}/src/gperf/record.cpp"
    )

    target_include_directories(gperf
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(gperf
        PRIVATE hwcpipe
    )

    target_compile_options(gperf
        PRIVATE -Werror
                -Wswitch-default
    )
endif()

# ==========================================
# Add deploy target (use adb push to device, Android only)
# ==========================================
if(ANDROID)
    find_program(ADB_EXECUTABLE NAMES adb adb.exe)

    if(ADB_EXECUTABLE)
        message(STATUS "Found adb: ${ADB_EXECUTABLE}")
        message(STATUS "Deploy targets available: deploy-oryon, deploy-cortex_x4, deploy-c1_ultra, deploy-ustress")

        # One deploy-<cpu> target per CPU type; each pushes the matching TOML config.
        set(CPU_DEPLOY_LIST oryon cortex_x4 c1_ultra ustress)
        foreach(cpu ${CPU_DEPLOY_LIST})
            add_custom_target(deploy-${cpu}
                COMMAND ${CMAKE_COMMAND} -E echo "--- Deploying hperf [${cpu}] to device ---"
                COMMAND ${ADB_EXECUTABLE} root
                COMMAND ${ADB_EXECUTABLE} shell "killall hperf 2>/dev/null || true"
                COMMAND ${ADB_EXECUTABLE} shell rm -rf /data/local/tmp/hperf
                COMMAND ${ADB_EXECUTABLE} shell rm -f /data/local/tmp/.hperf.toml
                COMMAND ${ADB_EXECUTABLE} push $<TARGET_FILE:hperf> /data/local/tmp/hperf
                COMMAND ${ADB_EXECUTABLE} shell chmod +x /data/local/tmp/hperf
                COMMAND ${ADB_EXECUTABLE} push ${CMAKE_CURRENT_SOURCE_DIR}/config/cpu_${cpu}.toml /data/local/tmp/.hperf.toml
                COMMAND ${CMAKE_COMMAND} -E echo "--- Deploying gperf to device ---"
                COMMAND ${ADB_EXECUTABLE} shell "killall gperf 2>/dev/null || true"
                COMMAND ${ADB_EXECUTABLE} shell rm -rf /data/local/tmp/gperf
                COMMAND ${ADB_EXECUTABLE} push $<TARGET_FILE:gperf> /data/local/tmp/gperf
                COMMAND ${ADB_EXECUTABLE} shell chmod +x /data/local/tmp/gperf
                COMMAND ${CMAKE_COMMAND} -E echo "--- Deployment complete [${cpu}] ---"
                COMMENT "Deploying hperf [${cpu} config] and gperf to device via adb"
                VERBATIM
            )
            add_dependencies(deploy-${cpu} hperf gperf)
        endforeach()
    else()
        message(WARNING "adb not found. The deploy-* targets will not be available.")
    endif()
endif()

# ==========================================
# Google Test (for unit tests)
# ==========================================
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ==========================================
# Test
# ==========================================
add_subdirectory(test)

get_property(HPERF_TEST_TARGETS GLOBAL PROPERTY HPERF_TEST_TARGETS)
if(HPERF_TEST_TARGETS)
    if(ANDROID AND ADB_EXECUTABLE)
        # Android: push binaries to device and run via adb
        add_custom_target(push_tests
            COMMAND ${CMAKE_COMMAND} -E echo "--- Deploying tests to device ---"
            COMMAND ${ADB_EXECUTABLE} root
            COMMAND ${ADB_EXECUTABLE} shell mkdir -p /data/local/tmp/test
        )
        add_dependencies(push_tests ${HPERF_TEST_TARGETS})
        foreach(t ${HPERF_TEST_TARGETS})
            add_custom_command(TARGET push_tests POST_BUILD
                COMMAND ${ADB_EXECUTABLE} push $<TARGET_FILE:${t}> /data/local/tmp/test/${t}
                COMMAND ${ADB_EXECUTABLE} shell chmod +x /data/local/tmp/test/${t}
            )
        endforeach()

        add_custom_target(run_tests
            COMMAND ${CMAKE_COMMAND} -E echo "--- Running tests on device ---"
            DEPENDS push_tests
            COMMENT "Push and run all unit tests on the connected Android device"
        )
        foreach(t ${HPERF_TEST_TARGETS})
            add_custom_command(TARGET run_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "[RUN] ${t}"
                COMMAND ${ADB_EXECUTABLE} shell /data/local/tmp/test/${t}
            )
        endforeach()
        add_custom_command(TARGET run_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "--- All tests passed ---"
        )
    elseif(NOT ANDROID)
        # Linux: run test binaries directly on the host
        add_custom_target(run_tests
            COMMENT "Run all unit tests locally"
            VERBATIM
        )
        add_dependencies(run_tests ${HPERF_TEST_TARGETS})
        foreach(t ${HPERF_TEST_TARGETS})
            add_custom_command(TARGET run_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "[RUN] ${t}"
                COMMAND $<TARGET_FILE:${t}>
            )
        endforeach()
        add_custom_command(TARGET run_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "--- All tests passed ---"
        )
    endif()
endif()
