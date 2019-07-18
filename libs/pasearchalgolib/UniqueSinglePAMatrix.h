#ifndef UNIQUESINGLEPAPATRIX_H
#define UNIQUESINGLEPAPATRIX_H

#include <array>
#include <iostream>
#include <valarray>
#include <set>
#include <map>

namespace mkc_searchalgo {

  using ComparisonEntryType = std::array<unsigned int, 4>;

  template <class Decimal> class UniqueSinglePAMatrix
  {
  public:
    UniqueSinglePAMatrix(const std::set<ComparisonEntryType>& uniqueComparisons, unsigned int dateIndexCount)
    {
      std::set<ComparisonEntryType>::iterator it = uniqueComparisons.begin();
      //"vector" initialized to zeros
      std::valarray<int> initVector(0, dateIndexCount);
      for (; it != uniqueComparisons.end(); ++ it)
        {
          mMatrix[(*it)] = initVector;
        }
    }

    ~UniqueSinglePAMatrix()
    {}

  private:
    std::map<ComparisonEntryType, std::valarray<int>> mMatrix;
  };

}

#endif // UNIQUESINGLEPAPATRIX_H
