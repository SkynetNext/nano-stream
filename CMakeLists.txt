cmake_minimum_required(VERSION 3.20)
project(nano-stream VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Find dependencies
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Build options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Enable testing only if requested
if(BUILD_TESTING)
    enable_testing()
endif()

# Create library with both headers and implementation
add_library(nano-stream 
    # Aeron utility implementations
    src/aeron/util/memory_mapped_file.cpp
    src/aeron/util/path_utils.cpp
    src/aeron/logbuffer/log_buffer_descriptor.cpp
    src/aeron/logbuffer/log_buffers.cpp
    src/aeron/logbuffer/frame_descriptor.cpp
    
    # Aeron driver implementations
    src/aeron/driver/conductor/conductor.cpp
    src/aeron/driver/sender/sender.cpp
    src/aeron/driver/receiver/receiver.cpp
    src/aeron/driver/status/counters_manager.cpp
    src/aeron/driver/media_driver.cpp
    
    # Aeron client implementations
    src/aeron/client/aeron.cpp
    src/aeron/client/publication.cpp
    src/aeron/client/subscription.cpp
)

target_include_directories(nano-stream 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(nano-stream 
    PUBLIC 
        Threads::Threads
)

# Set target properties
target_compile_features(nano-stream PUBLIC cxx_std_20)

# Suppress unused parameter warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano-stream PRIVATE -Wno-unused-parameter)
elseif(MSVC)
    target_compile_options(nano-stream PRIVATE /wd4100)
endif()

# Add subdirectories conditionally
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install targets
install(TARGETS nano-stream
    EXPORT nano-stream-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

install(EXPORT nano-stream-targets
    FILE nano-stream-targets.cmake
    NAMESPACE nano-stream::
    DESTINATION lib/cmake/nano-stream
)

# Configure package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/nano-stream-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nano-stream-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/nano-stream-config.cmake"
    INSTALL_DESTINATION lib/cmake/nano-stream
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/nano-stream-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/nano-stream-config-version.cmake"
    DESTINATION lib/cmake/nano-stream
)
