#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Graph.h"
#include "evaluate.h"
#include "solution.h"

using namespace std;

namespace {

bool is_number(const string &s) {
    if (s.empty()) return false;
    char *end_ptr = nullptr;
    std::strtod(s.c_str(), &end_ptr);
    return end_ptr != nullptr && *end_ptr == '\0';
}

string basename_without_ext(string path) {
    size_t slash_pos = path.find_last_of("/\\");
    if (slash_pos != string::npos) path = path.substr(slash_pos + 1);
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != string::npos) path = path.substr(0, dot_pos);
    return path;
}

void print_usage() {
    cout << "Usage:\n";
    cout << "  ./main benchmark_file [K] [r]\n";
    cout << "  ./main benchmark_file K r topology_file\n";
    cout << "  ./main benchmark_file topology_file [r]\n\n";
    cout << "Examples:\n";
    cout << "  ./main ../dataset/ibm01.hgr 4 0.25\n";
    cout << "  ./main ../dataset/TopoPart/Generated\\ Benchmarks/case1 ../dataset/TopoPart/FPGA\\ Graph/MFS2 0.0\n";
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 2) {
            print_usage();
            return 1;
        }

        string benchmark_name = argv[1];
        string topology_name;
        int K = 4;
        double r = -1.0;

        if (argc >= 3) {
            string arg2 = argv[2];
            if (is_number(arg2)) {
                K = stoi(arg2);
                if (argc >= 4) r = stod(argv[3]);
                if (argc >= 5) topology_name = argv[4];
            } else {
                topology_name = arg2;
                if (argc >= 4) r = stod(argv[3]);
            }
        }

        Solution solution;
        if (!topology_name.empty()) {
            solution.read_topology(topology_name);
            K = solution.get_topology_K();
            solution.enable_topology(true);
        }
        if (r < 0.0) r = 1.0 / static_cast<double>(K);

        Graph graph;
        solution.read_benchmark(graph, benchmark_name);

        cout << "K-Way Partition: K=" << K << ", Balance Ratio r=" << r << endl;

        auto start_time = chrono::high_resolution_clock::now();
        vector<int> part_result;
        solution.my_partition_algorithm(graph, K, r, part_result);
        auto end_time = chrono::high_resolution_clock::now();
        double elapsed_ms = chrono::duration<double, milli>(end_time - start_time).count();

        filesystem::create_directories("result");
        string output_name = "result/" + basename_without_ext(benchmark_name) + "_partition.txt";
        ofstream outfile(output_name);
        if (!outfile.is_open()) {
            throw runtime_error("failed to open output file: " + output_name);
        }
        for (int i = 0; i < graph.get_node_num(); ++i) {
            outfile << part_result.at(static_cast<size_t>(i)) << '\n';
        }
        outfile.close();

        int cut = evaluate_kway(graph, part_result, K);
        cout << "Final Cut size ((K-1) metric): " << cut << endl;
        if (solution.topology_loaded()) {
            int vio = evaluate_topology_violations(graph, part_result, solution.get_topology_allowed());
            long long hop = evaluate_topology_hop_cost(graph, part_result, solution.get_topology_dist());
            cout << "Topology violations: " << vio << endl;
            cout << "Topology hop cost: " << hop << endl;
        }
        cout << "Partition written to: " << output_name << endl;
        cout << "Total runtime: " << (elapsed_ms / 1000.0) << " s" << endl;
    } catch (const exception &ex) {
        cerr << "Error: " << ex.what() << endl;
        return 1;
    }
    return 0;
}
