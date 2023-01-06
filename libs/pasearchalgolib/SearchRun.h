#ifndef SEARCHRUN_H
#define SEARCHRUN_H

#include "McptConfigurationFileReader.h"
#include "SearchAlgoConfigurationFileReader.h"
#include "StdEstimator.h"
#include "SearchController.h"
#include "runner.hpp"
#include "RunParameters.h"
#include "TimeShiftedMultiTimeSeriesCreator.h"

using namespace mkc_timeseries;
using Decimal = num::DefaultNumber;

enum SideToRun
{LongOnly, ShortOnly, LongShort};

namespace mkc_searchalgo
{
  class SearchRun
  {
  public:
    SearchRun(std::shared_ptr<RunParameters> parameters) : mRunParameters(parameters)
    {

    // TODO: add runparameters to config file readers and get data
    //       from there for each local/api circumstance.

      std::cout << parameters->getConfigFile1Path() << std::endl;
      McptConfigurationFileReader reader(parameters);
      mConfiguration = reader.readConfigurationFile(true, true);

      StdEstimator<Decimal> estimator(mConfiguration);
      mTargetBase = estimator.estimate();

      std::cout << parameters->getSearchConfigFilePath() << std::endl;
      SearchAlgoConfigurationFileReader searchReader(parameters);
      mSearchConfig = searchReader.readConfigurationFile(mConfiguration);

      mNow = std::time(nullptr);
      std::cout << "Time since epoch: " << static_cast<long>(mNow) << std::endl;
    }


    size_t getTargetStopSize() const { return mSearchConfig->getTargetStopPair().size(); }


    void run(runner& Runner, const TimeShiftedMultiTimeSeriesCreator<Decimal>& timeShiftedData, bool inSampleOnly, SideToRun runSide, size_t targetStopIndex, ComparisonType patternSearchType)
    {
      //separate out short and long runs
      std::vector<bool> los;
      if (runSide != SideToRun::LongOnly)
          los.push_back(false);
      if (runSide != SideToRun::ShortOnly)
          los.push_back(true);

      std::vector<boost::unique_future<void>> resultsOrErrorsVector;

      auto targetstop = mSearchConfig->getTargetStopPair()[targetStopIndex];

      auto timeSeriesIt = timeShiftedData.beginShiftedTimeSeries();
      std::shared_ptr<OHLCTimeSeries<Decimal>> timeShiftedTimeSeries;
      for(size_t timeFrameId = 0; timeSeriesIt != timeShiftedData.endShiftedTimeSeries(); timeSeriesIt++, timeFrameId++)
      {
          timeShiftedTimeSeries = *timeSeriesIt;
          for (bool side: los)
            {
              std::shared_ptr<Decimal> profitTarget = std::make_shared<Decimal>(targetstop.first * mTargetBase);
              std::shared_ptr<Decimal> stopLoss = std::make_shared<Decimal>(targetstop.second * mTargetBase);
              std::cout << "Testing TimeFrame" << timeFrameId << ", side(isLong?): " << side << ", Profit target multiplier: " << targetstop.first << " in %: " << (*profitTarget) << ", with Stop loss multiplier: " << targetstop.second << " in %: " << (*stopLoss) << std::endl;
              resultsOrErrorsVector.emplace_back(Runner.post([this,
                                                             patternSearchType,
                                                             profitTarget,
                                                             stopLoss,
                                                             inSampleOnly,
                                                             timeFrameId,
                                                             side,
                                                             timeShiftedTimeSeries
                                                             ]()-> void {

                  std::cout << "Parsed search algo config: " << mRunParameters->getSearchConfigFilePath() << std::endl;
                  std::cout << (*mSearchConfig) << std::endl;
                  SearchController<Decimal> controller(mConfiguration, timeShiftedTimeSeries, mSearchConfig);
                  controller.prepare(patternSearchType, inSampleOnly);
                  if (side)
                    {
                      controller.run<true>(profitTarget, stopLoss, inSampleOnly);
                      std::string fileNameLong(std::string(ToString(patternSearchType)) + "_PatternsLong_" + std::to_string(static_cast<long>(this->mNow)) + "_" + std::to_string(timeFrameId) + "_" + std::to_string((*profitTarget).getAsDouble()) + "_" + std::to_string((*stopLoss).getAsDouble()) + "_" + std::to_string(inSampleOnly) + ".txt");
                      controller.exportSurvivingLongPatterns(profitTarget, stopLoss, fileNameLong);
                    }
                  else if (!side)
                    {
                      controller.run<false>(profitTarget, stopLoss, inSampleOnly);
                      std::string fileNameShort(std::string(ToString(patternSearchType)) + "_PatternsShort_" + std::to_string(static_cast<long>(this->mNow)) + "_" + std::to_string(timeFrameId) + "_" + std::to_string((*profitTarget).getAsDouble()) + "_" + std::to_string((*stopLoss).getAsDouble()) + "_" + std::to_string(inSampleOnly) + ".txt");
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
      const auto& targetStopVect = mSearchConfig->getTargetStopPair();
      const auto& targetStop = targetStopVect.at(ind);
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
    std::shared_ptr<RunParameters> mRunParameters;
    Decimal mTargetBase;
    std::time_t mNow;
  };

}



#endif // SEARCHRUN_H
