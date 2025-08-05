// PricePatternFactory.h
#ifndef PRICE_PATTERN_FACTORY_H
#define PRICE_PATTERN_FACTORY_H

#include <memory>
#include <stdexcept>
#include <string>

#include "SearchConfiguration.h"
#include "PatternTemplate.h"
#include "AstResourceManager.h"
#include "PalAst.h"
#include "BackTester.h"

using namespace mkc_timeseries;

class PricePatternFactoryException : public std::runtime_error
{
public:
    explicit PricePatternFactoryException(const std::string& msg)
        : std::runtime_error(msg) {}
};

/**
 * @brief Factory class for creating Price Action Lab patterns from templates.
 * 
 * This class encapsulates the logic for creating PAL patterns, including
 * long and short patterns, pattern expressions, and final patterns with
 * performance metrics. It uses dependency injection to receive an
 * AstResourceManager reference, maintaining thread safety and testability.
 */
template <class DecimalType>
class PricePatternFactory
{
public:
    /**
     * @brief Constructs a PricePatternFactory with dependency injection.
     * @param astResourceManager Reference to the AstResourceManager for AST creation.
     */
    explicit PricePatternFactory(mkc_palast::AstResourceManager& astResourceManager)
        : mAstResourceManager(astResourceManager)
    {
    }

    /**
     * @brief Creates a long PAL pattern from a pattern expression.
     * @param patternExpression The pattern expression to use.
     * @param config The search configuration containing profit target and stop loss.
     * @param patternName The base name for the pattern (will have "_Long" appended).
     * @return Shared pointer to the created PAL pattern.
     */
    PALPatternPtr createLongPalPattern(std::shared_ptr<PatternExpression> patternExpression,
                                       const SearchConfiguration<DecimalType>& config,
                                       const std::string& patternName) const;

    /**
     * @brief Creates a short PAL pattern from a pattern expression.
     * @param patternExpression The pattern expression to use.
     * @param config The search configuration containing profit target and stop loss.
     * @param patternName The base name for the pattern (will have "_Short" appended).
     * @return Shared pointer to the created PAL pattern.
     */
    PALPatternPtr createShortPalPattern(std::shared_ptr<PatternExpression> patternExpression,
                                        const SearchConfiguration<DecimalType>& config,
                                        const std::string& patternName) const;

    /**
     * @brief Creates a pattern expression from a pattern template.
     * @param patternTemplate The template containing the pattern conditions.
     * @return Shared pointer to the created pattern expression.
     * @throws PricePatternFactoryException if the template contains no conditions.
     */
    std::shared_ptr<PatternExpression> createPatternExpressionFromTemplate(
        const PatternTemplate& patternTemplate) const;

    /**
     * @brief Creates a final pattern with performance metrics from backtesting results.
     * @param partialPattern The partial pattern to enhance with performance data.
     * @param backTester The backtester containing performance results.
     * @return Shared pointer to the final pattern with performance metrics.
     */
    PALPatternPtr createFinalPattern(PALPatternPtr partialPattern,
                                     const mkc_timeseries::BackTester<DecimalType>& backTester) const;

private:
    /**
     * @brief Creates a price bar reference from a price component descriptor.
     * @param descriptor The descriptor specifying the price component and bar offset.
     * @return Shared pointer to the created price bar reference.
     * @throws PricePatternFactoryException if the component type is unknown.
     */
    std::shared_ptr<PriceBarReference> createPriceBarReference(
        const PriceComponentDescriptor& descriptor) const;

private:
    mkc_palast::AstResourceManager& mAstResourceManager;
};

// Template implementation
template <class DecimalType>
PALPatternPtr PricePatternFactory<DecimalType>::createLongPalPattern(
    std::shared_ptr<PatternExpression> patternExpression,
    const SearchConfiguration<DecimalType>& config,
    const std::string& patternName) const
{
    // Append direction to the name to ensure uniqueness
    std::string longPatternName = patternName + "_Long";

    auto marketEntry = mAstResourceManager.getLongMarketEntryOnOpen();
    auto patternDesc = std::make_shared<PatternDescription>(
        longPatternName.c_str(), 0, 0,
        mAstResourceManager.getDecimalNumber(0), mAstResourceManager.getDecimalNumber(0), 0, 0);

    auto profitTarget = mAstResourceManager.getDecimalNumber(num::toString(config.getProfitTarget()));
    auto profitTargetExpr = mAstResourceManager.getLongProfitTarget(profitTarget);

    auto stopLoss = mAstResourceManager.getDecimalNumber(num::toString(config.getStopLoss()));
    auto stopLossExpr = mAstResourceManager.getLongStopLoss(stopLoss);

    return mAstResourceManager.createPattern(patternDesc,
                                             patternExpression,
                                             marketEntry,
                                             profitTargetExpr,
                                             stopLossExpr);
}

template <class DecimalType>
PALPatternPtr PricePatternFactory<DecimalType>::createShortPalPattern(
    std::shared_ptr<PatternExpression> patternExpression,
    const SearchConfiguration<DecimalType>& config,
    const std::string& patternName) const
{
    // Append direction to the name to ensure uniqueness
    std::string shortPatternName = patternName + "_Short";

    auto patternDesc = std::make_shared<PatternDescription>(
        shortPatternName.c_str(), 0, 0,
        mAstResourceManager.getDecimalNumber(0), mAstResourceManager.getDecimalNumber(0), 0, 0);

    auto profitTarget = mAstResourceManager.getDecimalNumber(num::toString(config.getProfitTarget()));
    auto marketEntry = mAstResourceManager.getShortMarketEntryOnOpen();
    auto profitTargetExpr = mAstResourceManager.getShortProfitTarget(profitTarget);

    auto stopLoss = mAstResourceManager.getDecimalNumber(num::toString(config.getStopLoss()));
    auto stopLossExpr = mAstResourceManager.getShortStopLoss(stopLoss);

    return mAstResourceManager.createPattern(patternDesc,
                                             patternExpression,
                                             marketEntry,
                                             profitTargetExpr,
                                             stopLossExpr);
}

template <class DecimalType>
std::shared_ptr<PatternExpression> PricePatternFactory<DecimalType>::createPatternExpressionFromTemplate(
    const PatternTemplate& patternTemplate) const
{
    const auto& conditions = patternTemplate.getConditions();
    if (conditions.empty())
        throw PricePatternFactoryException("Cannot create pattern expression from empty template: " + patternTemplate.getName());

    std::shared_ptr<PatternExpression> root = nullptr;

    for (const auto& cond : conditions)
    {
        auto lhs = createPriceBarReference(cond.getLhs());
        auto rhs = createPriceBarReference(cond.getRhs());
        auto currentExpr = std::make_shared<GreaterThanExpr>(lhs, rhs);
        
        if (root == nullptr)
            root = currentExpr;
        else
            root = std::make_shared<AndExpr>(root, currentExpr);
    }
    return root;
}

template <class DecimalType>
std::shared_ptr<PriceBarReference> PricePatternFactory<DecimalType>::createPriceBarReference(
    const PriceComponentDescriptor& descriptor) const
{
    switch (descriptor.getComponentType())
    {
    case PriceComponentType::Open:
        return mAstResourceManager.getPriceOpen(descriptor.getBarOffset());

    case PriceComponentType::High:
        return mAstResourceManager.getPriceHigh(descriptor.getBarOffset());

    case PriceComponentType::Low:
        return mAstResourceManager.getPriceLow(descriptor.getBarOffset());

    case PriceComponentType::Close:
        return mAstResourceManager.getPriceClose(descriptor.getBarOffset());

    default:
        throw PricePatternFactoryException("Unknown PriceComponentType.");
    }
}

template <class DecimalType>
PALPatternPtr PricePatternFactory<DecimalType>::createFinalPattern(
    PALPatternPtr partialPattern,
    const mkc_timeseries::BackTester<DecimalType>& backTester) const
{
    auto initialDesc = partialPattern->getPatternDescription();
    auto finalDesc = std::make_shared<PatternDescription>(initialDesc->getFileName().c_str(),
                                                          initialDesc->getpatternIndex(),
                                                          initialDesc->getIndexDate(),
        mAstResourceManager.getDecimalNumber(num::toString(std::get<1>(backTester.getProfitability()))),
        mAstResourceManager.getDecimalNumber(0),
        backTester.getClosedPositionHistory().getNumPositions(), backTester.getNumConsecutiveLosses());
     
    return mAstResourceManager.createPattern(finalDesc,
                                             partialPattern->getPatternExpression(),
                                             partialPattern->getMarketEntry(),
                                             partialPattern->getProfitTarget(),
                                             partialPattern->getStopLoss());
}

#endif // PRICE_PATTERN_FACTORY_H