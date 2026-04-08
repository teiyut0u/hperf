cmake_minimum_required(VERSION 3.16)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

set(TEST_TARGETS "")

foreach(src ${TEST_SOURCES})
    get_filename_component(name "${src}" NAME_WE)
    add_executable(${name} "${src}")
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${name} PRIVATE hperf_lib gtest_main)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
    list(APPEND TEST_TARGETS ${name})
endforeach()

if(TEST_TARGETS)
    add_custom_target(test DEPENDS ${TEST_TARGETS})
    # 将测试目标列表存到全局属性，供顶层 deploy 使用
    set_property(GLOBAL PROPERTY HPERF_TEST_TARGETS "${TEST_TARGETS}")
endif()