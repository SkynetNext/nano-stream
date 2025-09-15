#include "RingBuffer.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <span>

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(std::min(NextPowerOfTwo(std::max(capacity, kMinCapacity)), kMaxCapacity))
    , mask_(capacity_ - 1) {
    // 预分配固定大小的缓冲区（借鉴 Disruptor 策略）
    buffer_.resize(capacity_);
}

RingBuffer::Error RingBuffer::Write(const std::string& data) {
    if (data.empty()) {
        return Error::kSuccess;
    }

    if (data.size() > kMaxCapacity) {
        failed_writes_.fetch_add(1, std::memory_order_relaxed);
        return Error::kInvalidArgument;
    }

    // 检查是否有足够空间
    size_t current_tail = tail_.load(std::memory_order_acquire);
    size_t current_head = head_.load(std::memory_order_acquire);
    size_t available_space;

    if (current_tail >= current_head) {
        available_space = capacity_ - (current_tail - current_head) - 1;
    } else {
        available_space = current_head - current_tail - 1;
    }

    if (data.size() > available_space) {
        // 固定大小设计：空间不足时直接返回错误（借鉴 Disruptor 策略）
        failed_writes_.fetch_add(1, std::memory_order_relaxed);
        return Error::kInsufficientCapacity;
    }

    // 执行写入操作
    size_t write_pos = current_tail;
    const uint8_t* src_data = reinterpret_cast<const uint8_t*>(data.data());

    // 使用 span 避免指针算术
    auto buffer_span = std::span<uint8_t>(buffer_);
    auto src_span = std::span<const uint8_t>(src_data, data.size());

    if (write_pos + data.size() <= capacity_) {
        // 不需要环绕
        std::memcpy(
            buffer_span.subspan(write_pos, data.size()).data(), src_span.data(), data.size());
    } else {
        // 需要环绕
        size_t first_part = capacity_ - write_pos;
        std::memcpy(buffer_span.subspan(write_pos, first_part).data(), src_span.data(), first_part);
        std::memcpy(buffer_span.subspan(0, data.size() - first_part).data(),
                    src_span.subspan(first_part).data(),
                    data.size() - first_part);
    }

    // 原子地更新尾部指针
    size_t new_tail = (write_pos + data.size()) & mask_;
    tail_.store(new_tail, std::memory_order_release);

    // 更新统计信息
    total_writes_.fetch_add(1, std::memory_order_relaxed);

    return Error::kSuccess;
}

RingBuffer::Error RingBuffer::PrepareWrite(size_t* head, size_t* tail) {
    if (!head || !tail) {
        return Error::kInvalidArgument;
    }

    // 获取当前头尾状态
    *head = head_.load(std::memory_order_acquire);
    *tail = tail_.load(std::memory_order_acquire);

    return Error::kSuccess;
}

RingBuffer::Error RingBuffer::CommitWrite(size_t final_head, size_t final_tail) {
    // 原子地更新头尾指针
    head_.store(final_head, std::memory_order_release);
    tail_.store(final_tail, std::memory_order_release);

    // 更新统计信息
    total_writes_.fetch_add(1, std::memory_order_relaxed);

    return Error::kSuccess;
}

size_t RingBuffer::DoWrite(const void* data, size_t size, size_t* head, size_t* tail) {
    if (!data || size == 0 || !head || !tail) {
        return 0;
    }

    // 检查是否需要扩容
    size_t available_space;
    if (*tail >= *head) {
        available_space = capacity_ - (*tail - *head) - 1;
    } else {
        available_space = *head - *tail - 1;
    }

    if (size > available_space) {
        // 固定大小设计：空间不足时直接返回 0（借鉴 Disruptor 策略）
        return 0;
    }

    // 执行写入（处理环绕）
    auto buffer_span = std::span<uint8_t>(buffer_);
    const uint8_t* src_data = static_cast<const uint8_t*>(data);
    size_t write_pos = *tail;

    if (write_pos + size <= capacity_) {
        // 不需要环绕：直接写入
        auto dest_span = buffer_span.subspan(write_pos, size);
        std::memcpy(dest_span.data(), src_data, size);
    } else {
        // 需要环绕：分两次写入
        size_t first_part = capacity_ - write_pos;
        auto first_dest_span = buffer_span.subspan(write_pos, first_part);
        auto second_dest_span = buffer_span.subspan(0, size - first_part);
        std::memcpy(first_dest_span.data(), src_data, first_part);
        auto src_span = std::span<const uint8_t>(src_data, size);
        std::memcpy(
            second_dest_span.data(), src_span.subspan(first_part).data(), size - first_part);
    }

    // 更新本地尾位置
    *tail = (*tail + size) & mask_;

    return size;
}

std::string RingBuffer::ReadAll() {
    size_t current_head = head_.load(std::memory_order_acquire);
    size_t current_tail = tail_.load(std::memory_order_acquire);

    if (current_head == current_tail) {
        return {};  // 缓冲区为空
    }

    size_t data_size;
    if (current_tail > current_head) {
        data_size = current_tail - current_head;
    } else {
        data_size = capacity_ - current_head + current_tail;
    }

    std::string result;
    result.reserve(data_size);

    // 使用 span 避免指针算术
    auto buffer_span = std::span<const uint8_t>(buffer_);

    if (current_head + data_size <= capacity_) {
        // 不需要环绕
        auto data_span = buffer_span.subspan(current_head, data_size);
        result.assign(reinterpret_cast<const char*>(data_span.data()), data_size);
    } else {
        // 需要环绕
        size_t first_part = capacity_ - current_head;
        auto first_span = buffer_span.subspan(current_head, first_part);
        auto second_span = buffer_span.subspan(0, data_size - first_part);
        result.assign(reinterpret_cast<const char*>(first_span.data()), first_part);
        result.append(reinterpret_cast<const char*>(second_span.data()), data_size - first_part);
    }

    // 原子地更新头部指针
    head_.store(current_tail, std::memory_order_release);

    // 更新统计信息
    total_reads_.fetch_add(1, std::memory_order_relaxed);

    return result;
}

RingBuffer::Statistics RingBuffer::GetStatistics() const {
    Statistics stats;
    stats.total_writes = total_writes_.load(std::memory_order_relaxed);
    stats.total_reads = total_reads_.load(std::memory_order_relaxed);
    stats.failed_writes = failed_writes_.load(std::memory_order_relaxed);
    stats.failed_reads = failed_reads_.load(std::memory_order_relaxed);

    // 计算当前可读数据大小
    size_t current_head = head_.load(std::memory_order_acquire);
    size_t current_tail = tail_.load(std::memory_order_acquire);
    if (current_tail >= current_head) {
        stats.current_size = current_tail - current_head;
    } else {
        stats.current_size = capacity_ - current_head + current_tail;
    }

    return stats;
}

void* RingBuffer::GetBufferAddress() const {
    return const_cast<void*>(static_cast<const void*>(buffer_.data()));
}

bool RingBuffer::IsPowerOfTwo(size_t capacity) {
    return capacity > 0 && std::has_single_bit(capacity);
}

size_t RingBuffer::NextPowerOfTwo(size_t n) {
    if (n == 0)
        return 1;
    if (std::has_single_bit(n))
        return n;
    return std::bit_ceil(n);
}

size_t RingBuffer::GetCapacity() const {
    return capacity_;
}

size_t RingBuffer::GetHead() const {
    return head_.load(std::memory_order_acquire);
}

size_t RingBuffer::GetTail() const {
    return tail_.load(std::memory_order_acquire);
}

RingBuffer::Error RingBuffer::CommitRead(size_t new_head) {
    // 验证参数
    if (new_head >= capacity_) {
        return Error::kInvalidArgument;
    }

    // 原子性更新head位置
    head_.store(new_head, std::memory_order_release);

    return Error::kSuccess;
}
