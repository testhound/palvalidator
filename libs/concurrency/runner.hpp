#ifndef RUNNER_HPP
#define RUNNER_HPP

#include <thread>
#include <boost/thread/future.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <type_traits>
#include <utility>

#include <iostream>

//extracts the number of cpus as reported by std::hardware_concurrency,
//eventually overriden by environment variable ncpu
// run as: ncpu=7 ./PalValidator BP_R0_5_Simpler_Config.txt 300 2
std::size_t getNCpus();

///////////////////////////////////////
/// \brief The runner struct
/// it sets up a thread pool for parallelizing a computation
/// for localized short runs, use it to start a pool
/// for running inside a loop, prefer to use a pool started
/// by runner& getRunner() which implements a singleton and can reuse the pool for subsequent calls

struct runner
{
  //constructor. if nthreads==0 the number of threads shall be
  //determined by the system.
  runner(std::size_t nthreads);
  //non-copyable object
  runner(const runner&) = delete;
  runner& operator=(const runner&) = delete;
  //destructor. tries to stop the pool and waits for threads to end
  ~runner();
  //tries to stop the threads in the pool
  void stop();

  // method to submit job to thread pool. reports back exceptions through a future
  template<typename F,typename ...Args>
  boost::unique_future<void> post(F f, Args&&...xargs)
  {
    using R = void;
    auto promise = std::make_shared<boost::promise<R>>();
    auto res = promise->get_future();
    ios.post([ promise=std::move(promise)
	       , task=boost::bind<R>(std::move(f), std::forward<Args>(xargs)...)](){
      try
	{
	  task();
	}
      catch(std::exception const&e)
	{
	  promise->set_exception(std::current_exception());
	  std::cout << "Runner Exception" << std::endl;
	  std::cout << "WHAT: " << e.what() << std::endl;
	  return;
	}
      promise->set_value();
    });

    return res;
  }

  /// Returns true if the singleton has already been constructed.
  static bool is_initialized() { return instance_ptr() != nullptr; }

  /// Construct the singleton if needed (auto–detect thread count if you pass 0).
  static void ensure_initialized(unsigned num_threads = 0) {
    if (!instance_ptr()) 
      new runner(num_threads);
  }
  static runner& instance() {
    if (!instance_ptr()) {
      // zero threads means “auto-detect”
      static runner defaultRunner(0);
    }
    return *instance_ptr();
  }

private:
  static runner*& instance_ptr();

private:
  boost::asio::io_service ios;
  std::shared_ptr<boost::asio::io_service::work> work;
  boost::thread_group pool;

  //worker method of thread pool
  void run();
};

#endif // RUNNER_HPP
