# NLRC k=64 三块故障测试

参数为 `CodeType=NLRC, k=64, r=3, z=5`，编号为 D0–D63、G0–G2（64–66）、L0–L4（67–71）。当前 XML 已设置这组参数。

运行三组三块故障场景：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 64 triple --repeat 30 --warmup 1
```

| 场景 | 读取和计算 |
| --- | --- |
| L0+L1+L2 | L0 proxy 读取 D0–D21，一次计算 L0、L1；L0 本地落盘，L1 转发到所属 proxy 落盘。L2 proxy 读取 D22–D43，计算并写回 L2 |
| L0+L1+L4 | L0 proxy 读取 D0–D21，一次计算 L0、L1；L0 本地落盘，L1 转发到所属 proxy 落盘。L4 proxy 读取 D44–D63、G0/G1/G2，计算并写回 L4 |
| G0+G1+G2 | G0 proxy 读取一次 D0–D63，计算 G0、G1、G2；G0 本地落盘，G1、G2 分别转发到所属 proxy 落盘 |

前两种场景的两个任务并行执行。每项计时从 coordinator 发起对应 proxy 的 recovery RPC 开始，到读取、计算、转发和所有写回完成后 RPC 返回为止。

每轮分别输出 `.../L0+L1` 与 `.../L2`（或 `.../L4`）的耗时，再输出 `Result ...: ... s (sum; wall=... s)`。`sum` 是两项时间之和，`wall` 是整组并行请求的实际耗时。重复测试会分别统计各项和总和，预热不计入统计。G0+G1+G2 输出完整共享任务的端到端耗时。

需要用当前源码重新构建并部署 `run_coordinator`、`run_proxy` 和 `repair_benchmark`，然后重启 coordinator 与 proxy。

本地构建与验证：

```bash
cmake -S project -B project/build
cmake --build project/build --target run_coordinator run_proxy repair_benchmark nlrc_k64_triple_repair_verify nlrc_k64_quadruple_proxy_integration benchmark_repeat_integration -j
ctest --test-dir project/build --output-on-failure -R 'nlrc_k64_triple|repair_benchmark_dispatch_verify|benchmark_repeat_integration'
```
