#ifndef UNIQUESINGLEPAPATRIX_H
#define UNIQUESINGLEPAPATRIX_H

#include <iostream>
#include <valarray>
#include "ComparisonsGenerator.h"
#include <algorithm>

namespace mkc_searchalgo {

  template <class Decimal> class UniqueSinglePAMatrix
  {
  public:
    UniqueSinglePAMatrix(const std::set<ComparisonEntryType>& uniqueComparisons, unsigned int dateIndexCount):
      mDateIndexCount(dateIndexCount)
    {
      std::set<ComparisonEntryType>::const_iterator it = uniqueComparisons.begin();
      //"vector" initialized to zeros
      std::valarray<int> initVector(0, dateIndexCount);
      for (; it != uniqueComparisons.end(); ++ it)
        {
          mMatrix[(*it)] = initVector;
          //std::cout << (*it)[0] << (*it)[1] << (*it)[2] << (*it)[3] << std::endl;
        }
    }

    void vectorizeComparisons(const std::map<unsigned int, std::vector<ComparisonEntryType>>& comparisonsBatches)
    {
      //iterate over "date indices"
      for (unsigned int i = 0; i < mDateIndexCount; i++)
        {
          const std::vector<ComparisonEntryType>& compVector = comparisonsBatches.at(i);
          //iterate the 0 matrix
          std::map<ComparisonEntryType, std::valarray<int>>::iterator it;
          for (it = mMatrix.begin(); it != mMatrix.end(); ++it)
            {
              const ComparisonEntryType& key = it->first;
              std::valarray<int>& vector = it->second;

              if (std::find(compVector.begin(), compVector.end(), key) != compVector.end())
                { //found comparison entry for this dateindex
                  vector[i] = 1;  //create "sparse" vector entry
                  //std::cout << "found: " << key[0] << key[1] << key[2] << key[3] << " in date:" << i << std::endl;
                }
            }
        }
    }
    const std::map<ComparisonEntryType, std::valarray<int>>& getMatrix() const { return mMatrix; }

    ~UniqueSinglePAMatrix()
    {}

  private:
    unsigned int mDateIndexCount;
    std::map<ComparisonEntryType, std::valarray<int>> mMatrix;
  };

}

#endif // UNIQUESINGLEPAPATRIX_H
