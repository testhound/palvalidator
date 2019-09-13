#ifndef SEARCHRUN_H
#define SEARCHRUN_H

#include "McptConfigurationFileReader.h"
#include "SearchAlgoConfigurationFileReader.h"
#include "StdEstimator.h"
#include "SearchController.h"
#include "runner.hpp"

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;

enum SideToRun
{LongOnly, ShortOnly, LongShort};

namespace mkc_searchalgo
{
  class SearchRun
  {
  public:
    SearchRun(const std::string& configurationFileName, const std::string& searchConfigFileName):
      mConfigurationFileName(configurationFileName),
      mSearchConfigFileName(searchConfigFileName)

    {
      //std::string configurationFileName (v[1]);
      std::cout << configurationFileName << std::endl;
      McptConfigurationFileReader reader(configurationFileName);
      mConfiguration = reader.readConfigurationFile(true);

      StdEstimator<Decimal> estimator(mConfiguration);
      mTargetBase = estimator.estimate();

      //std::string searchConfigFileName(v[2]);
      std::cout << searchConfigFileName << std::endl;
      SearchAlgoConfigurationFileReader searchReader(searchConfigFileName);
      mSearchConfig = searchReader.readConfigurationFile(mConfiguration->getSecurity(), 0);
      mNow = std::time(nullptr);
      std::cout << "Time since epoch: " << static_cast<long>(mNow) << std::endl;
    }


    size_t getTargetStopSize() const { return mSearchConfig->getTargetStopPair().size(); }


    void run(runner& Runner, bool inSampleOnly, SideToRun runSide, size_t targetStopIndex)
    {
      //separate out short and long runs
      std::vector<bool> los;
      if (runSide != SideToRun::LongOnly)
          los.push_back(false);
      if (runSide != SideToRun::ShortOnly)
          los.push_back(true);

      std::vector<boost::unique_future<void>> resultsOrErrorsVector;

      auto targetstop = mSearchConfig->getTargetStopPair()[targetStopIndex];

      for (size_t timeFrameId = 0; timeFrameId <= mSearchConfig->getNumTimeFrames(); timeFrameId++)
        {
          for (bool side: los)
            {
              std::shared_ptr<Decimal> profitTarget = std::make_shared<Decimal>(targetstop.first * mTargetBase);
              std::shared_ptr<Decimal> stopLoss = std::make_shared<Decimal>(targetstop.second * mTargetBase);
              std::cout << "Testing TimeFrame" << timeFrameId << ", side(isLong?): " << side << ", Profit target multiplier: " << targetstop.first << " in %: " << (*profitTarget) << ", with Stop loss multiplier: " << targetstop.second << " in %: " << (*stopLoss) << std::endl;
              resultsOrErrorsVector.emplace_back(Runner.post([this,
                                                             profitTarget,
                                                             stopLoss,
                                                             inSampleOnly,
                                                             timeFrameId,
                                                             side
                                                             ]()-> void {
                  McptConfigurationFileReader reader(this->mConfigurationFileName);

                  std::shared_ptr<McptConfiguration<Decimal>> configuration = reader.readConfigurationFile(true);

                  SearchAlgoConfigurationFileReader searchReader(this->mSearchConfigFileName);
                  std::shared_ptr<SearchAlgoConfiguration<Decimal>> searchConfig = searchReader.readConfigurationFile(configuration->getSecurity(), static_cast<int>(timeFrameId));

                  std::cout << "Parsed search algo config: " << this->mSearchConfigFileName << std::endl;
                  std::cout << (*searchConfig) << std::endl;
                  SearchController<Decimal> controller(configuration, searchConfig->getTimeSeries(), searchConfig);
                  controller.prepare();
                  if (side)
                    {
                      controller.run<true>(profitTarget, stopLoss, inSampleOnly);
                      std::string fileNameLong("PatternsLong_" + std::to_string(static_cast<long>(this->mNow)) + "_" + std::to_string(timeFrameId) + "_" + std::to_string((*profitTarget).getAsDouble()) + "_" + std::to_string((*stopLoss).getAsDouble()) + "_" + std::to_string(inSampleOnly) + ".txt");
                      controller.exportSurvivingLongPatterns(profitTarget, stopLoss, fileNameLong);
                    }
                  else if (!side)
                    {
                      controller.run<false>(profitTarget, stopLoss, inSampleOnly);
                      std::string fileNameShort("PatternsShort_" + std::to_string(static_cast<long>(this->mNow)) + "_" + std::to_string(timeFrameId) + "_" + std::to_string((*profitTarget).getAsDouble()) + "_" + std::to_string((*stopLoss).getAsDouble()) + "_" + std::to_string(inSampleOnly) + ".txt");
                      controller.exportSurvivingShortPatterns(profitTarget, stopLoss, fileNameShort);
                    }

                }
              ));
            }
        }

      for(std::size_t i=0;i<resultsOrErrorsVector.size();++i)
        {
          try{
            resultsOrErrorsVector[i].wait();
            resultsOrErrorsVector[i].get();
          }
          catch(std::exception const& e)
          {
            std::cerr<<"Parallel run exception in run id: " << i << " error: "<<e.what()<<std::endl;
          }
        }

    }

    std::pair<Decimal, Decimal> getTargetsAtIndex(size_t ind) const
    {
      auto targetStop = mSearchConfig->getTargetStopPair()[ind];
      return std::make_pair(targetStop.first * mTargetBase, targetStop.second * mTargetBase);
    }
    long getNowAsLong() const { return static_cast<long>(mNow); }

    const std::shared_ptr<SearchAlgoConfiguration<Decimal>>& getSearchConfig() const { return mSearchConfig; }

    const std::shared_ptr<McptConfiguration<Decimal>>& getConfig() const { return mConfiguration; }

  private:
    std::string mConfigurationFileName;
    std::string mSearchConfigFileName;
    std::shared_ptr<McptConfiguration<Decimal>> mConfiguration;
    std::shared_ptr<SearchAlgoConfiguration<Decimal>> mSearchConfig;
    Decimal mTargetBase;
    std::time_t mNow;
  };

}



#endif // SEARCHRUN_H
