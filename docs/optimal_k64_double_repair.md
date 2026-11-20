# OptimalLRC k=64 双块故障测试

参数为 `CodeType=OptimalLRC, k=64, r=3, z=3`，编号为 D0–D63、G0–G2（64–66）、L0–L2（67–69）。当前 XML 已设置这组参数。

运行三组双块故障场景：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 64 double --repeat 30 --warmup 1
```

| 场景 | 读取和计算 |
| --- | --- |
| L0+L1 | L0 proxy 单次读取 D0–D41、G0/G1/G2，一次计算 L0、L1；L0 写入本地 datanode，L1 转发到 owner proxy 写入其 datanode |
| L0+L2 | L0 proxy 单次读取 D0–D20、D42–D63、G0/G1/G2，一次计算 L0、L2；L0 写入本地 datanode，L2 转发到 owner proxy 写入其 datanode |
| G0+G1 | G0 proxy 读取一次 D0–D63，计算 G0、G1；G0 写入本地 datanode，G1 发往所属 proxy，再写入其 datanode |

三个场景均为单任务多输出。每项计时从 coordinator 发起 owner proxy 的 recovery RPC 开始，到源块读取、一次多输出计算、本地写回及远端转发写回全部完成后 RPC 返回为止；不包含 coordinator 预先进行的请求校验和可恢复性检查。

每轮每个场景只输出端到端 `Result`，不再输出本地场景的独立 `Task result` 或任务时间总和。重复测试的 `Summary` 统计该单任务端到端时间，预热不计入统计。

需重新构建并部署 `run_coordinator`、`run_proxy` 和 `repair_benchmark`。proxy 使用共享多输出和 owner 转发写回实现。

本地构建与验证（需要项目现有 Linux 依赖）：

```bash
cmake -S project -B project/build
cmake --build project/build --target run_coordinator run_proxy repair_benchmark optimal_k64_double_repair_verify optimal_k64_quadruple_proxy_integration benchmark_repeat_integration -j
ctest --test-dir project/build --output-on-failure -R 'optimal_k64_double|repair_benchmark_dispatch_verify|benchmark_repeat_integration'
```

`single` 入口仍用于单块故障；本文列出的三组通过 `double` 运行。
