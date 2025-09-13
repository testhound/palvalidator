// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, January 2025
//

#ifndef __TRADE_COMPARATOR_H
#define __TRADE_COMPARATOR_H 1

#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include "ExternalTrade.h"
#include "GeneratedTrade.h"
#include "ComparisonTolerance.h"
#include "DecimalConstants.h"
#include "number.h"

namespace mkc_timeseries
{
    /**
     * @brief Result of a trade comparison operation.
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    struct TradeMatchResult
    {
        bool isMatch;                           ///< Whether trades match within tolerances
        Decimal matchScore;                     ///< Weighted match score (0.0 to 1.0)
        std::string mismatchReason;             ///< Reason for mismatch if not matched
        
        // Individual component scores
        Decimal symbolScore;                    ///< Symbol matching score
        Decimal directionScore;                 ///< Direction matching score
        Decimal entryDateScore;                 ///< Entry date matching score
        Decimal exitDateScore;                  ///< Exit date matching score
        Decimal entryPriceScore;                ///< Entry price matching score
        Decimal exitPriceScore;                 ///< Exit price matching score
        Decimal returnScore;                    ///< Return matching score

        /**
         * @brief Constructs a TradeMatchResult with default values.
         */
        TradeMatchResult()
            : isMatch(false), matchScore(DecimalConstants<Decimal>::DecimalZero),
              symbolScore(DecimalConstants<Decimal>::DecimalZero),
              directionScore(DecimalConstants<Decimal>::DecimalZero),
              entryDateScore(DecimalConstants<Decimal>::DecimalZero),
              exitDateScore(DecimalConstants<Decimal>::DecimalZero),
              entryPriceScore(DecimalConstants<Decimal>::DecimalZero),
              exitPriceScore(DecimalConstants<Decimal>::DecimalZero),
              returnScore(DecimalConstants<Decimal>::DecimalZero)
        {
        }
    };

    /**
     * @brief Comprehensive comparison results for a set of trades.
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    struct ComparisonResults
    {
        std::vector<std::pair<GeneratedTrade<Decimal>, ExternalTrade<Decimal>>> matchedTrades;
        std::vector<GeneratedTrade<Decimal>> unmatchedGenerated;
        std::vector<ExternalTrade<Decimal>> unmatchedExternal;
        std::vector<TradeMatchResult<Decimal>> matchDetails;
        
        int totalGenerated;                     ///< Total number of generated trades
        int totalExternal;                      ///< Total number of external trades
        int totalMatched;                       ///< Total number of matched trades
        Decimal matchPercentage;                ///< Percentage of trades that matched
        Decimal averageMatchScore;              ///< Average match score for matched trades

        /**
         * @brief Constructs ComparisonResults with default values.
         */
        ComparisonResults()
            : totalGenerated(0), totalExternal(0), totalMatched(0),
              matchPercentage(DecimalConstants<Decimal>::DecimalZero),
              averageMatchScore(DecimalConstants<Decimal>::DecimalZero)
        {
        }
    };

    /**
     * @brief Advanced trade comparison engine with multi-criteria matching algorithms.
     * 
     * @details
     * The TradeComparator class implements sophisticated algorithms for comparing
     * PAL-generated trades against external backtesting results. It uses weighted
     * scoring across multiple criteria to determine trade equivalence and provides
     * detailed analysis of matching quality.
     * 
     * Key objectives:
     * - Implement multi-criteria trade matching with configurable tolerances
     * - Provide weighted scoring for partial matches and similarity assessment
     * - Handle one-to-one and one-to-many matching scenarios
     * - Generate comprehensive comparison statistics and mismatch analysis
     * - Support different matching strategies (strict, fuzzy, best-match)
     * 
     * Matching criteria:
     * - Symbol exact match or fuzzy matching
     * - Trade direction (Long/Short) with normalization
     * - Entry and exit dates with configurable tolerance
     * - Entry and exit prices with absolute and percentage tolerances
     * - Return calculations with precision-aware comparison
     * 
     * @tparam Decimal High-precision decimal type for financial calculations
     */
    template <class Decimal>
    class TradeComparator
    {
    public:
        /**
         * @brief Enumeration of matching strategies.
         */
        enum class MatchingStrategy
        {
            STRICT,         ///< Strict matching - all criteria must pass
            FUZZY,          ///< Fuzzy matching - weighted scoring with threshold
            BEST_MATCH      ///< Best match - find closest match for each trade
        };

    private:
        MatchingStrategy strategy_;             ///< Current matching strategy
        ComparisonTolerance<Decimal> tolerance_; ///< Comparison tolerance settings

        // Scoring weights for different criteria
        Decimal symbolWeight_;                  ///< Weight for symbol matching
        Decimal directionWeight_;               ///< Weight for direction matching
        Decimal entryDateWeight_;               ///< Weight for entry date matching
        Decimal exitDateWeight_;                ///< Weight for exit date matching
        Decimal entryPriceWeight_;              ///< Weight for entry price matching
        Decimal exitPriceWeight_;               ///< Weight for exit price matching
        Decimal returnWeight_;                  ///< Weight for return matching

    public:
        /**
         * @brief Constructs a TradeComparator with specified strategy and tolerance.
         * 
         * @param strategy Matching strategy to use
         * @param tolerance Comparison tolerance settings
         */
        TradeComparator(MatchingStrategy strategy = MatchingStrategy::FUZZY,
                       const ComparisonTolerance<Decimal>& tolerance = ComparisonTolerance<Decimal>())
            : strategy_(strategy), tolerance_(tolerance)
        {
            // Initialize default weights (can be customized)
            symbolWeight_ = Decimal("0.15");
            directionWeight_ = Decimal("0.15");
            entryDateWeight_ = Decimal("0.15");
            exitDateWeight_ = Decimal("0.15");
            entryPriceWeight_ = Decimal("0.15");
            exitPriceWeight_ = Decimal("0.15");
            returnWeight_ = Decimal("0.10");
        }

        /**
         * @brief Compares two individual trades and returns detailed match result.
         * 
         * @param generated PAL-generated trade
         * @param external External trade from backtesting platform
         * @return Detailed match result with scores and analysis
         */
        TradeMatchResult<Decimal> compareTrades(const GeneratedTrade<Decimal>& generated,
                                              const ExternalTrade<Decimal>& external)
        {
            TradeMatchResult<Decimal> result;

            // Calculate individual component scores
            result.symbolScore = calculateSymbolScore(generated.getSymbol(), external.getSymbol());
            result.directionScore = calculateDirectionScore(generated.getDirection(), external.getDirection());
            result.entryDateScore = calculateDateScore(generated.getEntryDate(), external.getEntryDate());
            result.exitDateScore = calculateDateScore(generated.getExitDate(), external.getExitDate());
            result.entryPriceScore = calculatePriceScore(generated.getEntryPrice(), external.getEntryPrice());
            result.exitPriceScore = calculatePriceScore(generated.getExitPrice(), external.getExitPrice());
            result.returnScore = calculateReturnScore(generated.getPercentReturn(), external.getProfitPercent());

            // Calculate weighted overall score
            result.matchScore = (result.symbolScore * symbolWeight_) +
                               (result.directionScore * directionWeight_) +
                               (result.entryDateScore * entryDateWeight_) +
                               (result.exitDateScore * exitDateWeight_) +
                               (result.entryPriceScore * entryPriceWeight_) +
                               (result.exitPriceScore * exitPriceWeight_) +
                               (result.returnScore * returnWeight_);

            // Determine if match based on strategy
            switch (strategy_)
            {
                case MatchingStrategy::STRICT:
                    result.isMatch = isStrictMatch(result);
                    break;
                case MatchingStrategy::FUZZY:
                    result.isMatch = (result.matchScore >= tolerance_.getMinimumMatchScore());
                    break;
                case MatchingStrategy::BEST_MATCH:
                    result.isMatch = (result.matchScore >= tolerance_.getMinimumMatchScore());
                    break;
            }

            // Generate mismatch reason if not matched
            if (!result.isMatch)
            {
                result.mismatchReason = generateMismatchReason(result);
            }

            return result;
        }

        /**
         * @brief Compares collections of generated and external trades.
         * 
         * @param generatedTrades Vector of PAL-generated trades
         * @param externalTrades Vector of external trades
         * @return Comprehensive comparison results
         */
        ComparisonResults<Decimal> compareTradeCollections(
            const std::vector<GeneratedTrade<Decimal>>& generatedTrades,
            const std::vector<ExternalTrade<Decimal>>& externalTrades)
        {
            ComparisonResults<Decimal> results;
            results.totalGenerated = static_cast<int>(generatedTrades.size());
            results.totalExternal = static_cast<int>(externalTrades.size());

            // Track which trades have been matched
            std::set<int> matchedGenerated;
            std::set<int> matchedExternal;

            // Find best matches
            for (int i = 0; i < results.totalGenerated; ++i)
            {
                const GeneratedTrade<Decimal>& generated = generatedTrades[i];
                
                int bestExternalIndex = -1;
                TradeMatchResult<Decimal> bestMatch;
                
                // Find best matching external trade
                for (int j = 0; j < results.totalExternal; ++j)
                {
                    if (matchedExternal.find(j) != matchedExternal.end())
                        continue; // Already matched
                    
                    const ExternalTrade<Decimal>& external = externalTrades[j];
                    TradeMatchResult<Decimal> matchResult = compareTrades(generated, external);
                    
                    if (matchResult.isMatch && matchResult.matchScore > bestMatch.matchScore)
                    {
                        bestMatch = matchResult;
                        bestExternalIndex = j;
                    }
                }

                // Record match if found
                if (bestExternalIndex >= 0)
                {
                    results.matchedTrades.push_back(std::make_pair(generated, externalTrades[bestExternalIndex]));
                    results.matchDetails.push_back(bestMatch);
                    matchedGenerated.insert(i);
                    matchedExternal.insert(bestExternalIndex);
                    results.totalMatched++;
                }
            }

            // Collect unmatched trades
            for (int i = 0; i < results.totalGenerated; ++i)
            {
                if (matchedGenerated.find(i) == matchedGenerated.end())
                {
                    results.unmatchedGenerated.push_back(generatedTrades[i]);
                }
            }

            for (int j = 0; j < results.totalExternal; ++j)
            {
                if (matchedExternal.find(j) == matchedExternal.end())
                {
                    results.unmatchedExternal.push_back(externalTrades[j]);
                }
            }

            // Calculate summary statistics
            if (results.totalGenerated > 0)
            {
                results.matchPercentage = (Decimal(results.totalMatched) / Decimal(results.totalGenerated)) * 
                                        DecimalConstants<Decimal>::DecimalOneHundred;
            }

            if (results.totalMatched > 0)
            {
                Decimal totalScore = DecimalConstants<Decimal>::DecimalZero;
                for (const auto& match : results.matchDetails)
                {
                    totalScore += match.matchScore;
                }
                results.averageMatchScore = totalScore / Decimal(results.totalMatched);
            }

            return results;
        }

        /**
         * @brief Sets custom scoring weights for different criteria.
         * 
         * @param symbolWeight Weight for symbol matching
         * @param directionWeight Weight for direction matching
         * @param entryDateWeight Weight for entry date matching
         * @param exitDateWeight Weight for exit date matching
         * @param entryPriceWeight Weight for entry price matching
         * @param exitPriceWeight Weight for exit price matching
         * @param returnWeight Weight for return matching
         */
        void setWeights(const Decimal& symbolWeight, const Decimal& directionWeight,
                       const Decimal& entryDateWeight, const Decimal& exitDateWeight,
                       const Decimal& entryPriceWeight, const Decimal& exitPriceWeight,
                       const Decimal& returnWeight)
        {
            symbolWeight_ = symbolWeight;
            directionWeight_ = directionWeight;
            entryDateWeight_ = entryDateWeight;
            exitDateWeight_ = exitDateWeight;
            entryPriceWeight_ = entryPriceWeight;
            exitPriceWeight_ = exitPriceWeight;
            returnWeight_ = returnWeight;
        }

        /**
         * @brief Sets the comparison tolerance settings.
         * 
         * @param tolerance New tolerance settings
         */
        void setTolerance(const ComparisonTolerance<Decimal>& tolerance)
        {
            tolerance_ = tolerance;
        }

        /**
         * @brief Sets the matching strategy.
         * 
         * @param strategy New matching strategy
         */
        void setStrategy(MatchingStrategy strategy)
        {
            strategy_ = strategy;
        }

    private:
        /**
         * @brief Calculates symbol matching score.
         */
        Decimal calculateSymbolScore(const std::string& generated, const std::string& external)
        {
            if (tolerance_.getRequireExactSymbolMatch())
            {
                return (generated == external) ? DecimalConstants<Decimal>::DecimalOne : 
                                               DecimalConstants<Decimal>::DecimalZero;
            }
            else
            {
                // Could implement fuzzy string matching here
                return (generated == external) ? DecimalConstants<Decimal>::DecimalOne : 
                                               DecimalConstants<Decimal>::DecimalZero;
            }
        }

        /**
         * @brief Calculates direction matching score.
         */
        Decimal calculateDirectionScore(const std::string& generated, const std::string& external)
        {
            if (tolerance_.getRequireExactDirectionMatch())
            {
                return (generated == external) ? DecimalConstants<Decimal>::DecimalOne : 
                                               DecimalConstants<Decimal>::DecimalZero;
            }
            else
            {
                return (generated == external) ? DecimalConstants<Decimal>::DecimalOne : 
                                               DecimalConstants<Decimal>::DecimalZero;
            }
        }

        /**
         * @brief Calculates date matching score.
         */
        Decimal calculateDateScore(const boost::gregorian::date& generated, 
                                 const boost::gregorian::date& external)
        {
            int daysDiff = std::abs((generated - external).days());
            
            if (daysDiff <= tolerance_.getDateTolerance())
            {
                return DecimalConstants<Decimal>::DecimalOne;
            }
            else
            {
                return DecimalConstants<Decimal>::DecimalZero;
            }
        }

        /**
         * @brief Calculates price matching score.
         */
        Decimal calculatePriceScore(const Decimal& generated, const Decimal& external)
        {
            Decimal diff = (generated > external) ? (generated - external) : (external - generated);
            
            bool withinAbsolute = !tolerance_.getUseAbsolutePriceTolerance() || 
                                (diff <= tolerance_.getPriceTolerance());
            
            bool withinPercent = !tolerance_.getUsePercentagePriceTolerance() || 
                               (diff <= (external * tolerance_.getPriceTolerancePercent() / 
                                       DecimalConstants<Decimal>::DecimalOneHundred));
            
            return (withinAbsolute && withinPercent) ? DecimalConstants<Decimal>::DecimalOne : 
                                                     DecimalConstants<Decimal>::DecimalZero;
        }

        /**
         * @brief Calculates return matching score.
         */
        Decimal calculateReturnScore(const Decimal& generated, const Decimal& external)
        {
            Decimal diff = (generated > external) ? (generated - external) : (external - generated);
            
            bool withinAbsolute = !tolerance_.getUseAbsoluteReturnTolerance() || 
                                (diff <= tolerance_.getReturnTolerance());
            
            bool withinPercent = !tolerance_.getUsePercentageReturnTolerance() || 
                               (diff <= (external * tolerance_.getReturnTolerancePercent() / 
                                       DecimalConstants<Decimal>::DecimalOneHundred));
            
            return (withinAbsolute && withinPercent) ? DecimalConstants<Decimal>::DecimalOne : 
                                                     DecimalConstants<Decimal>::DecimalZero;
        }

        /**
         * @brief Determines if result represents a strict match.
         */
        bool isStrictMatch(const TradeMatchResult<Decimal>& result)
        {
            return (result.symbolScore == DecimalConstants<Decimal>::DecimalOne) &&
                   (result.directionScore == DecimalConstants<Decimal>::DecimalOne) &&
                   (result.entryDateScore == DecimalConstants<Decimal>::DecimalOne) &&
                   (result.exitDateScore == DecimalConstants<Decimal>::DecimalOne) &&
                   (result.entryPriceScore == DecimalConstants<Decimal>::DecimalOne) &&
                   (result.exitPriceScore == DecimalConstants<Decimal>::DecimalOne) &&
                   (result.returnScore == DecimalConstants<Decimal>::DecimalOne);
        }

        /**
         * @brief Generates mismatch reason string.
         */
        std::string generateMismatchReason(const TradeMatchResult<Decimal>& result)
        {
            std::vector<std::string> reasons;
            
            if (result.symbolScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Symbol mismatch");
            if (result.directionScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Direction mismatch");
            if (result.entryDateScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Entry date outside tolerance");
            if (result.exitDateScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Exit date outside tolerance");
            if (result.entryPriceScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Entry price outside tolerance");
            if (result.exitPriceScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Exit price outside tolerance");
            if (result.returnScore == DecimalConstants<Decimal>::DecimalZero)
                reasons.push_back("Return outside tolerance");
            
            if (reasons.empty())
                return "Overall match score below threshold";
            
            std::string combined;
            for (size_t i = 0; i < reasons.size(); ++i)
            {
                if (i > 0) combined += ", ";
                combined += reasons[i];
            }
            return combined;
        }
    };
}

#endif