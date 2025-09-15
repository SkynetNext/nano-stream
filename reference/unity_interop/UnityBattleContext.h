#pragma once

#include "RingBuffer.h"

#include <memory>
#include <string>

// 前向声明
class Battle;

/**
 * Unity战斗上下文 - 基于环形缓冲区的零拷贝架构
 * 实现Unity C#与C++之间的高性能无锁通信
 */
class UnityBattleContext {
 public:
    UnityBattleContext();
    ~UnityBattleContext();

    // 初始化战斗上下文
    bool Initialize(const char* battle_enter_info, int data_size, bool is_server = false);

    // 清理资源
    void Cleanup();

    // 核心更新逻辑（每帧调用一次，唯一的P/Invoke）
    void Update(float delta_time);

    // 获取缓冲区状态信息
    struct BufferInfo {
        bool connected{false};
        size_t total_size{0};
        RingBuffer::Statistics input_stats;
        RingBuffer::Statistics output_stats;
    };

    BufferInfo GetBufferInfo() const;

    // 获取输入缓冲区内存地址（供Unity C#使用）
    void* GetInputBufferAddress() const;

    // 获取输出缓冲区内存地址（供Unity C#使用）
    void* GetOutputBufferAddress() const;

    // 获取缓冲区大小
    size_t GetBufferTotalSize() const;

    // 获取输入缓冲区（供内部使用）
    RingBuffer* GetInputBuffer() const {
        return input_buffer_.get();
    }

    // 获取输出缓冲区（供内部使用）
    RingBuffer* GetOutputBuffer() const {
        return output_buffer_.get();
    }

 private:
    // 处理输入消息
    void ProcessIncomingMessages();

    // 解析并分发单个消息
    bool parseAndDispatchMessage(uint16_t messageId, const std::string& messageBody);

    // 内部成员
    std::unique_ptr<Battle> battle_;

    // 双向环形缓冲区
    std::unique_ptr<RingBuffer> input_buffer_;   // Unity → C++
    std::unique_ptr<RingBuffer> output_buffer_;  // C++ → Unity

    // 状态标识
    bool initialized_{false};

    // 常量配置
    static constexpr size_t kDefaultBufferSize = RingBuffer::kDefaultCapacity;
};