import os
import pandas as pd
import matplotlib.pyplot as plt


def plot_random_restart_results():
    if not os.path.exists("fm_trial_results.csv"):
        print("fm_trial_results.csv not found. Run ./main first.")
        return
    df = pd.read_csv("fm_trial_results.csv")
    if df.empty:
        print("empty fm_trial_results.csv")
        return

    best_idx = df.sort_values(["Topology_Violations", "Final_Cut", "Hop_Cost"]).index[0]
    labels = df["Trial"].astype(str)
    values = df["Final_Cut"]
    colors = ["tab:red" if i == best_idx else "tab:blue" for i in df.index]

    plt.figure(figsize=(12, 5))
    plt.bar(labels, values, color=colors)
    plt.xlabel("Trial")
    plt.ylabel("Final Cut Size")
    plt.title("Random Restart Results")
    plt.tight_layout()
    plt.savefig("random_restart_results.png", dpi=300)
    print("Saved random_restart_results.png")


def plot_topology_violations():
    if not os.path.exists("fm_trial_results.csv"):
        print("fm_trial_results.csv not found. Run ./main first.")
        return
    df = pd.read_csv("fm_trial_results.csv")
    if "Topology_Violations" not in df.columns:
        print("Topology_Violations column not found")
        return
    plt.figure(figsize=(12, 5))
    plt.bar(df["Trial"].astype(str), df["Topology_Violations"])
    plt.xlabel("Trial")
    plt.ylabel("Topology Violations")
    plt.title("Topology Violations per Trial")
    plt.tight_layout()
    plt.savefig("topology_violations.png", dpi=300)
    print("Saved topology_violations.png")


if __name__ == "__main__":
    plot_random_restart_results()
    plot_topology_violations()
