#pragma once

#include <memory>
#include <ostream>
#include "Security.h"
#include "number.h"
// Include the analyzer header to see PyramidResults
#include "MetaStrategyAnalyzer.h" 

namespace palvalidator
{
  namespace filtering
  {
    using namespace mkc_timeseries;

    template <typename Num>
    class PositionSizingCalculator
    {
    public:
      /**
       * @brief Computes dynamic position sizing based on Strategy Risk vs. Regulatory Limits.
       * @param security The security being traded.
       * @param result The actual performance results (containing Drawdown UB).
       * @param outputStream The stream to print the recommendation to.
       * @param maxAccountDrawdownTolerance The % of total equity the user is willing to lose (default 0.20 = 20%).
       */
      static void recommendSizing(std::shared_ptr<Security<Num>> security,
      const PyramidResults& result,
      std::ostream& outputStream,
      double maxAccountDrawdownTolerance = 0.20);
    };
  }
}
