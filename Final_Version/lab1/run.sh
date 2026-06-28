#!/bin/bash
# ============================================================
# 批量测试脚本：运行所有 ibm01~ibm18 数据集并汇总结果
# ============================================================
mkdir -p ./result
DATASET_DIR="./dataset"
OUTPUT_FILE="./result/batch_result.txt"

echo "=============================================="
echo "  VLSI Lab1 - Hypergraph Bipartitioning Test"
echo "  FM Algorithm Benchmark Results"
echo "=============================================="
echo ""
printf "%-10s %-10s %-10s %-10s %-10s %-10s\n" "Dataset" "Nodes" "Nets" "Passes" "Cut" "Time(s)"
echo "--------------------------------------------------"

# 汇总结果写入文件
printf "%-10s %-10s %-10s %-10s %-10s %-10s\n" "Dataset" "Nodes" "Nets" "Passes" "Cut" "Time(s)" > "$OUTPUT_FILE"
echo "--------------------------------------------------" >> "$OUTPUT_FILE"

for i in $(seq -w 1 18); do
    BENCH="${DATASET_DIR}/ibm${i}.hgr"
    
    if [ ! -f "$BENCH" ]; then
        printf "%-10s %-10s\n" "ibm${i}" "FILE NOT FOUND"
        continue
    fi
    
    # 运行并计时
    start_time=$(date +%s.%N)
    output=$(timeout 300 ./main "$BENCH" 2>&1)
    exit_code=$?
    end_time=$(date +%s.%N)
    elapsed=$(echo "$end_time - $start_time" | bc 2>/dev/null || echo "N/A")
    
    if [ $exit_code -ne 0 ]; then
        printf "%-10s %-10s\n" "ibm${i}" "TIMEOUT/ERROR"
        printf "%-10s %-10s\n" "ibm${i}" "TIMEOUT/ERROR" >> "$OUTPUT_FILE"
        continue
    fi
    
    # 解析输出
    nodes=$(echo "$output" | grep "Num nodes" | awk '{print $NF}')
    nets=$(echo "$output" | grep "Num nets" | awk '{print $NF}')
    passes=$(echo "$output" | grep "Pass" | wc -l)
    cut=$(echo "$output" | grep "Cut size" | awk '{print $NF}')
    
    # 格式化时间
    if [ "$elapsed" != "N/A" ]; then
        time_str=$(printf "%.2f" "$elapsed")
    else
        time_str="N/A"
    fi
    
    printf "%-10s %-10s %-10s %-10s %-10s %-10s\n" "ibm${i}" "$nodes" "$nets" "$passes" "$cut" "$time_str"
    printf "%-10s %-10s %-10s %-10s %-10s %-10s\n" "ibm${i}" "$nodes" "$nets" "$passes" "$cut" "$time_str" >> "$OUTPUT_FILE"
done

echo "--------------------------------------------------"
echo ""
echo "Results saved to: $OUTPUT_FILE"
echo "=============================================="
