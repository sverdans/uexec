cmake_minimum_required(VERSION 3.15)

file(GLOB_RECURSE uexec_test_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(${uexec_test} ${uexec_test_sources})

target_compile_options(${uexec_test} PRIVATE ${uexec_flags})

target_include_directories(${uexec_test} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(${uexec_test} PUBLIC
    ${uexec_lib}
    GTest::gtest_main
    GTest::gmock_main)
