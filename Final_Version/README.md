# VLSI 课程大作业 — 代码与运行说明

> 本文件说明整个项目的文件结构、数据集存放方式，以及三个代码目录（`lab1`、`labFinal_kway`、`labFinal`）的编译与运行方法。

---

## 1. 文件树

```text
.
├── README.md                          # 本说明文件
├── dataset/                           # 数据集（详见第 2 节）
│
├── lab1/                              # 小作业：二路 FM 划分
│   ├── main.cpp                     
│   ├── solution.cpp / solution.h      # FM 划分算法核心实现
│   ├── Graph.cpp / Graph.h            # 超图结构（节点/网的邻接关系）
│   ├── Net.cpp / Net.h                # 超网抽象
│   ├── Node.cpp / Node.h              # 节点抽象
│   ├── evaluate.cpp / evaluate.h      # 割代价计算与结果评估
│   ├── run.sh                         # 批量测试 ibm01~ibm18 的脚本
│   ├── plot_best.py                   # 结果可视化脚本
│   └── Makefile                     
│
├── labFinal_kway/                     # 大作业·多路划分专项（验证选做要求）
│   ├── main.cpp                     
│   ├── solution.cpp / solution.h      # 多路 FM、K-1 割代价、r 平衡边界
│   ├── Graph/Net/Node/evaluate.*      # 与 lab1 同名文件，多路版实现
│   ├── verify_balance.sh              # 一键复现平衡性要求（A/B/C/D 四组验证，输出 result/balance_verify.csv）
│   ├── result/                        # 验证结果输出目录（平衡 CSV + 分区向量）
│   └── Makefile                     
│
└── labFinal/                          # 大作业·最终提交版（拓扑约束 + 并行化）
    ├── main.cpp                     
    ├── solution.cpp / solution.h      # 多路 FM + 拓扑约束 + OpenMP 并行
    ├── Graph/Net/Node/evaluate.*      # 基础数据结构（含固定节点、拓扑图支持）
    ├── test.cpp / test                # OpenMP 环境探测小程序（非核心，用于确认编译器支持 OpenMP）
    └── Makefile                       # 产物为 main
```

---

## 2. 数据集存放

数据集统一放在 `./dataset/` 下，结构如下：

```text
dataset/
├── ibm01.hgr ... ibm18.hgr            # 供 lab1（二路）、labFinal_kway（多路）使用
├── FPGA Graph/                        # FPGA 拓扑图
│   ├── MFS1                     
│   └── MFS2                     
├── Generated Benchmarks/              # 生成式网表
│   └── case1 ... case8          
└── Titan23 Benchmarks/                # Titan23 真实网表
    └── neuron, LU_Network, ...
```

> 数据集**不随压缩包提交**，按课程要求在本文档写明存放方式。复现时请从课程提供链接下载，按上述结构放入 `./dataset/` 即可。

---

## 3. 各目录运行方法

三个目录均可通过 `make` 编译生成可执行文件 `main`。

### 3.1 lab1（小作业·二路 FM）

```bash
cd lab1
make
./main ../dataset/ibm01.hgr
```

- **命令格式**：`./main <benchmark_file>`
- 参数为单个 `.hgr` 超网表路径。
- 批量测试：`bash run.sh`（自动跑 ibm01~ibm18 并汇总）。

### 3.2 labFinal_kway（大作业·多路划分专项）

```bash
cd labFinal_kway
make
# 三路划分，严格均衡 r=1/3（不传 r 即默认严格均衡）
./main ../dataset/ibm01.hgr 3
# 四路划分，松弛平衡 r=0.20
./main ../dataset/ibm01.hgr 4 0.20
```

- **命令格式**：`./main <benchmark_file> [K] [r]`
  - `K`：划分数，默认 4；
  - `r`：平衡比例 `r∈[0,1/K]`，默认 `1/K`（严格均衡）。
- 一键复现选做要求（平衡性验证）：
  ```bash
  bash verify_balance.sh
  ```

  默认对 `../dataset/ibm01.hgr`、`ibm02.hgr`、`ibm03.hgr` 跑 A/B/C/D 四组验证，结果写入 `result/balance_verify.csv`。
  也可指定数据集：`bash verify_balance.sh ../dataset/ibm05.hgr`。

### 3.3 labFinal（大作业·最终提交版：拓扑约束 + 并行化）

```bash
cd labFinal
make
./main "../dataset/Titan23 Benchmarks/bitcoin_miner" "../dataset/FPGA Graph/MFS1" 8 4
```

- **命令格式**：`./main <netlist_file> [topology_file] [K] [num_threads]`
  - `netlist_file`：网表路径（如 `Titan23 Benchmarks/neuron` 或 `Generated Benchmarks/case1`）；
  - `topology_file`：FPGA 拓扑图路径（`MFS1` 或 `MFS2`）；
  - `K`：划分数（对应 FPGA 节点数，如 MFS1 取 8）；
  - `num_threads`：并行线程数。
- 示例：对 `neuron` 网表，使用 `MFS1` 拓扑（8 节点），8 路划分，4 线程并行。
- 若需确认编译环境是否支持 OpenMP，可单独编译运行 `test.cpp`（`g++ -fopenmp test.cpp -o test && ./test`），它仅打印线程数，用于环境探测。
