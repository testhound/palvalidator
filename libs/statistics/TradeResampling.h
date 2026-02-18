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
    /**
     * @brief Default constructor: creates an empty trade.
     * 
     * Use addReturn() to populate incrementally, or assign from another Trade.
     * Required by std::vector::resize() for resampling operations.
     */
    Trade() = default;
    
    /**
     * @brief Move constructor from complete return sequence (efficient).
     * 
     * Use when you have a pre-built return sequence and want to avoid copying.
     * 
     * @param returns Complete sequence of daily returns (moved, not copied)
     * 
     * Example:
     *   std::vector<Decimal> rets = {0.01, 0.02, 0.03};
     *   Trade<Decimal> trade(std::move(rets));
     */
    explicit Trade(std::vector<Decimal>&& returns) 
      : m_daily_returns(std::move(returns)) 
    {}
    
    /**
     * @brief Copy constructor from complete return sequence.
     * 
     * Use when you need to keep the original vector.
     * 
     * @param returns Complete sequence of daily returns (copied)
     * 
     * Example:
     *   std::vector<Decimal> rets = {0.01, 0.02, 0.03};
     *   Trade<Decimal> trade(rets);  // rets still valid after
     */
    explicit Trade(const std::vector<Decimal>& returns) 
      : m_daily_returns(returns) 
    {}
    
    /**
     * @brief Add a single return to the trade (incremental construction).
     * 
     * Allows building trades bar-by-bar without pre-allocating a vector.
     * 
     * @param dailyReturn The return for one bar
     * 
     * Example:
     *   Trade<Decimal> trade;
     *   trade.addReturn(0.01);  // Bar 1
     *   trade.addReturn(0.02);  // Bar 2
     *   trade.addReturn(0.03);  // Bar 3
     */
    void addReturn(const Decimal& dailyReturn)
    {
      m_daily_returns.push_back(dailyReturn);
    }
    
    /**
     * @brief Reserve capacity for expected number of returns (optimization).
     * 
     * Call before addReturn() loop if you know the trade duration.
     * 
     * @param capacity Expected number of bars
     * 
     * Example:
     *   Trade<Decimal> trade;
     *   trade.reserve(8);  // Max 8 bars per your spec
     *   for (...) trade.addReturn(r);
     */
    void reserve(std::size_t capacity)
    {
      m_daily_returns.reserve(capacity);
    }

    /**
     * @brief Access the underlying daily mark-to-market returns.
     * @return Const reference to internal return sequence
     */
    const std::vector<Decimal>& getDailyReturns() const 
    { 
      return m_daily_returns; 
    }
    
    /**
     * @brief Returns the duration of the trade in bars.
     * @return Number of bars in the trade
     */
    std::size_t getDuration() const 
    { 
      return m_daily_returns.size(); 
    }
    
    /**
     * @brief Check if trade is empty (no returns).
     * @return true if duration is zero
     */
    bool empty() const
    {
      return m_daily_returns.empty();
    }

    /**
     * @brief Equality operator required for bootstrap degenerate distribution checks.
     */
    bool operator==(const Trade& other) const 
    {
      return m_daily_returns == other.m_daily_returns;
    }

    /**
     * @brief Less-than operator for sorting trades by total return.
     */
    bool operator<(const Trade& other) const
    {
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
