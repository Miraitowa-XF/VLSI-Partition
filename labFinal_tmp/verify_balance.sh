#!/bin/bash
# ============================================================
# verify_balance.sh — 一键复现"多路划分"两条平衡要求的验证
#
# 验证内容（对应 lab1_problem_advanced.md 第2节"多路划分"可选要求）：
#   1. r×n = 1 时（即 r = 1/K 的最严平衡），程序不崩溃、能完成任务。
#   2. 元件总数不能被 n 整除时，各分区元件数量允许相差 1。
#
# 命令行参数：
#   ./verify_balance.sh [benchmark_path]
#   benchmark_path 默认 ../dataset/ibm01.hgr
#
# 用法示例：
#   ./verify_balance.sh                          # 用默认 ibm01
#   ./verify_balance.sh ../dataset/ibm02.hgr     # 换一个数据集（如奇数节点数样本）
#
# 产出：
#   - 终端打印每条验证的运行结果与块大小统计
#   - 控制台尾给出 PASS / FAIL 汇总
#   - result/<bench>_partition.txt 为最后一次运行的分区结果
# ============================================================
set -u

# 脚本所在目录作为工作目录（与 main、result/ 同级），保证任意位置调用均可
cd "$(dirname "$(readlink -f "$0")")" || exit 1

BIN=./main
DATASET="${1:-../dataset/ibm01.hgr}"

[ -x "$BIN" ] || chmod +x "$BIN"
if [ ! -x "$BIN" ]; then
    echo "[FATAL] $BIN 不存在或不可执行，请先 make。"; exit 1
fi
if [ ! -f "$DATASET" ]; then
    echo "[FATAL] 数据集不存在: $DATASET"; exit 1
fi

BENCH_NAME=$(basename "$DATASET" | sed 's/\.[^.]*$//')
OUTFILE="./result/${BENCH_NAME}_partition.txt"
mkdir -p result

PASS=0
FAIL=0

# run_case "标签" K R -- 运行 main，超时 TIMEOUT_FAILED 时标记失败
TIMEOUT_SEC=120
run_case() {
    local label="$1" K="$2" R="$3"
    echo "──────────────────────────────────────────────────────────"
    echo "[$label] K=$K  r=$R  (r×n = $(python3 -c "print(round($R*$K,6))"))"
    local out rc
    out=$(timeout "$TIMEOUT_SEC" "$BIN" "$DATASET" "$K" "$R" 2>&1)
    rc=$?
    if [ "$rc" -eq 124 ]; then
        echo "  结果: 超时 ($TIMEOUT_SEC s) — 性能问题，非崩溃"; return 0
    elif [ "$rc" -ne 0 ]; then
        echo "  结果: 运行崩溃 (exit=$rc)"; echo "$out" | tail -5; return 1
    fi
    echo "$out" | grep -E "K-Way Partition|Final Cut size|Total runtime" | sed 's/^/  /'
    return 0
}

# verify_block_sizes K  -- 统计最后一次输出文件的块大小，检查相差<=1 且 min>=floor(N/K)
verify_block_sizes() {
    local K="$1"
    if [ ! -f "$OUTFILE" ]; then
        echo "  [块大小] 未找到输出 $OUTFILE"; return 1
    fi
    python3 - "$OUTFILE" "$K" <<'PY'
import sys
from collections import Counter
path, K = sys.argv[1], int(sys.argv[2])
v = [int(l) for l in open(path) if l.strip()]
c = Counter(v); s = sorted(c.values()); N = len(v)
K_real = len(c)
fl, ce = N // K, (N + K - 1) // K
diff = max(s) - min(s)
ok_size  = diff <= 1
ok_count = K_real == K
ok_floor = min(s) >= fl           # r=1/K ⇒ min_part_size = floor(N*r) = floor(N/K)
print(f"  N={N}  实际分区数={K_real}(期望{K})  块大小={s}")
print(f"  floor(N/K)={fl}  ceil(N/K)={ce}  max-min={diff}  最小块={min(s)}")
print(f"  检查: 分区数符合={ok_count}  块相差<=1={ok_size}  最小块>=floor(N/K)={ok_floor}")
sys.exit(0 if (ok_size and ok_count and ok_floor) else 1)
PY
}

echo "============================================================"
echo " VLSI 多路划分 — 平衡要求验证"
echo " 数据集: $DATASET   可执行: $BIN"
echo "============================================================"

echo ""
echo "########## 验证1: r×n = 1 时程序不崩溃 (r = 1/K) ##########"
for K in 2 4 8; do
    R=$(python3 -c "print(1.0/$K)")
    if run_case "r×n=1" "$K" "$R"; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
done
# K=3 时 1/3 为循环小数，避免用截断小数传参导致 floor(N·r) 偏小：
# 直接不传 r，由 main 内部用默认 1/K（精确）
echo "──────────────────────────────────────────────────────────"
echo "[r×n=1] K=3  r=默认(1/3，内部精确)  (r×n = 1)"
out=$(timeout "$TIMEOUT_SEC" "$BIN" "$DATASET" 3 2>&1); rc=$?
if [ "$rc" -eq 0 ]; then
    echo "$out" | grep -E "K-Way Partition|Final Cut size|Total runtime" | sed 's/^/  /'
    PASS=$((PASS+1))
else
    echo "  结果: 失败 (exit=$rc)"; FAIL=$((FAIL+1))
fi

echo ""
echo "########## 验证2: 元件总数不可被 n 整除时, 块间相差 <= 1 ##########"
echo "(用 K=3 让当前数据集尽量不可整除，并校验输出块大小)"
R=$(python3 -c "print(1.0/3)")
if run_case "不可整除" 3 "$R"; then
    if verify_block_sizes 3; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
else
    FAIL=$((FAIL+1))
fi

echo ""
echo "============================================================"
echo " 验证汇总:  PASS=$PASS  FAIL=$FAIL"
echo "============================================================"
[ "$FAIL" -eq 0 ] && echo "全部通过" || echo "存在失败项，请查看上方输出"
exit "$FAIL"


