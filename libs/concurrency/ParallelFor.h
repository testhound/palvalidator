#pragma once

#include <cstdint>    // for uint32_t
#include <thread>     // for std::thread::hardware_concurrency()
#include <vector>     // for std::vector
#include <future>     // for std::future
#include <algorithm>  // for std::min

namespace concurrency {

  // Split [0…total) into at most T chunks (where T = hardware_concurrency),
  // submit each chunk to executor.submit, waitAll, and internally
  // loop p from chunk.start to chunk.end calling your body(p).
  template<typename Executor, typename Body>
    void parallel_for(uint32_t total, Executor& exec, Body body) {
    if (total == 0) return;

    const unsigned hw = std::thread::hardware_concurrency();
    const unsigned numTasks = hw ? hw : 2;
    const uint32_t chunkSize = (total + numTasks - 1) / numTasks; // ceil-divide

    std::vector<std::future<void>> futures;
    for (uint32_t start = 0; start < total; start += chunkSize)
      {
	uint32_t end = std::min(total, start + chunkSize);
	futures.emplace_back(
			     exec.submit([=]() {
				 for (uint32_t p = start; p < end; ++p) {
				   body(p);
				 }
			       })
			     );
      }
    exec.waitAll(futures);
  }

  template<typename Executor, typename Container, typename Body>
  void parallel_for_each(Executor& exec, const Container& container, Body body) {
    if (container.empty()) return;

    const uint32_t total = container.size();
    const unsigned numTasks = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2;
    const uint32_t chunkSize = (total + numTasks - 1) / numTasks;

    std::vector<std::future<void>> futures;
    for (uint32_t start = 0; start < total; start += chunkSize) {
      uint32_t end = std::min(total, start + chunkSize);
      futures.emplace_back(
        exec.submit([&, start, end]() {
          for (uint32_t p = start; p < end; ++p) {
            body(container[p]);
          }
        })
      );
    }
    exec.waitAll(futures);
  }
}
