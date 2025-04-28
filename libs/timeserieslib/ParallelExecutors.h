#pragma once

#include "IParallelExecutor.h"
#include <future>
#include <thread>
#include <vector>
#include <functional>
#include "runner.hpp"  // for BoostRunnerExecutor

namespace concurrency {

/**
 * @brief Executes tasks synchronously on the calling thread.
 * Useful for deterministic unit tests or single-threaded fallbacks.
 */
class SingleThreadExecutor : public IParallelExecutor {
public:
    std::future<void> submit(std::function<void()> task) override {
        std::promise<void> prom;
        auto fut = prom.get_future();
        try {
            task();
            prom.set_value();
        } catch (...) {
            prom.set_exception(std::current_exception());
        }
        return fut;
    }
};

/**
 * @brief Executes each task via std::async(std::launch::async).
 * Portable and dependency-free, but may create a thread per task.
 */
class StdAsyncExecutor : public IParallelExecutor {
public:
    std::future<void> submit(std::function<void()> task) override {
        return std::async(std::launch::async, std::move(task));
    }
};

/**
 * @brief Submits tasks to the existing Boost-based runner thread pool.
 * Wraps boost::unique_future<void> into std::future<void>.
 */
class BoostRunnerExecutor : public IParallelExecutor {
public:
    std::future<void> submit(std::function<void()> task) override {
        // post to the Boost runner
        auto boostFut = runner::instance().post(std::move(task));

        // Wrap boost::unique_future into a std::future via std::promise + watcher thread
        std::promise<void> prom;
        auto stdFut = prom.get_future();

        std::thread([bf = std::move(boostFut), p = std::move(prom)]() mutable {
            try {
                bf.get();  // propagate exceptions from the task
                p.set_value();
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        }).detach();

        return stdFut;
    }
};

} // namespace concurrency
