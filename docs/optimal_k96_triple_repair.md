# OptimalLRC k=96 三块共享修复

## 场景

配置为 `k=96, r=3, z=4`，三块故障基准包含两个共享任务：

- `L0+L1+L2`（块 `99,100,101`）：由 L0 owner proxy 一次读取 `D0..D71,G0..G2`（共 75 块），一次计算三个输出；L0 本地写入，L1、L2 分别转发给各自 owner proxy 写入。
- `G0+G1+G2`（块 `96,97,98`）：由 G0 owner proxy 一次读取 `D0..D95`（共 96 块），一次计算三个输出；G0 本地写入，G1、G2 分别转发给各自 owner proxy 写入。

两个场景均以单个共享 task 执行，基准报告 coordinator RPC 的端到端墙钟时间，不累加 task 时间。

## 构建与验证

```bash
cmake -S project -B project/build
cmake --build project/build -j --target \
  optimal_k96_triple_repair_verify optimal_uneven_verify repair_benchmark \
  benchmark_repeat_integration optimal_k96_triple_proxy_integration
ctest --test-dir project/build --output-on-failure -R \
  'optimal_k96_triple_repair_verify|optimal_uneven_verify|repair_benchmark_dispatch_verify|benchmark_repeat_integration|optimal_k96_triple_proxy_integration'
```

真实 proxy/datanode 集成测试仅预置计划列出的 source 块，确认故障输出事先不存在，并验证首输出写入 leader 的本地 datanode、后两个输出经不同 owner proxy 写入正确 datanode；同时核对完整字节内容，并确认错误节点无副本。

## 部署要求

每个块必须具有明确且唯一的 owner proxy/datanode 映射。任务首个故障块的 owner 是 leader；其 proxy 必须能访问所有 source owner proxy，并能连接其余两个输出的 owner proxy。每个 owner proxy 只应把收到的修复输出写入其配对 datanode。生产配置还需保证 proxy RPC、datanode RPC 和转发端口互通，且 block key 在各节点上一致。
