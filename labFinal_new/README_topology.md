# labFinal topology version

本版本包含：

1. K-way 多路 FM 划分；
2. TopoPart 数据格式读取：第一行节点数、第二行边数、后续 edge lines、最后固定节点行；
3. FPGA/MFS 拓扑图读取；
4. 固定节点约束；
5. 拓扑约束初始化与拓扑合法 refinement；
6. 大规模 TopoPart 自动 fast topology mode。

## 编译

```bash
make clean && make
```

## IBM .hgr 多路划分

```bash
./main ../dataset/ibm01.hgr 4 0.25
```

## TopoPart 拓扑约束划分

推荐先使用快速模式；程序会在大规模拓扑数据上自动启用：

```bash
./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

快速模式逻辑是：拓扑驱动初始化 + 拓扑修复 + 一轮拓扑合法贪心 refinement，避免完整 K-way FM 在 30 万节点、74.9 万边、43 路划分上运行过久。

可调环境变量：

```bash
VLSI_TOPO_SWEEPS=0 ./main case1 MFS2 0.0        # 最快，只做初始化和拓扑修复
VLSI_TOPO_SWEEPS=2 ./main case1 MFS2 0.0        # 多做两轮贪心 refinement
VLSI_FULL_FM=1 ./main case1 MFS2 0.0            # 强制完整 FM，可能很慢
VLSI_RESTARTS=1 VLSI_MAX_PASSES=1 ./main ...    # 手动限制 FM 参数
```

## 输出指标

- `Final Cut size ((K-1) metric)`：多路划分割代价。
- `Topology violations`：拓扑约束违规 net 数，理想值为 0。
- `Topology hop cost`：按 MFS 最短路统计的 hop 代价，越小越好。

## Strict topology mode (2026 update)

如果需要尽量得到严格 non-hop 结果，可以开启 min-conflicts 严格修复器。该修复器优先把 `Topology violations` 降到 0；它不会移动 fixed nodes，也会在输出中报告 fixed-to-fixed 的不可修复下界。

推荐命令：

```bash
VLSI_TOPO_SWEEPS=1 VLSI_REPAIR_ROUNDS=50 VLSI_STRICT_TOPO=1 VLSI_STRICT_SECONDS=1100 ./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

如果希望先快速测试 3 分钟效果：

```bash
VLSI_TOPO_SWEEPS=1 VLSI_REPAIR_ROUNDS=50 VLSI_STRICT_SECONDS=180 ./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

最终以输出中的 `Topology violations` 为准：等于 0 表示严格满足拓扑约束；大于 0 表示当前 fixed constraints / heuristic 下仍是 minimum-violation solution。
