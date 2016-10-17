#include "runner.hpp"
#include <limits>

std::size_t getNCpus()
{
    std::size_t hwcpus  = std::thread::hardware_concurrency();
    int envcpus = 0;
    char* ncpu_env=getenv("ncpu");
    if(ncpu_env!=nullptr)
    {
        envcpus=std::atoi(ncpu_env);
        if(envcpus>=0)
            return envcpus%std::numeric_limits<unsigned char>::max();
    }
    return hwcpus;
}

runner*& runner::instance_ptr() {
  static runner* r = nullptr;
  return r;
}

runner::runner(std::size_t nthreads):
    work(std::make_shared<boost::asio::io_service::work>(ios))
{
    if(nthreads==0)  nthreads=getNCpus();

    std::cerr<<"Starting "<<nthreads<<" threads"<<std::endl;

    for(std::size_t i=0;i<nthreads;++i)
    {
        pool.add_thread(new boost::thread(boost::bind(&runner::run,this)));
    }

    assert(instance_ptr() == nullptr && "Only one instance may exist");
    instance_ptr() = this;
}

runner::~runner()
{
    try
    {
        stop();
        pool.join_all();
    }
    catch(std::exception const& e)
    {
        std::cerr<<e.what()<<std::endl;
    }
    catch(...){}
}

void runner::stop() { work.reset();}

void runner::run()
{
    try
    {
        ios.run();
    }
    catch(std::exception const& e)
    {
        std::cerr<<"run "<<e.what()<<std::endl;
    }
}
