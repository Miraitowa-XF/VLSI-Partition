# labFinal hard-constraint-first topology version

本版本在原 K-way FM / topology-aware 版本基础上，新增了 **hard-constraint-first feasible constructor / component repair**，目标是在拓扑模式下优先满足：

```text
Topology violations = 0
```

再考虑 cut size。

## 编译

```bash
make clean && make
```

## 普通 K-way 划分

```bash
./main ../dataset/ibm01.hgr 4 0.25
```

## TopoPart 严格拓扑优先运行

推荐先关闭 cut-oriented greedy sweep，专门冲严格拓扑：

```bash
VLSI_TOPO_SWEEPS=0 \
VLSI_REPAIR_ROUNDS=0 \
VLSI_STRICT_TOPO=1 \
VLSI_STRICT_SECONDS=1100 \
./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

如果已经能得到 `Topology violations: 0`，再运行：

```bash
VLSI_TOPO_SWEEPS=1 \
VLSI_REPAIR_ROUNDS=20 \
VLSI_STRICT_TOPO=1 \
VLSI_STRICT_SECONDS=600 \
./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

## 新增算法说明

1. 读取 FPGA topology，建立 `topo_allowed` 与 bitset 邻接掩码。
2. 读取 fixed constraints，固定节点不可移动。
3. 做 candidate propagation，收缩每个节点可选 FPGA 集。
4. 先做 topology-driven initialization。
5. 如果启用 `VLSI_STRICT_TOPO`，进入 hard-constraint-first component repair：
   - 只取当前违反拓扑约束的 pair edge 端点作为 active vertices；
   - 通过 bad edges 构造局部冲突连通分量；
   - 对每个冲突分量，把所有通向分量外部的边作为 hard boundary constraints；
   - 对分量内部做 arc-consistency pruning；
   - 在不破坏 boundary constraints 的前提下局部重染色，优先把该分量内部 violation 降到 0；
   - 若仍有 violation，再调用 min-conflicts 作为补充。

这样比原来的全局随机 min-conflicts 更接近“先构造可行解，再优化 cut”的严格拓扑流程。

## 判断标准

输出中：

```text
Topology violations: 0
```

表示严格拓扑约束满足。

如果仍大于 0，则说明在给定时间内没有找到严格 non-hop 解；程序输出的是 minimum-violation solution。
