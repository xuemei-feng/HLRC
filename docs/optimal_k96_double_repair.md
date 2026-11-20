# OptimalLRC k=96 双块修复

参数为 `CodeType=OptimalLRC, k=96, r=3, z=4`。块编号为 D0–D95、G0–G2（96–98）、L0（99）和 L1（100）。

运行双块修复基准：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 96 double --repeat 30 --warmup 1
```

- `L0+L1`：L0 proxy 按 D0–D47、G0–G2 的顺序单次读取 51 个源块，一次计算输出 L0 和 L1。L0 写入 leader proxy 配对的本地 datanode；L1 转发给其 owner proxy，再写入该 proxy 配对的 datanode。
- `G0+G1`：G0 proxy 按 D0–D95 的顺序单次读取 96 个源块，一次计算输出 G0 和 G1。G0 本地写回；G1 转发给其 owner proxy 写回。

两个场景均为单 task 多 output，且 `sum_task_times=false`。每轮 `Result` 是 coordinator 发起 leader proxy recovery RPC 到该 RPC 在源块读取、双输出计算、本地写回和远端转发写回全部完成后返回的端到端时间。不会输出或累加独立 `Task result`；`Summary` 统计重复轮次的端到端时间，预热轮次不参与统计。

部署时必须同步更新并重启 coordinator、proxy 和 benchmark 客户端，且每个 proxy 必须只把本地输出写到其配对 datanode。所有 source block 必须在修复前存在；失败输出不应预先落盘。owner 映射及 block key 必须与 coordinator 下发的目标 proxy 和 datanode 配置一致。

本地构建与验证：

```bash
cmake -S project -B project/build
cmake --build project/build --target repair_benchmark optimal_k96_double_repair_verify optimal_uneven_verify benchmark_repeat_integration optimal_k96_double_proxy_integration optimal_k64_quadruple_proxy_integration -j
ctest --test-dir project/build --output-on-failure -R 'optimal_k96_double_repair_verify|optimal_uneven_verify|repair_benchmark_dispatch_verify|benchmark_repeat_integration|optimal_k96_double_proxy_integration|optimal_k64_double_proxy_integration'
```

真实 proxy/datanode 集成测试仅 seed 计划中的 source blocks，并验证失败输出事先不存在、首输出由 leader proxy 本地写回、第二输出经 owner proxy 写入正确 datanode，以及两个持久化块与原始编码字节完全一致。
