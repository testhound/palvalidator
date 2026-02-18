#ifndef __TRADE_RESAMPLING_H
#define __TRADE_RESAMPLING_H

#include <vector>
#include <numeric>
#include <functional>
#include <cstddef>

namespace mkc_timeseries
{
  /**
   * @class Trade
   * @brief Encapsulates a sequence of mark-to-market daily returns for a single trade.
   * Treating the trade as the atomic unit preserves the structural integrity of 
   * multi-day holding periods during resampling.
   */
  template <class Decimal>
  class Trade
  {
  public:
    explicit Trade(std::vector<Decimal> returns) 
      : m_daily_returns(std::move(returns)) 
    {}

    /**
     * @brief Access the underlying daily mark-to-market returns.
     */
    const std::vector<Decimal>& getDailyReturns() const 
    { 
      return m_daily_returns; 
    }
    
    /**
     * @brief Returns the duration of the trade in bars.
     */
    std::size_t getDuration() const 
    { 
      return m_daily_returns.size(); 
    }

    /**
     * @brief Equality operator required for bootstrap degenerate distribution checks.
     */
    bool operator==(const Trade& other) const 
    {
      return m_daily_returns == other.m_daily_returns;
    }

    bool operator<(const Trade& other) const
    {
       // Required for certain sorting or diagnostic operations
       return std::accumulate(m_daily_returns.begin(), m_daily_returns.end(), Decimal{}) <
              std::accumulate(other.m_daily_returns.begin(), other.m_daily_returns.end(), Decimal{});
    }

  private:
    std::vector<Decimal> m_daily_returns;
  };

  /**
   * @class TradeFlatteningAdapter
   * @brief Adapts trade-level samples to flat-vector statistics.
   * * This class allows the bootstrap logic to work with Trade objects while 
   * enabling existing StatUtils functions (like Profit Factor or Geometric Mean) 
   * to operate on the resulting concatenated "flat" history.
   */
  template <class Decimal>
  class TradeFlatteningAdapter
  {
  public:
    using StatFn = std::function<Decimal(const std::vector<Decimal>&)>;

    /**
     * @brief Constructor taking a standard statistic function that expects 
     * a flat vector of returns.
     */
    explicit TradeFlatteningAdapter(StatFn flatStatFunc) 
      : m_flatStatFunc(std::move(flatStatFunc)) 
    {}

    /**
     * @brief Concatenates sampled Trade objects and applies the statistic.
     */
    Decimal operator()(const std::vector<Trade<Decimal>>& sampledTrades) const
    {
      std::vector<Decimal> flatVector;
      
      // Optimization: Reserve based on an assumed median duration of 3 bars.
      flatVector.reserve(sampledTrades.size() * 3); 

      for (const auto& trade : sampledTrades)
      {
        const auto& returns = trade.getDailyReturns();
        flatVector.insert(flatVector.end(), returns.begin(), returns.end());
      }

      return m_flatStatFunc(flatVector);
    }

  private:
    StatFn m_flatStatFunc;
  };
} // namespace mkc_timeseries

#endif
