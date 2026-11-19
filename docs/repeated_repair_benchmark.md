# 固定条带重复修复测试

一次创建条带，先预热 1 轮，再在同一条带上正式测试 30 轮（示例为 k=64 四块故障；参数必须与集群 XML 一致）：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 64 quadruple --repeat 30 --warmup 1
```

程序输出 `stripe=...`。需要在下一次命令中继续使用这个条带时，指定该 ID，例如：

```bash
./cluster_remote.sh benchmark project/config/cluster.ini 64 quadruple --stripe-id 34 --repeat 30 --warmup 1
```

`--stripe-id` 会跳过创建条带和初始数据写入；ID 不存在时直接报错。条带必须来自当前 coordinator，且编码、块大小、节点布局等配置一致。coordinator 重启后，内存中的条带元数据不会保留，届时需要重新创建。不要在同一条带上同时执行多组修复实验。

不传新参数时，保持创建一个条带、测试一轮的行为。`single`、`double`、`triple`、`quadruple` 都支持上述选项。每轮会依次运行该编码和故障类型已经配置的全部场景，不同轮次保持相同条带和块位置。修复仍执行实际读取、计算、转发和写回；修复后再次运行时不会因为目标块已存在而跳过计算。

预热以 `Warmup` 标记，不计入正式统计；预热失败会停止实验。`Summary` 按场景输出成功次数、最小值、中位数、p95 和最大值，耗时沿用原有 coordinator 返回的修复时间。统计只包含成功的正式轮次，任何正式轮次失败仍使整个命令返回失败。

## 线程池

每个 proxy 的直接自定义修复路径在首次使用时创建读取/写回线程池，后续请求复用。默认 32 个工作线程，可以在启动集群时调整（范围 1–128）：

```bash
HLRC_REPAIR_WORKERS=32 ./cluster_remote.sh start project/config/cluster.ini
```

proxy 间入口使用独立的 4 线程池，避免修复线程等待同一个池内的入口任务。两类线程池均随 proxy 进程存活，修改工作线程数需要重启 proxy。此处只复用处理线程，TCP 仍保持一次请求一条短连接；datanode 内部线程实现没有改动。

## 使用与结果解释

修改需同步到 Ali-72、编译并部署新的 `repair_benchmark` 和 `run_proxy`，启动更新后的 proxy 后生效。部署或启动操作按现有集群流程执行；这里只说明用法，不会自动部署。

固定条带消除了不同轮次块位置轮转的影响，线程池减少重复创建线程的开销。预热后复用同一条带会受到操作系统缓存的影响，结果应视为预热后的重复修复性能，与每轮新建条带的结果分开比较。网络排队、丢包重传和磁盘负载仍可能引入波动，此改动本身不保证消除长尾延迟。
