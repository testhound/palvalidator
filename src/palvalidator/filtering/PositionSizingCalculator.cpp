#include "filtering/PositionSizingCalculator.h"
#include "SecurityAttributesFactory.h"
#include "SecurityAttributes.h"
#include "DecimalConstants.h"
#include <cmath>
#include <iomanip>
#include <algorithm>

namespace palvalidator
{
  namespace filtering
  {
    template <typename Num>
    void PositionSizingCalculator<Num>::recommendSizing(std::shared_ptr<Security<Num>> security,
							const PyramidResults& result,
							std::ostream& os,
							double maxAccountDrawdownTolerance)
    {
      unsigned int pyramidLevel = result.getPyramidLevel();
      unsigned int totalPositions = pyramidLevel + 1;

      // ---------------------------------------------------------
      // 1. Determine Regulatory/Broker Cap (The "Hard Limit")
      // ---------------------------------------------------------
      double leverageRatio = 1.0;
      bool isLeveragedInstrument = false;
      double regulatoryAllocCap = 2.0; // Default: Reg T Margin (200%)
      std::string accountType = "REG T MARGIN (2:1)";

      try 
      {
        auto attrs = mkc_timeseries::getSecurityAttributes<Num>(security->getSymbol());
        if (attrs->isFund())
        {
          auto fundAttrs = std::dynamic_pointer_cast<FundSecurityAttributes<Num>>(attrs);
          if (fundAttrs)
          {
            leverageRatio = std::abs(fundAttrs->getLeverage().getAsDouble());
            if (leverageRatio > 1.1) isLeveragedInstrument = true;
          }
        }
      }
      catch (...) {}

      if (isLeveragedInstrument)
      {
        if (leverageRatio >= 2.5) 
        {
            // 3x ETFs: Strict Cash Only, 60% max buffer
            regulatoryAllocCap = 0.60;
            accountType = "CASH ONLY (Strict)";
        }
        else 
        {
            // 2x ETFs: Full Cash
            regulatoryAllocCap = 1.00;
            accountType = "CASH / 1:1 MARGIN";
        }
      }

      // ---------------------------------------------------------
      // 2. Determine Risk-Based Allocation (The "Soft Limit")
      // ---------------------------------------------------------
      double riskBasedAlloc = 0.0;
      double strategyDrawdownUB = 0.0;
      
      const auto& ddResults = result.getDrawdownResults();
      if (ddResults.hasResults() && ddResults.getUpperBound() > DecimalConstants<Num>::DecimalZero)
      {
          strategyDrawdownUB = ddResults.getUpperBound().getAsDouble();
          // Formula: (Max Tolerable Account Loss) / (Strategy Worst Case Loss)
          riskBasedAlloc = maxAccountDrawdownTolerance / strategyDrawdownUB;
      }
      else
      {
          // Fallback if stats missing: use Regulatory Cap
          riskBasedAlloc = regulatoryAllocCap;
      }

      // ---------------------------------------------------------
      // 3. Final Decision: MIN(Regulatory, Risk)
      // ---------------------------------------------------------
      double finalTotalAllocation = std::min(regulatoryAllocCap, riskBasedAlloc);
      double sizePerPosition = finalTotalAllocation / static_cast<double>(totalPositions);

      // ---------------------------------------------------------
      // 4. Output Report
      // ---------------------------------------------------------
      os << "\n      === Recommended Position Sizing (" << security->getSymbol() << ") ===\n";
      os << std::fixed << std::setprecision(2);
      os << "      Input: Strategy Drawdown (95% UB): " << (strategyDrawdownUB * 100.0) << "%\n";
      os << "      Input: Max Account Risk Target:    " << (maxAccountDrawdownTolerance * 100.0) << "%\n";
      os << "      --------------------------------------------------\n";
      
      os << "      1. Regulatory Cap:  " << (regulatoryAllocCap * 100.0) << "% (" << accountType << ")\n";
      os << "      2. Risk-Based Cap:  " << (riskBasedAlloc * 100.0) << "% (To stay within risk limit)\n";
      os << "      --------------------------------------------------\n";
      
      os << "      FINAL ALLOCATION (Total): " << (finalTotalAllocation * 100.0) << "% of Equity\n";
      os << "      PER POSITION (" << totalPositions << " pos):   " << (sizePerPosition * 100.0) << "% of Equity\n";
      
      if (finalTotalAllocation < regulatoryAllocCap)
      {
          os << "      NOTE: Sizing reduced below broker limits to respect your risk tolerance.\n";
      }
      else if (riskBasedAlloc > regulatoryAllocCap)
      {
          os << "      NOTE: Risk analysis permits larger size, but Broker/Regulatory limits cap you.\n";
      }
      os << "      --------------------------------------------------\n";
    }

    template class PositionSizingCalculator<num::DefaultNumber>;
  }
}
