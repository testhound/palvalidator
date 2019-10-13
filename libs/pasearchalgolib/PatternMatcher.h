// Copyright Tibor Szlavik for use by (C) MKC Associates, LLC
// All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Tibor Szlavik <seg2019s@gmail.com>, July-August 2019

#ifndef PATTERNMATCHER_H
#define PATTERNMATCHER_H

#include "PalParseDriver.h"
#include "PalToComparison.h"
#include "FileMatcher.h"
#include <boost/filesystem.hpp>
#include "ComparisonToPalStrategy.h"
#include "LogPalPattern.h"

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  static std::unique_ptr<PriceActionLabSystem> getPricePatterns(boost::filesystem::path filePath)
  {
    mkc_palast::PalParseDriver driver (filePath.string());

    driver.Parse();

    std::cout << "Parsing successfully completed." << std::endl << std::endl;
    PriceActionLabSystem* system = driver.getPalStrategies();
    std::cout << "Total number IR patterns = " << system->getNumPatterns() << std::endl;
    std::cout << "Total long IR patterns = " << system->getNumLongPatterns() << std::endl;
    std::cout << "Total short IR patterns = " << system->getNumShortPatterns() << std::endl;
    return std::unique_ptr<PriceActionLabSystem>(system);

  }

  void static populateOccurences(const boost::filesystem::path& fPath,
                                 std::multiset<PalToComparison>& multiOccur,
                                 std::set<PalToComparison>& singleOccur,
                                 bool isLong)
  {
    std::cout << "file: " << fPath.string() << std::endl;
    std::unique_ptr<PriceActionLabSystem> patterns = getPricePatterns(fPath);
    PriceActionLabSystem::ConstSortedPatternIterator it = (isLong)? patterns->patternLongsBegin(): patterns->patternShortsBegin();
    PriceActionLabSystem::ConstSortedPatternIterator end = (isLong)? patterns->patternLongsEnd(): patterns->patternShortsEnd();
    unsigned long numPatterns = patterns->getNumPatterns();
    std::set<PalToComparison> uniques;
    for (; it != end; it++)
      {
        PatternExpressionPtr pattern = (*it).second->getPatternExpression();
        PalToComparison comparison(pattern.get());
        uniques.insert(comparison);
      }
    std::cout << "of " << numPatterns << " pal patterns in file, " << uniques.size() << " were found unique." << std::endl;
    singleOccur.insert(uniques.begin(), uniques.end());
    multiOccur.insert(uniques.begin(), uniques.end());
  }


  class  PatternMatcher
  {
  public:
    PatternMatcher(const std::string& filePatternExpr, const std::string& filePatternExpr2, ComparisonType patternSearchType, bool isLong, bool inSampleOnly, unsigned int minNumOfStrats, size_t numTimeFrames):
      mIsLong(isLong),
      mMinNumOfStrats(minNumOfStrats),
      mExportPatternIndex(0)
    {
      std::string side = (isLong)? "Long": "Short";
      std::string typePattern = (patternSearchType == ComparisonType::Extended)? "*": std::string(ToString(patternSearchType));
      for (size_t i = 0; i < numTimeFrames + 1; i++)
        {
          std::string mergeSearchPattern = "./" + typePattern + "_Patterns" + side + "_" + filePatternExpr + "_" + std::to_string(i) + "_" + filePatternExpr2 + "_" + std::to_string(inSampleOnly) + ".txt";
          std::string targetFile = "./Merged_Patterns" + side + "_" + filePatternExpr + "_" + std::to_string(i) + "_" + filePatternExpr2 + "_" + std::to_string(inSampleOnly) + ".txt";
          std::cout << "Merge pattern: " << i << ":" << mergeSearchPattern << ", targetFile: " << targetFile << std::endl;
          std::vector<boost::filesystem::path> filePaths = FileMatcher::getFiles(".", mergeSearchPattern);
          FileMatcher::mergeFiles(filePaths, targetFile);
        }
      std::string searchPattern = "./Merged_Patterns" + side + "_" + filePatternExpr + "_*_" + filePatternExpr2 + "_" + std::to_string(inSampleOnly) + ".txt";
      std::cout << "Searching pattern: " << searchPattern << std::endl;
      std::vector<boost::filesystem::path> filePaths = FileMatcher::getFiles(".", searchPattern);

      for (const boost::filesystem::path& path: filePaths)
        {
          populateOccurences(path, mMultiOccur, mSingleOccur, isLong);
        }
      std::cout << side << " multiset size: " << mMultiOccur.size() << std::endl;
      std::cout << side << " single set size: " << mSingleOccur.size() << std::endl;
    }

    void countOccurences()
    {
        if (!mSelectedComparisons.empty())
          {
            std::cout << "Counting occurences was called but the selection has already been made." << std::endl;
            return;
          }
        std::map<size_t, std::vector<PalToComparison>> countsMap;
        std::set<PalToComparison>::const_iterator it;
        for (it = mSingleOccur.begin(); it != mSingleOccur.end(); it++)
          {
            size_t cnt = mMultiOccur.count(*it);
            if (countsMap.find(cnt) == countsMap.end())
              {
                countsMap.insert(std::make_pair(cnt, std::vector<PalToComparison>()));
              }
            countsMap.at(cnt).push_back(*it);
          }
        //size_t allCnt = countsMap.size();
        std::map<size_t, std::vector<PalToComparison>>::reverse_iterator rit;
        unsigned int i = 0;
        for (rit = countsMap.rbegin(); rit != countsMap.rend(); rit++)
          {
            i++;
            if (rit->second.size() > mMinNumOfStrats && mSelectedComparisons.empty())
              {
                std::cout << "This group of " << rit->second.size() << " strategies has been selected. " << std::endl;
                mSelectedComparisons = rit->second;
              }
            std::cout << "top: " << i << " = " << (rit->first) << " #patterns: " << rit->second.size() << std::endl;
          }
    }

    template <class Decimal>
    bool exportSelectPatterns(Decimal* profitTarget, Decimal* stopLoss, const std::string& exportFileName, std::shared_ptr<Portfolio<Decimal>> portfolio)
    {
      if (mSelectedComparisons.empty())
        {
          std::cout << "Nothing to export." << std::endl;
          return false;
        }
       std::cout << "Exporting select strategies into file: " << exportFileName << std::endl;
       std::ofstream exportFile(exportFileName);
       std::vector<std::vector<ComparisonEntryType>> select = getSelectComparisons();
       for (const std::vector<ComparisonEntryType>& strat: select)
         {
           if (mIsLong)
             {
              ComparisonToPalLongStrategy<Decimal> comp(strat, mExportPatternIndex++, 0, profitTarget, stopLoss, portfolio);
              LogPalPattern::LogPattern(comp.getPalPattern(), exportFile);
             }
           else
             {
               ComparisonToPalShortStrategy<Decimal> comp(strat, mExportPatternIndex++, 0, profitTarget, stopLoss, portfolio);
               LogPalPattern::LogPattern(comp.getPalPattern(), exportFile);
             }
         }
       return true;
    }

    std::vector<std::vector<ComparisonEntryType>> getSelectComparisons()
    {
      std::vector<std::vector<ComparisonEntryType>> ret;
      for (const PalToComparison& pComp: mSelectedComparisons)
        ret.push_back(pComp.getComparisons());
      return ret;
    }

    bool getIsLong() const { return mIsLong; }

  private:
    bool mIsLong;
    unsigned int mMinNumOfStrats;
    std::multiset<PalToComparison> mMultiOccur;
    std::set<PalToComparison> mSingleOccur;
    std::vector<PalToComparison> mSelectedComparisons;
    int mExportPatternIndex;


  };



}



#endif // PATTERNMATCHER_H
