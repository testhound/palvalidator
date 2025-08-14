#pragma once

#include "PriceComponentDescriptor.h"
#include "PatternCondition.h"
#include "SearchConfiguration.h"
#include <string>
#include <vector>

namespace patterndiscovery {

    // Component type utilities
    std::string componentTypeToString(PriceComponentType type);
    PriceComponentType stringToComponentType(const std::string& str);
    
    // Search type utilities  
    std::string searchTypeToString(SearchType type);
    SearchType stringToSearchType(const std::string& str);
    
    // Comparison operator utilities
    std::string comparisonOperatorToString(ComparisonOperator op);
    ComparisonOperator stringToComparisonOperator(const std::string& str);
    
    // Vector utilities
    std::string vectorToString(const std::vector<uint8_t>& vec);

} // namespace patterndiscovery