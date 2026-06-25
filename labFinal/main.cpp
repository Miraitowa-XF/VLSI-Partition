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
        cout << "Usage: ./main <netlist_file> [topology_file] [K]" << endl;
        cout << "Example: ./main ./dataset/case1 ./dataset/MSF1 8" << endl;
        exit(-1);
    }
    
    string netlist_name = argv[1];
    string topo_name = (argc >= 3) ? argv[2] : "";
    int K = 4;
    if (argc >= 4) {
        try {
            K = stoi(argv[3]);
        } catch (...) {
            cerr << "Error: K must be an integer, got: " << argv[3] << endl;
            exit(-1);
        }
    }

    Graph graph;

    // 如果提供了拓扑文件名，分别读取
    if (!topo_name.empty()) {
        solution.read_topology(topo_name);
        solution.read_netlist_and_fixed(graph, netlist_name);
    } else {
        // 兼容旧格式：从网表文件名推断拓扑文件名
        solution.read_benchmark(graph, netlist_name);
    }

    cout << "Num nodes: " << graph.get_node_num() << " | Num nets: " << graph.get_net_num() << endl;
    solution.convert_to_two_pin_graph(graph);
    cout << "After 2-pin expansion: " << graph.get_node_num() << " nodes | " << graph.get_net_num() << " 2-pin nets" << endl;
    cout << "K-Way Partition: K=" << K << endl;

    auto start_time = chrono::high_resolution_clock::now();

    vector<int> part_result;
    solution.my_partition_algorithm(graph, K, part_result);

    auto end_time = chrono::high_resolution_clock::now();
    double elapsed_ms = chrono::duration<double, milli>(end_time - start_time).count();

    string output_name = netlist_name;
    size_t slash_pos = output_name.find_last_of('/');
    if (slash_pos != string::npos) output_name = output_name.substr(slash_pos + 1);
    size_t dot_pos = output_name.find_last_of('.');
    if (dot_pos != string::npos) output_name = output_name.substr(0, dot_pos);
    // Try absolute path first, then relative
    string out_path = "./result/" + output_name + "_partition.txt";
    ofstream outfile(out_path);
    bool ok = outfile.is_open();
    if (!ok) {
        out_path = "D:\\VLSI-Partition\\labFinal\\result\\" + output_name + "_partition.txt";
        outfile.open(out_path);
        ok = outfile.is_open();
    }
    if (!ok) {
        out_path = "D:\\VLSI-Partition\\result\\" + output_name + "_partition.txt";
        outfile.open(out_path);
        ok = outfile.is_open();
    }
    if (ok) {
        int max_idx = 0;
        for (Node *node : graph.get_nodes()) {
            int idx = node->get_index();
            if (idx > max_idx) max_idx = idx;
        }
        for (int i = 0; i <= max_idx; i++) {
            outfile << part_result[i] << endl;
        }
        outfile.close();
    } else {
        cerr << "Warning: Could not write output file, but continuing with evaluation" << endl;
    }

    int cut = evaluate_kway(graph, part_result, K);
    int violations = evaluate_violations(graph, part_result, K, solution.get_topo_adj());

    cout << "Final Cut size ((p-1) 2-pin metric): " << cut << endl;
    cout << "Topology violations: " << violations << endl;
    cout << "Total runtime: " << (elapsed_ms / 1000.0) << " s" << endl;

    return 0;
}
