# Disruptor-CPP 生命周期与所有权

本文档详细描述了 Disruptor-CPP 库中关键对象（Sequence、Barrier、Processor、Thread）的生命周期和所有权关系，以及成员变量的声明顺序如何影响析构顺序，确保资源正确释放。

**重要提示**:
- C++ 中成员变量按声明顺序的**逆序**析构（最后声明的先析构）
- 所有线程必须在析构前 `join()`，否则会触发 `std::terminate`
- 原始指针不拥有所有权，需要确保被指向的对象生命周期足够长

## Sequence 生命周期

```
创建: BatchEventProcessor::sequence_ (成员变量)
  │
  ├─→ BatchEventProcessor::getSequence() → Sequence&
  │
  ├─→ Disruptor::createOne()
  │     └─→ outSequences.push_back(&seq)
  │         └─→ RingBuffer::addGatingSequences(sequences)
  │             └─→ AbstractSequencer::addGatingSequences()
  │                 ├─→ gatingSequences_ (atomic<shared_ptr<vector<Sequence*>>>) ← Sequence* (存储)
  │                 └─→ SingleProducerSequencer/MultiProducerSequencer::gatingSequencesCache_ = nullptr (缓存失效)
  │
  ├─→ ConsumerRepository::add()
  │     └─→ eventProcessorInfoBySequence_[&processor.getSequence()] ← Sequence* (key)
  │
  └─→ EventProcessorInfo::getSequences()
        └─→ static thread_local Sequence* sequences[1] ← Sequence* (临时)

持有:
  - `BatchEventProcessor::sequence_` (拥有，成员变量)
  - `AbstractSequencer::gatingSequences_` (`atomic<shared_ptr<vector<Sequence*>>>`, 存储指针，不拥有所有权)
  - `SingleProducerSequencer/MultiProducerSequencer::gatingSequencesCache_` (缓存的原始指针，用于性能优化，不拥有所有权)
  - `ConsumerRepository::eventProcessorInfoBySequence_` (指针作为 key，不拥有所有权)

销毁:
  BatchEventProcessor 析构 → sequence_ 析构
```

## Barrier 生命周期

```
创建: RingBuffer::newBarrier() → shared_ptr<SequenceBarrier>
  │
  ├─→ Disruptor::createOne()
  │     └─→ ownedBarriers_.push_back(barrier) ← shared_ptr (主所有权)
  │         └─→ EventProcessorInfo(processor, barrier) ← shared_ptr (共享所有权)
  │             └─→ EventProcessorInfo::barrier_ ← shared_ptr
  │
  └─→ Disruptor::keepBarrierAlive_()
        └─→ ownedBarriers_.push_back(barrier)

持有:
  - Disruptor::ownedBarriers_[] (shared_ptr, 主所有权)
  - EventProcessorInfo::barrier_ (shared_ptr, 共享所有权)

销毁:
  Disruptor 析构 → ownedBarriers_ 析构 → shared_ptr 引用计数归零 → Barrier 析构
```

## Processor (DSL创建) 生命周期

```
创建: Disruptor::createOne()
  │
  ├─→ ringBuffer_->newBarrier(...) → barrier (shared_ptr)
  │     └─→ ownedBarriers_.push_back(barrier) ← shared_ptr (主所有权)
  │
  ├─→ std::make_unique<BatchEventProcessor>(...)
  │     └─→ processor->setExceptionHandler(*exceptionHandler_)
  │
  ├─→ ConsumerRepository::add(*processor, handler, barrier)
  │     └─→ std::make_shared<EventProcessorInfo>(*processor, barrier)
  │         └─→ EventProcessorInfo::eventprocessor_ ← EventProcessor* (指针)
  │
  ├─→ ownedProcessors_.push_back(std::move(processor)) ← unique_ptr (拥有)
  │
  └─→ EventProcessorInfo::start()
        └─→ thread_ = threadFactory.newThread([ep] { ep->run(); })
            └─→ lambda 捕获 EventProcessor* ep

持有:
  - Disruptor::ownedBarriers_[] (shared_ptr, 持有 barrier)
  - Disruptor::ownedProcessors_[] (unique_ptr, 拥有 processor)
  - EventProcessorInfo::eventprocessor_ (EventProcessor*, 指针)
  - EventProcessorInfo::barrier_ (shared_ptr, 共享 barrier 所有权)
  - EventProcessorInfo::thread_ lambda (EventProcessor*, 捕获)

销毁:
  Disruptor 析构 → ownedProcessors_ 析构 → Processor 析构
```

## Processor (Factory创建) 生命周期

```
创建: EventProcessorFactory::createEventProcessor() → EventProcessor&
  │
  ├─→ Disruptor::createOne()
  │     └─→ consumerRepository_.add(processor) ← EventProcessor& (引用)
  │         └─→ std::make_shared<EventProcessorInfo>(processor, BarrierPtrT{})
  │             ├─→ EventProcessorInfo::eventprocessor_ ← EventProcessor* (指针)
  │             └─→ EventProcessorInfo::barrier_ ← BarrierPtrT{} (空)
  │
  └─→ EventProcessorInfo::start()
        └─→ thread_ = threadFactory.newThread([ep] { ep->run(); })
            └─→ lambda 捕获 EventProcessor* ep

持有:
  - 用户代码 (拥有，未在 Disruptor 中存储)
  - EventProcessorInfo::eventprocessor_ (EventProcessor*, 指针)
  - EventProcessorInfo::barrier_ (BarrierPtrT, 空，无实际 barrier)
  - EventProcessorInfo::thread_ lambda (EventProcessor*, 捕获)

销毁:
  用户代码销毁 → Processor 析构 (风险: EventProcessorInfo 可能持有悬空指针)
```

## Thread 生命周期

```
创建: EventProcessorInfo::start()
  │
  └─→ thread_ = threadFactory.newThread([ep] { ep->run(); })
      └─→ std::thread (立即启动)

持有:
  - EventProcessorInfo::thread_ (std::thread, 拥有)

销毁:
  ConsumerRepository::joinAll() → EventProcessorInfo::join() → thread_.join()
  EventProcessorInfo 析构 → thread_ 析构 (必须先 join，否则 std::terminate)
```

## 销毁顺序

**重要**: C++ 中成员变量按声明顺序的**逆序**析构（最后声明的先析构）。

```
Disruptor 析构流程:
  │
  ├─→ 1. ~Disruptor() 显式调用 halt()
  │     ├─→ consumerRepository_.haltAll() → EventProcessorInfo::halt()
  │     └─→ consumerRepository_.joinAll() → EventProcessorInfo::join() → thread_.join()
  │         (确保所有线程在析构前已 join，避免 std::terminate)
  │
  ├─→ 2. 成员变量按逆序析构 (最后声明 → 最先析构)
  │     │
  │     ├─→ ownedBarriers_ 析构 (第8个声明，最先析构)
  │     │     └─→ shared_ptr 引用计数减1
  │     │         (如果 consumerRepository_ 中的 EventProcessorInfo 仍持有引用，则不会真正析构)
  │     │
  │     ├─→ exceptionHandler_ 析构 (第7个声明)
  │     │     └─→ unique_ptr，自动释放默认创建的 ExceptionHandlerWrapper
  │     │         ✅ **已修复**: 使用 `std::unique_ptr` 管理，无内存泄漏
  │     │
  │     ├─→ started_ 析构 (第6个声明)
  │     │
  │     ├─→ consumerRepository_ 析构 (第5个声明)
  │     │     └─→ consumerInfos_ 析构
  │     │         └─→ EventProcessorInfo 析构 (shared_ptr 引用计数归零)
  │     │             ├─→ thread_ 析构 (已 join，安全)
  │     │             ├─→ barrier_ 析构 (shared_ptr 引用计数减1)
  │     │             ├─→ endOfChain_ 析构
  │     │             └─→ eventprocessor_ (原始指针，不析构)
  │     │
  │     ├─→ ownedProcessors_ 析构 (第4个声明)
  │     │     └─→ Processor 析构 (unique_ptr 引用计数归零)
  │     │         └─→ sequence_ 析构
  │     │
  │     ├─→ threadFactory_ 析构 (第3个声明，引用，不析构)
  │     │
  │     ├─→ ringBuffer_ 析构 (第2个声明)
  │     │     └─→ shared_ptr 引用计数减1 (如果外部仍持有，则不会真正析构)
  │     │
  │     └─→ ownedWaitStrategy_ 析构 (第1个声明，最后析构)
  │
  └─→ 3. Barrier 真正析构时机
        - ownedBarriers_ 析构时，shared_ptr 引用计数减1，但 EventProcessorInfo::barrier_ 仍持有引用
        - consumerRepository_ 析构时，EventProcessorInfo 析构，barrier_ 析构，shared_ptr 引用计数再减1
        - 只有当所有 shared_ptr 引用都释放后，Barrier 才会真正析构
        - 由于 consumerRepository_ 在 ownedBarriers_ 之后析构，Barrier 会在 consumerRepository_ 析构后才真正析构
```

## 成员变量声明顺序 (Disruptor)

```
class Disruptor {
    std::optional<WaitStrategyT> ownedWaitStrategy_;  // 1. 最先声明 (ringBuffer_ 需要引用)
    std::shared_ptr<RingBufferT> ringBuffer_;         // 2. 
    ThreadFactory &threadFactory_;                    // 3.
    std::vector<std::unique_ptr<EventProcessor>> ownedProcessors_;  // 4. 在 consumerRepository_ 之前
    ConsumerRepository<BarrierPtr> consumerRepository_;  // 5. (持有 EventProcessorInfo，需要 processors 后析构)
    std::atomic<bool> started_;                       // 6.
    ExceptionHandler<T> *exceptionHandler_;            // 7.
    std::vector<BarrierPtr> ownedBarriers_;           // 8. 最后声明
};
```

**关键顺序**:
- `ownedProcessors_` 必须在 `consumerRepository_` 之前声明，确保 processors 在 EventProcessorInfo **之后**析构
  - 由于 C++ 成员变量按**逆序**析构，`consumerRepository_` (第5个) 会先于 `ownedProcessors_` (第4个) 析构
  - 这意味着 `EventProcessorInfo` 先析构，然后 `ownedProcessors_` 才析构
  - **安全性保证**: `halt()` 在析构前被调用，所有线程已停止并 join，`EventProcessorInfo` 析构时不会再访问 processor 指针，因此即使 processor 在 EventProcessorInfo 之后析构也是安全的
  - **注意**: 虽然 `EventProcessorInfo` 持有 `EventProcessor*` 原始指针，但由于 `halt()` 确保线程已停止，且 `EventProcessorInfo` 先于 `ownedProcessors_` 析构，这是安全的
- `ownedBarriers_` 在最后声明，确保 barriers 在 processors 和 repository **之后**析构
  - 由于 `EventProcessorInfo::barrier_` 持有 `shared_ptr` 引用，即使 `ownedBarriers_` 先析构，只要 `EventProcessorInfo` 仍持有引用，Barrier 就不会被销毁
  - `consumerRepository_` 析构时，`EventProcessorInfo` 析构，释放对 barrier 的引用，此时 Barrier 才会真正析构
- `ownedWaitStrategy_` 必须在 `ringBuffer_` 之前声明，因为 `ringBuffer_` 的构造需要引用 `waitStrategy`

## 成员变量声明顺序 (EventProcessorInfo)

```
class EventProcessorInfo {
    EventProcessor* eventprocessor_;  // 1. 先声明 (原始指针，不拥有所有权)
    BarrierPtrT barrier_;             // 2. 再声明 (shared_ptr，共享所有权)
    bool endOfChain_;                 // 3. 状态标志
    std::thread thread_;              // 4. 最后声明 (必须在析构前 join)
};
```

**关键顺序**:
- `thread_` 必须在最后声明，确保在析构时已通过 `join()` 安全处理
- `eventprocessor_` 是原始指针，不拥有所有权（DSL 创建的由 `ownedProcessors_` 拥有，Factory 创建的由用户代码拥有）

## 潜在风险与最佳实践

### 1. Factory 创建的 Processor 生命周期风险

**风险**: 当使用 `EventProcessorFactory` 创建 Processor 时，如果用户在 `Disruptor` 销毁之前销毁了 Processor，`EventProcessorInfo` 将持有悬空指针，可能导致未定义行为。

**最佳实践**:
- 确保 Factory 创建的 Processor 的生命周期至少与 `Disruptor` 一样长
- 在销毁 `Disruptor` 之前，先调用 `halt()` 停止所有 Processor
- 考虑使用 `shared_ptr` 或 `unique_ptr` 管理 Factory 创建的 Processor

### 2. Sequence 指针有效性

**风险**: `gatingSequences_` 和 `eventProcessorInfoBySequence_` 中存储的是原始指针，如果 `BatchEventProcessor` 被提前销毁，这些指针将失效。

**最佳实践**:
- 对于 DSL 创建的 Processor，生命周期由 `Disruptor` 管理，无需担心
- 对于 Factory 创建的 Processor，确保在移除 gating sequence 之前 Processor 仍然有效

### 3. Thread 生命周期管理

**风险**: 如果 `std::thread` 在析构时仍处于 joinable 状态，会触发 `std::terminate`。

**最佳实践**:
- `Disruptor` 的析构函数会自动调用 `halt()`，确保所有线程在析构前已 join
- 手动调用 `halt()` 或 `shutdown()` 时，会确保线程安全退出

### 4. Barrier 引用计数

**说明**: Barrier 使用 `shared_ptr` 管理，`Disruptor::ownedBarriers_` 和 `EventProcessorInfo::barrier_` 共享所有权。即使 `ownedBarriers_` 先析构（第8个声明），只要 `EventProcessorInfo` 仍持有引用，Barrier 就不会被销毁。当 `consumerRepository_` 析构（第5个声明）时，`EventProcessorInfo` 析构，释放对 barrier 的引用，此时 Barrier 才会真正析构。这确保了 Barrier 在 Processor 使用期间保持有效。

### 5. RingBuffer 生命周期

**说明**: `RingBuffer` 由 `shared_ptr` 管理，`Disruptor` 持有引用，但 `start()` 方法也会返回 `shared_ptr`。如果外部代码持有返回的 `shared_ptr`，即使 `Disruptor` 析构，`RingBuffer` 也不会被销毁。这允许在 `Disruptor` 销毁后继续使用 `RingBuffer`。

### 6. ExceptionHandler 内存管理

**✅ 已修复**: `Disruptor` 现在使用 `std::unique_ptr<ExceptionHandler<T>>` 管理默认创建的 `ExceptionHandlerWrapper<T>`，避免了内存泄漏。

**当前实现**:
- `exceptionHandler_` 类型为 `std::unique_ptr<ExceptionHandler<T>>`，自动管理默认创建的 `ExceptionHandlerWrapper<T>` 的生命周期
- 两个构造函数都使用 `std::make_unique<ExceptionHandlerWrapper<T>>()` 创建默认异常处理器
- 当调用 `handleExceptionsWith()` 时，`unique_ptr` 被释放，`exceptionHandlerPtr_` 指向外部传入的异常处理器（由调用者管理生命周期）
- `setDefaultExceptionHandler()` 继续在默认的 wrapper 上工作，通过 `dynamic_cast` 检查确保 wrapper 仍然存在
- `getExceptionHandler()` 辅助方法统一处理两种情况：如果 `exceptionHandlerPtr_` 不为空则返回外部处理器，否则返回 `unique_ptr` 管理的默认处理器
