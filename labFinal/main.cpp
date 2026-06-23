#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include "Graph.h"
#include "evaluate.h"
#include "solution.h"

using namespace std;

int main(int argc, char **argv) {
    if(argc < 5) {
        cout << "Usage for TopoPart: ./main [bench_file] [topo_file] [K] [r]" << endl;
        cout << "Example: ./main ./dataset/TopoPart/Generated\\ Benchmarks/case1.txt ./dataset/TopoPart/FPGA\\ Graph/MFS1.txt 8 0.12" << endl;
        exit(-1);
    }
    
    string bench_file = argv[1];
    string topo_file = argv[2];
    int K = stoi(argv[3]);
    double r = stod(argv[4]);

    Solution solution;
    Graph graph;

    cout << "--- TopoPart FPGA Emulation Routing ---" << endl;
    cout << "Bench: " << bench_file << " | Topo: " << topo_file << endl;
    cout << "K: " << K << " | r: " << r << endl;

    auto start_time = chrono::high_resolution_clock::now();

    vector<int> part_result;
    // 调用全新的拓扑驱动主函数
    solution.my_partition_algorithm(graph, K, r, part_result, topo_file, bench_file);

    auto end_time = chrono::high_resolution_clock::now();
    double elapsed_s = chrono::duration<double>(end_time - start_time).count();

    // ================== 验证与输出 ==================
    string output_name = "./result/topo_partition.txt";
    ofstream outfile(output_name);
    for (int i = 1; i <= graph.get_max_node_index(); i++) {
        outfile << part_result[i] << endl;
    }
    outfile.close();

    // 计算最终 Cut (使用 K-1 metric)
    int cut = evaluate_kway(graph, part_result, K);
    cout << "\n>>> Final Cut size: " << cut << endl;
    cout << ">>> Total runtime: " << elapsed_s << " s" << endl;

    return 0;
}