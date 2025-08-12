#include "PatternStructureExtractor.h"
#include "PalAst.h"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace palanalyzer {

PatternStructureExtractor::PatternStructureExtractor() {
}

PatternStructureExtractor::~PatternStructureExtractor() {
}

PatternAnalysis PatternStructureExtractor::extractPatternAnalysis(
    std::shared_ptr<PriceActionLabPattern> pattern,
    const std::string& sourceFile) {
    
    uint32_t index = 0;
    double profitabilityLong = 0.0, profitabilityShort = 0.0;
    uint32_t trades = 0, consecutiveLosses = 0;
    
    auto description = pattern->getPatternDescription();
    if (description) {
        index = description->getpatternIndex();
        profitabilityLong = description->getPercentLong()->getAsDouble();
        profitabilityShort = description->getPercentShort()->getAsDouble();
        trades = description->numTrades();
        consecutiveLosses = description->numConsecutiveLosses();
    }
    
    std::vector<PriceComponentDescriptor> components;
    uint8_t conditionCount = 0;
    unsigned long long patternHash = 0;
    
    auto patternExpr = pattern->getPatternExpression();
    if (patternExpr) {
        extractComponentsFromExpression(patternExpr, components);
        conditionCount = countConditions(patternExpr);
        patternHash = patternExpr->hashCode();
    }
    
    bool isChained = analyzeChaining(components);
    uint8_t maxBarOffset = getMaxBarOffset(components);
    uint8_t barSpread = calculateBarSpread(components);
    std::string patternString = generatePatternString(components);
    
    return PatternAnalysis(index, sourceFile, patternHash, components, patternString,
                           isChained, maxBarOffset, barSpread, conditionCount,
                           std::chrono::system_clock::now(), profitabilityLong,
                           profitabilityShort, trades, consecutiveLosses);
}

BarCombinationInfo PatternStructureExtractor::extractBarCombinationInfo(
    const std::vector<PriceComponentDescriptor>& components,
    const std::string& searchType) {
    
    std::set<uint8_t> uniqueOffsets;
    std::set<PriceComponentType> componentTypes;
    for (const auto& comp : components) {
        uniqueOffsets.insert(comp.getBarOffset());
        componentTypes.insert(comp.getType());
    }
    
    std::vector<uint8_t> barOffsets(uniqueOffsets.begin(), uniqueOffsets.end());
    uint8_t patternLength = static_cast<uint8_t>(barOffsets.size());
    
    return BarCombinationInfo(barOffsets, componentTypes, 1, searchType, patternLength, patternLength,
                              std::chrono::system_clock::now(), std::chrono::system_clock::now(), {});
}

SearchType PatternStructureExtractor::determineSearchType(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("extended") != std::string::npos) return SearchType::EXTENDED;
    if (lower.find("deep") != std::string::npos) return SearchType::DEEP;
    if (lower.find("close") != std::string::npos) return SearchType::CLOSE;
    if (lower.find("high-low") != std::string::npos || lower.find("highlow") != std::string::npos) return SearchType::HIGH_LOW;
    if (lower.find("open-close") != std::string::npos || lower.find("openclose") != std::string::npos) return SearchType::OPEN_CLOSE;
    if (lower.find("basic") != std::string::npos) return SearchType::BASIC;
    if (lower.find("mixed") != std::string::npos) return SearchType::MIXED;
    
    return SearchType::UNKNOWN;
}

SearchType PatternStructureExtractor::determineSearchType(const std::string& filename, const std::string& explicitSearchType) {
    if (!explicitSearchType.empty()) {
        // Use explicit search type if provided
        return stringToSearchType(explicitSearchType);
    }
    
    // Fall back to filename inference
    return determineSearchType(filename);
}

void PatternStructureExtractor::extractComponentsFromExpression(
    std::shared_ptr<PatternExpression> expr,
    std::vector<PriceComponentDescriptor>& components) {
    
    if (!expr) return;
    
    // Handle AndExpr - recursive traversal
    auto andExpr = std::dynamic_pointer_cast<AndExpr>(expr);
    if (andExpr) {
        extractComponentsFromExpression(andExpr->getLHSShared(), components);
        extractComponentsFromExpression(andExpr->getRHSShared(), components);
        return;
    }
    
    // Handle GreaterThanExpr - extract price references
    auto gtExpr = std::dynamic_pointer_cast<GreaterThanExpr>(expr);
    if (gtExpr) {
        auto lhsRef = gtExpr->getLHSShared();
        auto rhsRef = gtExpr->getRHSShared();
        
        if (lhsRef) {
            PriceComponentDescriptor comp = extractComponentFromPriceRef(lhsRef);
            components.push_back(comp);
        }
        
        if (rhsRef) {
            PriceComponentDescriptor comp = extractComponentFromPriceRef(rhsRef);
            components.push_back(comp);
        }
    }
}

PriceComponentDescriptor PatternStructureExtractor::extractComponentFromPriceRef(
    std::shared_ptr<PriceBarReference> priceRef) {
    
    PriceComponentType type = getComponentType(priceRef);
    uint8_t barOffset = priceRef->getBarOffset();
    
    // Generate description
    std::ostringstream oss;
    oss << componentTypeToString(type) << " of "
        << static_cast<int>(barOffset) << " bars ago";
    
    return PriceComponentDescriptor(type, barOffset, oss.str());
}

PriceComponentType PatternStructureExtractor::getComponentType(
    std::shared_ptr<PriceBarReference> priceRef) {
    
    // Use dynamic casting to determine the specific price reference type
    if (std::dynamic_pointer_cast<PriceBarOpen>(priceRef)) {
        return PriceComponentType::OPEN;
    }
    if (std::dynamic_pointer_cast<PriceBarHigh>(priceRef)) {
        return PriceComponentType::HIGH;
    }
    if (std::dynamic_pointer_cast<PriceBarLow>(priceRef)) {
        return PriceComponentType::LOW;
    }
    if (std::dynamic_pointer_cast<PriceBarClose>(priceRef)) {
        return PriceComponentType::CLOSE;
    }
    if (std::dynamic_pointer_cast<VolumeBarReference>(priceRef)) {
        return PriceComponentType::VOLUME;
    }
    if (std::dynamic_pointer_cast<Roc1BarReference>(priceRef)) {
        return PriceComponentType::ROC1;
    }
    if (std::dynamic_pointer_cast<IBS1BarReference>(priceRef)) {
        return PriceComponentType::IBS1;
    }
    if (std::dynamic_pointer_cast<IBS2BarReference>(priceRef)) {
        return PriceComponentType::IBS2;
    }
    if (std::dynamic_pointer_cast<IBS3BarReference>(priceRef)) {
        return PriceComponentType::IBS3;
    }
    if (std::dynamic_pointer_cast<MeanderBarReference>(priceRef)) {
        return PriceComponentType::MEANDER;
    }
    if (std::dynamic_pointer_cast<VChartLowBarReference>(priceRef)) {
        return PriceComponentType::VCHARTLOW;
    }
    if (std::dynamic_pointer_cast<VChartHighBarReference>(priceRef)) {
        return PriceComponentType::VCHARTHIGH;
    }
    
    // Default fallback
    return PriceComponentType::CLOSE;
}

bool PatternStructureExtractor::analyzeChaining(const std::vector<PriceComponentDescriptor>& components) {
    if (components.size() < 3) return false;
    
    // Sort components by bar offset
    std::vector<PriceComponentDescriptor> sorted = components;
    std::sort(sorted.begin(), sorted.end());
    
    // Check for sequential bar offsets (indicating potential chaining)
    int consecutiveCount = 1;
    for (size_t i = 1; i < sorted.size(); ++i) {
        if (sorted[i].getBarOffset() == sorted[i-1].getBarOffset() + 1) {
            consecutiveCount++;
        } else {
            consecutiveCount = 1;
        }
        
        // If we have 3+ consecutive bars, likely chained
        if (consecutiveCount >= 3) {
            return true;
        }
    }
    
    return false;
}

uint8_t PatternStructureExtractor::calculateBarSpread(const std::vector<PriceComponentDescriptor>& components) {
    if (components.empty()) return 0;
    
    uint8_t minOffset = components[0].getBarOffset();
    uint8_t maxOffset = components[0].getBarOffset();
    
    for (const auto& comp : components) {
        minOffset = std::min(minOffset, comp.getBarOffset());
        maxOffset = std::max(maxOffset, comp.getBarOffset());
    }
    
    return maxOffset - minOffset;
}

uint8_t PatternStructureExtractor::getMaxBarOffset(const std::vector<PriceComponentDescriptor>& components) {
    if (components.empty()) return 0;
    
    uint8_t maxOffset = 0;
    for (const auto& comp : components) {
        maxOffset = std::max(maxOffset, comp.getBarOffset());
    }
    
    return maxOffset;
}

std::string PatternStructureExtractor::generatePatternString(const std::vector<PriceComponentDescriptor>& components) {
    if (components.empty()) return "Empty pattern";
    
    std::ostringstream oss;
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) oss << " AND ";
        oss << components[i].getDescription();
    }
    
    return oss.str();
}

uint8_t PatternStructureExtractor::countConditions(std::shared_ptr<PatternExpression> expr) {
    static int debugCallCount = 0;
    if (debugCallCount < 10) {
        std::cerr << "DEBUG countConditions called, expr is " << (expr ? "not null" : "null") << std::endl;
        debugCallCount++;
    }
    
    if (!expr) return 0;
    
    // Handle AndExpr - recursive count of both sides
    if (auto andExpr = std::dynamic_pointer_cast<AndExpr>(expr)) {
        if (debugCallCount < 10) {
            std::cerr << "DEBUG: Found AndExpr" << std::endl;
        }
        return countConditions(andExpr->getLHSShared()) + countConditions(andExpr->getRHSShared());
    }
    
    // Handle GreaterThanExpr - this is one condition
    if (auto gtExpr = std::dynamic_pointer_cast<GreaterThanExpr>(expr)) {
        if (debugCallCount < 10) {
            std::cerr << "DEBUG: Found GreaterThanExpr - returning 1" << std::endl;
        }
        return 1;
    }
    
    // Handle other expression types if needed
    if (debugCallCount < 10) {
        std::cerr << "DEBUG: Unknown expression type - returning 1" << std::endl;
    }
    return 1;
}

} // namespace palanalyzer