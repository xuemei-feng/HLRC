# OptimalLRC k=64 双块故障测试

参数为 `CodeType=OptimalLRC, k=64, r=3, z=3`，编号为 D0–D63、G0–G2（64–66）、L0–L2（67–69）。当前 XML 已设置这组参数。

运行三组双块故障场景：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 64 double --repeat 30 --warmup 1
```

| 场景 | 读取和计算 |
| --- | --- |
| L0+L1 | L0 proxy 读取 D0–D20、G0/G1/G2；L1 proxy 读取 D21–D41、G0/G1/G2，各自计算并写回 |
| L0+L2 | L0 proxy 读取 D0–D20、G0/G1/G2；L2 proxy 读取 D42–D63、G0/G1/G2，各自计算并写回 |
| G0+G1 | G0 proxy 读取一次 D0–D63，计算 G0、G1；G0 写入本地 datanode，G1 发往所属 proxy，再写入其 datanode |

本地校验块任务沿用并行执行。每项计时从 coordinator 发起对应 proxy 的 recovery RPC 开始，到读取、计算和写回完成后 RPC 返回为止；不包含 coordinator 预先进行的请求校验和可恢复性检查。

每轮 L0+L1 输出 `Task result ... L0+L1/L0`、`Task result ... L0+L1/L1` 两项时间，再输出 `Result ... L0+L1: ... s (sum; wall=... s)`；L0+L2 同理。`sum` 是两项时间之和，`wall` 是整组请求从 coordinator 入口到完成的实际耗时。重复测试的 `Summary` 分别统计各项和总和，预热不计入统计。G0+G1 的 `Result` 包含读取、两个输出的计算、本地写回和远端转发写回。

更新涉及 coordinator 回复协议，需要重新构建、部署并重启 coordinator，同时更新 `repair_benchmark`。proxy 需运行当前源码构建的 `run_proxy`，以使用共享多输出和转发写回实现；测试旧 coordinator 缺失分项时间时会明确报错。

本地构建与验证（需要项目现有 Linux 依赖）：

```bash
cmake -S project -B project/build
cmake --build project/build --target run_coordinator run_proxy repair_benchmark optimal_k64_double_repair_verify optimal_k64_quadruple_proxy_integration benchmark_repeat_integration -j
ctest --test-dir project/build --output-on-failure -R 'optimal_k64_double|repair_benchmark_dispatch_verify|benchmark_repeat_integration'
```

`single` 入口仍用于单块故障；本文列出的三组通过 `double` 运行。
