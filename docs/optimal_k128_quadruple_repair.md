# OptimalLRC k=128 四块共享修复

## 场景与 source 顺序

配置为 `k=128, r=3, z=5`。每个场景都是单 task、四 output 的一次共享修复：

- `L0+L1+L2+L3`：outputs `131,132,133,134`，leader `L0`；sources `0..101,128,129,130`。
- `L1+L2+L3+L4`：outputs `132,133,134,135`，leader `L1`；sources `25..127,128,129,130`。
- `G0+G1+G2+L0`：outputs `128,129,130,131`，leader `G0`；sources `0..127`。

source 顺序是协议的一部分，不得排序、分组或拆成多个请求。首 output 决定 leader；同一请求计算四个 outputs，并按 block owner 写回。

## 计时语义

所有场景均为 `sum_task_times=false`。每轮只报告一次从 coordinator 发起 leader recovery RPC 到全部读取、计算、本地写回和远端写回完成的端到端时间。不会输出 `Task result`，不会累加子任务时间；重复测试的 `Result` 和 `Summary` 都使用这一端到端样本。

## 构建与测试

```bash
cmake -S project -B project/build
cmake --build project/build --target \
  optimal_k128_quadruple_repair_verify optimal_uneven_verify repair_benchmark \
  benchmark_repeat_integration optimal_k128_quadruple_proxy_integration
ctest --test-dir project/build --output-on-failure -R \
  '^(optimal_k128_quadruple_repair_verify|optimal_uneven_verify|repair_benchmark_dispatch_verify|benchmark_repeat_integration|optimal_k128_quadruple_proxy_integration)$'
```

真实集成测试启动 6 组 proxy/datanode，仅预置 sources，确认四个故障 outputs 预先不存在，随后校验完整字节和错误节点无副本。owner 映射为 data `id % 6`、globals `G0..G2 -> 0..2`、locals `L0..L4 -> 0..4`。前两个场景的后三个 outputs 分别转发到三个互异的远端 owner；第三个场景中 `G0` 与 `L0` 按指定映射都属于 owner 0，因此 `L0` 与 leader 共置，`G1`、`G2` 分别转发到 owner 1、2。

## 部署

至少部署 6 组一一配对的 proxy/datanode，并在 `nodes.xml` 中配置互访地址。调度端必须把四个 outputs、精确顺序的 sources 和每个 output 的 target owner 放入一个 recovery request，并发送给首 output 的 owner。不要拆分 RPC，否则会重复读取 sources，并破坏共享端到端计时语义。
