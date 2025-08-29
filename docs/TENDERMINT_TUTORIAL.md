# Tendermint 实战教程

## 1. 核心概念

### 三阶段提交
```
Propose → Prevote → Precommit
```

### ABCI 接口
```go
type Application interface {
    InitChain(RequestInitChain) ResponseInitChain
    BeginBlock(RequestBeginBlock) ResponseBeginBlock
    DeliverTx(RequestDeliverTx) ResponseDeliverTx
    EndBlock(RequestEndBlock) ResponseEndBlock
    Commit() ResponseCommit
    Query(RequestQuery) ResponseQuery
}
```

## 2. 快速开始

### 安装和初始化
```bash
# 安装
go install github.com/tendermint/tendermint/cmd/tendermint@latest

# 初始化节点
tendermint init

# 启动节点
tendermint node
```

### 简单 KV 存储应用
```go
type KVStoreApplication struct {
    types.BaseApplication
    state map[string]string
}

func (app *KVStoreApplication) DeliverTx(req types.RequestDeliverTx) types.ResponseDeliverTx {
    var tx struct {
        Key   string `json:"key"`
        Value string `json:"value"`
    }
    json.Unmarshal(req.Tx, &tx)
    app.state[tx.Key] = tx.Value
    return types.ResponseDeliverTx{Code: 0}
}
```

## 3. 网络配置

### 多节点设置
```toml
[p2p]
persistent_peers = "node1@127.0.0.1:26657,node2@127.0.0.1:26658"
max_num_inbound_peers = 40
max_num_outbound_peers = 10
```

### 启动网络
```bash
# 节点 0
tendermint node --proxy_app=kvstore

# 节点 1
tendermint node --proxy_app=kvstore --p2p.laddr=tcp://0.0.0.0:26657

# 节点 2
tendermint node --proxy_app=kvstore --p2p.laddr=tcp://0.0.0.0:26658
```

## 4. 交易和查询

### 发送交易
```bash
curl -X POST http://localhost:26657/broadcast_tx_commit \
  -H "Content-Type: application/json" \
  -d '{"tx": "eyJrZXkiOiAidGVzdCIsICJ2YWx1ZSI6ICJoZWxsbyJ9"}'
```

### 查询状态
```bash
curl -X POST http://localhost:26657/abci_query \
  -H "Content-Type: application/json" \
  -d '{"data": "dGVzdA==", "path": "", "prove": false}'
```

## 5. 性能优化

### 配置优化
```toml
[consensus]
timeout_propose = "3s"
timeout_prevote = "1s"
timeout_precommit = "1s"
timeout_commit = "5s"
max_block_size_txs = 10000
```

### 内存优化
```go
var votePool = sync.Pool{
    New: func() interface{} { return &Vote{} },
}
```

## 6. 故障处理

### 节点恢复
```bash
# 检查状态
curl http://localhost:26657/status

# 重置节点
tendermint unsafe_reset_all
```

### 日志分析
```bash
tail -f ~/.tendermint/logs/tendermint.log | grep consensus
```

## 7. 实际应用

### 投票系统
```go
type VoteApp struct {
    types.BaseApplication
    votes map[string]int
    voters map[string]bool
}

func (app *VoteApp) DeliverTx(req types.RequestDeliverTx) types.ResponseDeliverTx {
    var vote struct {
        Voter string `json:"voter"`
        Candidate string `json:"candidate"`
    }
    json.Unmarshal(req.Tx, &vote)
    
    if app.voters[vote.Voter] {
        return types.ResponseDeliverTx{Code: 1, Log: "Already voted"}
    }
    
    app.votes[vote.Candidate]++
    app.voters[vote.Voter] = true
    return types.ResponseDeliverTx{Code: 0}
}
```

## 8. 监控和调试

### 性能监控
```bash
# 节点状态
curl http://localhost:26657/status

# 网络信息
curl http://localhost:26657/net_info

# 共识状态
curl http://localhost:26657/consensus_state
```

### 调试工具
```bash
tendermint debug
```

## 9. 最佳实践

1. **状态管理**: 保持应用状态一致性
2. **错误处理**: 正确验证交易
3. **性能优化**: 使用对象池和并行处理
4. **网络配置**: 至少 4 个节点，正确配置防火墙
5. **监控部署**: 使用 Prometheus + Grafana
6. **备份更新**: 定期备份，及时更新版本
