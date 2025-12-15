#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * 高性能线程安全环形缓冲区
 *
 * 设计特点（借鉴 Disruptor 设计哲学）：
 * - 无锁设计，使用原子操作保证线程安全
 * - 固定大小预分配，避免动态扩容开销
 * - 缓存行对齐，避免伪共享
 * - 零拷贝操作，性能优异
 * - 简单易用，维护成本低
 */
class RingBuffer {
 public:
    // 配置常量（借鉴 Disruptor 预分配策略）
    static constexpr size_t kDefaultCapacity = 8 * 1024 * 1024;  // 8MB 默认容量（预分配大容量）
    static constexpr size_t kMaxCapacity = 16 * 1024 * 1024;     // 16MB 最大容量
    static constexpr size_t kMinCapacity = 1024 * 1024;          // 1MB 最小容量

    // 错误码（简化，移除扩容相关错误）
    enum class Error {
        kSuccess = 0,
        kInsufficientCapacity = 1,  // 缓冲区空间不足（固定大小，无法扩容）
        kInvalidArgument = 2,
        kBufferFull = 3
    };

    // 统计信息（简化，移除扩容相关统计）
    struct Statistics {
        size_t total_writes{0};
        size_t total_reads{0};
        size_t failed_writes{0};
        size_t failed_reads{0};
        size_t current_size{0};
    };

    /**
     * 构造函数（借鉴 Disruptor 预分配策略）
     * @param capacity 固定容量，必须是2的幂次，运行时不可改变
     */
    explicit RingBuffer(size_t capacity = kDefaultCapacity);

    /**
     * 析构函数
     */
    ~RingBuffer() = default;

    // 禁用拷贝和移动
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    /**
     * 写入数据 - 线程安全
     * @param data 要写入的数据
     * @return Error::kSuccess 成功，其他值表示失败
     */
    Error Write(const std::string& data);

    /**
     * 准备写入 - 获取当前头尾状态
     * @param head 返回当前头位置
     * @param tail 返回当前尾位置
     * @return Error::kSuccess 成功，其他值表示失败
     */
    Error PrepareWrite(size_t* head, size_t* tail);

    /**
     * 执行写入 - 写入数据并处理环绕，维护本地尾值
     * @param data 要写入的数据
     * @param size 数据大小
     * @param head 当前头位置（只读）
     * @param tail 当前尾位置（输入输出参数）
     * @return 实际写入的大小，0表示失败
     */
    size_t DoWrite(const void* data, size_t size, size_t head, size_t* tail);

    /**
     * 提交写入 - 提交最终的尾位置
     * @param new_tail 新的尾位置
     * @return Error::kSuccess 成功，其他值表示失败
     */
    Error CommitWrite(size_t new_tail);

    /**
     * 提交读取 - 更新头位置（用于 C# 端读取后更新）
     * @param new_head 新的头位置
     * @return Error::kSuccess 成功，其他值表示失败
     */
    Error CommitRead(size_t new_head);

    /**
     * 读取所有可用数据 - 线程安全
     * @return 读取的数据，空字符串表示没有数据
     */
    std::string ReadAll();

    /**
     * 获取统计信息
     * @return 统计信息结构
     */
    Statistics GetStatistics() const;

    /**
     * 获取缓冲区内存地址（供Unity C#使用）
     * @return 缓冲区内存地址
     */
    void* GetBufferAddress() const;

    /**
     * 获取缓冲区容量
     * @return 当前缓冲区容量
     */
    size_t GetCapacity() const;

    /**
     * 获取当前头位置
     * @return 当前头位置
     */
    size_t GetHead() const;

    /**
     * 获取当前尾位置
     * @return 当前尾位置
     */
    size_t GetTail() const;

 private:
    // 缓存行对齐，避免伪共享
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    // 缓冲区数据（固定大小，预分配）
    std::vector<uint8_t> buffer_;
    const size_t capacity_;  // 固定容量，不可改变
    const size_t mask_;      // 用于快速取模运算 (capacity_ - 1)

    // 统计信息（简化）
    mutable std::atomic<size_t> total_writes_{0};
    mutable std::atomic<size_t> total_reads_{0};
    mutable std::atomic<size_t> failed_writes_{0};
    mutable std::atomic<size_t> failed_reads_{0};

    /**
     * 检查容量是否为2的幂次
     * @param capacity 要检查的容量
     * @return true 如果是2的幂次
     */
    static bool IsPowerOfTwo(size_t capacity);

    /**
     * 计算大于等于n的最小2的幂次
     * @param n 输入值
     * @return 最小2的幂次
     */
    static size_t NextPowerOfTwo(size_t n);
};
