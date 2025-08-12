#include "DataStructures.h"
#include <sstream>

namespace palanalyzer {

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

std::string componentTypeToString(PriceComponentType type) {
    switch (type) {
        case PriceComponentType::OPEN: return "OPEN";
        case PriceComponentType::HIGH: return "HIGH";
        case PriceComponentType::LOW: return "LOW";
        case PriceComponentType::CLOSE: return "CLOSE";
        case PriceComponentType::VOLUME: return "VOLUME";
        case PriceComponentType::ROC1: return "ROC1";
        case PriceComponentType::IBS1: return "IBS1";
        case PriceComponentType::IBS2: return "IBS2";
        case PriceComponentType::IBS3: return "IBS3";
        case PriceComponentType::MEANDER: return "MEANDER";
        case PriceComponentType::VCHARTLOW: return "VCHARTLOW";
        case PriceComponentType::VCHARTHIGH: return "VCHARTHIGH";
        default: return "UNKNOWN";
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
    return PriceComponentType::CLOSE; // Default fallback
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

} // namespace palanalyzer