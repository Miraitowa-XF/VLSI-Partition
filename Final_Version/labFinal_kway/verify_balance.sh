#!/bin/bash
# ============================================================
# verify_balance.sh — 一键复现"多路划分"平衡要求的验证
#
# 验证内容（对应 lab1_problem_advanced.md 第2节"多路划分"可选要求）：
#   A. r×n = 1 时（即 r = 1/K 的最严平衡），程序不崩溃、能完成任务。
#   B. 元件总数不能被 n 整除时，各分区元件数量允许相差 1。
#   C. (扩展) K=3 传入截断小数 r（如 0.333）也能正常完成，不因截断崩溃。
#   D. (扩展) r < 1/K 的松弛平衡（如 K=4, r=0.1）也能正常完成，边界随 r 变化。
#
# 覆盖数据集：ibm01 / ibm02 / ibm03（N 分别为 12752 / 19601 / 23136）
# 覆盖路数：  K = 2, 3, 4, 8
#
# 命令行参数：
#   ./verify_balance.sh [benchmark_path1] [benchmark_path2] ...
#   不传则默认 ../dataset/ibm01.hgr ../dataset/ibm02.hgr ../dataset/ibm03.hgr
#
# 产出：
#   - 终端打印每条验证的运行结果与块大小统计
#   - result/balance_verify.csv  汇总表（可直接贴进报告）
#   - result/<bench>_partition.txt 为最后一次运行的分区结果
# ============================================================
set -u

cd "$(dirname "$(readlink -f "$0")")" || exit 1

BIN=./main
DATASET_DIR=../dataset

# 默认三个数据集；允许命令行覆盖
if [ "$#" -ge 1 ]; then
   DATASETS=("$@")
else
   DATASETS=("$DATASET_DIR/ibm01.hgr" "$DATASET_DIR/ibm02.hgr" "$DATASET_DIR/ibm03.hgr")
fi

[ -x "$BIN" ] || chmod +x "$BIN" 2>/dev/null
if [ ! -x "$BIN" ]; then
    echo "[FATAL] $BIN 不存在或不可执行，请先 make。"; exit 1
fi
mkdir -p result

CSV=result/balance_verify.csv
# CSV 表头
echo "Dataset,K,r,r_times_n,N,RealParts,BlockSizes,MaxMinDiff,MinBlock,floor_NK,PASS,ExitCode,Cut,Runtime_s,Note" > "$CSV"

PASS=0
FAIL=0
TIMEOUT_SEC=300

# run_case "标签" bench K R -- 运行 main，返回退出码；输出经 stdout 变量 CASE_OUT 暴露
run_case() {
    local label="$1" bench="$2" K="$3" R="$4"
    local rn
    rn=$(python3 -c "print(round($R*$K,6))")
    echo "──────────────────────────────────────────────────────────"
    echo "[$label] $(basename "$bench")  K=$K  r=$R  (r×n = $rn)"
    CASE_OUT=$(timeout "$TIMEOUT_SEC" "$BIN" "$bench" "$K" "$R" 2>&1)
    local rc=$?
    CASE_RC=$rc
    if [ "$rc" -eq 124 ]; then
        echo "  结果: 超时 ($TIMEOUT_SEC s) — 性能问题，非崩溃"
    elif [ "$rc" -ne 0 ]; then
        echo "  结果: 运行崩溃 (exit=$rc)"; echo "$CASE_OUT" | tail -5 | sed 's/^/  /'
    else
        echo "$CASE_OUT" | grep -E "K-Way Partition|Final Cut size|Total runtime" | sed 's/^/  /'
    fi
}

# run_case_default bench K -- 不传 r，走 main 内部精确 1/K 默认值
run_case_default() {
    local bench="$1" K="$2"
    echo "──────────────────────────────────────────────────────────"
    echo "[r×n=1, 内部默认] $(basename "$bench")  K=$K  r=默认(1/$K, 精确)  (r×n = 1)"
    CASE_OUT=$(timeout "$TIMEOUT_SEC" "$BIN" "$bench" "$K" 2>&1)
    CASE_RC=$?
    if [ "$CASE_RC" -eq 124 ]; then
        echo "  结果: 超时 ($TIMEOUT_SEC s) — 性能问题，非崩溃"
    elif [ "$CASE_RC" -ne 0 ]; then
        echo "  结果: 运行崩溃 (exit=$CASE_RC)"; echo "$CASE_OUT" | tail -5 | sed 's/^/  /'
    else
        echo "$CASE_OUT" | grep -E "K-Way Partition|Final Cut size|Total runtime" | sed 's/^/  /'
    fi
}

# extract_cut / extract_runtime / extract_N  —— 从 CASE_OUT 解析字段
extract_cut() {
    echo "$CASE_OUT" | grep -oE "Final Cut size \(\(K-1\) metric\): [0-9]+" | grep -oE "[0-9]+$" | tail -1
}
extract_runtime() {
    echo "$CASE_OUT" | grep -oE "Total runtime: [0-9.]+ s" | grep -oE "[0-9.]+" | tail -1
}
extract_N() {
    echo "$CASE_OUT" | grep -oE "Num nodes: [0-9]+" | grep -oE "[0-9]+$" | head -1
}

# verify_block_sizes bench K  -- 统计输出文件块大小，输出一行 CSV 字段
# 用法: emit_csv_row bench K r label
emit_csv_row() {
    local bench="$1" K="$2" R="$3" label="$4"
    local bench_name
    bench_name=$(basename "$bench" | sed 's/\.[^.]*$//')
    local outfile="./result/${bench_name}_partition.txt"
    local N cut rt
    N=$(extract_N); [ -z "$N" ] && N="NA"
    cut=$(extract_cut); [ -z "$cut" ] && cut="NA"
    rt=$(extract_runtime); [ -z "$rt" ] && rt="NA"

    # 块大小统计
    local sizes realparts diff minblk floor_nk pass note
    if [ "$CASE_RC" -ne 0 ] || [ ! -f "$outfile" ]; then
        sizes="NA"; realparts="NA"; diff="NA"; minblk="NA"; floor_nk="NA"; pass="FAIL"
        note="run_failed_or_no_output"
    else
        read sizes realparts diff minblk floor_nk pass < <(python3 - "$outfile" "$K" "$N" <<'PY'
import sys
from collections import Counter
path, K, N = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]) if sys.argv[3]!="NA" else None
v = [int(l) for l in open(path) if l.strip()]
c = Counter(v); s = sorted(c.values())
K_real = len(c)
fl = (N // K) if N is not None else (len(v)//K)
diff = (max(s) - min(s)) if s else 0
ok_size  = diff <= 1
ok_count = K_real == K
ok_floor = (min(s) >= fl) if s else False
ok = ok_size and ok_count and ok_floor
# 用空格分隔的字段：sizes realparts diff minblk floor pass
print(" ".join(map(str,["|".join(map(str,s)), K_real, diff, (min(s) if s else 0), fl, ("PASS" if ok else "FAIL")])))
PY
)
        note="ok"
    fi

    local rn
    rn=$(python3 -c "print(round($R*$K,6))" 2>/dev/null || echo "NA")

    # 判定整体 PASS/FAIL：rc==0 且块校验 PASS
    local overall
    if [ "$CASE_RC" -eq 0 ] && [ "$pass" = "PASS" ]; then overall="PASS"; else overall="FAIL"; fi
    # 对于"只要求不崩溃"的用例（r×n=1 边界、截断小数、松弛平衡），块校验可能不适用，
    # 此时只要 rc==0 即算 PASS
    if [ "$label" = "rn_equals_one" ] || [ "$label" = "truncated_r" ] || [ "$label" = "loose_balance" ]; then
        if [ "$CASE_RC" -eq 0 ]; then overall="PASS"; [ "$pass" != "PASS" ] && note="${note};block_check=${pass}(此用例只要求不崩溃)"
        else overall="FAIL"; note="crashed"
        fi
    fi

    if [ "$overall" = "PASS" ]; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi

    echo "$bench_name,$K,$R,$rn,$N,$realparts,${sizes},$diff,$minblk,$floor_nk,$overall,$CASE_RC,$cut,$rt,$label,$note" >> "$CSV"
}

echo "============================================================"
echo " VLSI 多路划分 — 平衡要求验证 (扩展版)"
echo " 数据集: ${DATASETS[*]}"
echo " 可执行: $BIN"
echo " 汇总:   $CSV"
echo "============================================================"

for BENCH in "${DATASETS[@]}"; do
    if [ ! -f "$BENCH" ]; then
        echo "[SKIP] 数据集不存在: $BENCH"; continue
    fi
    echo ""
    echo "############################################"
    echo "# 数据集: $(basename "$BENCH")"
    echo "############################################"

    echo ""
    echo "===== A. r×n = 1 严格均衡 (r = 1/K), 不崩溃 ====="
    for K in 2 3 4 8; do
        # K=3 的 1/3 是循环小数，避免截断，走内部精确默认值
        if [ "$K" -eq 3 ]; then
            run_case_default "$BENCH" 3
            emit_csv_row "$BENCH" 3 "1/3(默认)" "rn_equals_one"
        else
            R=$(python3 -c "print(1.0/$K)")
            run_case "r×n=1" "$BENCH" "$K" "$R"
            emit_csv_row "$BENCH" "$K" "$R" "rn_equals_one"
        fi
    done

    echo ""
    echo "===== B. 不可整除时块间相差 <= 1 (各 K 都校验块大小) ====="
    for K in 2 3 4 8; do
        R=$(python3 -c "print(1.0/$K)")
        run_case "不可整除校验" "$BENCH" "$K" "$R"
        emit_csv_row "$BENCH" "$K" "$R" "indivisible_check"
    done

    echo ""
    echo "===== C. K=3 传截断小数 r (0.333 / 0.334), 不崩溃 ====="
    for R in 0.333 0.334; do
        run_case "截断小数r" "$BENCH" 3 "$R"
        emit_csv_row "$BENCH" 3 "$R" "truncated_r"
    done

    echo ""
    echo "===== D. r < 1/K 松弛平衡 (K=4, r=0.10; K=8, r=0.05) ====="
    run_case "松弛平衡" "$BENCH" 4 0.10
    emit_csv_row "$BENCH" 4 0.10 "loose_balance"
    run_case "松弛平衡" "$BENCH" 8 0.05
    emit_csv_row "$BENCH" 8 0.05 "loose_balance"
done

echo ""
echo "============================================================"
echo " 验证汇总:  PASS=$PASS  FAIL=$FAIL"
echo " 详细表格:  $CSV"
echo "============================================================"
[ "$FAIL" -eq 0 ] && echo "全部通过" || echo "存在失败项，请查看 $CSV"
exit "$FAIL"
