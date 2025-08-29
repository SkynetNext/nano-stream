# Nano-Stream Aeron 构建指南

## 🎯 项目概述

Nano-Stream 现在包含了**完整的Aeron实现**！这是一个header-only的C++实现，包括：

- ✅ 完整的Media Driver架构
- ✅ UDP和IPC传输支持  
- ✅ 真正的客户端-驱动分离
- ✅ NAK重传和流量控制
- ✅ 跨平台网络支持

## 🚀 快速开始

### 1. 构建项目

```bash
# 在项目根目录
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 2. 运行示例

```bash
# 基础Disruptor示例
./examples/basic_example

# 高级Disruptor示例
./examples/advanced_example

# 完整Aeron示例（推荐！）
./examples/complete_aeron_example

# Aeron full功能示例
./examples/aeron_full_example
```

## 📁 项目结构

```
nano-stream/
├── include/
│   ├── nano_stream/          # LMAX Disruptor实现
│   │   ├── ring_buffer.h
│   │   ├── sequence.h
│   │   ├── consumer.h
│   │   └── ...
│   └── aeron/               # 完整Aeron实现
│       ├── aeron.h          # 主客户端API
│       ├── publication.h    # 发布者接口
│       ├── subscription.h   # 订阅者接口
│       ├── context.h        # 配置管理
│       ├── driver/          # Media Driver
│       │   ├── media_driver.h
│       │   ├── driver_conductor.h
│       │   ├── receiver.h
│       │   └── sender.h
│       ├── concurrent/      # 并发工具
│       │   ├── atomic_buffer.h
│       │   └── logbuffer/
│       └── util/           # 工具类
│           ├── memory_mapped_file.h
│           ├── bit_util.h
│           └── network_types.h
├── examples/               # 示例代码
│   ├── basic_example.cpp          # Disruptor基础示例
│   ├── advanced_example.cpp       # Disruptor高级示例
│   ├── complete_aeron_example.cpp # 完整Aeron示例
│   └── aeron_full_example.cpp     # Aeron全功能示例
└── tests/                 # 测试代码
```

## 🔧 API使用指南

### Aeron 基础使用

```cpp
#include "nano_stream_aeron.h"

// 1. 启动Media Driver
auto driver = aeron::driver::MediaDriver::launch();

// 2. 连接客户端
auto aeron = aeron::Aeron::connect();

// 3. 创建发布者
auto publication = aeron->add_publication("aeron:ipc", 1001);

// 4. 创建订阅者
auto subscription = aeron->add_subscription("aeron:ipc", 1001);

// 5. 发送消息
std::string message = "Hello Aeron!";
auto result = publication->offer(
    reinterpret_cast<const std::uint8_t*>(message.data()), 
    message.length()
);

// 6. 接收消息
subscription->poll([](const auto& buffer, std::int32_t offset, 
                     std::int32_t length, const auto& header) {
    std::string msg(reinterpret_cast<const char*>(buffer.buffer() + offset), length);
    std::cout << "Received: " << msg << std::endl;
}, 10);
```

### 预设配置

```cpp
// 超低延迟配置
auto driver = aeron::driver::MediaDriver::launch(
    nano_stream_aeron::presets::ultra_low_latency_driver()
);
auto aeron = aeron::Aeron::connect(
    nano_stream_aeron::presets::ultra_low_latency_client()
);

// 高吞吐量配置
auto driver = aeron::driver::MediaDriver::launch(
    nano_stream_aeron::presets::high_throughput_driver()
);

// 测试配置
auto driver = aeron::driver::MediaDriver::launch(
    nano_stream_aeron::presets::testing_driver()
);
```

### 快速宏

```cpp
// 使用便捷宏
AERON_QUICK_START_DRIVER();
AERON_QUICK_START_CLIENT();

auto pub = AERON_IPC_PUB(1001);
auto sub = AERON_IPC_SUB(1001);

auto udp_pub = AERON_UDP_PUB("localhost:40124", 1001);
auto udp_sub = AERON_UDP_SUB("localhost:40124", 1001);
```

## 🌐 支持的传输

### 1. IPC传输（进程间通信）
```cpp
// 超高性能，同机器不同进程
auto pub = aeron->add_publication("aeron:ipc", stream_id);
auto sub = aeron->add_subscription("aeron:ipc", stream_id);
```

### 2. UDP传输（网络通信）
```cpp
// 跨网络通信
auto pub = aeron->add_publication("aeron:udp?endpoint=192.168.1.100:40124", stream_id);
auto sub = aeron->add_subscription("aeron:udp?endpoint=192.168.1.100:40124", stream_id);
```

### 3. 多播传输
```cpp
// 一对多广播
auto pub = aeron->add_publication("aeron:udp?endpoint=224.0.1.1:40124", stream_id);
auto sub = aeron->add_subscription("aeron:udp?endpoint=224.0.1.1:40124", stream_id);
```

## ⚡ 性能优化

### 1. 线程模式选择
```cpp
// 专用线程模式（最低延迟）
context->threading_mode(aeron::driver::ThreadingMode::DEDICATED);

// 共享网络线程模式（高吞吐量）
context->threading_mode(aeron::driver::ThreadingMode::SHARED_NETWORK);

// 共享模式（资源友好）
context->threading_mode(aeron::driver::ThreadingMode::SHARED);
```

### 2. 缓冲区大小调优
```cpp
// 小缓冲区 - 低延迟
context->publication_term_buffer_length(64 * 1024);

// 大缓冲区 - 高吞吐量  
context->publication_term_buffer_length(16 * 1024 * 1024);
```

### 3. 内存预触碰
```cpp
// 避免页面错误
context->pre_touch_mapped_memory(true);
```

## 🛠️ 故障排除

### 常见问题

1. **编译错误**: 确保使用C++17或更高版本
2. **链接错误**: 检查CMake配置和依赖
3. **运行时错误**: 检查Media Driver是否正常启动
4. **性能问题**: 调整线程模式和缓冲区大小

### 调试模式

```cpp
// 启用详细日志
context->error_handler([](const std::exception& e) {
    std::cerr << "Aeron error: " << e.what() << std::endl;
});
```

## 🎯 与原版Aeron的兼容性

我们的实现基于Aeron Java版本的设计，提供：

- ✅ 相同的API设计模式
- ✅ 相同的传输协议
- ✅ 相同的性能特征
- ✅ Header-only便捷性

## 📚 进一步学习

- 查看 `examples/` 目录中的完整示例
- 阅读 `docs/` 中的详细文档
- 研究 `tests/` 中的单元测试

享受ultra-low latency messaging! 🚀
