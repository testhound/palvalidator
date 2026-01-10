#pragma once

#include <memory>
#include "PalAst.h"

namespace mkc_palast
{
  /**
   * @brief Resource manager for AST factory and memory management.
   * 
   * This class provides a clean, modern C++ facade over AstFactory, eliminating
   * raw pointers from the client-facing API and providing clear ownership semantics
   * through shared_ptr.
   * 
   * @section thread_safety Thread Safety
   * 
   * **Object-Level Thread Safety:**
   * A single AstResourceManager instance is thread-safe for concurrent access
   * from multiple threads. This safety is provided by the underlying AstFactory's
   * internal mutexes that protect its caching mechanisms.
   * 
   * **Factory Isolation:**
   * Each AstResourceManager instance creates and owns its own AstFactory. This
   * means:
   * - Different AstResourceManager instances have separate caches
   * - Objects created by different managers are distinct, even for identical inputs
   * - No cross-manager synchronization is needed
   * 
   * **Safe Concurrent Operations:**
   * - ✓ Multiple threads can call methods on the same manager concurrently
   * - ✓ Multiple threads can create objects through the same manager
   * - ✓ Caching is thread-safe (protected by AstFactory's internal mutexes)
   * - ✓ Multiple managers can be used in different threads without synchronization
   * 
   * **Example - Safe Usage:**
   * @code
   * // Single manager shared across threads - SAFE
   * AstResourceManager manager;
   * 
   * std::thread t1([&manager]() {
   *     auto p = manager.getPriceOpen(0);  // Thread-safe
   * });
   * 
   * std::thread t2([&manager]() {
   *     auto p = manager.getPriceOpen(0);  // Thread-safe, returns cached object
   * });
   * @endcode
   * 
   * @section ownership Ownership and Lifetime
   * 
   * The manager owns its AstFactory via shared_ptr. Objects returned from the
   * manager may be cached by the factory, so:
   * - Cached objects (e.g., PriceBarOpen with offset < 10) persist with the factory
   * - Non-cached objects are owned by the returned shared_ptr
   * - Objects can safely outlive the manager if you hold a shared_ptr to them
   * 
   * @section usage Typical Usage Pattern
   * 
   * Create one manager per parsing context or application session:
   * @code
   * // Long-lived manager for the application
   * AstResourceManager manager;
   * 
   * // Use throughout application lifetime
   * auto pattern = manager.createPattern(...);
   * auto decimal = manager.getDecimalNumber("123.45");
   * @endcode
   * 
   * @note This class does not implement its own synchronization. Thread safety
   *       comes entirely from the underlying AstFactory. If AstFactory's thread
   *       safety guarantees change, this class's guarantees change accordingly.
   */
  class AstResourceManager
  {
  public:
    /**
     * @brief Constructs an AstResourceManager with a new AstFactory.
     * 
     * Each constructed manager gets its own factory with independent caches.
     * Thread-safe: Multiple threads can construct different managers concurrently.
     */
    AstResourceManager()
      : mFactory(std::make_shared<AstFactory>())
    {}
    
    /**
     * @brief Gets the underlying AstFactory.
     * 
     * @return Shared pointer to the AstFactory.
     * @note The factory is shared across all method calls within this manager.
     *       Multiple calls to getFactory() return the same factory instance.
     * @threadsafe Yes - Returns a shared_ptr to the same factory instance.
     */
    std::shared_ptr<AstFactory> getFactory() const
    {
      return mFactory;
    }
    
    /**
     * @brief Creates a PriceActionLabPattern with shared ownership.
     * 
     * @param description Shared pointer to the PatternDescription.
     * @param pattern Shared pointer to the PatternExpression.
     * @param entry Shared pointer to the MarketEntryExpression.
     * @param profitTarget Shared pointer to the ProfitTargetInPercentExpression.
     * @param stopLoss Shared pointer to the StopLossInPercentExpression.
     * @param volatilityAttr The volatility attribute (default: VOLATILITY_NONE).
     * @param portfolioAttr The portfolio filter attribute (default: PORTFOLIO_FILTER_NONE).
     * @return Shared pointer to the created PriceActionLabPattern.
     * @threadsafe Yes - Can be called concurrently from multiple threads.
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
    
    // ========================================================================
    // Price Bar Reference Methods
    // ========================================================================
    // All methods return shared_ptr for consistent memory management.
    // All methods are thread-safe - protected by AstFactory's internal mutexes.
    // Objects with offset < MaxNumBarOffsets are cached and reused.
    // ========================================================================
    
    /**
     * @brief Gets a reference to the opening price at the specified bar offset.
     * @param barOffset The bar offset (0 = current bar, 1 = previous bar, etc.).
     * @return Shared pointer to a PriceBarOpen object.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<PriceBarReference> getPriceOpen(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the highest price at the specified bar offset.
     * @param barOffset The bar offset.
     * @return Shared pointer to a PriceBarHigh object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getPriceHigh(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the lowest price at the specified bar offset.
     * @param barOffset The bar offset.
     * @return Shared pointer to a PriceBarLow object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getPriceLow(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the closing price at the specified bar offset.
     * @param barOffset The bar offset.
     * @return Shared pointer to a PriceBarClose object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getPriceClose(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the trading volume at the specified bar offset.
     * @param barOffset The bar offset.
     * @return Shared pointer to a VolumeBarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getVolume(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the Rate of Change (1-period) indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to a Roc1BarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getRoc1(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the IBS1 (Internal Bar Strength) indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to an IBS1BarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getIBS1(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the IBS2 indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to an IBS2BarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getIBS2(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the IBS3 indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to an IBS3BarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getIBS3(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the Meander indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to a MeanderBarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getMeander(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the VChart Low indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to a VChartLowBarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getVChartLow(unsigned int barOffset);
    
    /**
     * @brief Gets a reference to the VChart High indicator.
     * @param barOffset The bar offset.
     * @return Shared pointer to a VChartHighBarReference object.
     * @threadsafe Yes
     */
    std::shared_ptr<PriceBarReference> getVChartHigh(unsigned int barOffset);
    
    // ========================================================================
    // Market Entry Expression Methods
    // ========================================================================
    
    /**
     * @brief Gets a long market entry on open expression.
     * @return Shared pointer to a LongMarketEntryOnOpen object.
     * @note This returns a cached singleton object. Multiple calls return
     *       the same instance.
     * @threadsafe Yes
     */
    std::shared_ptr<MarketEntryExpression> getLongMarketEntryOnOpen();
    
    /**
     * @brief Gets a short market entry on open expression.
     * @return Shared pointer to a ShortMarketEntryOnOpen object.
     * @note This returns a cached singleton object. Multiple calls return
     *       the same instance.
     * @threadsafe Yes
     */
    std::shared_ptr<MarketEntryExpression> getShortMarketEntryOnOpen();
    
    // ========================================================================
    // Decimal Number Methods
    // ========================================================================
    
    /**
     * @brief Creates or retrieves a cached decimal number from a string.
     * 
     * @param numString The string representation of the number (e.g., "123.45").
     * @return Shared pointer to a decimal7 object.
     * 
     * @note Values are cached - multiple calls with the same string value
     *       return the same cached object.
     * @note The input string is never modified.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<decimal7> getDecimalNumber(const std::string& numString);
    
    /**
     * @brief Creates or retrieves a cached decimal number from an integer.
     * 
     * @param num The integer value.
     * @return Shared pointer to a decimal7 object.
     * 
     * @note Values are cached - multiple calls with the same integer
     *       return the same cached object.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<decimal7> getDecimalNumber(int num);
    
    // ========================================================================
    // Profit Target Methods
    // ========================================================================
    
    /**
     * @brief Creates or retrieves a cached long-side profit target.
     * 
     * @param profitTarget Shared pointer to the profit target value.
     * @return Shared pointer to a ProfitTargetInPercentExpression (base class).
     * 
     * @note Profit targets are cached by value - multiple calls with decimal
     *       numbers of the same value return the same cached object.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<ProfitTargetInPercentExpression> getLongProfitTarget(
        std::shared_ptr<decimal7> profitTarget);
    
    /**
     * @brief Creates or retrieves a cached short-side profit target.
     * 
     * @param profitTarget Shared pointer to the profit target value.
     * @return Shared pointer to a ProfitTargetInPercentExpression (base class).
     * 
     * @note Profit targets are cached by value.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<ProfitTargetInPercentExpression> getShortProfitTarget(
        std::shared_ptr<decimal7> profitTarget);
    
    // ========================================================================
    // Stop Loss Methods
    // ========================================================================
    
    /**
     * @brief Creates or retrieves a cached long-side stop loss.
     * 
     * @param stopLoss Shared pointer to the stop loss value.
     * @return Shared pointer to a StopLossInPercentExpression (base class).
     * 
     * @note Stop losses are cached by value - multiple calls with decimal
     *       numbers of the same value return the same cached object.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<StopLossInPercentExpression> getLongStopLoss(
        std::shared_ptr<decimal7> stopLoss);
    
    /**
     * @brief Creates or retrieves a cached short-side stop loss.
     * 
     * @param stopLoss Shared pointer to the stop loss value.
     * @return Shared pointer to a StopLossInPercentExpression (base class).
     * 
     * @note Stop losses are cached by value.
     * @threadsafe Yes - Caching is protected by AstFactory's mutex.
     */
    std::shared_ptr<StopLossInPercentExpression> getShortStopLoss(
        std::shared_ptr<decimal7> stopLoss);

  private:
    /**
     * @brief Shared pointer to the underlying AstFactory.
     * 
     * This factory is created during construction and is shared across all
     * method calls within this manager instance. The factory's internal
     * mutexes provide thread-safe access to cached objects.
     */
    std::shared_ptr<AstFactory> mFactory;
  };

} // namespace mkc_palast
