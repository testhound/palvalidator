// concurrency/IParallelExecutor.h
#pragma once
#include <future>
#include <vector>
#include <functional>

class IParallelExecutor {
public:
  virtual ~IParallelExecutor() = default;

  // Schedule a void() task; returns a std::future you can wait on.
  virtual std::future<void> submit(std::function<void()> task) = 0;

  // Wait on a collection of futures (optional helper).
  virtual void waitAll(std::vector<std::future<void>>& futures) {
    for (auto& f : futures) f.get();
  }
};
