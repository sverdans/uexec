set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(add_example name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE uexec)
endfunction()

add_example(main)
add_example(coro)
