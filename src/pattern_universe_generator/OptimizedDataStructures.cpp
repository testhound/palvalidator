#include "OptimizedDataStructures.h"
#include <stdexcept>

namespace pattern_universe {

std::string searchTypeToString(SearchType type) {
    switch (type) {
        case SearchType::EXTENDED:        return "EXTENDED";
        case SearchType::DEEP:            return "DEEP";
        case SearchType::CLOSE_ONLY:      return "CLOSE_ONLY";
        case SearchType::MIXED:           return "MIXED";
        case SearchType::HIGH_LOW_ONLY:   return "HIGH_LOW_ONLY";
        case SearchType::OPEN_CLOSE_ONLY: return "OPEN_CLOSE_ONLY";
        default:                          return "Unknown";
    }
}

SearchType stringToSearchType(const std::string& str) {
    if (str == "EXTENDED" || str == "Extended") return SearchType::EXTENDED;
    if (str == "DEEP" || str == "Deep") return SearchType::DEEP;
    if (str == "CLOSE_ONLY" || str == "Close_Only") return SearchType::CLOSE_ONLY;
    if (str == "MIXED" || str == "Mixed") return SearchType::MIXED;
    if (str == "HIGH_LOW_ONLY" || str == "High_Low_Only") return SearchType::HIGH_LOW_ONLY;
    if (str == "OPEN_CLOSE_ONLY" || str == "Open_Close_Only") return SearchType::OPEN_CLOSE_ONLY;
    
    throw std::invalid_argument("Unknown search type: " + str);
}

std::string componentTypeToString(PriceComponentType type) {
    switch (type) {
        case PriceComponentType::OPEN:       return "OPEN";
        case PriceComponentType::HIGH:       return "HIGH";
        case PriceComponentType::LOW:        return "LOW";
        case PriceComponentType::CLOSE:      return "CLOSE";
        case PriceComponentType::VOLUME:     return "VOLUME";
        case PriceComponentType::ROC1:       return "ROC1";
        case PriceComponentType::IBS1:       return "IBS1";
        case PriceComponentType::IBS2:       return "IBS2";
        case PriceComponentType::IBS3:       return "IBS3";
        case PriceComponentType::MEANDER:    return "MEANDER";
        case PriceComponentType::VCHARTLOW:  return "VCHARTLOW";
        case PriceComponentType::VCHARTHIGH: return "VCHARTHIGH";
        default:                             return "UNKNOWN";
    }
}

PriceComponentType stringToComponentType(const std::string& str) {
    if (str == "OPEN") return PriceComponentType::OPEN;
    if (str == "HIGH") return PriceComponentType::HIGH;
    if (str == "LOW") return PriceComponentType::LOW;
    if (str == "CLOSE") return PriceComponentType::CLOSE;
    if (str == "VOLUME") return PriceComponentType::VOLUME;
    if (str == "ROC1") return PriceComponentType::ROC1;
    if (str == "IBS1") return PriceComponentType::IBS1;
    if (str == "IBS2") return PriceComponentType::IBS2;
    if (str == "IBS3") return PriceComponentType::IBS3;
    if (str == "MEANDER") return PriceComponentType::MEANDER;
    if (str == "VCHARTLOW") return PriceComponentType::VCHARTLOW;
    if (str == "VCHARTHIGH") return PriceComponentType::VCHARTHIGH;
    
    throw std::invalid_argument("Unknown component type: " + str);
}

std::string componentTierToString(ComponentTier tier) {
    switch (tier) {
        case ComponentTier::FullOHLC:    return "FullOHLC";
        case ComponentTier::Mixed:       return "Mixed";
        case ComponentTier::Dual:        return "Dual";
        case ComponentTier::Single:      return "Single";
        case ComponentTier::Unknown:
        default:                         return "Unknown";
    }
}

ComponentTier stringToComponentTier(const std::string& str) {
    if (str == "FullOHLC") return ComponentTier::FullOHLC;
    if (str == "Mixed") return ComponentTier::Mixed;
    if (str == "Dual") return ComponentTier::Dual;
    if (str == "Single") return ComponentTier::Single;
    if (str == "Unknown") return ComponentTier::Unknown;
    
    throw std::invalid_argument("Unknown component tier: " + str);
}

std::string componentComplexityToString(ComponentComplexity complexity) {
    switch (complexity) {
        case ComponentComplexity::Simple:    return "Simple";
        case ComponentComplexity::Moderate:  return "Moderate";
        case ComponentComplexity::Complex:   return "Complex";
        case ComponentComplexity::Full:      return "Full";
        default:                             return "Unknown";
    }
}

ComponentComplexity stringToComponentComplexity(const std::string& str) {
    if (str == "Simple") return ComponentComplexity::Simple;
    if (str == "Moderate") return ComponentComplexity::Moderate;
    if (str == "Complex") return ComponentComplexity::Complex;
    if (str == "Full") return ComponentComplexity::Full;
    
    throw std::invalid_argument("Unknown component complexity: " + str);
}

} // namespace pattern_universe