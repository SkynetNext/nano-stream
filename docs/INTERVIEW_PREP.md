# DEX 岗位面试准备计划

## 🎯 目标岗位

### DEX Blockchain System Expert
- **技能**: Rust/C++、高性能系统、交易撮合、清算结算
- **要求**: 20万订单/秒处理能力

### DEX Trading System Expert  
- **技能**: 共识算法、状态机、低延迟系统
- **要求**: 延迟 < 200ms、吞吐量 ≥ 10K tx/s

## 📚 学习路径

### 第一阶段：C++ 高性能编程 (2周)
```cpp
// 异步编程
auto future = std::async(std::launch::async, processOrder);

// 内存管理
std::unique_ptr<Order> order = std::make_unique<Order>();

// 并发控制
std::atomic<int> counter{0};
```

### 第二阶段：交易系统实现 (3周)
```cpp
class OrderBook {
    std::map<uint64_t, std::vector<Order>> bids_;
    std::map<uint64_t, std::vector<Order>> asks_;
public:
    void addOrder(const Order& order);
    std::vector<Trade> matchOrder(const Order& order);
};

class MatchingEngine {
    OrderBook orderBook_;
    std::thread matchingThread_;
public:
    void submitOrder(const Order& order);
    void matchingLoop();
};
```

### 第三阶段：共识算法实现 (4周)
```cpp
class TendermintNode {
    enum class RoundStep { PROPOSE, PREVOTE, PRECOMMIT, COMMIT };
    RoundStep currentStep_;
    uint64_t currentRound_;
public:
    void propose(const Block& block);
    void vote(const Vote& vote);
    void commit(const Block& block);
};
```

## 🚀 项目实战

### 项目一：高性能交易引擎
- **订单管理**: 订单簿、匹配算法
- **撮合引擎**: 价格时间优先、批量撮合
- **风险控制**: 实时检查、限额管理
- **性能目标**: 延迟 < 1μs，吞吐量 > 1M orders/s

### 项目二：共识算法实现
- **网络层**: P2P通信、消息序列化
- **共识层**: Tendermint、HotStuff
- **状态机**: 交易执行、状态存储
- **性能目标**: 延迟 < 200ms，吞吐量 > 10K tx/s

## 📝 面试准备

### 技术问题
1. **高性能系统设计**: 如何设计 20万订单/秒的交易系统？
2. **共识算法**: Tendermint vs HotStuff 区别？
3. **内存管理**: 如何优化 C++ 内存使用？
4. **并发控制**: 如何设计无锁订单簿？

### 项目展示
1. **技术选型**: 为什么选择 C++？架构设计考虑
2. **性能测试**: 延迟、吞吐量、内存使用数据
3. **问题解决**: 技术难点、解决方案、优化效果

## 🎯 学习资源

### 书籍
- 《C++ Concurrency in Action》
- 《Designing Data-Intensive Applications》

### 开源项目
- [LMAX Disruptor](https://github.com/LMAX-Exchange/disruptor)
- [Aeron](https://github.com/real-logic/aeron)
- [Tendermint](https://github.com/tendermint/tendermint)

## ⏰ 时间安排

- **Week 1-2**: C++ 异步编程、网络编程、内存优化
- **Week 3-4**: 订单簿、撮合引擎、清算系统
- **Week 5-6**: 共识框架、Tendermint、状态机
- **Week 7-8**: 性能优化、测试验证、文档完善

## 🎯 成功标准

- [ ] 交易引擎延迟 < 1μs
- [ ] 共识系统延迟 < 200ms
- [ ] 代码覆盖率 > 90%
- [ ] 技术问题回答熟练
- [ ] 项目展示准备充分
