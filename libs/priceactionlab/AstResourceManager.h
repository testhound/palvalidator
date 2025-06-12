#pragma once

#include <memory>
#include <shared_mutex>
#include "PalAst.h"

namespace mkc_palast
{
  /**
   * @brief Resource manager for AST factory and memory management.
   * 
   * This class replaces the global AstFactory with a managed resource approach
   * that provides thread-safe access and clear ownership semantics using shared_ptr.
   */
  class AstResourceManager
  {
  public:
    /**
     * @brief Constructs an AstResourceManager with a new AstFactory.
     */
    AstResourceManager()
      : mFactory(std::make_shared<AstFactory>())
    {}
    
    /**
     * @return Shared pointer to the AstFactory.
     */
    std::shared_ptr<AstFactory> getFactory() const
    {
      return mFactory;
    }
    
    /**
     * @brief Creates a PriceActionLabPattern with shared ownership.
     * @param description Shared pointer to the PatternDescription.
     * @param pattern Shared pointer to the PatternExpression.
     * @param entry Pointer to the MarketEntryExpression.
     * @param profitTarget Pointer to the ProfitTargetInPercentExpression.
     * @param stopLoss Pointer to the StopLossInPercentExpression.
     * @param volatilityAttr The volatility attribute.
     * @param portfolioAttr The portfolio filter attribute.
     * @return Shared pointer to the created PriceActionLabPattern.
     */
    std::shared_ptr<PriceActionLabPattern> createPattern(
							 std::shared_ptr<PatternDescription> description,
							 std::shared_ptr<PatternExpression> pattern,
							 std::shared_ptr<MarketEntryExpression> entry,
							 std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
							 std::shared_ptr<StopLossInPercentExpression> stopLoss,
							 PriceActionLabPattern::VolatilityAttribute volatilityAttr = PriceActionLabPattern::VOLATILITY_NONE,
							 PriceActionLabPattern::PortfolioAttribute portfolioAttr = PriceActionLabPattern::PORTFOLIO_FILTER_NONE
							 );
    
    // All methods now return shared_ptr for consistent memory management
    std::shared_ptr<PriceBarReference> getPriceOpen(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getPriceHigh(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getPriceLow(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getPriceClose(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getVolume(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getRoc1(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getIBS1(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getIBS2(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getIBS3(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getMeander(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getVChartLow(unsigned int barOffset);
    std::shared_ptr<PriceBarReference> getVChartHigh(unsigned int barOffset);
    
    std::shared_ptr<MarketEntryExpression> getLongMarketEntryOnOpen();
    std::shared_ptr<MarketEntryExpression> getShortMarketEntryOnOpen();
    
    std::shared_ptr<decimal7> getDecimalNumber(const std::string& numString);
    std::shared_ptr<decimal7> getDecimalNumber(int num);
    
    std::shared_ptr<ProfitTargetInPercentExpression> getLongProfitTarget(std::shared_ptr<decimal7> profitTarget);
    std::shared_ptr<ProfitTargetInPercentExpression> getShortProfitTarget(std::shared_ptr<decimal7> profitTarget);
    
    std::shared_ptr<StopLossInPercentExpression> getLongStopLoss(std::shared_ptr<decimal7> stopLoss);
    std::shared_ptr<StopLossInPercentExpression> getShortStopLoss(std::shared_ptr<decimal7> stopLoss);

  private:
    std::shared_ptr<AstFactory> mFactory;
  };

} // namespace mkc_palast
