#include "PatternUtilities.h"
#include <sstream>
#include <algorithm>

namespace patterndiscovery {

std::string componentTypeToString(PriceComponentType type) {
    switch (type) {
        case PriceComponentType::Open: return "OPEN";
        case PriceComponentType::High: return "HIGH";
        case PriceComponentType::Low: return "LOW";
        case PriceComponentType::Close: return "CLOSE";
        case PriceComponentType::Volume: return "VOLUME";
        case PriceComponentType::Roc1: return "ROC1";
        case PriceComponentType::Ibs1: return "IBS1";
        case PriceComponentType::Ibs2: return "IBS2";
        case PriceComponentType::Ibs3: return "IBS3";
        case PriceComponentType::Meander: return "MEANDER";
        case PriceComponentType::VChartLow: return "VCHARTLOW";
        case PriceComponentType::VChartHigh: return "VCHARTHIGH";
        default: return "UNKNOWN";
    }
}

PriceComponentType stringToComponentType(const std::string& str) {
    if (str == "OPEN") return PriceComponentType::Open;
    if (str == "HIGH") return PriceComponentType::High;
    if (str == "LOW") return PriceComponentType::Low;
    if (str == "CLOSE") return PriceComponentType::Close;
    if (str == "VOLUME") return PriceComponentType::Volume;
    if (str == "ROC1") return PriceComponentType::Roc1;
    if (str == "IBS1") return PriceComponentType::Ibs1;
    if (str == "IBS2") return PriceComponentType::Ibs2;
    if (str == "IBS3") return PriceComponentType::Ibs3;
    if (str == "MEANDER") return PriceComponentType::Meander;
    if (str == "VCHARTLOW") return PriceComponentType::VChartLow;
    if (str == "VCHARTHIGH") return PriceComponentType::VChartHigh;
    return PriceComponentType::Close; // Default fallback
}

std::string searchTypeToString(SearchType type) {
    switch (type) {
        case SearchType::BASIC: return "Basic";
        case SearchType::EXTENDED: return "Extended";
        case SearchType::DEEP: return "Deep";
        case SearchType::CLOSE: return "Close";
        case SearchType::HIGH_LOW: return "High-Low";
        case SearchType::OPEN_CLOSE: return "Open-Close";
        case SearchType::MIXED: return "Mixed";
        case SearchType::UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

SearchType stringToSearchType(const std::string& str) {
    if (str == "Basic") return SearchType::BASIC;
    if (str == "Extended") return SearchType::EXTENDED;
    if (str == "Deep") return SearchType::DEEP;
    if (str == "Close") return SearchType::CLOSE;
    if (str == "High-Low") return SearchType::HIGH_LOW;
    if (str == "Open-Close") return SearchType::OPEN_CLOSE;
    if (str == "Mixed") return SearchType::MIXED;
    return SearchType::UNKNOWN;
}

std::string comparisonOperatorToString(ComparisonOperator op) {
    switch (op) {
        case ComparisonOperator::GreaterThan: return "GreaterThan";
        case ComparisonOperator::LessThan: return "LessThan";
        case ComparisonOperator::GreaterThanOrEqual: return "GreaterThanOrEqual";
        case ComparisonOperator::LessThanOrEqual: return "LessThanOrEqual";
        case ComparisonOperator::Equal: return "Equal";
        case ComparisonOperator::NotEqual: return "NotEqual";
        default: return "GreaterThan";
    }
}

ComparisonOperator stringToComparisonOperator(const std::string& str) {
    if (str == "GreaterThan" || str == ">") return ComparisonOperator::GreaterThan;
    if (str == "LessThan" || str == "<") return ComparisonOperator::LessThan;
    if (str == "GreaterThanOrEqual" || str == ">=") return ComparisonOperator::GreaterThanOrEqual;
    if (str == "LessThanOrEqual" || str == "<=") return ComparisonOperator::LessThanOrEqual;
    if (str == "Equal" || str == "==") return ComparisonOperator::Equal;
    if (str == "NotEqual" || str == "!=") return ComparisonOperator::NotEqual;
    return ComparisonOperator::GreaterThan; // Default fallback
}

std::string vectorToString(const std::vector<uint8_t>& vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << static_cast<int>(vec[i]);
    }
    oss << "]";
    return oss.str();
}

} // namespace patterndiscovery