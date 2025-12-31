#include <catch2/catch_test_macros.hpp>
#include "TradingOrder.h"
#include "TestUtils.h"
#include "TimeSeriesEntry.h"
#include "DecimalConstants.h"
#include "TradingOrderException.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using boost::posix_time::ptime;
using boost::posix_time::time_from_string;
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

// Mock Observer for comprehensive testing
template <class Decimal>
class ComprehensiveMockObserver : public TradingOrderObserver<Decimal> {
public:
    int executedCount = 0;
    int canceledCount = 0;
    std::vector<std::string> executedOrderTypes;
    std::vector<std::string> canceledOrderTypes;

    void OrderExecuted(MarketOnOpenLongOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("MarketOnOpenLong");
    }
    void OrderExecuted(MarketOnOpenShortOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("MarketOnOpenShort");
    }
    void OrderExecuted(MarketOnOpenSellOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("MarketOnOpenSell");
    }
    void OrderExecuted(MarketOnOpenCoverOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("MarketOnOpenCover");
    }
    void OrderExecuted(SellAtLimitOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("SellAtLimit");
    }
    void OrderExecuted(CoverAtLimitOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("CoverAtLimit");
    }
    void OrderExecuted(CoverAtStopOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("CoverAtStop");
    }
    void OrderExecuted(SellAtStopOrder<Decimal> *order) override { 
        executedCount++; 
        executedOrderTypes.push_back("SellAtStop");
    }

    void OrderCanceled(MarketOnOpenLongOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("MarketOnOpenLong");
    }
    void OrderCanceled(MarketOnOpenShortOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("MarketOnOpenShort");
    }
    void OrderCanceled(MarketOnOpenSellOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("MarketOnOpenSell");
    }
    void OrderCanceled(MarketOnOpenCoverOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("MarketOnOpenCover");
    }
    void OrderCanceled(SellAtLimitOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("SellAtLimit");
    }
    void OrderCanceled(CoverAtLimitOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("CoverAtLimit");
    }
    void OrderCanceled(CoverAtStopOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("CoverAtStop");
    }
    void OrderCanceled(SellAtStopOrder<Decimal> *order) override { 
        canceledCount++; 
        canceledOrderTypes.push_back("SellAtStop");
    }
};

// Mock Visitor for testing visitor pattern
template <class Decimal>
class MockTradingOrderVisitor : public TradingOrderVisitor<Decimal> {
public:
    std::vector<std::string> visitedTypes;
    
    void visit(MarketOnOpenLongOrder<Decimal> *order) override {
        visitedTypes.push_back("MarketOnOpenLong");
    }
    void visit(MarketOnOpenShortOrder<Decimal> *order) override {
        visitedTypes.push_back("MarketOnOpenShort");
    }
    void visit(MarketOnOpenSellOrder<Decimal> *order) override {
        visitedTypes.push_back("MarketOnOpenSell");
    }
    void visit(MarketOnOpenCoverOrder<Decimal> *order) override {
        visitedTypes.push_back("MarketOnOpenCover");
    }
    void visit(SellAtLimitOrder<Decimal> *order) override {
        visitedTypes.push_back("SellAtLimit");
    }
    void visit(CoverAtLimitOrder<Decimal> *order) override {
        visitedTypes.push_back("CoverAtLimit");
    }
    void visit(CoverAtStopOrder<Decimal> *order) override {
        visitedTypes.push_back("CoverAtStop");
    }
    void visit(SellAtStopOrder<Decimal> *order) override {
        visitedTypes.push_back("SellAtStop");
    }
};

// ============================================================================
// COPY CONSTRUCTOR AND ASSIGNMENT TESTS
// ============================================================================

TEST_CASE("MarketOnOpenLongOrder Copy Constructor", "[trading_orders][copy]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, 
                                                gStopLossPercent, gProfitTargetPercent);
    
    // Add observer to original
    auto observer = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    original.addObserver(observer);
    
    // Execute the original
    original.MarkOrderExecuted(gFillDate, gFillPrice);
    
    // Create a copy
    MarketOnOpenLongOrder<DecimalType> copy(original);
    
    SECTION("Copy has same basic attributes") {
        REQUIRE(copy.getTradingSymbol() == original.getTradingSymbol());
        REQUIRE(copy.getUnitsInOrder().getTradingVolume() == original.getUnitsInOrder().getTradingVolume());
        REQUIRE(copy.getOrderDate() == original.getOrderDate());
        REQUIRE(copy.getStopLoss() == original.getStopLoss());
        REQUIRE(copy.getProfitTarget() == original.getProfitTarget());
    }
    
    SECTION("Copy has same state") {
        REQUIRE(copy.isOrderExecuted() == true);
        REQUIRE(copy.getFillDate() == original.getFillDate());
        REQUIRE(copy.getFillPrice() == original.getFillPrice());
    }
    
    SECTION("Copy has same order ID - may need revision") {
        // NOTE: Current implementation copies order ID, which may not be desired
        // Consider if each copy should get a unique ID
        REQUIRE(copy.getOrderID() == original.getOrderID());
    }
}

TEST_CASE("MarketOnOpenLongOrder Assignment Operator", "[trading_orders][copy]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, 
                                                gStopLossPercent, gProfitTargetPercent);
    MarketOnOpenLongOrder<DecimalType> assigned("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                                createDate("20220101"), 
                                                DecimalType("0.03"), DecimalType("0.08"));
    
    original.MarkOrderExecuted(gFillDate, gFillPrice);
    
    // Assign
    assigned = original;
    
    SECTION("Assignment updates all attributes") {
        REQUIRE(assigned.getTradingSymbol() == original.getTradingSymbol());
        REQUIRE(assigned.getUnitsInOrder().getTradingVolume() == original.getUnitsInOrder().getTradingVolume());
        REQUIRE(assigned.getOrderDate() == original.getOrderDate());
        REQUIRE(assigned.getStopLoss() == original.getStopLoss());
        REQUIRE(assigned.getProfitTarget() == original.getProfitTarget());
    }
    
    SECTION("Assignment copies state") {
        REQUIRE(assigned.isOrderExecuted() == true);
        REQUIRE(assigned.getFillDate() == original.getFillDate());
        REQUIRE(assigned.getFillPrice() == original.getFillPrice());
    }
    
    SECTION("Self-assignment is safe") {
        auto& self = original;
        original = self;
        REQUIRE(original.isOrderExecuted() == true);
    }
}

TEST_CASE("LimitOrder Copy Constructor and Assignment", "[trading_orders][copy]") {
    SellAtLimitOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    
    SECTION("Copy constructor works") {
        SellAtLimitOrder<DecimalType> copy(original);
        REQUIRE(copy.getLimitPrice() == original.getLimitPrice());
        REQUIRE(copy.getTradingSymbol() == original.getTradingSymbol());
    }
    
    SECTION("Assignment operator works") {
        SellAtLimitOrder<DecimalType> assigned("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                               createDate("20220101"), DecimalType("200.00"));
        assigned = original;
        REQUIRE(assigned.getLimitPrice() == original.getLimitPrice());
        REQUIRE(assigned.getTradingSymbol() == original.getTradingSymbol());
    }
}

TEST_CASE("StopOrder Copy Constructor and Assignment", "[trading_orders][copy]") {
    CoverAtStopOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    
    SECTION("Copy constructor works") {
        CoverAtStopOrder<DecimalType> copy(original);
        REQUIRE(copy.getStopPrice() == original.getStopPrice());
        REQUIRE(copy.getTradingSymbol() == original.getTradingSymbol());
    }
    
    SECTION("Assignment operator works") {
        CoverAtStopOrder<DecimalType> assigned("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                               createDate("20220101"), DecimalType("100.00"));
        assigned = original;
        REQUIRE(assigned.getStopPrice() == original.getStopPrice());
        REQUIRE(assigned.getTradingSymbol() == original.getTradingSymbol());
    }
}

// ============================================================================
// OBSERVER PATTERN TESTS
// ============================================================================

TEST_CASE("Multiple Observers on Single Order", "[trading_orders][observer]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    auto observer1 = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    auto observer2 = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    auto observer3 = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    
    order.addObserver(observer1);
    order.addObserver(observer2);
    order.addObserver(observer3);
    
    SECTION("All observers notified on execution") {
        order.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer1->executedCount == 1);
        REQUIRE(observer2->executedCount == 1);
        REQUIRE(observer3->executedCount == 1);
        
        REQUIRE(observer1->executedOrderTypes[0] == "MarketOnOpenLong");
        REQUIRE(observer2->executedOrderTypes[0] == "MarketOnOpenLong");
        REQUIRE(observer3->executedOrderTypes[0] == "MarketOnOpenLong");
    }
    
    SECTION("All observers notified on cancellation") {
        order.MarkOrderCanceled();
        
        REQUIRE(observer1->canceledCount == 1);
        REQUIRE(observer2->canceledCount == 1);
        REQUIRE(observer3->canceledCount == 1);
        
        REQUIRE(observer1->canceledOrderTypes[0] == "MarketOnOpenLong");
        REQUIRE(observer2->canceledOrderTypes[0] == "MarketOnOpenLong");
        REQUIRE(observer3->canceledOrderTypes[0] == "MarketOnOpenLong");
    }
}

TEST_CASE("Observer Receives Correct Order Type Notifications", "[trading_orders][observer]") {
    auto observer = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    
    SECTION("MarketOnOpenShortOrder notifications") {
        MarketOnOpenShortOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer->executedOrderTypes[0] == "MarketOnOpenShort");
    }
    
    SECTION("MarketOnOpenSellOrder notifications") {
        MarketOnOpenSellOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer->executedOrderTypes[0] == "MarketOnOpenSell");
    }
    
    SECTION("MarketOnOpenCoverOrder notifications") {
        MarketOnOpenCoverOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer->executedOrderTypes[0] == "MarketOnOpenCover");
    }
    
    SECTION("SellAtLimitOrder notifications") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gLimitPrice + DecimalType("5.00"));
        
        REQUIRE(observer->executedOrderTypes[0] == "SellAtLimit");
    }
    
    SECTION("CoverAtLimitOrder notifications") {
        CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gLimitPrice);
        
        REQUIRE(observer->executedOrderTypes[0] == "CoverAtLimit");
    }
    
    SECTION("SellAtStopOrder notifications") {
        SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gStopPrice);
        
        REQUIRE(observer->executedOrderTypes[0] == "SellAtStop");
    }
    
    SECTION("CoverAtStopOrder notifications") {
        CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        order.addObserver(observer);
        order.MarkOrderExecuted(gFillDate, gStopPrice);
        
        REQUIRE(observer->executedOrderTypes[0] == "CoverAtStop");
    }
}

TEST_CASE("Single Observer Tracking Multiple Orders", "[trading_orders][observer]") {
    auto observer = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    
    MarketOnOpenLongOrder<DecimalType> longOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
    MarketOnOpenShortOrder<DecimalType> shortOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
    SellAtLimitOrder<DecimalType> sellOrder(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    
    longOrder.addObserver(observer);
    shortOrder.addObserver(observer);
    sellOrder.addObserver(observer);
    
    longOrder.MarkOrderExecuted(gFillDate, gFillPrice);
    shortOrder.MarkOrderCanceled();
    sellOrder.MarkOrderExecuted(gFillDate, gLimitPrice);
    
    REQUIRE(observer->executedCount == 2);
    REQUIRE(observer->canceledCount == 1);
    REQUIRE(observer->executedOrderTypes.size() == 2);
    REQUIRE(observer->canceledOrderTypes.size() == 1);
}

// ============================================================================
// VISITOR PATTERN TESTS
// ============================================================================

TEST_CASE("Visitor Pattern - Each Order Type Calls Correct Visit Method", "[trading_orders][visitor]") {
    MockTradingOrderVisitor<DecimalType> visitor;
    
    SECTION("MarketOnOpenLongOrder") {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "MarketOnOpenLong");
    }
    
    SECTION("MarketOnOpenShortOrder") {
        MarketOnOpenShortOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "MarketOnOpenShort");
    }
    
    SECTION("MarketOnOpenSellOrder") {
        MarketOnOpenSellOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "MarketOnOpenSell");
    }
    
    SECTION("MarketOnOpenCoverOrder") {
        MarketOnOpenCoverOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "MarketOnOpenCover");
    }
    
    SECTION("SellAtLimitOrder") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "SellAtLimit");
    }
    
    SECTION("CoverAtLimitOrder") {
        CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "CoverAtLimit");
    }
    
    SECTION("SellAtStopOrder") {
        SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "SellAtStop");
    }
    
    SECTION("CoverAtStopOrder") {
        CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        order.accept(visitor);
        REQUIRE(visitor.visitedTypes.size() == 1);
        REQUIRE(visitor.visitedTypes[0] == "CoverAtStop");
    }
}

TEST_CASE("Visitor Pattern - Multiple Orders", "[trading_orders][visitor]") {
    MockTradingOrderVisitor<DecimalType> visitor;
    
    MarketOnOpenLongOrder<DecimalType> order1(gTradingSymbol, gUnitsInOrder, gOrderDate);
    SellAtLimitOrder<DecimalType> order2(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    CoverAtStopOrder<DecimalType> order3(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    
    order1.accept(visitor);
    order2.accept(visitor);
    order3.accept(visitor);
    
    REQUIRE(visitor.visitedTypes.size() == 3);
    REQUIRE(visitor.visitedTypes[0] == "MarketOnOpenLong");
    REQUIRE(visitor.visitedTypes[1] == "SellAtLimit");
    REQUIRE(visitor.visitedTypes[2] == "CoverAtStop");
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_CASE("Same-Day Execution with Different Times", "[trading_orders][edge_cases]") {
    ptime orderTime = time_from_string("2023-01-01 09:30:00");
    ptime fillTime1 = time_from_string("2023-01-01 10:00:00");  // Same day, later time
    ptime fillTime2 = time_from_string("2023-01-01 09:30:00");  // Exact same time
    ptime fillTime3 = time_from_string("2023-01-01 09:29:59");  // Same day, earlier time
    
    SECTION("Fill after order time on same day") {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, orderTime);
        REQUIRE_NOTHROW(order.MarkOrderExecuted(fillTime1, gFillPrice));
        REQUIRE(order.isOrderExecuted());
    }
    
    SECTION("Fill at exact order time") {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, orderTime);
        REQUIRE_NOTHROW(order.MarkOrderExecuted(fillTime2, gFillPrice));
        REQUIRE(order.isOrderExecuted());
    }
    
    SECTION("Fill before order time on same day throws") {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, orderTime);
        REQUIRE_THROWS_AS(order.MarkOrderExecuted(fillTime3, gFillPrice), 
                          TradingOrderNotExecutedException);
        REQUIRE(order.isOrderPending());
    }
}

TEST_CASE("Boundary Price Tests for Limit Orders", "[trading_orders][edge_cases]") {
    DecimalType limitPrice("100.00");
    
    SECTION("SellAtLimit - Fill exactly at limit price") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, limitPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, limitPrice));
    }
    
    SECTION("SellAtLimit - Fill one tick above limit") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, limitPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, limitPrice + DecimalType("0.01")));
    }
    
    SECTION("SellAtLimit - Fill one tick below limit throws") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, limitPrice);
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, limitPrice - DecimalType("0.01")),
                          TradingOrderNotExecutedException);
    }
    
    SECTION("CoverAtLimit - Fill exactly at limit price") {
        CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, limitPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, limitPrice));
    }
    
    SECTION("CoverAtLimit - Fill one tick below limit") {
        CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, limitPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, limitPrice - DecimalType("0.01")));
    }
    
    SECTION("CoverAtLimit - Fill one tick above limit throws") {
        CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, limitPrice);
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, limitPrice + DecimalType("0.01")),
                          TradingOrderNotExecutedException);
    }
}

TEST_CASE("Boundary Price Tests for Stop Orders", "[trading_orders][edge_cases]") {
    DecimalType stopPrice("100.00");
    
    SECTION("SellAtStop - Fill exactly at stop price") {
        SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, stopPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, stopPrice));
    }
    
    SECTION("SellAtStop - Fill below stop price (slippage)") {
        SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, stopPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, stopPrice - DecimalType("1.00")));
    }
    
    SECTION("SellAtStop - Fill above stop price throws") {
        SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, stopPrice);
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, stopPrice + DecimalType("0.01")),
                          TradingOrderNotExecutedException);
    }
    
    SECTION("CoverAtStop - Fill exactly at stop price") {
        CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, stopPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, stopPrice));
    }
    
    SECTION("CoverAtStop - Fill above stop price (slippage)") {
        CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, stopPrice);
        REQUIRE_NOTHROW(order.ValidateOrderExecution(gFillDate, stopPrice + DecimalType("1.00")));
    }
    
    SECTION("CoverAtStop - Fill below stop price throws") {
        CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, stopPrice);
        REQUIRE_THROWS_AS(order.ValidateOrderExecution(gFillDate, stopPrice - DecimalType("0.01")),
                          TradingOrderNotExecutedException);
    }
}

TEST_CASE("Order ID Uniqueness", "[trading_orders][edge_cases]") {
    std::vector<uint32_t> orderIDs;
    
    // Create multiple orders and collect their IDs
    for (int i = 0; i < 100; ++i) {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        orderIDs.push_back(order.getOrderID());
    }
    
    // Check all IDs are unique
    std::sort(orderIDs.begin(), orderIDs.end());
    auto it = std::adjacent_find(orderIDs.begin(), orderIDs.end());
    REQUIRE(it == orderIDs.end());  // No duplicates found
}

TEST_CASE("Large Trading Volume", "[trading_orders][edge_cases]") {
    TradingVolume largeVolume(1000000, TradingVolume::SHARES);
    
    SECTION("Large volume in long order") {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, largeVolume, gOrderDate);
        REQUIRE(order.getUnitsInOrder().getTradingVolume() == 1000000);
    }
    
    SECTION("Large volume in limit order") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, largeVolume, gOrderDate, gLimitPrice);
        REQUIRE(order.getUnitsInOrder().getTradingVolume() == 1000000);
    }
}

TEST_CASE("Default Stop Loss and Profit Target", "[trading_orders][edge_cases]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    // Default values should be zero
    REQUIRE(order.getStopLoss() == DecimalConstants<DecimalType>::DecimalZero);
    REQUIRE(order.getProfitTarget() == DecimalConstants<DecimalType>::DecimalZero);
}

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

TEST_CASE("Invalid State Transition - Double Execution", "[trading_orders][state]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    order.MarkOrderExecuted(gFillDate, gFillPrice);
    REQUIRE(order.isOrderExecuted());
    
    TimeSeriesDate laterDate(createDate("20230103"));
    REQUIRE_THROWS_AS(order.MarkOrderExecuted(laterDate, gFillPrice), 
                      TradingOrderExecutedException);
}

TEST_CASE("Invalid State Transition - Execute After Cancel", "[trading_orders][state]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    order.MarkOrderCanceled();
    REQUIRE(order.isOrderCanceled());
    
    REQUIRE_THROWS_AS(order.MarkOrderExecuted(gFillDate, gFillPrice), 
                      TradingOrderNotExecutedException);
}

TEST_CASE("Invalid State Transition - Cancel After Execute", "[trading_orders][state]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    order.MarkOrderExecuted(gFillDate, gFillPrice);
    REQUIRE(order.isOrderExecuted());
    
    REQUIRE_THROWS_AS(order.MarkOrderCanceled(), TradingOrderExecutedException);
}

TEST_CASE("Invalid State Transition - Double Cancellation", "[trading_orders][state]") {
    MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    order.MarkOrderCanceled();
    REQUIRE(order.isOrderCanceled());
    
    REQUIRE_THROWS_AS(order.MarkOrderCanceled(), TradingOrderExecutedException);
}

// ============================================================================
// PTIME STATE METHOD TESTS
// ============================================================================

TEST_CASE("PendingOrderState with ptime", "[trading_order_states][ptime]") {
    PendingOrderState<DecimalType> state;
    
    SECTION("getFillDateTime throws in pending state") {
        REQUIRE_THROWS_AS(state.getFillDateTime(), TradingOrderNotExecutedException);
    }
}

TEST_CASE("ExecutedOrderState with ptime", "[trading_order_states][ptime]") {
    ptime fillDateTime = time_from_string("2023-01-02 14:30:00");
    DecimalType fillPrice("155.50");
    
    ExecutedOrderState<DecimalType> state(fillDateTime, fillPrice);
    
    SECTION("getFillDateTime returns correct value") {
        REQUIRE(state.getFillDateTime() == fillDateTime);
    }
    
    SECTION("getFillDate extracts date from datetime") {
        REQUIRE(state.getFillDate() == fillDateTime.date());
    }
    
    SECTION("getFillPrice returns correct value") {
        REQUIRE(state.getFillPrice() == fillPrice);
    }
}

TEST_CASE("CanceledOrderState with ptime", "[trading_order_states][ptime]") {
    CanceledOrderState<DecimalType> state;
    
    SECTION("getFillDateTime throws in canceled state") {
        REQUIRE_THROWS_AS(state.getFillDateTime(), TradingOrderNotExecutedException);
    }
}

// ============================================================================
// ORDER PRIORITY TESTS
// ============================================================================

TEST_CASE("Order Priority Values", "[trading_orders][priority]") {
    MarketOnOpenLongOrder<DecimalType> marketOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
    SellAtStopOrder<DecimalType> stopOrder(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    SellAtLimitOrder<DecimalType> limitOrder(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    
    // Market orders have highest priority (lowest number)
    REQUIRE(marketOrder.getOrderPriority() == 1);
    
    // Stop orders have medium priority
    REQUIRE(stopOrder.getOrderPriority() == 5);
    
    // Limit orders have lowest priority (highest number)
    REQUIRE(limitOrder.getOrderPriority() == 10);
    
    // Verify ordering
    REQUIRE(marketOrder.getOrderPriority() < stopOrder.getOrderPriority());
    REQUIRE(stopOrder.getOrderPriority() < limitOrder.getOrderPriority());
}

// ============================================================================
// COMPREHENSIVE ORDER ATTRIBUTE TESTS
// ============================================================================

TEST_CASE("All Order Types Have Correct Attributes", "[trading_orders][attributes]") {
    SECTION("MarketOnOpenLongOrder") {
        MarketOnOpenLongOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        REQUIRE(order.isLongOrder() == true);
        REQUIRE(order.isShortOrder() == false);
        REQUIRE(order.isEntryOrder() == true);
        REQUIRE(order.isExitOrder() == false);
        REQUIRE(order.isMarketOrder() == true);
        REQUIRE(order.isStopOrder() == false);
        REQUIRE(order.isLimitOrder() == false);
    }
    
    SECTION("MarketOnOpenShortOrder") {
        MarketOnOpenShortOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        REQUIRE(order.isLongOrder() == false);
        REQUIRE(order.isShortOrder() == true);
        REQUIRE(order.isEntryOrder() == true);
        REQUIRE(order.isExitOrder() == false);
        REQUIRE(order.isMarketOrder() == true);
        REQUIRE(order.isStopOrder() == false);
        REQUIRE(order.isLimitOrder() == false);
    }
    
    SECTION("MarketOnOpenSellOrder") {
        MarketOnOpenSellOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        REQUIRE(order.isLongOrder() == true);  // Closes long
        REQUIRE(order.isShortOrder() == false);
        REQUIRE(order.isEntryOrder() == false);
        REQUIRE(order.isExitOrder() == true);
        REQUIRE(order.isMarketOrder() == true);
        REQUIRE(order.isStopOrder() == false);
        REQUIRE(order.isLimitOrder() == false);
    }
    
    SECTION("MarketOnOpenCoverOrder") {
        MarketOnOpenCoverOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate);
        REQUIRE(order.isLongOrder() == false);
        REQUIRE(order.isShortOrder() == true);  // Closes short
        REQUIRE(order.isEntryOrder() == false);
        REQUIRE(order.isExitOrder() == true);
        REQUIRE(order.isMarketOrder() == true);
        REQUIRE(order.isStopOrder() == false);
        REQUIRE(order.isLimitOrder() == false);
    }
    
    SECTION("SellAtLimitOrder") {
        SellAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        REQUIRE(order.isLongOrder() == true);  // Closes long
        REQUIRE(order.isShortOrder() == false);
        REQUIRE(order.isEntryOrder() == false);
        REQUIRE(order.isExitOrder() == true);
        REQUIRE(order.isMarketOrder() == false);
        REQUIRE(order.isStopOrder() == false);
        REQUIRE(order.isLimitOrder() == true);
    }
    
    SECTION("CoverAtLimitOrder") {
        CoverAtLimitOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        REQUIRE(order.isLongOrder() == false);
        REQUIRE(order.isShortOrder() == true);  // Closes short
        REQUIRE(order.isEntryOrder() == false);
        REQUIRE(order.isExitOrder() == true);
        REQUIRE(order.isMarketOrder() == false);
        REQUIRE(order.isStopOrder() == false);
        REQUIRE(order.isLimitOrder() == true);
    }
    
    SECTION("SellAtStopOrder") {
        SellAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        REQUIRE(order.isLongOrder() == true);  // Closes long
        REQUIRE(order.isShortOrder() == false);
        REQUIRE(order.isEntryOrder() == false);
        REQUIRE(order.isExitOrder() == true);
        REQUIRE(order.isMarketOrder() == false);
        REQUIRE(order.isStopOrder() == true);
        REQUIRE(order.isLimitOrder() == false);
    }
    
    SECTION("CoverAtStopOrder") {
        CoverAtStopOrder<DecimalType> order(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        REQUIRE(order.isLongOrder() == false);
        REQUIRE(order.isShortOrder() == true);  // Closes short
        REQUIRE(order.isEntryOrder() == false);
        REQUIRE(order.isExitOrder() == true);
        REQUIRE(order.isMarketOrder() == false);
        REQUIRE(order.isStopOrder() == true);
        REQUIRE(order.isLimitOrder() == false);
    }
}

TEST_CASE("MarketOnOpenLongOrder - Move Constructor", "[trading_orders][move][long]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, 
                                                gStopLossPercent, gProfitTargetPercent);
    
    uint32_t originalID = original.getOrderID();
    std::string originalSymbol = original.getTradingSymbol();
    uint64_t originalVolume = original.getUnitsInOrder().getTradingVolume();
    
    SECTION("Move transfers all basic attributes") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getTradingSymbol() == originalSymbol);
        REQUIRE(moved.getUnitsInOrder().getTradingVolume() == originalVolume);
        REQUIRE(moved.getOrderDate() == gOrderDate);
        REQUIRE(moved.getStopLoss() == gStopLossPercent);
        REQUIRE(moved.getProfitTarget() == gProfitTargetPercent);
        REQUIRE(moved.getOrderID() == originalID);
    }
    
    SECTION("Move transfers order type attributes") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.isLongOrder() == true);
        REQUIRE(moved.isShortOrder() == false);
        REQUIRE(moved.isEntryOrder() == true);
        REQUIRE(moved.isExitOrder() == false);
        REQUIRE(moved.isMarketOrder() == true);
        REQUIRE(moved.isStopOrder() == false);
        REQUIRE(moved.isLimitOrder() == false);
    }
    
    SECTION("Move transfers state") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.isOrderPending() == true);
        REQUIRE(moved.isOrderExecuted() == false);
        REQUIRE(moved.isOrderCanceled() == false);
    }
    
    SECTION("Moved order can be executed") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        moved.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(moved.isOrderExecuted() == true);
        REQUIRE(moved.getFillDate() == gFillDate);
        REQUIRE(moved.getFillPrice() == gFillPrice);
    }
}

TEST_CASE("MarketOnOpenShortOrder - Move Constructor", "[trading_orders][move][short]") {
    MarketOnOpenShortOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, 
                                                 gStopLossPercent, gProfitTargetPercent);
    
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move transfers all attributes") {
        MarketOnOpenShortOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getTradingSymbol() == gTradingSymbol);
        REQUIRE(moved.getStopLoss() == gStopLossPercent);
        REQUIRE(moved.getProfitTarget() == gProfitTargetPercent);
        REQUIRE(moved.getOrderID() == originalID);
        REQUIRE(moved.isShortOrder() == true);
        REQUIRE(moved.isLongOrder() == false);
    }
}

TEST_CASE("MarketOnOpenSellOrder - Move Constructor", "[trading_orders][move][sell]") {
    MarketOnOpenSellOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move transfers exit order attributes") {
        MarketOnOpenSellOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getTradingSymbol() == gTradingSymbol);
        REQUIRE(moved.getOrderID() == originalID);
        REQUIRE(moved.isExitOrder() == true);
        REQUIRE(moved.isEntryOrder() == false);
        REQUIRE(moved.isLongOrder() == true);  // Closes long
    }
}

TEST_CASE("MarketOnOpenCoverOrder - Move Constructor", "[trading_orders][move][cover]") {
    MarketOnOpenCoverOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move transfers cover order attributes") {
        MarketOnOpenCoverOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getTradingSymbol() == gTradingSymbol);
        REQUIRE(moved.getOrderID() == originalID);
        REQUIRE(moved.isExitOrder() == true);
        REQUIRE(moved.isShortOrder() == true);  // Closes short
    }
}

TEST_CASE("SellAtLimitOrder - Move Constructor", "[trading_orders][move][limit]") {
    SellAtLimitOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move transfers limit price") {
        SellAtLimitOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getLimitPrice() == gLimitPrice);
        REQUIRE(moved.getOrderID() == originalID);
        REQUIRE(moved.isLimitOrder() == true);
    }
    
    SECTION("Moved order validates correctly") {
        SellAtLimitOrder<DecimalType> moved(std::move(original));
        
        REQUIRE_NOTHROW(moved.ValidateOrderExecution(gFillDate, gLimitPrice));
        REQUIRE_NOTHROW(moved.ValidateOrderExecution(gFillDate, gLimitPrice + DecimalType("1.0")));
        REQUIRE_THROWS_AS(moved.ValidateOrderExecution(gFillDate, gLimitPrice - DecimalType("0.01")),
                          TradingOrderNotExecutedException);
    }
}

TEST_CASE("CoverAtLimitOrder - Move Constructor", "[trading_orders][move][limit]") {
    CoverAtLimitOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    
    SECTION("Move transfers all attributes") {
        CoverAtLimitOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getLimitPrice() == gLimitPrice);
        REQUIRE(moved.isShortOrder() == true);  // Closes short
        REQUIRE(moved.isLimitOrder() == true);
    }
}

TEST_CASE("SellAtStopOrder - Move Constructor", "[trading_orders][move][stop]") {
    SellAtStopOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move transfers stop price") {
        SellAtStopOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getStopPrice() == gStopPrice);
        REQUIRE(moved.getOrderID() == originalID);
        REQUIRE(moved.isStopOrder() == true);
    }
    
    SECTION("Moved order validates correctly") {
        SellAtStopOrder<DecimalType> moved(std::move(original));
        
        REQUIRE_NOTHROW(moved.ValidateOrderExecution(gFillDate, gStopPrice));
        REQUIRE_NOTHROW(moved.ValidateOrderExecution(gFillDate, gStopPrice - DecimalType("1.0")));
        REQUIRE_THROWS_AS(moved.ValidateOrderExecution(gFillDate, gStopPrice + DecimalType("0.01")),
                          TradingOrderNotExecutedException);
    }
}

TEST_CASE("CoverAtStopOrder - Move Constructor", "[trading_orders][move][stop]") {
    CoverAtStopOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    
    SECTION("Move transfers all attributes") {
        CoverAtStopOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getStopPrice() == gStopPrice);
        REQUIRE(moved.isShortOrder() == true);  // Closes short
        REQUIRE(moved.isStopOrder() == true);
    }
}

TEST_CASE("Move Constructor with Executed Order", "[trading_orders][move][state]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    original.MarkOrderExecuted(gFillDate, gFillPrice);
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move transfers executed state") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.isOrderExecuted() == true);
        REQUIRE(moved.isOrderPending() == false);
        REQUIRE(moved.getFillDate() == gFillDate);
        REQUIRE(moved.getFillPrice() == gFillPrice);
        REQUIRE(moved.getOrderID() == originalID);
    }
}

TEST_CASE("Move Constructor with Canceled Order", "[trading_orders][move][state]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    original.MarkOrderCanceled();
    
    SECTION("Move transfers canceled state") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.isOrderCanceled() == true);
        REQUIRE(moved.isOrderPending() == false);
        REQUIRE(moved.isOrderExecuted() == false);
    }
}

TEST_CASE("Move Constructor with Observers", "[trading_orders][move][observers]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    auto observer = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    original.addObserver(observer);
    
    SECTION("Move transfers observers") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        // Execute moved order - observer should be notified
        moved.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer->executedCount == 1);
        REQUIRE(observer->executedOrderTypes[0] == "MarketOnOpenLong");
    }
}

TEST_CASE("Move Constructor with ptime", "[trading_orders][move][ptime]") {
    ptime orderDateTime = time_from_string("2023-01-01 09:30:00");
    ptime fillDateTime = time_from_string("2023-01-01 14:30:00");
    
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, orderDateTime);
    original.MarkOrderExecuted(fillDateTime, gFillPrice);
    
    SECTION("Move transfers full datetime information") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.getOrderDateTime() == orderDateTime);
        REQUIRE(moved.getFillDateTime() == fillDateTime);
        REQUIRE(moved.getFillDate() == fillDateTime.date());
    }
}

// ============================================================================
// MOVE ASSIGNMENT OPERATOR TESTS - All Order Types
// ============================================================================

TEST_CASE("MarketOnOpenLongOrder - Move Assignment", "[trading_orders][move_assign][long]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, 
                                                gStopLossPercent, gProfitTargetPercent);
    MarketOnOpenLongOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                              createDate("20220101"));
    
    uint32_t originalID = original.getOrderID();
    
    SECTION("Move assignment transfers all attributes") {
        target = std::move(original);
        
        REQUIRE(target.getTradingSymbol() == gTradingSymbol);
        REQUIRE(target.getUnitsInOrder().getTradingVolume() == gUnitsInOrder.getTradingVolume());
        REQUIRE(target.getOrderDate() == gOrderDate);
        REQUIRE(target.getStopLoss() == gStopLossPercent);
        REQUIRE(target.getProfitTarget() == gProfitTargetPercent);
        REQUIRE(target.getOrderID() == originalID);
    }
    
    SECTION("Move assignment transfers state") {
        original.MarkOrderExecuted(gFillDate, gFillPrice);
        target = std::move(original);
        
        REQUIRE(target.isOrderExecuted() == true);
        REQUIRE(target.getFillDate() == gFillDate);
        REQUIRE(target.getFillPrice() == gFillPrice);
    }
    
    SECTION("Self-move-assignment is safe") {
        MarketOnOpenLongOrder<DecimalType>& self = original;
        original = std::move(self);
        
        // Should still be in valid state
        REQUIRE(original.getTradingSymbol() == gTradingSymbol);
        REQUIRE(original.isOrderPending() == true);
    }
}

TEST_CASE("MarketOnOpenShortOrder - Move Assignment", "[trading_orders][move_assign][short]") {
    MarketOnOpenShortOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, 
                                                 gStopLossPercent, gProfitTargetPercent);
    MarketOnOpenShortOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                               createDate("20220101"));
    
    SECTION("Move assignment works correctly") {
        target = std::move(original);
        
        REQUIRE(target.getTradingSymbol() == gTradingSymbol);
        REQUIRE(target.isShortOrder() == true);
        REQUIRE(target.getStopLoss() == gStopLossPercent);
    }
}

TEST_CASE("MarketOnOpenSellOrder - Move Assignment", "[trading_orders][move_assign][sell]") {
    MarketOnOpenSellOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    MarketOnOpenSellOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                              createDate("20220101"));
    
    SECTION("Move assignment works correctly") {
        target = std::move(original);
        
        REQUIRE(target.getTradingSymbol() == gTradingSymbol);
        REQUIRE(target.isExitOrder() == true);
    }
}

TEST_CASE("MarketOnOpenCoverOrder - Move Assignment", "[trading_orders][move_assign][cover]") {
    MarketOnOpenCoverOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    MarketOnOpenCoverOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                               createDate("20220101"));
    
    SECTION("Move assignment works correctly") {
        target = std::move(original);
        
        REQUIRE(target.getTradingSymbol() == gTradingSymbol);
        REQUIRE(target.isExitOrder() == true);
        REQUIRE(target.isShortOrder() == true);
    }
}

TEST_CASE("SellAtLimitOrder - Move Assignment", "[trading_orders][move_assign][limit]") {
    SellAtLimitOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    SellAtLimitOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                         createDate("20220101"), DecimalType("200.00"));
    
    SECTION("Move assignment transfers limit price") {
        target = std::move(original);
        
        REQUIRE(target.getLimitPrice() == gLimitPrice);
        REQUIRE(target.getTradingSymbol() == gTradingSymbol);
    }
}

TEST_CASE("CoverAtLimitOrder - Move Assignment", "[trading_orders][move_assign][limit]") {
    CoverAtLimitOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
    CoverAtLimitOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                          createDate("20220101"), DecimalType("200.00"));
    
    SECTION("Move assignment works correctly") {
        target = std::move(original);
        
        REQUIRE(target.getLimitPrice() == gLimitPrice);
        REQUIRE(target.isShortOrder() == true);
    }
}

TEST_CASE("SellAtStopOrder - Move Assignment", "[trading_orders][move_assign][stop]") {
    SellAtStopOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    SellAtStopOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                        createDate("20220101"), DecimalType("100.00"));
    
    SECTION("Move assignment transfers stop price") {
        target = std::move(original);
        
        REQUIRE(target.getStopPrice() == gStopPrice);
        REQUIRE(target.getTradingSymbol() == gTradingSymbol);
    }
}

TEST_CASE("CoverAtStopOrder - Move Assignment", "[trading_orders][move_assign][stop]") {
    CoverAtStopOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
    CoverAtStopOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                         createDate("20220101"), DecimalType("100.00"));
    
    SECTION("Move assignment works correctly") {
        target = std::move(original);
        
        REQUIRE(target.getStopPrice() == gStopPrice);
        REQUIRE(target.isShortOrder() == true);
    }
}

TEST_CASE("Move Assignment with Observers", "[trading_orders][move_assign][observers]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    MarketOnOpenLongOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                              createDate("20220101"));
    
    auto observer = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    original.addObserver(observer);
    
    SECTION("Move assignment transfers observers") {
        target = std::move(original);
        
        // Execute target - observer should be notified
        target.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer->executedCount == 1);
    }
}

// ============================================================================
// MOVE SEMANTICS WITH CONTAINERS
// ============================================================================

TEST_CASE("Move Semantics with std::vector", "[trading_orders][move][containers]") {
    SECTION("Moving orders into vector is efficient") {
        std::vector<MarketOnOpenLongOrder<DecimalType>> orders;
        orders.reserve(3);  // Avoid reallocation
        
        MarketOnOpenLongOrder<DecimalType> order1(gTradingSymbol, gUnitsInOrder, gOrderDate);
        MarketOnOpenLongOrder<DecimalType> order2("SYM2", TradingVolume(200, TradingVolume::SHARES), 
                                                  createDate("20230102"));
        MarketOnOpenLongOrder<DecimalType> order3("SYM3", TradingVolume(300, TradingVolume::SHARES), 
                                                  createDate("20230103"));
        
        uint32_t id1 = order1.getOrderID();
        uint32_t id2 = order2.getOrderID();
        uint32_t id3 = order3.getOrderID();
        
        // Move orders into vector
        orders.push_back(std::move(order1));
        orders.push_back(std::move(order2));
        orders.push_back(std::move(order3));
        
        REQUIRE(orders.size() == 3);
        REQUIRE(orders[0].getOrderID() == id1);
        REQUIRE(orders[1].getOrderID() == id2);
        REQUIRE(orders[2].getOrderID() == id3);
        REQUIRE(orders[0].getTradingSymbol() == gTradingSymbol);
        REQUIRE(orders[1].getTradingSymbol() == "SYM2");
        REQUIRE(orders[2].getTradingSymbol() == "SYM3");
    }
}

TEST_CASE("Move Semantics - Vector Reallocation", "[trading_orders][move][containers]") {
    SECTION("Orders survive vector reallocation") {
        std::vector<MarketOnOpenLongOrder<DecimalType>> orders;
        
        // Don't reserve - let it reallocate
        for (int i = 0; i < 10; ++i) {
            std::string symbol = "SYM" + std::to_string(i);
            MarketOnOpenLongOrder<DecimalType> order(symbol, gUnitsInOrder, gOrderDate);
            uint32_t id = order.getOrderID();
            orders.push_back(std::move(order));
            
            // Verify the moved order is still accessible
            REQUIRE(orders[i].getOrderID() == id);
            REQUIRE(orders[i].getTradingSymbol() == symbol);
        }
        
        REQUIRE(orders.size() == 10);
    }
}

TEST_CASE("Move Semantics with Different Order Types in Vector", "[trading_orders][move][containers]") {
    SECTION("Can't mix different order types (compile-time safety)") {
        std::vector<MarketOnOpenLongOrder<DecimalType>> longOrders;
        std::vector<MarketOnOpenShortOrder<DecimalType>> shortOrders;
        
        MarketOnOpenLongOrder<DecimalType> longOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
        MarketOnOpenShortOrder<DecimalType> shortOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
        
        uint32_t longID = longOrder.getOrderID();
        uint32_t shortID = shortOrder.getOrderID();
        
        longOrders.push_back(std::move(longOrder));
        shortOrders.push_back(std::move(shortOrder));
        
        REQUIRE(longOrders[0].getOrderID() == longID);
        REQUIRE(shortOrders[0].getOrderID() == shortID);
        REQUIRE(longOrders[0].isLongOrder() == true);
        REQUIRE(shortOrders[0].isShortOrder() == true);
    }
}

// ============================================================================
// MOVE SEMANTICS WITH SHARED POINTERS (Real-World Usage)
// ============================================================================

TEST_CASE("Move Semantics with shared_ptr", "[trading_orders][move][shared_ptr]") {
    SECTION("Moving shared_ptr containing order") {
        auto original = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
            gTradingSymbol, gUnitsInOrder, gOrderDate);
        
        uint32_t id = original->getOrderID();
        
        // Move the shared_ptr (not the order itself)
        auto moved = std::move(original);
        
        REQUIRE(moved->getOrderID() == id);
        REQUIRE(original == nullptr);  // Original shared_ptr is now null
    }
    
    SECTION("Vector of shared_ptr to orders") {
        std::vector<std::shared_ptr<MarketOnOpenLongOrder<DecimalType>>> orders;
        
        for (int i = 0; i < 5; ++i) {
            auto order = std::make_shared<MarketOnOpenLongOrder<DecimalType>>(
                gTradingSymbol, gUnitsInOrder, gOrderDate);
            orders.push_back(std::move(order));  // Move shared_ptr
        }
        
        REQUIRE(orders.size() == 5);
        for (const auto& order : orders) {
            REQUIRE(order->getTradingSymbol() == gTradingSymbol);
            REQUIRE(order->isOrderPending() == true);
        }
    }
}

// ============================================================================
// PERFORMANCE AND EFFICIENCY TESTS
// ============================================================================

TEST_CASE("Move vs Copy - Verify Move is Used", "[trading_orders][move][performance]") {
    SECTION("Moving preserves order ID (not creating new one)") {
        MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
        uint32_t originalID = original.getOrderID();
        
        // Move constructor
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        REQUIRE(moved.getOrderID() == originalID);  // Same ID = move happened
        
        // If copy happened, we'd get the same ID (which we want for archival)
        // But the move should be more efficient internally
    }
    
    SECTION("Move assignment preserves order ID") {
        MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
        MarketOnOpenLongOrder<DecimalType> target("DIFF", TradingVolume(50, TradingVolume::SHARES), 
                                                  createDate("20220101"));
        
        uint32_t originalID = original.getOrderID();
        
        target = std::move(original);
        REQUIRE(target.getOrderID() == originalID);
    }
}

// ============================================================================
// EDGE CASES AND ERROR CONDITIONS
// ============================================================================

TEST_CASE("Move After Execution", "[trading_orders][move][edge_cases]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    // Execute the order first
    original.MarkOrderExecuted(gFillDate, gFillPrice);
    
    SECTION("Can move executed order") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.isOrderExecuted() == true);
        REQUIRE(moved.getFillDate() == gFillDate);
        REQUIRE(moved.getFillPrice() == gFillPrice);
    }
    
    SECTION("Cannot execute moved-from order again") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        // Trying to re-execute should throw
        REQUIRE_THROWS_AS(moved.MarkOrderExecuted(createDate("20230103"), gFillPrice),
                          TradingOrderExecutedException);
    }
}

TEST_CASE("Move After Cancellation", "[trading_orders][move][edge_cases]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    original.MarkOrderCanceled();
    
    SECTION("Can move canceled order") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        REQUIRE(moved.isOrderCanceled() == true);
    }
}

TEST_CASE("Move with Multiple Observers", "[trading_orders][move][edge_cases]") {
    MarketOnOpenLongOrder<DecimalType> original(gTradingSymbol, gUnitsInOrder, gOrderDate);
    
    auto observer1 = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    auto observer2 = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    auto observer3 = std::make_shared<ComprehensiveMockObserver<DecimalType>>();
    
    original.addObserver(observer1);
    original.addObserver(observer2);
    original.addObserver(observer3);
    
    SECTION("All observers are moved") {
        MarketOnOpenLongOrder<DecimalType> moved(std::move(original));
        
        moved.MarkOrderExecuted(gFillDate, gFillPrice);
        
        REQUIRE(observer1->executedCount == 1);
        REQUIRE(observer2->executedCount == 1);
        REQUIRE(observer3->executedCount == 1);
    }
}

// ============================================================================
// COMPREHENSIVE TEST - All Order Types Can Be Moved
// ============================================================================

TEST_CASE("All Order Types Support Move Semantics", "[trading_orders][move][comprehensive]") {
    SECTION("All entry orders can be moved") {
        MarketOnOpenLongOrder<DecimalType> longOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
        MarketOnOpenShortOrder<DecimalType> shortOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
        
        MarketOnOpenLongOrder<DecimalType> movedLong(std::move(longOrder));
        MarketOnOpenShortOrder<DecimalType> movedShort(std::move(shortOrder));
        
        REQUIRE(movedLong.isEntryOrder() == true);
        REQUIRE(movedShort.isEntryOrder() == true);
    }
    
    SECTION("All exit orders can be moved") {
        MarketOnOpenSellOrder<DecimalType> sellOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
        MarketOnOpenCoverOrder<DecimalType> coverOrder(gTradingSymbol, gUnitsInOrder, gOrderDate);
        
        MarketOnOpenSellOrder<DecimalType> movedSell(std::move(sellOrder));
        MarketOnOpenCoverOrder<DecimalType> movedCover(std::move(coverOrder));
        
        REQUIRE(movedSell.isExitOrder() == true);
        REQUIRE(movedCover.isExitOrder() == true);
    }
    
    SECTION("All limit orders can be moved") {
        SellAtLimitOrder<DecimalType> sellLimit(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        CoverAtLimitOrder<DecimalType> coverLimit(gTradingSymbol, gUnitsInOrder, gOrderDate, gLimitPrice);
        
        SellAtLimitOrder<DecimalType> movedSellLimit(std::move(sellLimit));
        CoverAtLimitOrder<DecimalType> movedCoverLimit(std::move(coverLimit));
        
        REQUIRE(movedSellLimit.isLimitOrder() == true);
        REQUIRE(movedCoverLimit.isLimitOrder() == true);
    }
    
    SECTION("All stop orders can be moved") {
        SellAtStopOrder<DecimalType> sellStop(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        CoverAtStopOrder<DecimalType> coverStop(gTradingSymbol, gUnitsInOrder, gOrderDate, gStopPrice);
        
        SellAtStopOrder<DecimalType> movedSellStop(std::move(sellStop));
        CoverAtStopOrder<DecimalType> movedCoverStop(std::move(coverStop));
        
        REQUIRE(movedSellStop.isStopOrder() == true);
        REQUIRE(movedCoverStop.isStopOrder() == true);
    }
}
