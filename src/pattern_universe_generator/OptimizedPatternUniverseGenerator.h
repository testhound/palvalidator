#pragma once

#include "OptimizedDataStructures.h"
#include "PALAnalysisLoader.h"
#include "CuratedGroupManager.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <future>
#include <chrono>

namespace pattern_universe {

/**
 * @brief High-performance pattern universe generator based on reverse-engineered PAL algorithm
 * 
 * This class implements an optimized pattern generation system that leverages comprehensive
 * analysis of PAL's algorithm to achieve >24x speedup (sub-hour vs PAL's 24+ hours).
 * 
 * Key optimizations based on PAL analysis:
 * - Curated group system with component specialization hierarchy
 * - Component usage optimization (CLOSE dominance: 37.7%, HIGH/LOW balance, OPEN specialization: 13.4%)
 * - Pattern chaining optimization (20.06% of patterns use chaining)
 * - Parallel generation with optimal batch sizing
 * - Pre-computation of high-yield groups (>500 patterns)
 */
class OptimizedPatternUniverseGenerator {
public:
    /**
     * @brief Configuration for pattern generation
     */
    class GenerationConfig
    {
    public:
        /**
         * @brief Construct a new Generation Config object
         * @param targetSearchType Target search type for pattern generation
         * @param enableParallelProcessing Enable parallel processing
         * @param maxThreads Maximum number of threads (0 = auto-detect)
         * @param enableChaining Enable pattern chaining optimization
         * @param enablePreComputation Enable pre-computation of high-yield groups
         * @param preComputationThreshold Minimum pattern count for pre-computation
         * @param prioritizeHighYield Prioritize high-yield groups in generation order
         * @param targetSpeedupFactor Target speedup factor over PAL
         * @param enableDelayPatterns Enable delay pattern generation
         * @param maxDelayBars Maximum delay bars for delay patterns
         * @param maxLookbackWithDelay Maximum total lookback including delay
         * @param progressCallback Callback function for progress reporting
         * @param logCallback Callback function for log messages
         */
        GenerationConfig(SearchType targetSearchType,
                        bool enableParallelProcessing = true,
                        size_t maxThreads = 0,
                        bool enableChaining = true,
                        bool enablePreComputation = true,
                        uint32_t preComputationThreshold = 500,
                        bool prioritizeHighYield = true,
                        double targetSpeedupFactor = 24.0,
                        bool enableDelayPatterns = false,
                        uint8_t maxDelayBars = 5,
                        uint8_t maxLookbackWithDelay = 15,
                        std::function<void(const GenerationProgress&)> progressCallback = nullptr,
                        std::function<void(const std::string&)> logCallback = nullptr)
            : mTargetSearchType(targetSearchType),
              mEnableParallelProcessing(enableParallelProcessing),
              mMaxThreads(maxThreads),
              mEnableChaining(enableChaining),
              mEnablePreComputation(enablePreComputation),
              mPreComputationThreshold(preComputationThreshold),
              mPrioritizeHighYield(prioritizeHighYield),
              mTargetSpeedupFactor(targetSpeedupFactor),
              mEnableDelayPatterns(enableDelayPatterns),
              mMaxDelayBars(maxDelayBars),
              mMaxLookbackWithDelay(maxLookbackWithDelay),
              mProgressCallback(progressCallback),
              mLogCallback(logCallback)
        {
        }

        /**
         * @brief Get the target search type
         * @return SearchType Target search type for pattern generation
         */
        SearchType getTargetSearchType() const
        {
            return mTargetSearchType;
        }

        /**
         * @brief Check if parallel processing is enabled
         * @return true if parallel processing is enabled
         */
        bool isParallelProcessingEnabled() const
        {
            return mEnableParallelProcessing;
        }

        /**
         * @brief Get the maximum number of threads
         * @return size_t Maximum number of threads (0 = auto-detect)
         */
        size_t getMaxThreads() const
        {
            return mMaxThreads;
        }

        /**
         * @brief Check if chaining is enabled
         * @return true if pattern chaining optimization is enabled
         */
        bool isChainingEnabled() const
        {
            return mEnableChaining;
        }

        /**
         * @brief Check if pre-computation is enabled
         * @return true if pre-computation of high-yield groups is enabled
         */
        bool isPreComputationEnabled() const
        {
            return mEnablePreComputation;
        }

        /**
         * @brief Get the pre-computation threshold
         * @return uint32_t Minimum pattern count for pre-computation
         */
        uint32_t getPreComputationThreshold() const
        {
            return mPreComputationThreshold;
        }

        /**
         * @brief Check if high-yield prioritization is enabled
         * @return true if high-yield groups are prioritized in generation order
         */
        bool isPrioritizeHighYieldEnabled() const
        {
            return mPrioritizeHighYield;
        }

        /**
         * @brief Get the target speedup factor
         * @return double Target speedup factor over PAL
         */
        double getTargetSpeedupFactor() const
        {
            return mTargetSpeedupFactor;
        }

        /**
         * @brief Check if delay patterns are enabled
         * @return true if delay pattern generation is enabled
         */
        bool isDelayPatternsEnabled() const
        {
            return mEnableDelayPatterns;
        }

        /**
         * @brief Get the maximum delay bars
         * @return uint8_t Maximum delay bars for delay patterns
         */
        uint8_t getMaxDelayBars() const
        {
            return mMaxDelayBars;
        }

        /**
         * @brief Get the maximum lookback with delay
         * @return uint8_t Maximum total lookback including delay
         */
        uint8_t getMaxLookbackWithDelay() const
        {
            return mMaxLookbackWithDelay;
        }

        /**
         * @brief Get the progress callback function
         * @return const std::function<void(const GenerationProgress&)>& Progress callback function
         */
        const std::function<void(const GenerationProgress&)>& getProgressCallback() const
        {
            return mProgressCallback;
        }

        /**
         * @brief Get the log callback function
         * @return const std::function<void(const std::string&)>& Log callback function
         */
        const std::function<void(const std::string&)>& getLogCallback() const
        {
            return mLogCallback;
        }

    private:
        SearchType mTargetSearchType;
        bool mEnableParallelProcessing;
        size_t mMaxThreads;
        bool mEnableChaining;
        bool mEnablePreComputation;
        uint32_t mPreComputationThreshold;
        bool mPrioritizeHighYield;
        double mTargetSpeedupFactor;
        bool mEnableDelayPatterns;
        uint8_t mMaxDelayBars;
        uint8_t mMaxLookbackWithDelay;
        std::function<void(const GenerationProgress&)> mProgressCallback;
        std::function<void(const std::string&)> mLogCallback;
    };
    
    /**
     * @brief Initialize generator with PAL analysis data
     * @param palAnalysisDir Directory containing PAL analysis reports
     */
    explicit OptimizedPatternUniverseGenerator(const std::string& palAnalysisDir);
    
    /**
     * @brief Initialize generator with pre-loaded analysis data
     * @param analysisData Complete PAL analysis data
     */
    explicit OptimizedPatternUniverseGenerator(std::unique_ptr<PALAnalysisData> analysisData);
    
    /**
     * @brief Generate complete pattern universe for specified search type
     * @param config Generation configuration
     * @return Generated pattern universe with performance metrics
     */
    PatternUniverseResult generatePatternUniverse(const GenerationConfig& config);
    
    /**
     * @brief Generate patterns for specific curated groups
     * @param groupIndices Vector of PAL index numbers to generate
     * @param config Generation configuration
     * @return Generated patterns for specified groups
     */
    PatternUniverseResult generateForGroups(const std::vector<uint32_t>& groupIndices, 
                                           const GenerationConfig& config);
    
    /**
     * @brief Generate patterns incrementally with progress reporting
     * @param config Generation configuration with progress callback
     * @return Future for asynchronous generation
     */
    std::future<PatternUniverseResult> generateAsync(const GenerationConfig& config);
    
    /**
     * @brief Validate generated patterns against PAL reference
     * @param generatedPatterns Patterns to validate
     * @param palReferenceDir Directory containing PAL reference patterns
     * @return Validation results with accuracy metrics
     */
    ValidationResult validateAgainstPAL(const PatternUniverseResult& generatedPatterns,
                                       const std::string& palReferenceDir);
    
    /**
     * @brief Get performance estimates for generation
     * @param config Generation configuration
     * @return Estimated generation time and resource usage
     */
    PerformanceEstimate estimatePerformance(const GenerationConfig& config) const;
    
    /**
     * @brief Get curated group manager for advanced operations
     * @return Reference to curated group manager
     */
    const CuratedGroupManager& getCuratedGroupManager() const;
    
    /**
     * @brief Get loaded PAL analysis data
     * @return Reference to PAL analysis data
     */
    const PALAnalysisData& getPALAnalysisData() const;
    
    /**
     * @brief Get generation statistics from last run
     * @return Generation statistics and performance metrics
     */
    GenerationStatistics getLastGenerationStats() const;
    
    /**
     * @brief Export generated patterns to various formats
     * @param patterns Generated pattern universe
     * @param outputPath Output file path
     * @param format Export format (JSON, CSV, PAL, etc.)
     * @return True if export successful
     */
    bool exportPatterns(const PatternUniverseResult& patterns,
                       const std::string& outputPath,
                       ExportFormat format = ExportFormat::JSON) const;

private:
    // Core components
    std::unique_ptr<PALAnalysisData> analysisData_;
    std::unique_ptr<CuratedGroupManager> groupManager_;
    
    // Generation state
    mutable GenerationStatistics lastStats_;
    std::chrono::system_clock::time_point initTime_;
    
    // Performance optimization
    class OptimizationCache {
    public:
        OptimizationCache() = default;
        
        // Cache determines its own initialization state based on populated data
        bool isInitialized() const {
            return !mOptimalGenerationOrder.empty() && !mComponentOptimizations.empty();
        }
        
        // Methods to populate the cache with meaningful operations
        void buildOptimalGenerationOrder(SearchType searchType, const std::vector<uint32_t>& order) {
            mOptimalGenerationOrder[searchType] = order;
        }
        
        void addComponentOptimization(PriceComponentType component, const ComponentOptimizationData& data) {
            mComponentOptimizations[component] = data;
        }
        
        void cachePreComputedTemplates(uint32_t groupIndex, const std::vector<PatternTemplate>& templates) {
            mPreComputedTemplates[groupIndex] = templates;
        }
        
        // Getters for accessing cache data (read-only)
        const std::map<uint32_t, std::vector<PatternTemplate>>& getPreComputedTemplates() const { return mPreComputedTemplates; }
        const std::map<SearchType, std::vector<uint32_t>>& getOptimalGenerationOrder() const { return mOptimalGenerationOrder; }
        const std::map<PriceComponentType, ComponentOptimizationData>& getComponentOptimizations() const { return mComponentOptimizations; }
        
    private:
        std::map<uint32_t, std::vector<PatternTemplate>> mPreComputedTemplates;
        std::map<SearchType, std::vector<uint32_t>> mOptimalGenerationOrder;
        std::map<PriceComponentType, ComponentOptimizationData> mComponentOptimizations;
    };
    mutable OptimizationCache cache_;
    
    // Private generation methods
    PatternUniverseResult generateInternal(const GenerationConfig& config);
    std::vector<PatternTemplate> generateGroupPatterns(const CuratedGroup& group, 
                                                      const GenerationConfig& config);
    std::vector<PatternTemplate> generatePatternsParallel(const std::vector<uint32_t>& groupIndices,
                                                         const GenerationConfig& config);
    std::vector<PatternTemplate> generatePatternsSequential(const std::vector<uint32_t>& groupIndices,
                                                           const GenerationConfig& config);
    
    // Optimization methods
    void initializeOptimizationCache(const GenerationConfig& config) const;
    std::vector<PatternTemplate> applyChainedGeneration(const CuratedGroup& group,
                                                       const std::vector<PatternTemplate>& basePatterns) const;
    ComponentOptimizationData buildComponentOptimization(PriceComponentType component) const;
    
    // Pattern generation core algorithms
    std::vector<PatternTemplate> generateBasicPatterns(const CuratedGroup& group) const;
    std::vector<PatternTemplate> generateExtendedPatterns(const CuratedGroup& group) const;
    std::vector<PatternTemplate> generateDeepPatterns(const CuratedGroup& group) const;
    
    /**
     * @brief Generate delay patterns from base patterns
     * @param basePatterns Vector of base patterns to create delay variants from
     * @param group Curated group containing delay configuration
     * @param config Generation configuration with delay settings
     * @return Vector of delay patterns with shifted bar offsets
     */
    std::vector<PatternTemplate> generateDelayPatterns(
        const std::vector<PatternTemplate>& basePatterns,
        const CuratedGroup& group,
        const GenerationConfig& config) const;
    
    /**
     * @brief Create a single delayed pattern by shifting bar offsets
     * @param basePattern Original pattern to create delay variant from
     * @param delayBars Number of bars to delay (shift offsets by)
     * @return New pattern with all bar offsets shifted by delayBars
     */
    PatternTemplate createDelayedPattern(
        const PatternTemplate& basePattern,
        uint8_t delayBars) const;
    
    /**
     * @brief Validate that a delay pattern meets lookback constraints
     * @param pattern Pattern to validate
     * @param delayBars Number of delay bars applied
     * @param maxLookback Maximum allowed lookback including delay
     * @return True if pattern is valid with the specified delay
     */
    bool isValidDelayPattern(
        const PatternTemplate& pattern,
        uint8_t delayBars,
        uint8_t maxLookback) const;
    
    // Validation and quality assurance
    bool validatePatternIntegrity(const std::vector<PatternTemplate>& patterns) const;
    void reportProgress(const GenerationProgress& progress, const GenerationConfig& config) const;
    void logMessage(const std::string& message, const GenerationConfig& config) const;
    
    // Performance monitoring
    GenerationStatistics updateGenerationStats(const GenerationConfig& config,
                              const std::chrono::duration<double>& duration,
                              size_t patternsGenerated) const;
    double calculateSpeedupFactor(const std::chrono::duration<double>& duration,
                                 size_t patternsGenerated) const;
    
    // Export methods
    bool exportPatternsBinary(const PatternUniverseResult& patterns, const std::string& outputPath) const;
    bool exportPatternsJSON(const PatternUniverseResult& patterns, const std::string& outputPath) const;
    bool exportPatternsCSV(const PatternUniverseResult& patterns, const std::string& outputPath) const;
    
    // Utility methods
    size_t determineOptimalThreadCount(const GenerationConfig& config) const;
    size_t calculateOptimalBatchSize(const CuratedGroup& group, size_t threadCount) const;
    std::vector<uint32_t> getGenerationOrder(SearchType searchType, bool prioritizeHighYield) const;
    
    // New methods for authentic pattern generation
    std::vector<PatternCondition> generateMeaningfulConditions(
        const CuratedGroup& group,
        int conditionCount,
        const std::map<SearchType, ComponentUsageStats>& componentStats) const;
    
    PatternCondition createAuthenticCondition(
        const std::vector<PriceComponentType>& components,
        const std::vector<uint8_t>& barOffsets,
        int conditionIndex,
        const std::map<SearchType, ComponentUsageStats>& componentStats) const;
    
    int determineConditionCount(
        const CuratedGroup& group,
        const AlgorithmInsights& insights) const;
    
    bool validatePatternTradingLogic(
        const PatternTemplate& pattern,
        const CuratedGroup& group) const;
    
    PatternTemplate createChainedPattern(
        const PatternTemplate& basePattern,
        const CuratedGroup& group) const;
    
    std::vector<PatternCondition> generateExtensionConditions(
        const PatternTemplate& basePattern,
        const CuratedGroup& group,
        int additionalConditions) const;
    
    PriceComponentType selectComponentByPALFrequency(
        const std::vector<PriceComponentType>& availableComponents,
        const std::map<SearchType, ComponentUsageStats>& componentStats,
        bool preferHighFrequency) const;
    
    uint8_t selectBarOffsetByPALPattern(
        const std::vector<uint8_t>& availableOffsets,
        int conditionIndex,
        bool isLHS) const;
    
    std::vector<size_t> selectChainingCandidates(
        const std::vector<PatternTemplate>& basePatterns,
        size_t chainingCount) const;
    
    size_t calculateTargetPatternCount(const CuratedGroup& group) const;
    
    std::string generateAuthenticPatternName(
        const CuratedGroup& group,
        size_t patternIndex) const;
    
    bool validatePatternComplexityForTier(
        const PatternTemplate& pattern,
        ComponentTier tier) const;
    
    // Pattern validation and quality control
    bool validatePatternQuality(const PatternTemplate& pattern, const CuratedGroup& group) const;
    
    // Deterministic pattern generation based on PAL analysis
    int determineAdditionalConditions(const PatternTemplate& basePattern, const CuratedGroup& group) const;
};

/**
 * @brief Factory for creating optimized pattern universe generators
 */
class OptimizedPatternUniverseGeneratorFactory {
public:
    /**
     * @brief Create generator from PAL analysis directory
     * @param palAnalysisDir Directory containing PAL analysis reports
     * @return Unique pointer to configured generator
     */
    static std::unique_ptr<OptimizedPatternUniverseGenerator> 
    createFromAnalysisDir(const std::string& palAnalysisDir);
    
    /**
     * @brief Create generator with custom analysis data
     * @param analysisData Pre-loaded PAL analysis data
     * @return Unique pointer to configured generator
     */
    static std::unique_ptr<OptimizedPatternUniverseGenerator> 
    createFromAnalysisData(std::unique_ptr<PALAnalysisData> analysisData);
    
    /**
     * @brief Create generator with performance optimization settings
     * @param palAnalysisDir PAL analysis directory
     * @param optimizationLevel Optimization level (1-3, higher = more aggressive)
     * @return Unique pointer to optimized generator
     */
    static std::unique_ptr<OptimizedPatternUniverseGenerator> 
    createOptimized(const std::string& palAnalysisDir, int optimizationLevel = 2);
};

// Note: PatternUniverseResult and PerformanceEstimate are defined in OptimizedDataStructures.h

} // namespace pattern_universe