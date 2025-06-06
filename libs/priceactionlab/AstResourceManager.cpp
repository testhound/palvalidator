#include "AstResourceManager.h"

namespace mkc_palast {

std::shared_ptr<PriceActionLabPattern> AstResourceManager::createPattern(
    std::shared_ptr<PatternDescription> description,
    std::shared_ptr<PatternExpression> pattern,
    std::shared_ptr<MarketEntryExpression> entry,
    std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
    std::shared_ptr<StopLossInPercentExpression> stopLoss,
    PriceActionLabPattern::VolatilityAttribute volatilityAttr,
    PriceActionLabPattern::PortfolioAttribute portfolioAttr)
{
    // No need for no-op deleters - all parameters are already shared_ptr
    return std::make_shared<PriceActionLabPattern>(
        description, pattern, entry, profitTarget, stopLoss, volatilityAttr, portfolioAttr);
}

// PriceBarReference methods - return shared_ptr from factory
std::shared_ptr<PriceBarReference> AstResourceManager::getPriceOpen(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getPriceOpen(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getPriceHigh(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getPriceHigh(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getPriceLow(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getPriceLow(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getPriceClose(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getPriceClose(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getVolume(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getVolume(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getRoc1(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getRoc1(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getIBS1(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getIBS1(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getIBS2(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getIBS2(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getIBS3(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getIBS3(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getMeander(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getMeander(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getVChartLow(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getVChartLow(barOffset);
}

std::shared_ptr<PriceBarReference> AstResourceManager::getVChartHigh(unsigned int barOffset) {
    auto factory = getFactory();
    return factory->getVChartHigh(barOffset);
}

// MarketEntryExpression methods
std::shared_ptr<MarketEntryExpression> AstResourceManager::getLongMarketEntryOnOpen() {
    auto factory = getFactory();
    return factory->getLongMarketEntryOnOpen();
}

std::shared_ptr<MarketEntryExpression> AstResourceManager::getShortMarketEntryOnOpen() {
    auto factory = getFactory();
    return factory->getShortMarketEntryOnOpen();
}

// Decimal number methods
std::shared_ptr<decimal7> AstResourceManager::getDecimalNumber(const std::string& numString) {
    auto factory = getFactory();
    return factory->getDecimalNumber(const_cast<char*>(numString.c_str()));
}

std::shared_ptr<decimal7> AstResourceManager::getDecimalNumber(int num) {
    auto factory = getFactory();
    return factory->getDecimalNumber(num);
}

// Profit target methods
std::shared_ptr<ProfitTargetInPercentExpression> AstResourceManager::getLongProfitTarget(std::shared_ptr<decimal7> profitTarget) {
    auto factory = getFactory();
    auto longProfitTarget = factory->getLongProfitTarget(profitTarget);
    // Cast to base class
    return std::static_pointer_cast<ProfitTargetInPercentExpression>(longProfitTarget);
}

std::shared_ptr<ProfitTargetInPercentExpression> AstResourceManager::getShortProfitTarget(std::shared_ptr<decimal7> profitTarget) {
    auto factory = getFactory();
    auto shortProfitTarget = factory->getShortProfitTarget(profitTarget);
    // Cast to base class
    return std::static_pointer_cast<ProfitTargetInPercentExpression>(shortProfitTarget);
}

// Stop loss methods
std::shared_ptr<StopLossInPercentExpression> AstResourceManager::getLongStopLoss(std::shared_ptr<decimal7> stopLoss) {
    auto factory = getFactory();
    auto longStopLoss = factory->getLongStopLoss(stopLoss);
    // Cast to base class
    return std::static_pointer_cast<StopLossInPercentExpression>(longStopLoss);
}

std::shared_ptr<StopLossInPercentExpression> AstResourceManager::getShortStopLoss(std::shared_ptr<decimal7> stopLoss) {
    auto factory = getFactory();
    auto shortStopLoss = factory->getShortStopLoss(stopLoss);
    // Cast to base class
    return std::static_pointer_cast<StopLossInPercentExpression>(shortStopLoss);
}

} // namespace mkc_palast