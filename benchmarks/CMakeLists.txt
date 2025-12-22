# Use Google Benchmark from submodule
set(BENCHMARK_ROOT ${CMAKE_SOURCE_DIR}/third_party/benchmark)

if(EXISTS ${BENCHMARK_ROOT}/CMakeLists.txt)
    message(STATUS "Using Google Benchmark submodule at ${BENCHMARK_ROOT}")
    # Disable benchmark's own tests and examples
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(${BENCHMARK_ROOT} ${CMAKE_BINARY_DIR}/benchmark-build EXCLUDE_FROM_ALL)
    
    # Suppress format-nonliteral warning for Google Benchmark (known issue in older versions)
    # This warning occurs in third_party/benchmark/src/string_util.cc
    # Google Benchmark's CMakeLists.txt sets -Wformat=2 and -Werror.
    # We need to add -Wno-format-nonliteral AFTER all other options so it takes precedence.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        if(TARGET benchmark)
            # Get current compile options
            get_target_property(_benchmark_compile_options benchmark COMPILE_OPTIONS)
            # Add -Wno-format-nonliteral at the end to override -Wformat=2
            if(_benchmark_compile_options)
                list(APPEND _benchmark_compile_options "-Wno-format-nonliteral")
            else()
                set(_benchmark_compile_options "-Wno-format-nonliteral")
            endif()
            set_target_properties(benchmark PROPERTIES COMPILE_OPTIONS "${_benchmark_compile_options}")
            unset(_benchmark_compile_options)
        endif()
        
        if(TARGET benchmark_main)
            get_target_property(_benchmark_main_compile_options benchmark_main COMPILE_OPTIONS)
            if(_benchmark_main_compile_options)
                list(APPEND _benchmark_main_compile_options "-Wno-format-nonliteral")
            else()
                set(_benchmark_main_compile_options "-Wno-format-nonliteral")
            endif()
            set_target_properties(benchmark_main PROPERTIES COMPILE_OPTIONS "${_benchmark_main_compile_options}")
            unset(_benchmark_main_compile_options)
        endif()
    endif()
else()
    message(FATAL_ERROR "Google Benchmark submodule not found. Please run: git submodule update --init --recursive")
endif()

# Create benchmark executable
file(GLOB_RECURSE NANO_STREAM_JMH_BENCH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/jmh/*.cpp"
)

file(GLOB_RECURSE NANO_STREAM_PERFTEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/perftest/**/*.cpp"
)

add_executable(nano_stream_benchmarks
    ${NANO_STREAM_JMH_BENCH_SOURCES}
    ${NANO_STREAM_PERFTEST_SOURCES}
    benchmark_main.cpp
)

target_link_libraries(nano_stream_benchmarks
    PRIVATE
        disruptor-cpp
        benchmark::benchmark
)

# TSF4G tbus reference (Linux-only, SysV headers). Used by JMH_TBusSingleProducerSingleConsumer_producing.
if (UNIX AND NOT APPLE)
    if (EXISTS ${CMAKE_SOURCE_DIR}/reference/tsf4g/tbus/CMakeLists.txt)
        add_subdirectory(${CMAKE_SOURCE_DIR}/reference/tsf4g/tbus ${CMAKE_BINARY_DIR}/tsf4g-tbus-build EXCLUDE_FROM_ALL)
        target_link_libraries(nano_stream_benchmarks PRIVATE tsf4g_tbus)
    endif()
endif()

target_include_directories(nano_stream_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set optimization flags for benchmarks
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nano_stream_benchmarks PRIVATE -O3 -march=native -mtune=native -Wno-unused-parameter)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(nano_stream_benchmarks PRIVATE /O2 /arch:AVX2 /wd4100)
endif()
