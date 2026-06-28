#include <omp.h>
#include <iostream>

int main() {
    #pragma omp parallel
    std::cout << "Thread " << omp_get_thread_num() << std::endl;
    std::cout << "Max threads: " << omp_get_max_threads() << std::endl;
    return 0;
}
