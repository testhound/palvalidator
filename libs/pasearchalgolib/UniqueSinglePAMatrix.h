#ifndef UNIQUESINGLEPAPATRIX_H
#define UNIQUESINGLEPAPATRIX_H

#include <iostream>
#include <valarray>
#include "ComparisonsGenerator.h"
#include "PalAst.h"

namespace mkc_searchalgo {


  template <class Decimal, class TComparison> class UniqueSinglePAMatrix
  {
  public:
    UniqueSinglePAMatrix(const ComparisonsGenerator<Decimal>& compareGenerator, unsigned int dateIndexCount):
      mDateIndexCount(dateIndexCount)
    {
      typename std::set<TComparison>::const_iterator it = compareGenerator.getUniqueComparisons().begin();

      unsigned int i = 0;
      for (; it != compareGenerator.getUniqueComparisons().end(); ++ it)
        {
          mUniquesMap[i] = (*it);
          i++;
        }
      std::cout << "Unique maps size: " << mUniquesMap.size() << std::endl;
    }

    const std::unordered_map<unsigned int, TComparison>& getMap() const { return mUniquesMap; }

    const typename std::unordered_map<unsigned int, TComparison>::const_iterator getMapBegin() const { return mUniquesMap.begin(); }

    const typename std::unordered_map<unsigned int, TComparison>::const_iterator getMapEnd() const { return mUniquesMap.end(); }

    const TComparison& getMappedElement(unsigned int id) const { return mUniquesMap[id]; }

    ~UniqueSinglePAMatrix()
    {}

  private:
    unsigned int mDateIndexCount;
    std::unordered_map<unsigned int, TComparison> mUniquesMap;

  };

  ///
  /// Specialization with vectorized representation of comparisons (0 1 sparse vector)
  ///
template <class Decimal> class UniqueSinglePAMatrix<Decimal, std::valarray<int>>
{
public:
  UniqueSinglePAMatrix(const ComparisonsGenerator<Decimal>& compareGenerator, unsigned int dateIndexCount):
    mDateIndexCount(dateIndexCount)
  {
    std::set<ComparisonEntryType>::const_iterator it = compareGenerator.getUniqueComparisons().begin();
    //"vector" initialized to zeros
    std::valarray<int> initVector(0, dateIndexCount);
    unsigned int i = 0;
    for (; it != compareGenerator.getUniqueComparisons().end(); ++ it)
      {
        mUniqueMaps[i] = (*it);
        mMatrix[i] = initVector;
        i++;

      }
    std::cout << "Unique maps size: " << mUniqueMaps.size() << ", underlying mMatrix size: " << mMatrix.size() << std::endl;
    vectorizeComparisons(compareGenerator.getComparisons());
  }

  void vectorizeComparisons(const std::unordered_map<unsigned int, std::unordered_set<ComparisonEntryType>>& comparisonsBatches)
  {
    //iterate over "date indices"
    for (unsigned int i = 0; i < mDateIndexCount; i++)
      {
        const auto& compareSet = comparisonsBatches.at(i);
        //iterate the 0 matrix
        std::unordered_map<unsigned int, ComparisonEntryType>::const_iterator it;
        for (it = mUniqueMaps.begin(); it != mUniqueMaps.end(); ++it)
          {
            const ComparisonEntryType& compareKey = it->second;
            std::valarray<int>& vector = mMatrix[it->first];
            std::unordered_set<ComparisonEntryType>::const_iterator fnd = compareSet.find(compareKey);
            if (fnd != compareSet.end())
              { //found comparison entry for this dateindex
                vector[i] = 1;  //create "sparse" vector entry
              }
          }
      }
  }
  const std::unordered_map<unsigned int, std::valarray<int>>& getMap() const { return mMatrix; }

  const std::unordered_map<unsigned int, std::valarray<int>>::const_iterator getMapBegin() const { return mMatrix.begin(); }

  const std::unordered_map<unsigned int, std::valarray<int>>::const_iterator getMapEnd() const { return mMatrix.end(); }

  const std::valarray<int>& getMappedElement(unsigned int id) const { return mMatrix.at(id); }

  ~UniqueSinglePAMatrix()
  {}

private:
  unsigned int mDateIndexCount;
  std::unordered_map<unsigned int, std::valarray<int>> mMatrix;
  //for this specialized class unique map is helper map and private
  std::unordered_map<unsigned int, ComparisonEntryType> mUniqueMaps;

};


}
#endif // UNIQUESINGLEPAPATRIX_H
