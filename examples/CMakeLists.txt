# Examples for nano-stream library

add_executable(basic_example
    basic_example.cpp
)

target_link_libraries(basic_example
    PRIVATE
        nano-stream
)

target_include_directories(basic_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
