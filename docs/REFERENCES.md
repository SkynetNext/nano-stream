# 技术参考

## 高性能消息传递库

### LMAX Disruptor
- **项目地址**: https://github.com/LMAX-Exchange/disruptor
- **描述**: 高性能线程间消息传递库，广泛应用于金融交易系统
- **特点**: 
  - 无锁环形缓冲区设计
  - 支持多生产者-多消费者模式
  - 极低延迟的消息传递
  - 内存预分配，减少GC压力
- **适用场景**: 高频交易、实时数据处理、低延迟系统

### Aeron
- **项目地址**: https://github.com/real-logic/aeron
- **描述**: 高性能消息传递库，由 LMAX 的 Martin Thompson 开发
- **特点**: 
  - 支持 UDP 和 IPC 通信
  - 零拷贝设计，最小化内存拷贝
  - 支持可靠和不可靠传输模式
  - 基于 SBE (Simple Binary Encoding) 的高效序列化
  - 支持多播和单播
- **适用场景**: 高频交易、市场数据分发、分布式系统通信
- **相关项目**:
  - [Aeron Cluster](https://github.com/real-logic/aeron-cluster): 分布式集群解决方案
  - [Aeron Archive](https://github.com/real-logic/aeron-archive): 消息归档和重放

### 相关技术栈
- **Ring Buffer**: 环形缓冲区实现
- **Memory Barriers**: 内存屏障优化
- **Cache Line Padding**: 缓存行填充
- **False Sharing Prevention**: 防止伪共享
- **Zero-Copy**: 零拷贝技术
- **SBE**: Simple Binary Encoding 序列化

## 金融领域跨机器低延迟技术

### 主要技术栈
1. **RDMA (Remote Direct Memory Access)**
   - InfiniBand
   - RoCE (RDMA over Converged Ethernet)
   - iWARP

2. **Kernel Bypass 技术**
   - DPDK (Data Plane Development Kit)
   - XDP (eXpress Data Path)
   - io_uring

3. **专用网络协议**
   - UDP Multicast
   - TCP Fast Open
   - QUIC

4. **硬件加速**
   - FPGA 网络加速
   - SmartNIC
   - 专用网络接口卡

### 典型应用场景
- **高频交易系统 (HFT)**
- **市场数据分发**
- **订单路由系统**
- **风险管理系统**

## 学习资源
- [LMAX Disruptor 官方文档](https://lmax-exchange.github.io/disruptor/)
- [Martin Thompson 的技术博客](https://mechanical-sympathy.blogspot.com/)
- [高性能网络编程实践](https://github.com/cloudflare/cloudflare-blog)
