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

# Collect all ported Java tests (C++ gtest) under tests/disruptor/**.
file(GLOB_RECURSE DISRUPTOR_PORTED_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/disruptor/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/disruptor/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/disruptor/*/*/*.cpp"
)

if(DISRUPTOR_PORTED_TEST_SOURCES)
    add_executable(nano_stream_tests
        ${DISRUPTOR_PORTED_TEST_SOURCES}
    )

    target_link_libraries(nano_stream_tests
        PRIVATE
            disruptor-cpp
            gtest
            gtest_main
    )

    target_include_directories(nano_stream_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}
    )

    # Suppress unused parameter warnings for tests
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(nano_stream_tests PRIVATE -Wno-unused-parameter)
    elseif(MSVC)
        target_compile_options(nano_stream_tests PRIVATE /wd4100)
    endif()

    include(GoogleTest)
    gtest_discover_tests(nano_stream_tests)
else()
    message(STATUS "No ported Disruptor tests found under tests/disruptor/**")
endif()
