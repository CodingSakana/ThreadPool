#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

int main(int argc, char *argv[])
{
    if(argc != 2){
        std::cerr << "Usage: " << argv[0] << " <nums>." << std::endl;
        return 1;
    }
    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector< std::future<int> > results;

    auto num = std::stoi(argv[1]);

    for(int i = 0; i < num; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                std::cout << "world " << i << std::endl;
                return i + 1;
            })
        );
    }

    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
    
    return 0;
}
