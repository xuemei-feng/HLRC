# 调整修复流量的排队策略

`limit_common.sh` 现在默认在物理网卡出站和 IFB 入站的 HTB 受限类 `1:10` 下配置 `fq_codel`。不同 TCP 流分别排队，CoDel 控制持续排队时间。现有带宽上限、sport/dport 分类及非修复流量类 `1:30` 保持原设置。

这是一组待实测的起始参数，不保证消除所有丢包或耗时异常。CoDel 本身也可能为控制拥塞而丢包；不能仅以 drops 是否为零判断效果。

## 应用

将本地修改后的 **limit_common.sh** 上传到 Ali-72 的 `/root/HLRC/limit_common.sh`。无需重新编译 C++，也无需为此覆盖 XML 或 cluster.ini。

在两轮实验之间，于 Ali-72 的仓库根目录执行：

```bash
cd /root/HLRC
bash limit_all_2Gb.sh
```

已有 `limit_all_2Gb.sh` 会同步限速脚本到配置的全部存储节点，再同时配置各节点的物理网卡和 IFB。成功输出应包含 `queue=fq_codel`。

`cluster_remote.sh deploy` 不同步根目录的限速脚本，单独执行它不能应用此次修改。

## 确认实际生效

在存储节点执行（物理网卡名按实际情况替换）：

```bash
tc -s qdisc show dev eth0
tc -s qdisc show dev ifb0
tc class show dev eth0
tc class show dev ifb0
```

两个设备都应出现 `qdisc fq_codel 110: parent 1:10`，受限类的 rate/ceil 仍约为 2097 Mbit/s，即原脚本的 2 Gb/s 档位。可从 Ali-72 用 `ssh root@172.16.2.127 'tc -s qdisc show dev ifb0'` 查看示例存储节点。

## 参数与恢复原策略

| 环境变量 | 默认值 | 含义 |
| --- | --- | --- |
| PROXY_QUEUE_MODE | fq_codel | `fq_codel` 或原策略 `legacy` |
| PROXY_QUEUE_LIMIT | 4096 | 每个叶子队列的包数上限 |
| PROXY_QUEUE_MEMORY_BYTES | 16777216 | 每个叶子队列的内存上限，16 MiB |
| PROXY_QUEUE_TARGET_MS | 5 | CoDel 的目标排队时间，不是人为增加的等待 |
| PROXY_QUEUE_INTERVAL_MS | 100 | CoDel 检查持续拥塞的时间区间 |

包数和内存上限共同约束容量，先到达哪一个就受哪一个限制。TARGET 不是整个修复的延迟保证。

例如比较 2048 包、8 MiB：

```bash
PROXY_QUEUE_LIMIT=2048 PROXY_QUEUE_MEMORY_BYTES=8388608 bash limit_all_2Gb.sh
```

恢复原来的 HTB 叶子队列策略，同时保留 2 Gb/s 限速：

```bash
PROXY_QUEUE_MODE=legacy bash limit_all_2Gb.sh
```

环境变量会显式传给所有远端节点。只对当前这次命令生效；后续不带变量执行时恢复脚本默认参数。

## 验证

```bash
bash -n limit_common.sh
bash project/tests/limit_queue_policy_verify.sh
sudo unshare -n -- bash project/tests/limit_queue_policy_verify.sh --kernel
```

内核检查只在隔离网络命名空间内创建 dummy/IFB 设备，验证双向叶子队列、rate/ceil、过滤器、参数保护及恢复原策略，不修改当前主机网卡。退出命名空间后设备自动消失。

正式性能验证应固定条带和节点，在同样块大小、带宽和缓存条件下比较中位数、p95、最大值，并关联发送端 TCP 重传。此次队列修改不包括连接复用、读取并发调整或修复算法变更。
