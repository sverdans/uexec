cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")

project(uexec LANGUAGES CXX C)
option(uexec_examples "" OFF)
option(uexec_tests    "" OFF)
option(uexec_werror   "" ON)

set(uexec_lib ${PROJECT_NAME})
set(uexec_test ${uexec_lib}_test)

include(FetchContent)

FetchContent_Declare(execution
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG nvhpc-26.05)
FetchContent_MakeAvailable(execution)

if(${uexec_tests})
    FetchContent_Declare(googletest
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    FetchContent_MakeAvailable(googletest)
endif()

find_package(uring REQUIRED)

set(uexec_dependencies
    STDEXEC::stdexec
    uring::uring)

set(uexec_flags
    -Werror
    -Wall
    -Wextra
    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wextra-semi
    -Wsign-compare
    -Wsign-conversion)

add_library(${uexec_lib} INTERFACE)

target_include_directories(${uexec_lib} INTERFACE include)

target_link_libraries(${uexec_lib} INTERFACE ${uexec_dependencies})

if(${uexec_werror})
    message(STATUS "[uexec] build with flags: ${uexec_flags}")
    target_compile_options(${uexec_lib} INTERFACE ${uexec_flags})
endif()

if(${uexec_tests})
    message(STATUS "[uexec] build with tests")
    add_subdirectory(test)
endif()

if(${uexec_examples})
    message(STATUS "[uexec] build with examples")
    add_subdirectory(examples)
endif()
