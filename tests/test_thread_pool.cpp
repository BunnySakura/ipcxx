#include "ipcxx/thread_pool.h"

#include <iostream>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
  ThreadPool pool(std::thread::hardware_concurrency());
  std::vector<std::future<int>> results;

  results.reserve(8);
  for (int i = 0; i < 8; ++i) {
    results.emplace_back(
      pool.enqueue(
        [i] {
          std::cout << "hello world! " << i << "\n";
          std::this_thread::sleep_for(1s);
          return i * i;
        }
      )
    );
  }

  for (auto &&result : results)
    std::cout << result.get() << ' ';
  std::cout << std::endl;

  return 0;
}
