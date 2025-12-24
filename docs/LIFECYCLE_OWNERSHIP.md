# Disruptor-CPP 生命周期与所有权

## Sequence 生命周期

```
创建: BatchEventProcessor::sequence_ (成员变量)
  │
  ├─→ BatchEventProcessor::getSequence() → Sequence&
  │
  ├─→ Disruptor::createOne()
  │     └─→ outSequences.push_back(&seq)
  │         └─→ RingBuffer::addGatingSequences(sequences)
  │             └─→ gatingSequences_[] ← Sequence*
  │
  ├─→ ConsumerRepository::add()
  │     └─→ eventProcessorInfoBySequence_[&processor.getSequence()] ← Sequence* (key)
  │
  └─→ EventProcessorInfo::getSequences()
        └─→ static thread_local Sequence* sequences[1] ← Sequence* (临时)

持有:
  - BatchEventProcessor::sequence_ (拥有)
  - RingBuffer::gatingSequences_[] (指针)
  - ConsumerRepository::eventProcessorInfoBySequence_ (指针作为key)

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
  ├─→ std::make_unique<BatchEventProcessor>(...)
  │     └─→ ownedProcessors_.push_back(std::move(processor)) ← unique_ptr (拥有)
  │
  ├─→ ConsumerRepository::add(*processor, handler, barrier)
  │     └─→ std::make_shared<EventProcessorInfo>(*processor, barrier)
  │         └─→ EventProcessorInfo::eventprocessor_ ← EventProcessor* (指针)
  │
  └─→ EventProcessorInfo::start()
        └─→ thread_ = threadFactory.newThread([ep] { ep->run(); })
            └─→ lambda 捕获 EventProcessor* ep

持有:
  - Disruptor::ownedProcessors_[] (unique_ptr, 拥有)
  - EventProcessorInfo::eventprocessor_ (EventProcessor*, 指针)
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
  │         └─→ EventProcessorInfo::eventprocessor_ ← EventProcessor* (指针)
  │
  └─→ EventProcessorInfo::start()
        └─→ thread_ = threadFactory.newThread([ep] { ep->run(); })
            └─→ lambda 捕获 EventProcessor* ep

持有:
  - 用户代码 (拥有，未在 Disruptor 中存储)
  - EventProcessorInfo::eventprocessor_ (EventProcessor*, 指针)
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

```
Disruptor 析构
  │
  ├─→ ~Disruptor() → halt()
  │
  ├─→ ownedBarriers_ 析构 (shared_ptr 引用计数归零)
  │     └─→ Barrier 析构
  │
  ├─→ consumerRepository_ 析构
  │     └─→ consumerInfos_ 析构
  │         └─→ EventProcessorInfo 析构
  │             ├─→ thread_ 析构 (必须先 join)
  │             ├─→ barrier_ 析构 (shared_ptr 引用计数归零)
  │             └─→ eventprocessor_ (原始指针，不析构)
  │
  └─→ ownedProcessors_ 析构
        └─→ Processor 析构
            └─→ sequence_ 析构
```

## 成员变量声明顺序 (Disruptor)

```
class Disruptor {
    std::vector<BarrierPtr> ownedBarriers_;           // 1. 先声明
    std::vector<std::unique_ptr<EventProcessor>> ownedProcessors_;  // 2. 再声明
    ConsumerRepository<BarrierPtr> consumerRepository_;  // 3. 最后声明
};
```

## 成员变量声明顺序 (EventProcessorInfo)

```
class EventProcessorInfo {
    EventProcessor* eventprocessor_;  // 1. 先声明
    BarrierPtrT barrier_;            // 2. 再声明
    std::thread thread_;              // 3. 最后声明 (必须在析构前 join)
};
```
