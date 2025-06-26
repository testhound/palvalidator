#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>
#include "PalAst.h"                 // For building pattern ASTs
#include "PALPatternClassifier.h"   // The class under test

// To see debug output from the classifier, uncomment the following line
// #define DEBUG_CLASSIFIER

using namespace mkc_timeseries;

// Helper function to create a pattern from a vector of expressions, reducing boilerplate.
PALPatternPtr createTestPattern(AstFactory& factory,
                                const std::vector<PatternExpressionPtr>& expressions,
                                bool is_long_pattern,
                                const std::string& profit_target_str,
                                const std::string& stop_loss_str) {

    if (expressions.empty()) {
        return nullptr;
    }

    // Chain all expressions together with AND operators
    PatternExpressionPtr final_expr = expressions[0];
    for (size_t i = 1; i < expressions.size(); ++i) {
        final_expr = std::make_shared<AndExpr>(final_expr, expressions[i]);
    }

    auto desc = std::make_shared<PatternDescription>("test.txt", 1, 20240101,
                                                     factory.getDecimalNumber(0), factory.getDecimalNumber(0),
                                                     0, 0);

    if (is_long_pattern) {
        auto entry = factory.getLongMarketEntryOnOpen();
        auto pt = factory.getLongProfitTarget(factory.getDecimalNumber(const_cast<char*>(profit_target_str.c_str())));
        auto sl = factory.getLongStopLoss(factory.getDecimalNumber(const_cast<char*>(stop_loss_str.c_str())));
        return std::make_shared<PriceActionLabPattern>(desc, final_expr, entry, pt, sl);
    } else {
        auto entry = factory.getShortMarketEntryOnOpen();
        auto pt = factory.getShortProfitTarget(factory.getDecimalNumber(const_cast<char*>(profit_target_str.c_str())));
        auto sl = factory.getShortStopLoss(factory.getDecimalNumber(const_cast<char*>(stop_loss_str.c_str())));
        return std::make_shared<PriceActionLabPattern>(desc, final_expr, entry, pt, sl);
    }
}


TEST_CASE("PALPatternClassifier operations", "[PALPatternClassifier]") {
    AstFactory factory;

    SECTION("Momentum Pullback (Long) Strategy Classification") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceOpen(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0), factory.getPriceClose(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(2), factory.getPriceOpen(3)),
            std::make_shared<GreaterThanExpr>(factory.getPriceOpen(3), factory.getPriceOpen(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceOpen(2), factory.getPriceClose(3))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, true, "1.2", "1.2");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::MOMENTUM);
        REQUIRE(result.sub_type == StrategySubType::PULLBACK);
    }

    SECTION("Mean-Reversion (Short) Strategy Classification") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(1), factory.getPriceClose(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceHigh(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(2), factory.getPriceLow(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(0), factory.getPriceLow(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(2), factory.getPriceLow(1))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, false, "0.7927805", "1.5855610");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::MEAN_REVERSION);
        REQUIRE(result.sub_type == StrategySubType::TREND_EXHAUSTION);
    }

    SECTION("Momentum Breakout (Long) Strategy Classification") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceHigh(10))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, true, "3.0", "1.5");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::MOMENTUM);
        REQUIRE(result.sub_type == StrategySubType::BREAKOUT);
    }

    SECTION("Trend-Following (Long) Strategy Classification") {
         std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceClose(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(1), factory.getPriceClose(2))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, true, "4.0", "2.0");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::TREND_FOLLOWING);
        REQUIRE(result.sub_type == StrategySubType::CONTINUATION);
    }

    SECTION("Mean-Reversion (Long) Strategy Classification") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(1), factory.getPriceClose(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(2), factory.getPriceClose(1))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, true, "1.0", "2.0");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::MEAN_REVERSION);
        REQUIRE(result.sub_type == StrategySubType::TREND_EXHAUSTION);
    }

    // --- NEW TEST CASES ---

    SECTION("New Pattern 1: Mean-Reversion Fade of Blow-off Top") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(0), factory.getPriceClose(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceClose(0), factory.getPriceLow(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(0), factory.getPriceHigh(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(1), factory.getPriceHigh(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(2), factory.getPriceLow(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(1), factory.getPriceHigh(3)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(3), factory.getPriceLow(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(2), factory.getPriceLow(3))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, true, "0.1232734", "0.2465467");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::MEAN_REVERSION);
    }

    SECTION("New Pattern 2: Complex Momentum Pullback") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(1), factory.getPriceHigh(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(0), factory.getPriceHigh(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(2), factory.getPriceLow(0)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(0), factory.getPriceLow(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(1), factory.getPriceHigh(3)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(3), factory.getPriceLow(2))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, true, "0.1232734", "0.2465467");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::MOMENTUM);
        REQUIRE(result.sub_type == StrategySubType::PULLBACK);
    }

    SECTION("New Pattern 3: Strong Trend-Following Continuation") {
        std::vector<PatternExpressionPtr> expressions = {
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(3), factory.getPriceHigh(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(2), factory.getPriceHigh(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceHigh(1), factory.getPriceLow(1)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(1), factory.getPriceLow(2)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(2), factory.getPriceLow(3)),
            std::make_shared<GreaterThanExpr>(factory.getPriceLow(3), factory.getPriceClose(0))
        };
        PALPatternPtr pattern = createTestPattern(factory, expressions, false, "0.1232734", "0.2465467");
        ClassificationResult result = PALPatternClassifier::classify(pattern);

        REQUIRE(result.primary_classification == StrategyCategory::TREND_FOLLOWING);
    }

    // --- END NEW TEST CASES ---

    SECTION("Edge Case Handling") {
        PALPatternPtr null_pattern = nullptr;
        ClassificationResult result_null = PALPatternClassifier::classify(null_pattern);
        REQUIRE(result_null.primary_classification == StrategyCategory::ERROR_TYPE);

        PALPatternPtr no_expr_pattern = createTestPattern(factory, {}, true, "1", "1");
        REQUIRE(no_expr_pattern == nullptr);
        ClassificationResult result_no_expr = PALPatternClassifier::classify(no_expr_pattern);
        REQUIRE(result_no_expr.primary_classification == StrategyCategory::ERROR_TYPE);

        std::vector<PatternExpressionPtr> ambiguous_list = {
             std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0), factory.getPriceLow(0))
        };
        PALPatternPtr unclassified_pattern = createTestPattern(factory, ambiguous_list, true, "1.1", "1.0");
        ClassificationResult result_unclassified = PALPatternClassifier::classify(unclassified_pattern);
        REQUIRE(result_unclassified.primary_classification == StrategyCategory::UNCLASSIFIED);
        REQUIRE(result_unclassified.sub_type == StrategySubType::AMBIGUOUS);
    }
}
