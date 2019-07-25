#ifndef UNIQUESINGLEPAPATRIX_H
#define UNIQUESINGLEPAPATRIX_H

#include <iostream>
#include <valarray>
#include "ComparisonsGenerator.h"

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
      unsigned int i = 0;
      for (; it != uniqueComparisons.end(); ++ it)
        {
          mUniqueMaps[i] = (*it);
          mMatrix[i] = initVector;
          i++;
          //std::cout << (*it)[0] << (*it)[1] << (*it)[2] << (*it)[3] << std::endl;
        }
      std::cout << "Unique maps size: " << mUniqueMaps.size() << ", mMatrix size: " << mMatrix.size() << std::endl;
    }

    void vectorizeComparisons(const std::unordered_map<unsigned int, std::unordered_set<ComparisonEntryType>>& comparisonsBatches)
    {
      //iterate over "date indices"
      for (unsigned int i = 0; i < mDateIndexCount; i++)
        {
          const auto& compareSet = comparisonsBatches.at(i);
          //std::cout << compareSet.size() << std::endl;
          //iterate the 0 matrix
          std::unordered_map<unsigned int, ComparisonEntryType>::const_iterator it;
          for (it = mUniqueMaps.begin(); it != mUniqueMaps.end(); ++it)
            {
              const ComparisonEntryType& compareKey = it->second;
              std::valarray<int>& vector = mMatrix[it->first];
              //std::cout << "key: " << key[0] << key[1] << key[2] << key[3] << " and vector size: " << vector.size() << std::endl;
              std::unordered_set<ComparisonEntryType>::const_iterator fnd = compareSet.find(compareKey);
              if (fnd != compareSet.end())
                { //found comparison entry for this dateindex
                  vector[i] = 1;  //create "sparse" vector entry
                  //std::cout << "found pattern" << std::endl;
                  //std::cout << "found: " << key[0] << key[1] << key[2] << key[3] << " in date:" << i << std::endl;
                }
            }
        }
    }
    const std::unordered_map<unsigned int, std::valarray<int>>& getMatrix() const { return mMatrix; }

    const std::unordered_map<size_t, ComparisonEntryType>& getMappings() const { return mUniqueMaps; }

    std::unordered_map<unsigned int, std::valarray<int>>::const_iterator& getMatrixBegin() const { return mMatrix.begin(); }

    std::unordered_map<unsigned int, std::valarray<int>>::const_iterator& getMatrixEnd() const { return mMatrix.end(); }

    std::unordered_map<unsigned int, ComparisonEntryType>::const_iterator& getComparisonsBegin() const { return mUniqueMaps.begin(); }

    std::unordered_map<unsigned int, ComparisonEntryType>::const_iterator& getComparisonsEnd() const { return mUniqueMaps.end(); }

    const std::valarray<int>& getMatrixElement(unsigned int id) const { return mMatrix[id]; }

    const ComparisonEntryType& getComparisonsElement(unsigned int id) const { return mUniqueMaps[id]; }

    ~UniqueSinglePAMatrix()
    {}

  private:
    unsigned int mDateIndexCount;
    std::unordered_map<unsigned int, std::valarray<int>> mMatrix;
    std::unordered_map<unsigned int, ComparisonEntryType> mUniqueMaps;

  };

}

#endif // UNIQUESINGLEPAPATRIX_H
