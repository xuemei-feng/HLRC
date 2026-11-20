# OptimalLRC k=128 三块共享修复

## 语义

配置为 `k=128, r=3, z=5`。每个故障场景只发起一次共享修复 RPC：leader proxy 按给定顺序读取 source blocks 一次，同时计算三个输出；第一个输出写入 leader 的本地 datanode，后两个输出分别转发给各自 owner proxy。

基准测试只报告一次 RPC 的端到端耗时。`sum_task_times=false`，不会输出 `Task result`，也不会把子任务时间相加。重复测试的 `Result` 和 `Summary` 都以这一个端到端时间为样本。

场景及严格 source 顺序：

- `L0+L1+L2`：outputs `131,132,133`，leader `L0`；sources `0..75,128,129,130`。
- `L1+L2+L3`：outputs `132,133,134`，leader `L1`；sources `25..101,128,129,130`。
- `L2+L3+L4`：outputs `133,134,135`，leader `L2`；sources `50..127,128,129,130`。
- `G0+G1+G2`：outputs `128,129,130`，leader `G0`；sources `0..127`。

## 构建与验证

```bash
cmake --build build --target optimal_k128_triple_repair_verify \
  optimal_uneven_verify repair_benchmark benchmark_repeat_integration \
  optimal_k128_triple_proxy_integration
ctest --test-dir build --output-on-failure -R \
  '^(optimal_k128_triple_repair_verify|optimal_uneven_verify|repair_benchmark_dispatch_verify|benchmark_repeat_integration|optimal_k128_triple_proxy_integration)$'
```

真实 proxy 集成测试启动 6 组 proxy/datanode。映射为 data block `id % 6`，global blocks `G0..G2` 映射到 owner `0..2`，local blocks `L0..L4` 映射到 owner `0..4`。测试仅预置 sources，并确认故障 outputs 事先不存在；修复后校验完整字节以及每个输出只存在于正确 owner 节点。

## 部署要求

六个 proxy 必须各自连接一个本地 datanode，并能通过 `nodes.xml` 访问其他 proxy。调度端把三个 outputs 和全部 sources 放在同一个 recovery request 中，并将请求发送给首输出的 owner（leader）。每个 output 的 target proxy 必须指向该 block 的 owner；不要拆成三个 RPC，否则 source 会被重复读取，计时语义也会改变。
