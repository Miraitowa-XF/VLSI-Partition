# labFinal topology-aware K-way partitioner

This version supports:

1. IBM `.hgr` K-way partitioning.
2. TopoPart benchmark format: first line node count, second line edge count, then 2-pin nets, then fixed-node lines.
3. FPGA/MFS topology graph parsing.
4. Fixed node constraints.
5. Topology-aware initialization, candidate FPGA propagation, violation repair, fast topology refinement, and final topology violation/hop-cost reporting.

## Build

```bash
make clean && make
```

## IBM K-way example

```bash
./main ../dataset/ibm01.hgr 4 0.25
```

## TopoPart example

```bash
./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

For large TopoPart cases, the program automatically uses fast topology mode. Useful knobs:

```bash
# only topology-driven initialization + repair; no greedy cut refinement
VLSI_TOPO_SWEEPS=0 VLSI_REPAIR_ROUNDS=30 ./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0

# better cut, usually a little slower
VLSI_TOPO_SWEEPS=1 VLSI_REPAIR_ROUNDS=30 ./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0

# force full FM; may be very slow on a laptop
VLSI_FULL_FM=1 VLSI_MAX_PASSES=2 ./main "../dataset/TopoPart/Generated Benchmarks/case1" "../dataset/TopoPart/FPGA Graph/MFS2" 0.0
```

The strict success condition for topology is:

```text
Topology violations: 0
```

If the value is larger than 0, the output is a minimum-violation heuristic result rather than a strictly non-hop topology solution.
