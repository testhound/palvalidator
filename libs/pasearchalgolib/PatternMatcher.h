#ifndef PATTERNMATCHER_H
#define PATTERNMATCHER_H

#include "PalParseDriver.h"
#include "PalToComparison.h"
#include "FileMatcher.h"
#include <boost/filesystem.hpp>

using namespace mkc_timeseries;

namespace mkc_searchalgo
{

  class  PatternMatcher
  {
  private:
    PatternMatcher();


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

    void static countOccurences(const std::multiset<PalToComparison>& multiOccur, const std::set<PalToComparison>& singleOccur)
    {
        std::map<size_t, std::vector<PalToComparison>> countsMap;
        std::set<PalToComparison>::const_iterator it;
        for (it = singleOccur.begin(); it != singleOccur.end(); it++)
          {
            size_t cnt = multiOccur.count(*it);
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
            std::cout << "top: " << i << " = " << (rit->first) << " #patterns: " << rit->second.size() << std::endl;

          }

    }

  public:
    int static testPatternMatching(const std::string& filePatternExpr, const std::string filePatternExpr2, bool isLong)
    {
      std::string side = (isLong)? "Long": "Short";
      std::string searchPattern = "*" + side + "_" + filePatternExpr + "_*" + filePatternExpr2 + "*";
      std::cout << "Searching pattern: " << searchPattern << std::endl;
      std::vector<boost::filesystem::path> filePaths = FileMatcher::getFiles(".", searchPattern);
      std::multiset<PalToComparison> multiOccur;
      std::set<PalToComparison> singleOccur;

      for (const boost::filesystem::path& path: filePaths)
        {
          populateOccurences(path, multiOccur, singleOccur, isLong);
        }
      std::cout << side << " multiset size: " << multiOccur.size() << std::endl;
      std::cout << side << " single set size: " << singleOccur.size() << std::endl;

      countOccurences(multiOccur, singleOccur);

      return 0;
    }
  };

}



#endif // PATTERNMATCHER_H
