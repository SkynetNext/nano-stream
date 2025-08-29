# Examples for nano-stream library

add_executable(basic_example
    basic_example.cpp
)

add_executable(advanced_example
    advanced_example.cpp
)

# Suppress unused parameter warnings for each target
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(basic_example PRIVATE -Wno-unused-parameter)
    target_compile_options(advanced_example PRIVATE -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(basic_example PRIVATE /wd4100)
    target_compile_options(advanced_example PRIVATE /wd4100)
endif()

target_link_libraries(basic_example
    PRIVATE
        nano-stream
)

target_link_libraries(advanced_example
    PRIVATE
        nano-stream
)

target_include_directories(basic_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(advanced_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
