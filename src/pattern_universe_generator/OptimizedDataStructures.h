#pragma once

#include <vector>
#include <set>
#include <map>
#include <string>
#include <cstdint>
#include <chrono>
#include <PatternTemplate.h>

namespace pattern_universe {

// Forward declarations
enum class PriceComponentType;
enum class SearchType;
class ComponentUsageStats;

/**
 * @brief Search type enumeration based on PAL analysis
 */
enum class SearchType {
    EXTENDED,     // 2-6 bar patterns, mixture of O,H,L,C
    DEEP,         // 2-9 bar patterns, mixture of O,H,L,C
    CLOSE_ONLY,   // 3-9 bar patterns, only Close prices
    MIXED,        // 2-9 bar patterns, mixture of O,H,L,C
    HIGH_LOW_ONLY,// 3-9 bar patterns, only High and Low prices
    OPEN_CLOSE_ONLY // 3-9 bar patterns, only Open and Close prices
};

/**
 * @brief Price component types from PAL analysis
 */
enum class PriceComponentType {
    OPEN,
    HIGH,
    LOW,
    CLOSE,
    VOLUME,
    ROC1,
    IBS1,
    IBS2,
    IBS3,
    MEANDER,
    VCHARTLOW,
    VCHARTHIGH
};

/**
 * @brief Component tier enumeration for PAL's hierarchy
 */
enum class ComponentTier {
    Unknown,
    FullOHLC,    // All four components (indices 1-153)
    Mixed,       // Three components (indices 154-325)
    Dual,        // Two components (indices 326-478)
    Single       // Single component (indices 480-545)
};

/**
 * @brief Component complexity levels for pattern generation
 */
enum class ComponentComplexity {
    Simple,      // 1-2 components
    Moderate,    // 3 components
    Complex,     // 4+ components
    Full         // All OHLC components
};

/**
 * @brief Curated group representing PAL's discovered index mappings
 */
class CuratedGroup {
public:
    CuratedGroup() : mIndexNumber(0), mMinPatternLength(0), mMaxPatternLength(0),
                    mSupportsChaining(false), mGenerationPriority(0.0), mPatternCount(0),
                    mSupportsDelayPatterns(true), mMaxDelayBars(5), mMaxLookbackWithDelay(15) {}

    CuratedGroup(uint32_t indexNumber, std::vector<uint8_t> barOffsets,
                 std::set<PriceComponentType> componentTypes, SearchType searchType,
                 uint8_t minPatternLength, uint8_t maxPatternLength, uint32_t patternCount,
                 double generationPriority, bool supportsChaining)
        : mIndexNumber(indexNumber), mBarOffsets(std::move(barOffsets)),
          mComponentTypes(std::move(componentTypes)), mSearchType(searchType),
          mMinPatternLength(minPatternLength), mMaxPatternLength(maxPatternLength),
          mSupportsChaining(supportsChaining), mGenerationPriority(generationPriority),
          mPatternCount(patternCount), mSupportsDelayPatterns(true), mMaxDelayBars(5),
          mMaxLookbackWithDelay(15) {}

    uint32_t getIndexNumber() const { return mIndexNumber; }
    const std::vector<uint8_t>& getBarOffsets() const { return mBarOffsets; }
    const std::set<PriceComponentType>& getComponentTypes() const { return mComponentTypes; }
    SearchType getSearchType() const { return mSearchType; }
    uint8_t getMinPatternLength() const { return mMinPatternLength; }
    uint8_t getMaxPatternLength() const { return mMaxPatternLength; }
    bool isSupportingChaining() const { return mSupportsChaining; }
    double getGenerationPriority() const { return mGenerationPriority; }
    uint32_t getPatternCount() const { return mPatternCount; }
    bool isSupportingDelayPatterns() const { return mSupportsDelayPatterns; }
    uint8_t getMaxDelayBars() const { return mMaxDelayBars; }
    uint8_t getMaxLookbackWithDelay() const { return mMaxLookbackWithDelay; }

private:
    uint32_t mIndexNumber;
    std::vector<uint8_t> mBarOffsets;
    std::set<PriceComponentType> mComponentTypes;
    SearchType mSearchType;
    uint8_t mMinPatternLength;
    uint8_t mMaxPatternLength;
    bool mSupportsChaining;
    double mGenerationPriority;
    uint32_t mPatternCount;
    bool mSupportsDelayPatterns;
    uint8_t mMaxDelayBars;
    uint8_t mMaxLookbackWithDelay;
};

/**
 * @brief Component combination for pattern generation
 */
class ComponentCombination {
public:
    ComponentCombination(std::set<PriceComponentType> components, double usageFrequency,
                         uint8_t minLength, uint8_t maxLength)
        : mComponents(std::move(components)), mUsageFrequency(usageFrequency),
          mMinLength(minLength), mMaxLength(maxLength) {}

    const std::set<PriceComponentType>& getComponents() const { return mComponents; }
    double getUsageFrequency() const { return mUsageFrequency; }
    uint8_t getMinLength() const { return mMinLength; }
    uint8_t getMaxLength() const { return mMaxLength; }

private:
    std::set<PriceComponentType> mComponents;
    double mUsageFrequency;
    uint8_t mMinLength;
    uint8_t mMaxLength;
};

/**
 * @brief Component variation within a curated group
 */
class ComponentVariation {
public:
    ComponentVariation(std::vector<PriceComponentType> sequence,
                       std::vector<uint8_t> barOffsets, double weight)
        : mSequence(std::move(sequence)), mBarOffsets(std::move(barOffsets)), mWeight(weight) {}

    const std::vector<PriceComponentType>& getSequence() const { return mSequence; }
    const std::vector<uint8_t>& getBarOffsets() const { return mBarOffsets; }
    double getWeight() const { return mWeight; }

private:
    std::vector<PriceComponentType> mSequence;
    std::vector<uint8_t> mBarOffsets;
    double mWeight;
};

/**
 * @brief Component usage statistics from PAL analysis
 */
class ComponentUsageStats {
public:
    /**
     * @brief Constructor for ComponentUsageStats
     * @param totalPatterns Total number of patterns analyzed
     * @param uniqueIndices Number of unique indices
     * @param componentUsage Map of component types to their usage counts
     * @param patternLengthDistribution Map of pattern lengths to their counts
     */
    ComponentUsageStats(uint32_t totalPatterns,
                       size_t uniqueIndices,
                       std::map<PriceComponentType, uint32_t> componentUsage,
                       std::map<uint8_t, uint32_t> patternLengthDistribution = {})
        : mTotalPatterns(totalPatterns)
        , mUniqueIndices(uniqueIndices)
        , mComponentUsage(std::move(componentUsage))
        , mPatternLengthDistribution(std::move(patternLengthDistribution)) {}

    const std::map<PriceComponentType, uint32_t>& getComponentUsage() const { return mComponentUsage; }
    const std::map<uint8_t, uint32_t>& getPatternLengthDistribution() const { return mPatternLengthDistribution; }
    uint32_t getTotalPatterns() const { return mTotalPatterns; }
    size_t getUniqueIndices() const { return mUniqueIndices; }

    double getUsagePercentage(PriceComponentType type) const {
        auto it = mComponentUsage.find(type);
        if (it == mComponentUsage.end() || mTotalPatterns == 0) return 0.0;
        
        uint32_t totalUsage = 0;
        for (const auto& pair : mComponentUsage) {
            totalUsage += pair.second;
        }
        
        return totalUsage > 0 ? (100.0 * it->second / totalUsage) : 0.0;
    }

private:
    uint32_t mTotalPatterns;
    size_t mUniqueIndices;
    std::map<PriceComponentType, uint32_t> mComponentUsage;
    std::map<uint8_t, uint32_t> mPatternLengthDistribution;
};

/**
 * @brief PAL index mappings loaded from analysis reports
 */
class PALIndexMappings {
public:
    /**
     * @brief Constructor for PALIndexMappings
     * @param indexToGroup Map of index numbers to curated groups
     * @param searchTypeToIndices Map of search types to their indices
     * @param componentStats Component usage statistics by search type
     * @param totalPatterns Total number of patterns
     * @param totalIndices Total number of indices
     * @param analysisDate Date of analysis
     */
    PALIndexMappings(std::map<uint32_t, CuratedGroup> indexToGroup = {},
                    std::map<SearchType, std::vector<uint32_t>> searchTypeToIndices = {},
                    std::map<SearchType, ComponentUsageStats> componentStats = {},
                    size_t totalPatterns = 0,
                    size_t totalIndices = 0,
                    std::chrono::system_clock::time_point analysisDate = std::chrono::system_clock::now())
        : mIndexToGroup(std::move(indexToGroup))
        , mSearchTypeToIndices(std::move(searchTypeToIndices))
        , mComponentStats(std::move(componentStats))
        , mTotalPatterns(totalPatterns)
        , mTotalIndices(totalIndices)
        , mAnalysisDate(analysisDate) {}

    // Builder methods for constructing the mappings
    void addGroup(uint32_t index, CuratedGroup group) {
        mIndexToGroup[index] = std::move(group);
    }

    void addSearchTypeIndex(SearchType type, uint32_t index) {
        mSearchTypeToIndices[type].push_back(index);
    }

    void addComponentStats(SearchType type, ComponentUsageStats stats) {
        mComponentStats.emplace(type, std::move(stats));
    }

    const std::map<uint32_t, CuratedGroup>& getIndexToGroup() const { return mIndexToGroup; }
    const std::map<SearchType, std::vector<uint32_t>>& getSearchTypeToIndices() const { return mSearchTypeToIndices; }
    const std::map<SearchType, ComponentUsageStats>& getComponentStats() const { return mComponentStats; }
    size_t getTotalPatterns() const { return mTotalPatterns; }
    size_t getTotalIndices() const { return mTotalIndices; }
    std::chrono::system_clock::time_point getAnalysisDate() const { return mAnalysisDate; }

private:
    std::map<uint32_t, CuratedGroup> mIndexToGroup;
    std::map<SearchType, std::vector<uint32_t>> mSearchTypeToIndices;
    std::map<SearchType, ComponentUsageStats> mComponentStats;
    size_t mTotalPatterns;
    size_t mTotalIndices;
    std::chrono::system_clock::time_point mAnalysisDate;
};

/**
 * @brief Algorithm insights from PAL analysis
 */
class AlgorithmInsights {
public:
    /**
     * @brief Constructor for AlgorithmInsights
     * @param totalPatterns Total number of patterns analyzed
     * @param chainedPatterns Number of patterns using chaining
     * @param chainingPercentage Percentage of patterns using chaining
     * @param curatedGroups Description of curated groups
     * @param componentConstraints Description of component constraints
     * @param searchSpaceReduction Description of search space reduction
     * @param barSpreadDistribution Distribution of bar spreads
     * @param maxOffsetDistribution Distribution of maximum offsets
     */
    AlgorithmInsights(size_t totalPatterns = 0,
                     size_t chainedPatterns = 0,
                     double chainingPercentage = 0.0,
                     const std::string& curatedGroups = "",
                     const std::string& componentConstraints = "",
                     const std::string& searchSpaceReduction = "",
                     std::map<uint8_t, size_t> barSpreadDistribution = {},
                     std::map<uint8_t, size_t> maxOffsetDistribution = {})
        : mTotalPatterns(totalPatterns)
        , mChainedPatterns(chainedPatterns)
        , mChainingPercentage(chainingPercentage)
        , mCuratedGroups(curatedGroups)
        , mComponentConstraints(componentConstraints)
        , mSearchSpaceReduction(searchSpaceReduction)
        , mBarSpreadDistribution(std::move(barSpreadDistribution))
        , mMaxOffsetDistribution(std::move(maxOffsetDistribution)) {}

    // Builder methods for loading from analysis files
    void addBarSpread(uint8_t spread, size_t count) { mBarSpreadDistribution[spread] = count; }
    void addMaxOffset(uint8_t offset, size_t count) { mMaxOffsetDistribution[offset] = count; }

    const std::string& getCuratedGroups() const { return mCuratedGroups; }
    const std::string& getComponentConstraints() const { return mComponentConstraints; }
    const std::string& getSearchSpaceReduction() const { return mSearchSpaceReduction; }
    size_t getTotalPatterns() const { return mTotalPatterns; }
    size_t getChainedPatterns() const { return mChainedPatterns; }
    double getChainingPercentage() const { return mChainingPercentage; }
    const std::map<uint8_t, size_t>& getBarSpreadDistribution() const { return mBarSpreadDistribution; }
    const std::map<uint8_t, size_t>& getMaxOffsetDistribution() const { return mMaxOffsetDistribution; }

private:
    size_t mTotalPatterns;
    size_t mChainedPatterns;
    double mChainingPercentage;
    std::string mCuratedGroups;
    std::string mComponentConstraints;
    std::string mSearchSpaceReduction;
    std::map<uint8_t, size_t> mBarSpreadDistribution;
    std::map<uint8_t, size_t> mMaxOffsetDistribution;
};

/**
 * @brief Component hierarchy rules extracted from PAL
 */
class ComponentHierarchyRules {
public:
    ComponentHierarchyRules() = default;
    
    ComponentHierarchyRules(std::map<uint32_t, std::set<PriceComponentType>> indexToAllowedComponents)
        : mIndexToAllowedComponents(std::move(indexToAllowedComponents))
    {
        // Build reverse mapping
        for (const auto& [index, components] : mIndexToAllowedComponents) {
            mComponentSetToIndices[components].push_back(index);
        }
    }

    void addAllowedComponents(uint32_t index, std::set<PriceComponentType> components) {
        mIndexToAllowedComponents[index] = std::move(components);
    }

    void addComponentSetIndex(const std::set<PriceComponentType>& components, uint32_t index) {
        mComponentSetToIndices[components].push_back(index);
    }

    const std::map<uint32_t, std::set<PriceComponentType>>& getIndexToAllowedComponents() const { return mIndexToAllowedComponents; }
    const std::map<std::set<PriceComponentType>, std::vector<uint32_t>>& getComponentSetToIndices() const { return mComponentSetToIndices; }

    bool isValidCombination(const std::set<PriceComponentType>& components, uint32_t indexNumber) const {
        auto it = mIndexToAllowedComponents.find(indexNumber);
        if (it == mIndexToAllowedComponents.end()) return false;
        
        for (const auto& comp : components) {
            if (it->second.find(comp) == it->second.end()) {
                return false;
            }
        }
        return true;
    }

private:
    std::map<uint32_t, std::set<PriceComponentType>> mIndexToAllowedComponents;
    std::map<std::set<PriceComponentType>, std::vector<uint32_t>> mComponentSetToIndices;
};

/**
 * @brief Validation result for PAL comparison
 */
class ValidationResult {
public:
    ValidationResult() : mSuccess(false), mExpectedPatterns(0), mActualPatterns(0),
                        mAccuracyPercentage(0.0), mTotalGenerated(0) {}

    ValidationResult(bool success, const std::string& message, size_t expectedPatterns,
                    size_t actualPatterns, double accuracyPercentage,
                    const std::vector<std::string>& errors, size_t totalGenerated,
                    std::chrono::system_clock::time_point validationStartTime,
                    std::chrono::system_clock::time_point validationEndTime)
        : mSuccess(success), mMessage(message), mExpectedPatterns(expectedPatterns),
          mActualPatterns(actualPatterns), mAccuracyPercentage(accuracyPercentage),
          mErrors(errors), mTotalGenerated(totalGenerated),
          mValidationStartTime(validationStartTime), mValidationEndTime(validationEndTime) {}

    bool isSuccess() const { return mSuccess; }
    const std::string& getMessage() const { return mMessage; }
    size_t getExpectedPatterns() const { return mExpectedPatterns; }
    size_t getActualPatterns() const { return mActualPatterns; }
    double getAccuracyPercentage() const { return mAccuracyPercentage; }
    const std::vector<std::string>& getErrors() const { return mErrors; }
    size_t getTotalGenerated() const { return mTotalGenerated; }
    std::chrono::system_clock::time_point getValidationStartTime() const { return mValidationStartTime; }
    std::chrono::system_clock::time_point getValidationEndTime() const { return mValidationEndTime; }

private:
    bool mSuccess;
    std::string mMessage;
    size_t mExpectedPatterns;
    size_t mActualPatterns;
    double mAccuracyPercentage;
    std::vector<std::string> mErrors;
    size_t mTotalGenerated;
    std::chrono::system_clock::time_point mValidationStartTime;
    std::chrono::system_clock::time_point mValidationEndTime;
};

/**
 * @brief Performance metrics for benchmarking
 */
class PerformanceMetrics {
public:
    PerformanceMetrics(std::chrono::milliseconds generationTime, size_t patternsGenerated,
                       size_t memoryUsedMB, double patternsPerSecond, size_t threadsUsed)
        : mGenerationTime(generationTime), mPatternsGenerated(patternsGenerated),
          mMemoryUsedMB(memoryUsedMB), mPatternsPerSecond(patternsPerSecond),
          mThreadsUsed(threadsUsed) {}

    std::chrono::milliseconds getGenerationTime() const { return mGenerationTime; }
    size_t getPatternsGenerated() const { return mPatternsGenerated; }
    size_t getMemoryUsedMB() const { return mMemoryUsedMB; }
    double getPatternsPerSecond() const { return mPatternsPerSecond; }
    size_t getThreadsUsed() const { return mThreadsUsed; }

private:
    std::chrono::milliseconds mGenerationTime;
    size_t mPatternsGenerated;
    size_t mMemoryUsedMB;
    double mPatternsPerSecond;
    size_t mThreadsUsed;
};

/**
 * @brief Accuracy report for validation
 */
class AccuracyReport {
public:
    AccuracyReport() : mIndexMappingAccurate(false), mComponentUsageAccurate(false),
                      mPatternCountAccurate(false), mOverallAccuracy(0.0) {}

    bool isIndexMappingAccurate() const { return mIndexMappingAccurate; }
    bool isComponentUsageAccurate() const { return mComponentUsageAccurate; }
    bool isPatternCountAccurate() const { return mPatternCountAccurate; }
    const std::map<uint32_t, bool>& getIndexValidation() const { return mIndexValidation; }
    const std::vector<std::string>& getDiscrepancies() const { return mDiscrepancies; }
    double getOverallAccuracy() const { return mOverallAccuracy; }

private:
    bool mIndexMappingAccurate;
    bool mComponentUsageAccurate;
    bool mPatternCountAccurate;
    std::map<uint32_t, bool> mIndexValidation;
    std::vector<std::string> mDiscrepancies;
    double mOverallAccuracy;
};

/**
 * @brief Group optimization settings for curated group manager
 */
class GroupOptimizationSettings {
public:
    GroupOptimizationSettings()
        : mEnablePreComputation(true)
        , mPrioritizeHighYield(true)
        , mPreComputationThreshold(1000)
        , mChainingThreshold(0.195)
        , mMaxBatchSize(10000) {}

    bool isPreComputationEnabled() const { return mEnablePreComputation; }
    bool isPrioritizeHighYieldEnabled() const { return mPrioritizeHighYield; }
    uint32_t getPreComputationThreshold() const { return mPreComputationThreshold; }
    double getChainingThreshold() const { return mChainingThreshold; }
    size_t getMaxBatchSize() const { return mMaxBatchSize; }

private:
    bool mEnablePreComputation;
    bool mPrioritizeHighYield;
    uint32_t mPreComputationThreshold;
    double mChainingThreshold;
    size_t mMaxBatchSize;
};

/**
 * @brief Component usage information for optimization
 */
class ComponentUsageInfo {
public:
    ComponentUsageInfo() : mTotalUsage(0), mUsagePercentage(0.0), mPrimaryTier(ComponentTier::Unknown), mIsHighEfficiency(false) {}

    ComponentUsageInfo(uint32_t totalUsage, double usagePercentage,
                      const std::vector<uint32_t>& associatedIndices,
                      ComponentTier primaryTier, bool isHighEfficiency)
        : mTotalUsage(totalUsage), mUsagePercentage(usagePercentage),
          mAssociatedIndices(associatedIndices), mPrimaryTier(primaryTier),
          mIsHighEfficiency(isHighEfficiency) {}

    uint32_t getTotalUsage() const { return mTotalUsage; }
    double getUsagePercentage() const { return mUsagePercentage; }
    const std::vector<uint32_t>& getAssociatedIndices() const { return mAssociatedIndices; }
    ComponentTier getPrimaryTier() const { return mPrimaryTier; }
    bool isHighEfficiency() const { return mIsHighEfficiency; }

private:
    uint32_t mTotalUsage;
    double mUsagePercentage;
    std::vector<uint32_t> mAssociatedIndices;
    ComponentTier mPrimaryTier;
    bool mIsHighEfficiency;
};

/**
 * @brief Export format enumeration
 */
enum class ExportFormat {
    JSON,
    CSV,
    Binary
};

/**
 * @brief Generation progress information
 */
class GenerationProgress {
public:
    GenerationProgress(size_t completedGroups, size_t totalGroups, double percentComplete,
                       size_t patternsGenerated, uint32_t currentGroup)
        : mCompletedGroups(completedGroups), mTotalGroups(totalGroups),
          mPercentComplete(percentComplete), mPatternsGenerated(patternsGenerated),
          mCurrentGroup(currentGroup) {}

    size_t getCompletedGroups() const { return mCompletedGroups; }
    size_t getTotalGroups() const { return mTotalGroups; }
    double getPercentComplete() const { return mPercentComplete; }
    size_t getPatternsGenerated() const { return mPatternsGenerated; }
    uint32_t getCurrentGroup() const { return mCurrentGroup; }

private:
    size_t mCompletedGroups;
    size_t mTotalGroups;
    double mPercentComplete;
    size_t mPatternsGenerated;
    uint32_t mCurrentGroup;
};

/**
 * @brief Generation statistics
 */
class GenerationStatistics {
public:
    GenerationStatistics(std::chrono::duration<double> totalGenerationTime,
                        size_t totalPatternsGenerated,
                        double patternsPerSecond,
                        double speedupFactor,
                        size_t threadsUsed,
                        bool chainingEnabled,
                        bool preComputationEnabled)
        : mTotalGenerationTime(totalGenerationTime)
        , mTotalPatternsGenerated(totalPatternsGenerated)
        , mPatternsPerSecond(patternsPerSecond)
        , mSpeedupFactor(speedupFactor)
        , mThreadsUsed(threadsUsed)
        , mChainingEnabled(chainingEnabled)
        , mPreComputationEnabled(preComputationEnabled) {}

    std::chrono::duration<double> getTotalGenerationTime() const { return mTotalGenerationTime; }
    size_t getTotalPatternsGenerated() const { return mTotalPatternsGenerated; }
    double getPatternsPerSecond() const { return mPatternsPerSecond; }
    double getSpeedupFactor() const { return mSpeedupFactor; }
    size_t getThreadsUsed() const { return mThreadsUsed; }
    bool isChainingEnabled() const { return mChainingEnabled; }
    bool isPreComputationEnabled() const { return mPreComputationEnabled; }

private:
    std::chrono::duration<double> mTotalGenerationTime;
    size_t mTotalPatternsGenerated;
    double mPatternsPerSecond;
    double mSpeedupFactor;
    size_t mThreadsUsed;
    bool mChainingEnabled;
    bool mPreComputationEnabled;
};

/**
 * @brief Performance estimation result
 */
class PerformanceEstimate {
public:
    PerformanceEstimate(size_t estimatedPatterns,
                       std::chrono::duration<double> estimatedTime,
                       double estimatedSpeedup,
                       size_t recommendedThreads,
                       size_t estimatedMemoryUsageMB,
                       std::vector<std::string> optimizationRecommendations)
        : mEstimatedPatterns(estimatedPatterns)
        , mEstimatedTime(estimatedTime)
        , mEstimatedSpeedup(estimatedSpeedup)
        , mRecommendedThreads(recommendedThreads)
        , mEstimatedMemoryUsageMB(estimatedMemoryUsageMB)
        , mOptimizationRecommendations(std::move(optimizationRecommendations)) {}

    size_t getEstimatedPatterns() const { return mEstimatedPatterns; }
    std::chrono::duration<double> getEstimatedTime() const { return mEstimatedTime; }
    double getEstimatedSpeedup() const { return mEstimatedSpeedup; }
    size_t getRecommendedThreads() const { return mRecommendedThreads; }
    size_t getEstimatedMemoryUsageMB() const { return mEstimatedMemoryUsageMB; }
    const std::vector<std::string>& getOptimizationRecommendations() const { return mOptimizationRecommendations; }

private:
    size_t mEstimatedPatterns;
    std::chrono::duration<double> mEstimatedTime;
    double mEstimatedSpeedup;
    size_t mRecommendedThreads;
    size_t mEstimatedMemoryUsageMB;
    std::vector<std::string> mOptimizationRecommendations;
};

/**
 * @brief Component optimization data
 */
class ComponentOptimizationData {
public:
    ComponentOptimizationData() : mUsageFrequency(0), mIsHighEfficiency(false), mOptimizationWeight(0.0) {}

    ComponentOptimizationData(size_t usageFrequency, bool isHighEfficiency, double optimizationWeight = 0.0)
        : mUsageFrequency(usageFrequency), mIsHighEfficiency(isHighEfficiency), mOptimizationWeight(optimizationWeight) {}

    size_t getUsageFrequency() const { return mUsageFrequency; }
    bool isHighEfficiency() const { return mIsHighEfficiency; }
    double getOptimizationWeight() const { return mOptimizationWeight; }

private:
    size_t mUsageFrequency;
    bool mIsHighEfficiency;
    double mOptimizationWeight;
};

/**
 * @brief Pattern universe generation result
 */
class PatternUniverseResult {
public:
    PatternUniverseResult(std::vector<PatternTemplate> patterns,
                         size_t totalPatternsGenerated,
                         std::chrono::duration<double> totalGenerationTime,
                         double patternsPerSecond,
                         double speedupFactor,
                         std::chrono::system_clock::time_point generatedAt,
                         const std::string& generatorVersion,
                         GenerationStatistics statistics,
                         size_t basePatterns,
                         size_t delayPatterns,
                         std::map<int, size_t> delayDistribution)
        : mPatterns(std::move(patterns))
        , mTotalPatternsGenerated(totalPatternsGenerated)
        , mTotalGenerationTime(totalGenerationTime)
        , mPatternsPerSecond(patternsPerSecond)
        , mSpeedupFactor(speedupFactor)
        , mGeneratedAt(generatedAt)
        , mGeneratorVersion(generatorVersion)
        , mStatistics(std::move(statistics))
        , mBasePatterns(basePatterns)
        , mDelayPatterns(delayPatterns)
        , mDelayDistribution(std::move(delayDistribution)) {}

    const std::vector<PatternTemplate>& getPatterns() const { return mPatterns; }
    std::vector<PatternTemplate>& getPatterns() { return mPatterns; }
    size_t getTotalPatternsGenerated() const { return mTotalPatternsGenerated; }
    std::chrono::duration<double> getTotalGenerationTime() const { return mTotalGenerationTime; }
    double getPatternsPerSecond() const { return mPatternsPerSecond; }
    double getSpeedupFactor() const { return mSpeedupFactor; }
    std::chrono::system_clock::time_point getGeneratedAt() const { return mGeneratedAt; }
    const std::string& getGeneratorVersion() const { return mGeneratorVersion; }
    const GenerationStatistics& getStatistics() const { return mStatistics; }
    size_t getBasePatterns() const { return mBasePatterns; }
    size_t getDelayPatterns() const { return mDelayPatterns; }
    const std::map<int, size_t>& getDelayDistribution() const { return mDelayDistribution; }

private:
    std::vector<PatternTemplate> mPatterns;
    size_t mTotalPatternsGenerated;
    std::chrono::duration<double> mTotalGenerationTime;
    double mPatternsPerSecond;
    double mSpeedupFactor;
    std::chrono::system_clock::time_point mGeneratedAt;
    std::string mGeneratorVersion;
    GenerationStatistics mStatistics;
    size_t mBasePatterns;
    size_t mDelayPatterns;
    std::map<int, size_t> mDelayDistribution;
};

// Helper functions for type conversion
std::string searchTypeToString(SearchType type);
SearchType stringToSearchType(const std::string& str);
std::string componentTypeToString(PriceComponentType type);
PriceComponentType stringToComponentType(const std::string& str);
std::string componentTierToString(ComponentTier tier);
ComponentTier stringToComponentTier(const std::string& str);
std::string componentComplexityToString(ComponentComplexity complexity);
ComponentComplexity stringToComponentComplexity(const std::string& str);

} // namespace pattern_universe