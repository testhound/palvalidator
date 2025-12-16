// DummyBacktesterStrategy.h
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include "BacktesterStrategy.h"

namespace mkc_timeseries
{
  /**
   * @brief Minimal concrete BacktesterStrategy for testing / integration.
   *
   * Responsibilities:
   *  - Own a fixed vector of per-bar / per-trade returns supplied by the test.
   *  - Provide trivial implementations of the pure-virtual hooks so it can be
   *    passed anywhere a BacktesterStrategy<Decimal>& is required
   *    (e.g. TradingBootstrapFactory, StrategyAutoBootstrap).
   *
   * Notes:
   *  - eventEntryOrders / eventExitOrders are no-ops; this strategy does not
   *    submit any orders.
   *  - getPositionReturnsVector() returns the synthetic returns passed in at
   *    construction time.
   *  - getPositionDirectionVector() is derived from the sign of the returns:
   *      > 0  → +1
   *      < 0  → -1
   *      == 0 →  0
   *  - numTradingOpportunities() simply returns returns.size().
   *  - hashCode() is overridden to be deterministic based on the strategy name
   *    so RNG seeding via TradingBootstrapFactory is stable across runs.
   */
  template <class Decimal>
  class DummyBacktesterStrategy : public BacktesterStrategy<Decimal>
  {
  public:
    using Base          = BacktesterStrategy<Decimal>;
    using PortfolioType = Portfolio<Decimal>;

    DummyBacktesterStrategy(
        const std::string&                    strategyName,
        std::shared_ptr<PortfolioType>        portfolio,
        const std::vector<Decimal>&           returns,
        const StrategyOptions&                strategyOptions = defaultStrategyOptions)
      : Base(strategyName, portfolio, strategyOptions),
        m_returns(returns),
        m_directions(makeDirections(returns))
    {
    }

    // -----------------------------------------------------------------------
    // Core BacktesterStrategy overrides
    // -----------------------------------------------------------------------

    void eventExitOrders(Security<Decimal>* /*aSecurity*/,
                         const InstrumentPosition<Decimal>& /*instrPos*/,
                         const boost::posix_time::ptime& /*processingDateTime*/) override
    {
      // No-op: this dummy strategy never submits orders.
    }

    void eventEntryOrders(Security<Decimal>* /*aSecurity*/,
                          const InstrumentPosition<Decimal>& /*instrPos*/,
                          const boost::posix_time::ptime& /*processingDateTime*/) override
    {
      // No-op: this dummy strategy never submits orders.
    }

    const TradingVolume&
    getSizeForOrder(const Security<Decimal>& aSecurity) const override
    {
      // Reuse the default equity / contract sizing logic from BacktesterStrategy.
      return Base::getSizeForOrder(aSecurity);
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    clone(const std::shared_ptr<PortfolioType>& portfolio) const override
    {
      // New strategy instance bound to the provided portfolio, but with the same
      // synthetic returns and strategy options.
      return std::make_shared<DummyBacktesterStrategy<Decimal>>(
          this->getStrategyName(),
          portfolio,
          m_returns,
          this->getStrategyOptions());
    }

    std::shared_ptr<BacktesterStrategy<Decimal>>
    cloneForBackTesting() const override
    {
      // For testing, just clone using the current portfolio pointer.
      return clone(this->getPortfolio());
    }

    std::vector<int> getPositionDirectionVector() const override
    {
      return m_directions;
    }

    std::vector<Decimal> getPositionReturnsVector() const override
    {
      return m_returns;
    }

    unsigned long numTradingOpportunities() const override
    {
      // In this dummy strategy, treat each return as one opportunity.
      return static_cast<unsigned long>(m_returns.size());
    }

    uint32_t getPatternMaxBarsBack() const override
    {
      // No pattern / lookback requirement for the dummy strategy.
      return 0u;
    }

    // -----------------------------------------------------------------------
    // Deterministic hash for stable RNG seeding
    // -----------------------------------------------------------------------
    unsigned long long hashCode() const override
    {
      // Stable hash based solely on the strategy name to keep RNG seeding
      // deterministic across runs.
      boost::hash<std::string> hasher;
      return static_cast<unsigned long long>(hasher(this->getStrategyName()));
    }

  private:
    static std::vector<int> makeDirections(const std::vector<Decimal>& returns)
    {
      std::vector<int> dirs;
      dirs.reserve(returns.size());
      for (const auto& r : returns)
      {
        if (r > Decimal(0))
          dirs.push_back(+1);
        else if (r < Decimal(0))
          dirs.push_back(-1);
        else
          dirs.push_back(0);
      }
      return dirs;
    }

    std::vector<Decimal> m_returns;
    std::vector<int>     m_directions;
  };

} // namespace mkc_timeseries
