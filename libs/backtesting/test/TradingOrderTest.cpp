#include <catch2/catch_test_macros.hpp>
#include "TradingOrder.h"
#include "TestUtils.h" // For DecimalType and helper functions
#include "TimeSeriesEntry.h" // For TimeSeriesDate
#include "DecimalConstants.h"
#include "TradingOrderException.h"

// Using declarations for convenience
using namespace mkc_timeseries;

// Test constants
const std::string gTradingSymbol = "TEST";
const TradingVolume gUnitsInOrder(100, TradingVolume::SHARES);
const TimeSeriesDate gOrderDate(createDate("20230101"));
const TimeSeriesDate gFillDate(createDate("20230102"));
const DecimalType gFillPrice("155.50");
const DecimalType gLimitPrice("150.00");
const DecimalType gStopPrice("140.00");
const DecimalType gStopLossPercent("0.05");
const DecimalType gProfitTargetPercent("0.10");


// Mock Observer for testing notifications (optional, but good practice)
template <class Decimal>
class MockTradingOrderObserver : public TradingOrderObserver<Decimal> {
public:
    int executedCount = 0;
    int canceledCount = 0;
    TradingOrder<Decimal>* lastExecutedOrder = nullptr;
    TradingOrder<Decimal>* lastCanceledOrder = nullptr;

    void OrderExecuted (MarketOnOpenLongOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (MarketOnOpenShortOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (MarketOnOpenSellOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (MarketOnOpenCoverOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (SellAtLimitOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (CoverAtLimitOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (CoverAtStopOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }
    void OrderExecuted (SellAtStopOrder<Decimal> *order) override { executedCount++; lastExecutedOrder = order; }

    void OrderCanceled (MarketOnOpenLongOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (MarketOnOpenShortOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (MarketOnOpenSellOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (MarketOnOpenCoverOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (SellAtLimitOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (CoverAtLimitOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (CoverAtStopOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
    void OrderCanceled (SellAtStopOrder<Decimal> *order) override { canceledCount++; lastCanceledOrder = order; }
};

TEST_CASE("MarketOnOpenLongOrder Tests", "[trading_orders]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopLossPercent, gProfitTargetPercent);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);
    REQUIRE(order.getStopLoss() == gStopLossPercent);
    REQUIRE(order.getProfitTarget() == gProfitTargetPercent);

    REQUIRE(order.isLongOrder() == true);
    REQUIRE(order.isShortOrder() == false);
    REQUIRE(order.isEntryOrder() == true);
    REQUIRE(order.isExitOrder() == false);
    REQUIRE(order.isMarketOrder() == true);
    REQUIRE(order.isStopOrder() == false);
    REQUIRE(order.isLimitOrder() == false);
    REQUIRE(order.getOrderPriority() == 1); // Market orders have highest priority

    REQUIRE(order.isOrderPending() == true);
    REQUIRE(order.isOrderExecuted() == false);
    REQUIRE(order.isOrderCanceled() == false);

    SECTION("MarkOrderExecuted") {
        order.MarkOrderExecuted(gFillDate, gFillPrice);
        REQUIRE(order.isOrderExecuted() == true);
        REQUIRE(order.getFillDate() == gFillDate);
        REQUIRE(order.getFillPrice() == gFillPrice);
    }

    SECTION("MarkOrderCanceled") {
        order.MarkOrderCanceled();
        REQUIRE(order.isOrderCanceled() == true);
    }
}

TEST_CASE("MarketOnOpenShortOrder Tests", "[trading_orders]") {
    MarketOnOpenShortOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopLossPercent, gProfitTargetPercent);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);
    REQUIRE(order.getStopLoss() == gStopLossPercent);
    REQUIRE(order.getProfitTarget() == gProfitTargetPercent);

    REQUIRE(order.isLongOrder() == false);
    REQUIRE(order.isShortOrder() == true);
    REQUIRE(order.isEntryOrder() == true);
    REQUIRE(order.isExitOrder() == false);
    REQUIRE(order.isMarketOrder() == true);
    REQUIRE(order.isStopOrder() == false);
    REQUIRE(order.isLimitOrder() == false);
    REQUIRE(order.getOrderPriority() == 1);

    REQUIRE(order.isOrderPending() == true);
}

TEST_CASE("MarketOnOpenSellOrder Tests", "[trading_orders]") {
    MarketOnOpenSellOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);

    REQUIRE(order.isLongOrder() == true); // Selling to close a long
    REQUIRE(order.isShortOrder() == false);
    REQUIRE(order.isEntryOrder() == false);
    REQUIRE(order.isExitOrder() == true);
    REQUIRE(order.isMarketOrder() == true);
    REQUIRE(order.isStopOrder() == false);
    REQUIRE(order.isLimitOrder() == false);
    REQUIRE(order.getOrderPriority() == 1);

    REQUIRE(order.isOrderPending() == true);
}

TEST_CASE("MarketOnOpenCoverOrder Tests", "[trading_orders]") {
    MarketOnOpenCoverOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);

    REQUIRE(order.isLongOrder() == false);
    REQUIRE(order.isShortOrder() == true); // Covering to close a short
    REQUIRE(order.isEntryOrder() == false);
    REQUIRE(order.isExitOrder() == true);
    REQUIRE(order.isMarketOrder() == true);
    REQUIRE(order.isStopOrder() == false);
    REQUIRE(order.isLimitOrder() == false);
    REQUIRE(order.getOrderPriority() == 1);

    REQUIRE(order.isOrderPending() == true);
}

TEST_CASE("SellAtLimitOrder Tests", "[trading_orders]") {
    SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);
    REQUIRE(order.getLimitPrice() == gLimitPrice);

    REQUIRE(order.isLongOrder() == true); // Selling to close a long
    REQUIRE(order.isShortOrder() == false);
    REQUIRE(order.isEntryOrder() == false);
    REQUIRE(order.isExitOrder() == true);
    REQUIRE(order.isMarketOrder() == false);
    REQUIRE(order.isStopOrder() == false);
    REQUIRE(order.isLimitOrder() == true);
    REQUIRE(order.getOrderPriority() == 10); // Limit order priority

    REQUIRE(order.isOrderPending() == true);

    SECTION("ValidateOrderExecution") {
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gLimitPrice));
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gLimitPrice + DecimalType("1.0")));
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, gLimitPrice - DecimalType("0.01")), TradingOrderNotExecutedException);
    }
}

TEST_CASE("CoverAtLimitOrder Tests", "[trading_orders]") {
    CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);
    REQUIRE(order.getLimitPrice() == gLimitPrice);

    REQUIRE(order.isLongOrder() == false);
    REQUIRE(order.isShortOrder() == true); // Covering to close a short
    REQUIRE(order.isEntryOrder() == false);
    REQUIRE(order.isExitOrder() == true);
    REQUIRE(order.isMarketOrder() == false);
    REQUIRE(order.isStopOrder() == false);
    REQUIRE(order.isLimitOrder() == true);
    REQUIRE(order.getOrderPriority() == 10);

    REQUIRE(order.isOrderPending() == true);

    SECTION("ValidateOrderExecution") {
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gLimitPrice));
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gLimitPrice - DecimalType("1.0")));
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, gLimitPrice + DecimalType("0.01")), TradingOrderNotExecutedException);
    }
}

TEST_CASE("CoverAtStopOrder Tests", "[trading_orders]") {
    CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);
    REQUIRE(order.getStopPrice() == gStopPrice);

    REQUIRE(order.isLongOrder() == false);
    REQUIRE(order.isShortOrder() == true); // Covering to close a short
    REQUIRE(order.isEntryOrder() == false);
    REQUIRE(order.isExitOrder() == true);
    REQUIRE(order.isMarketOrder() == false);
    REQUIRE(order.isStopOrder() == true);
    REQUIRE(order.isLimitOrder() == false);
    REQUIRE(order.getOrderPriority() == 5); // Stop order priority

    REQUIRE(order.isOrderPending() == true);

    SECTION("ValidateOrderExecution") {
        // Stop orders become market orders when triggered. The price can be worse.
        // For a CoverAtStop (buy stop), fill price must be >= stop price.
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gStopPrice));
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gStopPrice + DecimalType("1.0")));
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, gStopPrice - DecimalType("0.01")), TradingOrderNotExecutedException);
    }
}

TEST_CASE("SellAtStopOrder Tests", "[trading_orders]") {
    SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);

    REQUIRE(order.getTradingSymbol() == gTradingSymbol);
    REQUIRE(order.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
    REQUIRE(order.getOrderDate() == gOrderDate);
    REQUIRE(order.getStopPrice() == gStopPrice);

    REQUIRE(order.isLongOrder() == true); // Selling to close a long
    REQUIRE(order.isShortOrder() == false);
    REQUIRE(order.isEntryOrder() == false);
    REQUIRE(order.isExitOrder() == true);
    REQUIRE(order.isMarketOrder() == false);
    REQUIRE(order.isStopOrder() == true);
    REQUIRE(order.isLimitOrder() == false);
    REQUIRE(order.getOrderPriority() == 5);

    REQUIRE(order.isOrderPending() == true);

    SECTION("ValidateOrderExecution") {
        // For a SellAtStop (sell stop), fill price must be <= stop price.
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gStopPrice));
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, gStopPrice - DecimalType("1.0")));
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, gStopPrice + DecimalType("0.01")), TradingOrderNotExecutedException);
    }
}

// Dummy TradingOrder concrete class for testing states
template <class Decimal>
class DummyTradingOrder : public TradingOrder<Decimal> {
public:
    DummyTradingOrder(const std::string& symbol, const TradingVolume& units, const TimeSeriesDate& date)
        : TradingOrder<Decimal>(symbol, units, date) {}

    uint32_t getOrderPriority() const override { return 0; }
    bool isLongOrder() const override { return false; }
    bool isShortOrder() const override { return false; }
    bool isEntryOrder() const override { return false; }
    bool isExitOrder() const override { return false; }
    bool isMarketOrder() const override { return false; }
    bool isStopOrder() const override { return false; }
    bool isLimitOrder() const override { return false; }
    void accept(TradingOrderVisitor<Decimal>& visitor) override { /* Do nothing for dummy */ }

protected:
    void notifyOrderExecuted() override { /* Do nothing */ }
    void notifyOrderCanceled() override { /* Do nothing */ }
    void ValidateOrderExecution(const TimeSeriesDate& fillDate, const Decimal& fillPrice) const override {
        if (fillDate < this->getOrderDate())
            throw TradingOrderNotExecutedException("Fill date before order date.");
    }
};


TEST_CASE("PendingOrderState Tests", "[trading_order_states]") {
    PendingOrderState<DecimalType> state;
    DummyTradingOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);

    REQUIRE(state.isOrderPending() == true);
    REQUIRE(state.isOrderExecuted() == false);
    REQUIRE(state.isOrderCanceled() == false);

    REQUIRE_THROWS_AS(state.getFillPrice(), TradingOrderNotExecutedException);
    REQUIRE_THROWS_AS(state.getFillDate(), TradingOrderNotExecutedException);

    SECTION("MarkOrderExecuted") {
        REQUIRE(order.isOrderPending()); // Starts pending
        state.MarkOrderExecuted(&order, gFillDate, gFillPrice);
        REQUIRE(order.isOrderExecuted()); // Should transition to ExecutedOrderState
        REQUIRE(order.getFillPrice() == gFillPrice);
        REQUIRE(order.getFillDate() == gFillDate);
    }

    SECTION("MarkOrderCanceled") {
        REQUIRE(order.isOrderPending());
        state.MarkOrderCanceled(&order);
        REQUIRE(order.isOrderCanceled()); // Should transition to CanceledOrderState
    }
}

TEST_CASE("ExecutedOrderState Tests", "[trading_order_states]") {
    ExecutedOrderState<DecimalType> state(gFillDate, gFillPrice);
    DummyTradingOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    // Manually set the order to this state for testing (though usually done via PendingOrderState)
    // This is tricky because TradingOrder manages its state internally via ChangeState.
    // We'll test the state's methods directly.

    REQUIRE(state.isOrderPending() == false);
    REQUIRE(state.isOrderExecuted() == true);
    REQUIRE(state.isOrderCanceled() == false);

    REQUIRE(state.getFillPrice() == gFillPrice);
    REQUIRE(state.getFillDate() == gFillDate);

    SECTION("MarkOrderExecuted (on already executed)") {
        REQUIRE_THROWS_AS(state.MarkOrderExecuted(&order, gFillDate, gFillPrice), TradingOrderExecutedException);
    }

    SECTION("MarkOrderCanceled (on already executed)") {
        REQUIRE_THROWS_AS(state.MarkOrderCanceled(&order), TradingOrderExecutedException);
    }
}

TEST_CASE("CanceledOrderState Tests", "[trading_order_states]") {
    CanceledOrderState<DecimalType> state;
    DummyTradingOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);

    REQUIRE(state.isOrderPending() == false);
    REQUIRE(state.isOrderExecuted() == false);
    REQUIRE(state.isOrderCanceled() == true);

    REQUIRE_THROWS_AS(state.getFillPrice(), TradingOrderNotExecutedException);
    REQUIRE_THROWS_AS(state.getFillDate(), TradingOrderNotExecutedException);

    SECTION("MarkOrderExecuted (on canceled)") {
        REQUIRE_THROWS_AS(state.MarkOrderExecuted(&order, gFillDate, gFillPrice), TradingOrderNotExecutedException);
    }

    SECTION("MarkOrderCanceled (on already canceled)") {
        REQUIRE_THROWS_AS(state.MarkOrderCanceled(&order), TradingOrderExecutedException); // As per implementation
    }
}

TEST_CASE("TradingOrder State Transitions and Notifications", "[trading_orders]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    auto observer = std::make_shared<MockTradingOrderObserver<DecimalType>>();
    order.addObserver(observer);

    REQUIRE(order.isOrderPending() == true);

    SECTION("Execute Order") {
        order.MarkOrderExecuted(gFillDate, gFillPrice);
        REQUIRE(order.isOrderExecuted() == true);
        REQUIRE(order.getFillDate() == gFillDate);
        REQUIRE(order.getFillPrice() == gFillPrice);
        REQUIRE(observer->executedCount == 1);
        REQUIRE(observer->canceledCount == 0);
        REQUIRE(observer->lastExecutedOrder == &order);

        // Try to execute again
        REQUIRE_THROWS_AS(order.MarkOrderExecuted(gFillDate, gFillPrice), TradingOrderExecutedException);
        // Try to cancel
        REQUIRE_THROWS_AS(order.MarkOrderCanceled(), TradingOrderExecutedException);

    }

    SECTION("Cancel Order") {
        order.MarkOrderCanceled();
        REQUIRE(order.isOrderCanceled() == true);
        REQUIRE(observer->executedCount == 0);
        REQUIRE(observer->canceledCount == 1);
        REQUIRE(observer->lastCanceledOrder == &order);

        // Try to cancel again
        REQUIRE_THROWS_AS(order.MarkOrderCanceled(), TradingOrderExecutedException); // As per implementation
         // Try to execute
        REQUIRE_THROWS_AS(order.MarkOrderExecuted(gFillDate, gFillPrice), TradingOrderNotExecutedException);
    }

    SECTION("Execute with invalid date") {
        TimeSeriesDate pastDate(createDate("20221231"));
        REQUIRE_THROWS_AS(order.MarkOrderExecuted(pastDate, gFillPrice), TradingOrderNotExecutedException);
        REQUIRE(order.isOrderPending() == true); // State should not change
        REQUIRE(observer->executedCount == 0);
    }

    SECTION("Zero units in order throws")
    {
      REQUIRE_THROWS_AS((MarketOnOpenLongOrder<DecimalType>(gTradingSymbol,
							   TradingVolume(0,TradingVolume::SHARES),
							    gOrderDate)), TradingOrderException);
    }
}
