# OptimalLRC k=128 双块共享修复

参数为 `CodeType=OptimalLRC, k=128, r=3, z=5`。块编号为 D0–D127、G0–G2（128–130）和 L0–L4（131–135）。

运行双块修复基准：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 128 double --repeat 30 --warmup 1
```

四个场景都由失败 ID 列表首项对应的 proxy 担任 leader，使用一个 task 读取一次合并后的 sources 并计算两个 outputs：

- `L0+L1`：leader L0（131），sources 为 D0–D49、G0–G2，即 `0..49,128,129,130`。
- `L0+L2`：leader L0（131），sources 为 D0–D24、D50–D75、G0–G2。
- `L2+L3`：leader L2（133），sources 为 D50–D101、G0–G2。
- `G0+G1`：leader G0（128），sources 为 D0–D127。

每个场景的首输出由 leader proxy 写入其配对的本地 datanode，第二输出转发到不同的 owner proxy 后写入该 proxy 配对的 datanode。四个场景均为单 task 双 output，且 `sum_task_times=false`。每轮 `Result` 是从 coordinator 发起 leader proxy recovery RPC，直至源块读取、双输出计算、本地写回和远端转发写回全部完成的端到端时间；不会输出或累加独立 `Task result`。`Summary` 仅统计测量轮次，预热轮次不参与。

部署时必须同步更新并重启 coordinator、proxy 和 benchmark 客户端。owner 映射与 block key 必须和 coordinator 下发目标一致，并保证每对输出位于不同 proxy；所有 source blocks 必须在修复前存在，失败输出不得预先落盘。

本地构建与验证：

```bash
cmake -S project -B project/build
cmake --build project/build --target repair_benchmark optimal_k128_double_repair_verify optimal_uneven_verify benchmark_repeat_integration optimal_k128_double_proxy_integration optimal_k96_triple_proxy_integration optimal_k96_double_proxy_integration optimal_k64_quadruple_proxy_integration -j
ctest --test-dir project/build --output-on-failure -R 'optimal_k128_double_repair_verify|optimal_uneven_verify|repair_benchmark_dispatch_verify|benchmark_repeat_integration|optimal_k128_double_proxy_integration|optimal_k96_triple_proxy_integration|optimal_k96_double_proxy_integration|optimal_k64_double_proxy_integration'
```

`optimal_k128_double_proxy_integration` 启动 6 组真实 proxy/datanode。data owner 为 `id % 6`，G0–G2 映射到 proxy 0–2，L0–L4 映射到 proxy 0–4；测试仅 seed 当前场景的 sources，并逐场断言失败输出事先不存在、首输出本地落盘、第二输出经不同 owner proxy 落盘、字节与原始编码一致，且错误 datanode 上无副本。
