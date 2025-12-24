# C++ 专家级 - 2025版

## 概述

本文档总结了2025年展示C++专家级能力需要掌握的10大核心问题。

---

## 1. 内存模型与并发原语

### 核心问题
- `std::memory_order` 的6种语义及其使用场景
- `std::atomic` 的 acquire-release 语义与性能影响
- 无锁编程：CAS、ABA问题、内存屏障
- C++20 `std::atomic_ref` 与 `std::atomic<std::shared_ptr>`

### 代码示例

```cpp
// 解释为什么这样写是安全的
std::atomic<int> flag{0};
int data = 0;

// Thread 1
data = 42;
flag.store(1, std::memory_order_release);  // 为什么用 release？

// Thread 2
if (flag.load(std::memory_order_acquire) == 1) {  // 为什么用 acquire？
    assert(data == 42);  // 这个断言为什么成立？
}
```

### 面试展示点
- 理解 release-acquire 语义：release 之前的所有写操作对 acquire 之后可见
- 知道 `memory_order_seq_cst` 的性能代价（全序一致性）
- 能解释为什么无锁编程需要理解 CPU 缓存一致性协议
- 了解 false sharing 对性能的影响

---

## 2. 移动语义与完美转发

### 核心问题
- 左值/右值引用、`std::move` vs `std::forward`
- 移动构造/赋值、异常安全
- 引用折叠与转发引用
- `std::optional`、`std::variant` 的移动优化

### 代码示例

```cpp
template<typename T>
void wrapper(T&& arg) {
    // 解释为什么这里用 forward 而不是 move
    process(std::forward<T>(arg));
}

// 解释移动后对象的状态
std::vector<int> v1{1,2,3};
std::vector<int> v2 = std::move(v1);
// v1 现在是什么状态？为什么？
```

### 面试展示点
- 理解 `std::move` 只是类型转换，不实际移动
- 知道移动构造后源对象处于"有效但未指定"状态
- 能解释引用折叠规则：`T&& &&` → `T&&`
- 理解完美转发如何避免不必要的拷贝

---

## 3. 模板元编程与概念（Concepts）

### 核心问题
- C++20 Concepts 与约束
- SFINAE 与 `requires` 表达式
- 编译期计算：`constexpr`、`consteval`、`constinit`
- 模板特化与偏特化

### 代码示例

```cpp
// C++20 Concepts
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

// 解释为什么 Concepts 比 SFINAE 更好
template<Addable T>
auto add(T a, T b) { return a + b; }

// 编译期计算
constexpr int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n-1) + fibonacci(n-2);
}
constexpr int fib10 = fibonacci(10);  // 编译期计算
```

### 面试展示点
- 理解 Concepts 如何替代复杂的 SFINAE 技巧
- 知道 `constexpr` 函数可以在运行时和编译时使用
- 能解释模板特化的匹配规则
- 了解 `if constexpr` 与普通 `if` 的区别

---

## 4. RAII 与智能指针

### 核心问题
- `unique_ptr`、`shared_ptr`、`weak_ptr` 的实现与开销
- 循环引用与 `weak_ptr` 的使用
- 自定义 deleter 与 `make_shared` 的性能影响
- RAII 与异常安全

### 代码示例

```cpp
// 解释为什么这样会泄漏
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;  // 问题在哪里？
};

// 如何修复？
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // 使用 weak_ptr 打破循环
};

// make_shared 的性能优势
auto ptr1 = std::make_shared<Widget>();  // 一次分配
auto ptr2 = std::shared_ptr<Widget>(new Widget);  // 两次分配
```

### 面试展示点
- 理解 `shared_ptr` 的控制块开销（引用计数、弱引用计数）
- 知道 `make_shared` 将对象和控制块放在一起分配
- 能解释为什么 `weak_ptr` 需要额外的弱引用计数
- 理解 RAII 如何保证异常安全

---

## 5. 标准库容器与算法

### 核心问题
- `std::vector` 的内存布局与重新分配策略
- `std::unordered_map` 的哈希冲突与负载因子
- `std::string_view`、`std::span` 的使用场景
- 算法复杂度与缓存友好性

### 代码示例

```cpp
// 解释为什么这样写性能差
std::vector<std::string> vec;
for (size_t i = 0; i < 1000000; ++i) {
    vec.push_back("very long string...");  // 问题？
    // 多次重新分配，字符串拷贝
}

// 如何优化？
std::vector<std::string> vec;
vec.reserve(1000000);  // 预分配
for (size_t i = 0; i < 1000000; ++i) {
    vec.emplace_back("very long string...");  // 原地构造
}

// string_view 避免拷贝
void process(std::string_view sv) {  // 不拥有字符串
    // 处理字符串，无需拷贝
}
```

### 面试展示点
- 理解 `vector` 的指数增长策略（通常1.5或2倍）
- 知道 `unordered_map` 的负载因子影响性能
- 能解释 `string_view` 如何避免不必要的字符串拷贝
- 理解缓存友好的数据结构设计

---

## 6. 协程（C++20）

### 核心问题
- 协程的底层机制：`promise_type`、`coroutine_handle`
- `co_await`、`co_yield`、`co_return` 的语义
- 无栈协程 vs 有栈协程
- 与异步 I/O 的集成

### 代码示例

```cpp
// 实现一个简单的 generator
template<typename T>
generator<T> fibonacci() {
    T a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a + b;
        a = b;
        b = temp;
    }
}

// 使用
auto fib = fibonacci<int>();
for (int i = 0; i < 10; ++i) {
    std::cout << fib() << " ";  // 0 1 1 2 3 5 8 13 21 34
}

// 异步 I/O 示例
task<void> fetch_data() {
    auto data = co_await async_read_file("data.txt");
    process(data);
}
```

### 面试展示点
- 理解协程的状态机实现机制
- 知道协程栈帧的分配与释放
- 能解释 `co_await` 如何挂起和恢复执行
- 了解协程与回调、Promise 的对比

---

## 7. 编译器优化与性能

### 核心问题
- 内联、常量折叠、死代码消除
- 缓存行对齐与 false sharing
- `[[likely]]`、`[[unlikely]]` 与分支预测
- 编译器屏障：`asm volatile("" ::: "memory")`

### 代码示例

```cpp
// 解释 false sharing
struct alignas(64) Counter {  // 为什么是 64？
    std::atomic<int> count;
    char padding[64 - sizeof(std::atomic<int>)];
};

// 多个 Counter 对象不会共享缓存行

// 分支预测提示
if (condition) [[likely]] {
    // 常见路径
} else [[unlikely]] {
    // 罕见路径
}

// 编译器屏障
void memory_barrier() {
    asm volatile("" ::: "memory");  // 防止编译器重排序
}
```

### 面试展示点
- 理解编译器优化的限制（别名分析、指针逃逸）
- 知道如何用 `alignas` 避免 false sharing
- 能解释 CPU 分支预测的工作原理
- 了解编译器屏障与 CPU 内存屏障的区别

---

## 8. 异常安全与错误处理

### 核心问题
- 异常安全保证：basic、strong、nothrow
- `noexcept` 与性能影响
- `std::expected`（C++23）与错误处理策略
- 异常 vs 错误码的选择

### 代码示例

```cpp
// 解释异常安全保证
class Widget {
    std::vector<int> data_;
public:
    // Basic guarantee: 对象处于有效状态
    void add(int value) {
        data_.push_back(value);  // 如果这里抛异常？
        // 对象状态如何？
    }
    
    // Strong guarantee: 要么成功，要么不变
    void safe_add(int value) {
        auto backup = data_;  // 备份
        try {
            data_.push_back(value);
        } catch (...) {
            data_ = std::move(backup);  // 恢复
            throw;
        }
    }
    
    // Nothrow guarantee: 绝不抛异常
    void clear() noexcept {
        data_.clear();  // clear() 是 noexcept 的
    }
};

// C++23 std::expected
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return std::unexpected("division by zero");
    }
    return a / b;
}
```

### 面试展示点
- 理解三种异常安全保证的区别
- 知道 `noexcept` 如何影响代码生成（可能更优化）
- 能解释何时使用异常，何时使用错误码
- 了解 `std::expected` 如何统一错误处理

---

## 9. 多线程与并发模式

### 核心问题
- `std::thread`、`std::async`、`std::future` 的使用
- 线程池与工作窃取队列
- 无锁数据结构：队列、栈、哈希表
- `std::latch`、`std::barrier`（C++20）

### 代码示例

```cpp
// 实现一个无锁队列
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data{nullptr};
        std::atomic<Node*> next{nullptr};
    };
    std::atomic<Node*> head_{new Node};
    std::atomic<Node*> tail_{head_.load()};
    
public:
    void push(T value) {
        Node* new_node = new Node;
        T* data = new T(std::move(value));
        
        Node* prev_tail = tail_.exchange(new_node, std::memory_order_acq_rel);
        prev_tail->data.store(data, std::memory_order_release);
        prev_tail->next.store(new_node, std::memory_order_release);
    }
    
    bool pop(T& result) {
        Node* head = head_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            return false;  // 队列为空
        }
        
        T* data = next->data.load(std::memory_order_acquire);
        if (data == nullptr) {
            return false;  // 数据还未写入
        }
        
        result = *data;
        head_.store(next, std::memory_order_release);
        delete head;
        delete data;
        return true;
    }
};

// C++20 同步原语
std::latch latch(10);  // 等待10个线程
std::barrier barrier(10);  // 10个线程同步点
```

### 面试展示点
- 理解无锁编程的复杂性（ABA问题、内存回收）
- 知道工作窃取队列如何提高负载均衡
- 能解释 `std::future` 的共享状态开销
- 了解不同并发模式的适用场景

---

## 10. 现代 C++ 特性与最佳实践

### 核心问题
- C++20：Ranges、Modules、Coroutines
- C++23：`std::expected`、`std::mdspan`、`std::print`
- 代码组织：头文件、内联、ODR
- 调试与性能分析工具

### 代码示例

```cpp
// C++20 Ranges
#include <ranges>
#include <vector>
#include <algorithm>

auto result = data 
    | std::views::filter([](int x) { return x > 0; })
    | std::views::transform([](int x) { return x * 2; })
    | std::ranges::to<std::vector>();  // C++23

// C++20 Modules
// math.ixx
export module math;
export int add(int a, int b) { return a + b; }

// main.cpp
import math;
int result = add(1, 2);

// C++23 std::print
#include <print>
std::print("Hello, {}!\n", "World");  // 类型安全，性能好

// C++23 std::mdspan (多维数组视图)
std::vector<int> data(100);
std::mdspan<int, std::extents<size_t, 10, 10>> matrix(data.data());
matrix[2][3] = 42;
```

### 面试展示点
- 理解 Ranges 如何提供组合式算法
- 知道 Modules 如何解决头文件依赖问题
- 能解释 `std::print` 相比 `printf` 的优势
- 了解现代 C++ 的演进方向

---

## 加分项

### 1. 编译器实现细节
- 虚函数表（vtable）的布局
- RTTI（运行时类型信息）的实现
- 异常处理机制（EH table）
- Name mangling 规则

### 2. 标准提案与未来方向
- C++26 可能的特性（reflection、contracts）
- 了解标准委员会的工作流程
- 关注核心语言工作组（CWG）和库工作组（LWG）

### 3. 实际项目经验
- 高性能系统开发（交易系统、游戏引擎）
- 跨平台开发经验
- 大型项目的架构设计
- 性能优化实战案例

### 4. 工具链深入理解
- Clang/LLVM、GCC、MSVC 的差异
- 链接器的工作原理
- 调试符号格式（DWARF、PDB）
- 静态分析与动态分析工具

### 5. 跨语言互操作
- C ABI 调用约定
- FFI（Foreign Function Interface）
- JNI（Java Native Interface）
- Python C API

---

## 面试准备建议

### 1. 准备代码示例
每个主题准备 2-3 个可运行的代码示例，能解释每一行的作用。

### 2. 理解原理
不仅要知道"是什么"，更要理解"为什么"。能解释设计决策背后的原因。

### 3. 性能分析
能用工具（perf、vtune、valgrind）分析性能瓶颈，并给出优化方案。

### 4. 实际项目
能讲清楚这些知识在实际项目中的具体应用，有真实的案例。

### 5. 持续学习
关注 C++ 标准演进、编译器更新、社区动态，展示持续学习能力。

---

## 推荐学习资源

### 书籍
- 《Effective Modern C++》- Scott Meyers
- 《C++ Concurrency in Action》- Anthony Williams
- 《The C++ Programming Language》- Bjarne Stroustrup

### 在线资源
- cppreference.com - 最权威的 C++ 参考
- isocpp.org - C++ 标准委员会官网
- CppCon 视频 - 年度 C++ 会议

### 实践项目
- 实现一个简单的协程库
- 实现无锁数据结构
- 性能优化实战

---

## 总结

这10大问题涵盖了现代C++的核心领域，掌握它们能够展示出专家级的C++能力。记住，专家级不仅仅是知道这些概念，更重要的是：

1. **深度理解**：知道为什么这样设计
2. **实践经验**：能在实际项目中应用
3. **性能意识**：理解性能影响和优化方法
4. **持续学习**：跟上语言和标准的发展

祝面试顺利！


