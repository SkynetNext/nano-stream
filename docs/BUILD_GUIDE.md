# Nano-Stream Build Guide

## Prerequisites

Before building Nano-Stream, ensure you have the following installed:

### Windows
1. **Visual Studio 2019 or later** with C++ support
2. **CMake 3.20+** - Download from https://cmake.org/download/
3. **Git** (for downloading dependencies)

### Linux
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake git

# CentOS/RHEL/Fedora
sudo yum install gcc-c++ cmake git
# or for newer versions:
sudo dnf install gcc-c++ cmake git
```

### macOS
```bash
# Using Homebrew
brew install cmake git

# Xcode Command Line Tools
xcode-select --install
```

## Building

### Step 1: Clone and Navigate
```bash
git clone <repository-url>
cd nano-stream
```

### Step 2: Create Build Directory
```bash
mkdir build
cd build
```

### Step 3: Configure with CMake

#### Release Build (Recommended for Performance Testing)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

#### Debug Build (For Development)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

#### Windows with Visual Studio
```bash
cmake .. -G "Visual Studio 16 2019" -A x64
# or for Visual Studio 2022:
cmake .. -G "Visual Studio 17 2022" -A x64
```

### Step 4: Build
```bash
cmake --build . --config Release
```

## Running Tests

After successful build:

### Unit Tests
```bash
# Linux/macOS
./tests/nano_stream_tests

# Windows
.\tests\Release\nano_stream_tests.exe
```

### Benchmarks
```bash
# Linux/macOS
./benchmarks/nano_stream_benchmarks

# Windows
.\benchmarks\Release\nano_stream_benchmarks.exe
```

### Example Program
```bash
# Linux/macOS
./examples/basic_example

# Windows
.\examples\Release\basic_example.exe
```

## Performance Tuning

For optimal performance in production:

### Compiler Flags
The CMakeLists.txt already includes optimal flags:
- `-O3 -march=native -mtune=native` (GCC/Clang)
- `/O2 /arch:AVX2` (MSVC)

### System Configuration

#### Linux
```bash
# Disable CPU frequency scaling
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Increase process priority
sudo nice -n -20 ./your_program

# Pin to specific CPU cores
taskset -c 0,1 ./your_program
```

#### Windows
- Set process priority to "High" or "Real-time" in Task Manager
- Use process affinity to pin to specific CPU cores
- Disable Windows Defender real-time scanning for the build directory

### Memory Configuration

#### Huge Pages (Linux)
```bash
# Enable huge pages
echo 128 | sudo tee /proc/sys/vm/nr_hugepages

# Mount hugetlbfs
sudo mkdir -p /mnt/huge
sudo mount -t hugetlbfs nodev /mnt/huge
```

## Troubleshooting

### Common Issues

1. **CMake Not Found**
   - Ensure CMake is installed and in PATH
   - Windows: Add CMake bin directory to system PATH

2. **C++20 Not Supported**
   - Update compiler: GCC 10+, Clang 11+, MSVC 2019+
   - Check compiler version: `g++ --version` or `clang++ --version`

3. **Google Test/Benchmark Download Fails**
   - Check internet connection
   - For corporate networks, configure proxy settings

4. **Linker Errors on Windows**
   - Ensure correct Visual Studio generator is used
   - Try building with `/permissive-` flag

### Performance Issues

1. **Slow Performance**
   - Ensure Release build (`-DCMAKE_BUILD_TYPE=Release`)
   - Check CPU frequency scaling
   - Verify compiler optimization flags

2. **High Latency**
   - Disable CPU frequency scaling
   - Use process/thread affinity
   - Consider NUMA topology

## Integration

### Using in Your Project

#### CMake Integration
```cmake
find_package(nano-stream REQUIRED)
target_link_libraries(your_target nano-stream::nano-stream)
```

#### Manual Integration
```cmake
add_subdirectory(path/to/nano-stream)
target_link_libraries(your_target nano-stream)
```

## Advanced Configuration

### Custom Memory Allocators
The library supports custom allocators for specific use cases:

```cpp
// Custom allocator example
class CustomAllocator {
    // Implementation
};

// Use with ring buffer
RingBuffer<Event, CustomAllocator> buffer(size, factory, allocator);
```

### NUMA-Aware Configuration
For multi-socket systems:

```cpp
// Bind to specific NUMA node
#include <numa.h>
numa_set_preferred(0);  // Use NUMA node 0

// Create ring buffer on specific node
RingBuffer<Event> buffer(size, factory);
```

## Validation

After building, run the validation suite:

```bash
# Run all tests
ctest --output-on-failure

# Run specific test categories
ctest -R "sequence"  # Only sequence tests
ctest -R "ring"      # Only ring buffer tests
```

Expected benchmark results (varies by hardware):
- Sequence operations: < 1ns
- Ring buffer operations: < 5ns
- Producer-consumer throughput: > 50M events/sec
