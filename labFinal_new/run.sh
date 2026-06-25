#!/bin/bash
set -euo pipefail

# ============================================================
# 批量测试脚本
#   1) IBM .hgr:        ./run.sh ibm ./dataset 4 0.25
#   2) TopoPart cases:  ./run.sh topo "./dataset/TopoPart/Generated Benchmarks" "./dataset/TopoPart/FPGA Graph/MFS2" 0.0
# ============================================================

MODE=${1:-ibm}
mkdir -p result

if [[ "$MODE" == "ibm" ]]; then
    DATASET_DIR=${2:-./dataset}
    K=${3:-4}
    R=${4:-$(awk "BEGIN{print 1/$K}")}
    OUTPUT_FILE="./result/batch_result_ibm.txt"

    printf "%-14s %-10s %-10s %-12s %-12s\n" "Dataset" "Nodes" "Nets" "Cut" "Time(s)" | tee "$OUTPUT_FILE"
    echo "----------------------------------------------------------------" | tee -a "$OUTPUT_FILE"

    for i in $(seq -w 1 18); do
        BENCH="${DATASET_DIR}/ibm${i}.hgr"
        if [[ ! -f "$BENCH" ]]; then
            printf "%-14s %-10s\n" "ibm${i}" "FILE_NOT_FOUND" | tee -a "$OUTPUT_FILE"
            continue
        fi
        start_time=$(date +%s.%N)
        output=$(timeout 300 ./main "$BENCH" "$K" "$R" 2>&1)
        end_time=$(date +%s.%N)
        elapsed=$(awk "BEGIN{printf \"%.3f\", $end_time-$start_time}")
        nodes=$(echo "$output" | grep "Loaded benchmark" | sed -E 's/.*nodes=([0-9]+).*/\1/')
        nets=$(echo "$output" | grep "Loaded benchmark" | sed -E 's/.*nets=([0-9]+).*/\1/')
        cut=$(echo "$output" | grep "Final Cut size" | awk '{print $NF}')
        printf "%-14s %-10s %-10s %-12s %-12s\n" "ibm${i}" "$nodes" "$nets" "$cut" "$elapsed" | tee -a "$OUTPUT_FILE"
    done
elif [[ "$MODE" == "topo" ]]; then
    BENCH_DIR=${2:?"need benchmark directory"}
    TOPO_FILE=${3:?"need topology file"}
    R=${4:-0.0}
    OUTPUT_FILE="./result/batch_result_topo.txt"

    printf "%-24s %-10s %-10s %-12s %-12s %-12s\n" "Dataset" "Nodes" "Nets" "Cut" "TopoVio" "Time(s)" | tee "$OUTPUT_FILE"
    echo "--------------------------------------------------------------------------------" | tee -a "$OUTPUT_FILE"

    for BENCH in "$BENCH_DIR"/*; do
        [[ -f "$BENCH" ]] || continue
        name=$(basename "$BENCH")
        start_time=$(date +%s.%N)
        output=$(timeout 600 ./main "$BENCH" "$TOPO_FILE" "$R" 2>&1)
        end_time=$(date +%s.%N)
        elapsed=$(awk "BEGIN{printf \"%.3f\", $end_time-$start_time}")
        nodes=$(echo "$output" | grep "Loaded benchmark" | sed -E 's/.*nodes=([0-9]+).*/\1/')
        nets=$(echo "$output" | grep "Loaded benchmark" | sed -E 's/.*nets=([0-9]+).*/\1/')
        cut=$(echo "$output" | grep "Final Cut size" | awk '{print $NF}')
        vio=$(echo "$output" | grep "Topology violations" | tail -1 | awk '{print $NF}')
        printf "%-24s %-10s %-10s %-12s %-12s %-12s\n" "$name" "$nodes" "$nets" "$cut" "$vio" "$elapsed" | tee -a "$OUTPUT_FILE"
    done
else
    echo "Unknown mode: $MODE" >&2
    exit 1
fi
