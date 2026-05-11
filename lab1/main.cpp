#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include "Net.h"
#include "Node.h"
#include "Graph.h"
#include "evaluate.h"
#include "solution.h"

using namespace std;

int main(int argc, char **argv) {

    Solution solution;

    if(argc != 2) {
        cout << "Usage: ./main benchmark_file" << endl;
        exit(-1);
    }
    string benchmark_name = argv[1];
    Graph graph;

    solution.read_benchmark(graph, benchmark_name);    

    cout << "Num nodes: " << graph.get_node_num() << endl;
    cout << "Num nets: " << graph.get_net_num() << endl;

    // 1. 执行划分算法
    set<int> X, Y;
    solution.my_partition_algorithm(graph, X, Y);

    // 2. 生成输出文件名: <benchmark_name_without_extension>_partition.txt
    string output_name = benchmark_name;
    // 移除路径前缀
    size_t slash_pos = output_name.find_last_of('/');
    if (slash_pos != string::npos) {
        output_name = output_name.substr(slash_pos + 1);
    }
    // 移除扩展名
    size_t dot_pos = output_name.find_last_of('.');
    if (dot_pos != string::npos) {
        output_name = output_name.substr(0, dot_pos);
    }
    output_name = "./result/" + output_name + "_partition.txt";

    // 3. 输出划分结果到文件 (每行一个整数 0 或 1, 按节点 index 1..N 顺序)
    ofstream outfile(output_name);
    if (!outfile.is_open()) {
        cerr << "Failed to open output file: " << output_name << endl;
        exit(-1);
    }
    int N = graph.get_node_num();
    for (int i = 1; i <= N; i++) {
        if (X.find(i) != X.end())
            outfile << 0 << endl;
        else
            outfile << 1 << endl;
    }
    outfile.close();
    cout << "Partition result written to: " << output_name << endl;

    // 4. 评估划分结果
    int cut = evaluate(graph, output_name);
    cout << "Cut size: " << cut << endl;

    return 0;
}
