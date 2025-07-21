# Concurrency Library

This directory contains a C++ concurrency library designed to facilitate parallel execution of tasks. It provides various executor policies to suit different needs, from single-threaded execution for debugging to fixed-size thread pools for high-throughput scenarios.

## Files

- `CMakeLists.txt`: CMake build script for the `concurrency` library.

- `IParallelExecutor.h`: Defines the `IParallelExecutor` interface, which all executor policies must implement.

- `ParallelExecutors.h`: Implements various `IParallelExecutor` policies.

- `ParallelFor.h`: Provides a `parallel_for` utility function for parallelizing loops.

- `runner.hpp` / `runner.cpp`: Implements a Boost-based thread pool utilized by `BoostRunnerExecutor`.

## Classes and Usage

### `runner` (Boost-based Thread Pool)

The `runner` struct sets up a thread pool for parallel computations, primarily for localized short runs or reuse within a loop via a singleton instance.

**Key Features:**

- Constructor allows specifying the number of threads; if 0, it auto-detects based on `std::thread::hardware_concurrency()` or an `ncpu` environment variable.

- Non-copyable.

- `post` method to submit jobs, returning a `boost::unique_future<void>` to report exceptions.

- Singleton access: `is_initialized()`, `ensure_initialized(unsigned num_threads = 0)`, and `instance()`.

**Example Usage (as used by `BoostRunnerExecutor`):**

C++

```
runner::ensure_initialized(getNCpus()); // Ensures the runner singleton is set up
auto boostFut = runner::instance().post(std::move(task)); // Submits a task
```

### `IParallelExecutor`

An abstract interface for parallel task execution. It defines a `submit` method for scheduling tasks and an optional `waitAll` helper method.

C++

```
class IParallelExecutor {
public:
  virtual ~IParallelExecutor() = default;
  virtual std::future<void> submit(std::function<void()> task) = 0;
  virtual void waitAll(std::vector<std::future<void>>& futures) {
    for (auto& f : futures) f.get();
  }
};
```

### Executor Policies (from `ParallelExecutors.h`)

This file provides several implementations of the `IParallelExecutor` interface, each with different characteristics and use cases.

#### 1. `SingleThreadExecutor`

Executes tasks synchronously on the calling thread, offering no actual concurrency.

- **Use Cases:** Unit testing, debugging, or scenarios where concurrency must be explicitly disabled.

- **Characteristics:** Deterministic, very low overhead.

#### 2. `StdAsyncExecutor`

Uses `std::async(std::launch::async)` to spawn each task.

- **Use Cases:** Prototyping or parallelizing a small number of long-running tasks where thread startup cost is negligible, or in environments where only the C++ standard library is available.

- **Characteristics:** Portable (part of C++ standard), unbounded (may spawn a new thread per task), high per-task overhead for short-lived tasks.

#### 3. `BoostRunnerExecutor`

Delegates tasks to the existing Boost-based `runner` thread pool.

- **Use Cases:** Ideal for integration with existing Boost-based task systems, reusing threads from the `runner` pool.

- **Characteristics:** Requires Boost `runner` integration. Wraps `boost::unique_future<void>` into `std::future<void>`.

#### 4. `ThreadPoolExecutor<N>`

A fixed-size thread pool where submitted tasks are queued and executed by `N` worker threads. If `N` is 0, it defaults to `std::thread::hardware_concurrency()` (or 2 if that returns 0).

- **Use Cases:** Best for high-throughput scenarios with many small tasks, as it amortizes thread creation cost.

- **Characteristics:** Fine-grained control over the number of threads and queue behavior.

### `parallel_for` Utility

The `parallel_for` template function splits a range `[0...total)` into chunks and submits each chunk to a provided executor for parallel processing. It automatically determines the number of tasks based on `std::thread::hardware_concurrency()`.

**Template Parameters:**

- `Executor`: Any class implementing the `IParallelExecutor` interface.

- `Body`: A callable object (lambda, function, etc.) that takes a `uint32_t` parameter (the loop index).
