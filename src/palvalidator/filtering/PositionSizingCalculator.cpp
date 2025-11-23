#include "filtering/PositionSizingCalculator.h"
#include "SecurityAttributesFactory.h"
#include "SecurityAttributes.h"
#include "DecimalConstants.h"
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <string>

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

      // Beginner-friendly labels and explanations
      std::string assetTypeLabel = "Standard Asset (1x)";
      std::string accountTypeLabel = "Standard Margin (2:1 Buying Power)";
      std::string beginnerExplanation;

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
            // 3x ETFs
            regulatoryAllocCap = 0.60;
            assetTypeLabel = "Triple Leveraged ETF (3x)";
            accountTypeLabel = "CASH ONLY (No Borrowing)";

            beginnerExplanation =
                "      [Beginner's Guide to 3x ETFs]\n"
                "      • This asset moves 3x faster than the market. It is volatile!\n"
                "      • Do NOT borrow money (margin loan) to buy this.\n"
                "      • We limit Total Allocation to 60% of your cash to create a safety buffer.\n"
                "        (e.g. If you have $10,000, buy only $6,000 worth total).\n"
                "      • This prevents a 'Margin Call' if the asset drops 33% in a day.";
        }
        else 
        {
            // 2x ETFs
            regulatoryAllocCap = 1.00;
            assetTypeLabel = "Double Leveraged ETF (2x)";
            accountTypeLabel = "CASH ONLY (No Borrowing)";

            beginnerExplanation =
                "      [Beginner's Guide to 2x ETFs]\n"
                "      • This asset moves 2x faster than the market.\n"
                "      • Do NOT borrow money (margin loan) to buy this.\n"
                "      • You can safely use up to 100% of your cash because the asset itself\n"
                "        provides the leverage. No need to borrow from the broker.";
        }
      }
      else
      {
        // Standard Assets
        // Default values set above: regulatoryAllocCap = 2.0

        beginnerExplanation = 
            "      [Beginner's Guide to Standard Margin]\n"
            "      • This is a standard stock/ETF. To maximize returns, we use 'Regulation T' leverage.\n"
            "      • 'Reg T' allows you to hold $2 of stock for every $1 of cash you have.\n"
            "        (e.g. If you have $10,000, you can hold $20,000 of positions).\n"
            "      • You are borrowing the difference from your broker. This requires a 'Margin Account'.\n"
            "      • You will pay a small amount of daily interest on the borrowed part, but\n"
            "        for short-term trades (days), this cost is negligible compared to profit potential.";
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
          riskBasedAlloc = maxAccountDrawdownTolerance / strategyDrawdownUB;
      }
      else
      {
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
      os << "      Asset Type:        " << assetTypeLabel << "\n";
      os << "      Execution Mode:    " << accountTypeLabel << "\n";
      os << "      --------------------------------------------------\n";
      os << "      Strategy Max Drawdown (95% Conf): " << (strategyDrawdownUB * 100.0) << "%\n";
      os << "      Your Max Account Risk Tolerance:  " << (maxAccountDrawdownTolerance * 100.0) << "%\n";
      os << "      --------------------------------------------------\n";
      
      os << "      1. Regulatory Limit: " << (regulatoryAllocCap * 100.0) << "% (Legal Broker Limit)\n";
      os << "      2. Risk Limit:       " << (riskBasedAlloc * 100.0) << "% (Your Safety Limit)\n";
      os << "      --------------------------------------------------\n";
      
      os << "      RECOMMENDED TOTAL ALLOCATION: " << (finalTotalAllocation * 100.0) << "% of Account Equity\n";
      os << "      SIZE PER TRADE (" << totalPositions << " total):    " << (sizePerPosition * 100.0) << "% of Account Equity\n";
      os << "      --------------------------------------------------\n";
      os << beginnerExplanation << "\n";
      
      if (finalTotalAllocation < regulatoryAllocCap)
      {
          os << "      * Note: Sizing reduced below broker limits to match your risk tolerance.\n";
      }
      os << "      --------------------------------------------------\n";
    }

    template class PositionSizingCalculator<num::DefaultNumber>;
  }
}
