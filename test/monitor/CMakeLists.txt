cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)

FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE TEST_MONITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Filter out mock directory sources, add them separately with different handling
list(FILTER TEST_MONITOR_SOURCES EXCLUDE REGEX "mocks/.*\\.cpp$")

# Collect mock sources
file(GLOB_RECURSE MOCK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/mocks/*.cpp"
)

add_executable(test_monitor ${TEST_MONITOR_SOURCES} ${MOCK_SOURCES})

target_link_libraries(test_monitor PRIVATE Catch2::Catch2WithMain hperf_lib)
target_include_directories(test_monitor PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_monitor PRIVATE -Wall -fexceptions)
target_link_options(test_monitor PRIVATE
    -Wl,--wrap=syscall
    -Wl,--wrap=ioctl
)

set_target_properties(test_monitor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/monitor"
)
