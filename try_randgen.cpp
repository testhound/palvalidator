#include "runner.hpp"
#include "RandomMersenne.h"
#include <vector>
#include <iostream>

template<typename T>
std::ostream& operator<<(std::ostream& os,std::vector<T> const&v)
{
    for(const auto& x:v) os<<x<<" ";
    return os;
}

void func(std::vector<u_int32_t>& v)
{
      RandomMersenne mRandGenerator;
      for(auto& x:v)
          x=mRandGenerator.DrawNumber(0,60000);
}

int main(int argc, char* argv[])
{
    runner& Runner=getRunner();
    std::vector<std::future<void>> errsVec;
    std::vector<std::vector<u_int32_t>> results(10);

    for(int i=0;i<results.size();++i)
    {
        results[i].resize(100);
        errsVec.emplace_back(Runner.post(func,std::ref(results[i])));
    }
    for(auto& f:errsVec) f.get();
    for(auto&& r:results) std::cout<<r<<std::endl<<std::endl;
    return 0;
}
