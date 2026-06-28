#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <chrono>
#include "Graph.h"
#include "evaluate.h"
#include "solution.h"

using namespace std;

int main(int argc, char **argv) {
    Solution solution;
    
    if(argc < 2) {
        cout << "Usage: ./main benchmark_file [K] [r]" << endl;
        cout << "Example for 4-way, r=0.20: ./main ./dataset/ibm01.hgr 4 0.20" << endl;
        exit(-1);
    }
    
    string benchmark_name = argv[1];
    int K = (argc >= 3) ? stoi(argv[2]) : 4;         // 默认 4 路划分
    double r = (argc >= 4) ? stod(argv[3]) : 1.0 / K; // 默认严格均衡

    Graph graph;
    solution.read_benchmark(graph, benchmark_name);    

    cout << "Num nodes: " << graph.get_node_num() << " | Num nets: " << graph.get_net_num() << endl;
    cout << "K-Way Partition: K=" << K << ", Balance Ratio r=" << r << endl;

    auto start_time = chrono::high_resolution_clock::now();

    vector<int> part_result;
    solution.my_partition_algorithm(graph, K, r, part_result);

    auto end_time = chrono::high_resolution_clock::now();
    double elapsed_ms = chrono::duration<double, milli>(end_time - start_time).count();

    string output_name = benchmark_name;
    size_t slash_pos = output_name.find_last_of('/');
    if (slash_pos != string::npos) output_name = output_name.substr(slash_pos + 1);
    size_t dot_pos = output_name.find_last_of('.');
    if (dot_pos != string::npos) output_name = output_name.substr(0, dot_pos);
    output_name = "./result/" + output_name + "_partition.txt";

    ofstream outfile(output_name);
    if (!outfile.is_open()) { cerr << "Failed to open output file" << endl; exit(-1); }
    
    for (int i = 1; i <= graph.get_node_num(); i++) {
        outfile << part_result[i] << endl;
    }
    outfile.close();

    int cut = evaluate(graph, output_name, K);
    cout << "Final Cut size ((K-1) metric): " << cut << endl;
    cout << "Total runtime: " << (elapsed_ms / 1000.0) << " s" << endl;

    return 0;
}