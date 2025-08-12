#pragma once

#include "DataStructures.h"
#include "PalAst.h"
#include <memory>

namespace palanalyzer {

/**
 * @brief Extracts pattern structure information from PAL AST nodes
 * 
 * This class traverses the AST representation of PAL patterns to extract:
 * - Bar combinations and offsets
 * - Component types used in patterns
 * - Pattern chaining analysis
 * - Bar spread calculations
 */
class PatternStructureExtractor {
public:
    PatternStructureExtractor();
    ~PatternStructureExtractor();
    
    /**
     * @brief Extract complete pattern analysis from a PriceActionLabPattern
     * @param pattern The PAL pattern to analyze
     * @param sourceFile The source file path for tracking
     * @return Complete pattern analysis data
     */
    PatternAnalysis extractPatternAnalysis(
        std::shared_ptr<PriceActionLabPattern> pattern,
        const std::string& sourceFile);
    
    /**
     * @brief Extract bar combination info from pattern components
     * @param components Vector of price component descriptors
     * @return Bar combination information
     */
    BarCombinationInfo extractBarCombinationInfo(
        const std::vector<PriceComponentDescriptor>& components,
        const std::string& searchType);
    
    /**
     * @brief Determine search type from filename
     * @param filename The PAL file name
     * @return Search type enum (Extended, Deep, Close, etc.)
     */
    SearchType determineSearchType(const std::string& filename);
    
    /**
     * @brief Determine search type with explicit override
     * @param filename The PAL file name (for inference fallback)
     * @param explicitSearchType Explicit search type string (empty means use inference)
     * @return Search type enum
     */
    SearchType determineSearchType(const std::string& filename, const std::string& explicitSearchType);
    
private:
    /**
     * @brief Extract components from pattern expression AST
     * @param expr The pattern expression to traverse
     * @param components Output vector for components
     */
    void extractComponentsFromExpression(
        std::shared_ptr<PatternExpression> expr,
        std::vector<PriceComponentDescriptor>& components);
    
    /**
     * @brief Extract component from price bar reference
     * @param priceRef The price bar reference
     * @return Price component descriptor
     */
    PriceComponentDescriptor extractComponentFromPriceRef(
        std::shared_ptr<PriceBarReference> priceRef);
    
    /**
     * @brief Convert AST price bar reference to component type
     * @param priceRef The price bar reference
     * @return Component type enum
     */
    PriceComponentType getComponentType(
        std::shared_ptr<PriceBarReference> priceRef);
    
    /**
     * @brief Analyze if pattern shows chaining behavior
     * @param components Vector of components to analyze
     * @return True if pattern shows transitive chaining
     */
    bool analyzeChaining(const std::vector<PriceComponentDescriptor>& components);
    
    /**
     * @brief Calculate bar spread (max - min bar offset)
     * @param components Vector of components
     * @return Bar spread value
     */
    uint8_t calculateBarSpread(const std::vector<PriceComponentDescriptor>& components);
    
    /**
     * @brief Get maximum bar offset from components
     * @param components Vector of components
     * @return Maximum bar offset
     */
    uint8_t getMaxBarOffset(const std::vector<PriceComponentDescriptor>& components);
    
    /**
     * @brief Generate human-readable pattern string
     * @param components Vector of components
     * @return Pattern description string
     */
    std::string generatePatternString(const std::vector<PriceComponentDescriptor>& components);
    
    /**
     * @brief Count the number of conditions (AND clauses) in a pattern expression
     * @param expr The pattern expression to analyze
     * @return Number of conditions in the pattern
     */
    uint8_t countConditions(std::shared_ptr<PatternExpression> expr);
};

} // namespace palanalyzer