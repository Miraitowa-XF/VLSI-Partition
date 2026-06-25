import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 绘制随机重启结果图
def plot_random_restart_results():
    try:
        trial_df = pd.read_csv("fm_trial_results.csv")
    except FileNotFoundError:
        print("Error: fm_trial_results.csv not found. Run ./main first.")
        return
    # 读取最佳 trial 编号
    try:
        with open("best_trial_id.txt", "r") as f:
            best_trial = int(f.read().strip())
    except FileNotFoundError:
        print("Error: best_trial_id.txt not found. Run ./main first.")
        return

    trials = trial_df['Trial'].values
    cuts = trial_df['Final_Cut'].values

    # 颜色设置：最佳 trial 用红色，其余用蓝色
    colors = ['red' if t == best_trial else 'steelblue' for t in trials]
    plt.figure(figsize=(12, 5))
    bars = plt.bar(trials, cuts, color=colors, edgecolor='black', linewidth=0.5)
    # 在柱子上标注数值
    for bar, cut in zip(bars, cuts):
        plt.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + max(cuts) * 0.01,
                 str(cut), ha='center', va='bottom', fontsize=8)

    plt.title("Random Restart Results: Final Cut Size per Trial")
    plt.xlabel("Trial")
    plt.ylabel("Final Cut Size")
    plt.xticks(trials)
    plt.grid(axis='y', linestyle='--', alpha=0.7)

    # 添加图例
    from matplotlib.patches import Patch
    legend_elements = [Patch(facecolor='red', label=f'Best Trial ({best_trial})'),
                       Patch(facecolor='steelblue', label='Other Trials')]
    plt.legend(handles=legend_elements)
    plt.tight_layout()
    plt.savefig("random_restart_results.png", dpi=300, bbox_inches='tight')
    print("Saved: random_restart_results.png")
    plt.close()

# 绘制最佳 trial 的 pass 图
def plot_best_trial_passes():
    try:
        df = pd.read_csv("fm_all_logs.csv")
    except FileNotFoundError:
        print("Error: fm_all_logs.csv not found. Run ./main first.")
        return
    # 读取最佳 trial 编号
    try:
        with open("best_trial_id.txt", "r") as f:
            best_trial = int(f.read().strip())
    except FileNotFoundError:
        print("Error: best_trial_id.txt not found. Run ./main first.")
        return

    print(f"Best trial: {best_trial}")
    # 筛选最佳 trial 的数据
    trial_data = df[df['Trial'] == best_trial]
    # 获取该 trial 的所有 pass 编号
    passes = sorted(trial_data['Pass'].unique())

    # 对于每个 pass，找出 Cumulative_Gain 的最大值
    best_per_pass = []
    for p in passes:
        pass_data = trial_data[trial_data['Pass'] == p]
        best_val = pass_data['Cumulative_Gain'].max()
        best_per_pass.append({'Pass': p, 'Best_Cumulative_Gain': best_val})
    best_df = pd.DataFrame(best_per_pass)

    plt.figure(figsize=(10, 5))
    plt.plot(best_df['Pass'], best_df['Best_Cumulative_Gain'],
             marker='o', linestyle='-', color='blue', linewidth=2, markersize=6)
    # 标注每个点的数值
    for _, row in best_df.iterrows():
        plt.text(row['Pass'], row['Best_Cumulative_Gain'] + best_df['Best_Cumulative_Gain'].max() * 0.01,
                 str(row['Best_Cumulative_Gain']), ha='center', va='bottom', fontsize=9)

    plt.title(f"Best Trial (Trial {best_trial}): Best Cumulative Gain per Pass")
    plt.xlabel("Pass")
    plt.ylabel("Best Cumulative Gain")
    plt.xticks(passes)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig("best_trial_passes.png", dpi=300, bbox_inches='tight')
    print("Saved: best_trial_passes.png")
    plt.close()


if __name__ == "__main__":
    plot_random_restart_results()
    plot_best_trial_passes()
