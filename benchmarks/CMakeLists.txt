# Use Google Benchmark from submodule
set(BENCHMARK_ROOT ${CMAKE_SOURCE_DIR}/third_party/benchmark)

if(EXISTS ${BENCHMARK_ROOT}/CMakeLists.txt)
    message(STATUS "Using Google Benchmark submodule at ${BENCHMARK_ROOT}")
    # Disable benchmark's own tests and examples
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(${BENCHMARK_ROOT} ${CMAKE_BINARY_DIR}/benchmark-build EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "Google Benchmark submodule not found. Please run: git submodule update --init --recursive")
endif()

# Create benchmark executable
add_executable(nano_stream_benchmarks
    benchmark_sequence.cpp
    benchmark_ring_buffer.cpp
    benchmark_comparison.cpp
    benchmark_main.cpp
)

target_link_libraries(nano_stream_benchmarks
    PRIVATE
        nano-stream
        benchmark::benchmark
)

target_include_directories(nano_stream_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Set optimization flags for benchmarks
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nano_stream_benchmarks PRIVATE -O3 -march=native -mtune=native -Wno-unused-parameter)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(nano_stream_benchmarks PRIVATE /O2 /arch:AVX2 /wd4100)
endif()
