# Use Google Test from submodule
set(GOOGLETEST_ROOT ${CMAKE_SOURCE_DIR}/third_party/googletest)

if(EXISTS ${GOOGLETEST_ROOT}/CMakeLists.txt)
    message(STATUS "Using Google Test submodule at ${GOOGLETEST_ROOT}")
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # Disable Google Test's own tests and examples
    set(gtest_build_tests OFF CACHE BOOL "" FORCE)
    set(gtest_build_samples OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(${GOOGLETEST_ROOT} ${CMAKE_BINARY_DIR}/googletest-build EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "Google Test submodule not found. Please run: git submodule update --init --recursive")
endif()

# Create test executable
add_executable(nano_stream_tests
    test_sequence.cpp
    test_ring_buffer.cpp
    test_main.cpp
)

target_link_libraries(nano_stream_tests
    PRIVATE
        nano-stream
        gtest
        gtest_main
)

target_include_directories(nano_stream_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(nano_stream_tests)
