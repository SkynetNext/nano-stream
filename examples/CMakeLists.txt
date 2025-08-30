# Examples for nano-stream library

add_executable(basic_example
    basic_example.cpp
)

add_executable(advanced_example
    advanced_example.cpp
)





add_executable(complete_aeron_example
    complete_aeron_example.cpp
)

add_executable(aeron_full_example
    aeron_full_example.cpp
)

add_executable(log_buffer_example
    log_buffer_example.cpp
)

add_executable(udp_transport_example
    udp_transport_example.cpp
)

# Suppress unused parameter warnings for each target
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(basic_example PRIVATE -Wno-unused-parameter)
    target_compile_options(advanced_example PRIVATE -Wno-unused-parameter)
    target_compile_options(complete_aeron_example PRIVATE -Wno-unused-parameter)
    target_compile_options(aeron_full_example PRIVATE -Wno-unused-parameter)
    target_compile_options(log_buffer_example PRIVATE -Wno-unused-parameter)
    target_compile_options(udp_transport_example PRIVATE -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(basic_example PRIVATE /wd4100)
    target_compile_options(advanced_example PRIVATE /wd4100)
    target_compile_options(complete_aeron_example PRIVATE /wd4100)
    target_compile_options(aeron_full_example PRIVATE /wd4100)
    target_compile_options(log_buffer_example PRIVATE /wd4100)
    target_compile_options(udp_transport_example PRIVATE /wd4100)
endif()

target_link_libraries(basic_example
    PRIVATE
        nano-stream
)

target_link_libraries(advanced_example
    PRIVATE
        nano-stream
)



target_link_libraries(complete_aeron_example
    PRIVATE
        nano-stream
)

target_link_libraries(aeron_full_example
    PRIVATE
        nano-stream
)

target_link_libraries(log_buffer_example
    PRIVATE
        nano-stream
)

target_link_libraries(udp_transport_example
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


target_include_directories(complete_aeron_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(aeron_full_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(log_buffer_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(udp_transport_example
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
