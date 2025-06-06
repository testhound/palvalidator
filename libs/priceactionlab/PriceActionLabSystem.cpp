/**
 * @file PriceActionLabSystem.cpp
 * @brief Implements the PriceActionLabSystem and SmallestVolatilityTieBreaker classes.
 *
 * This file contains the logic for managing a collection of Price Action Lab patterns,
 * including adding patterns, handling potential hash collisions with a tie-breaker mechanism,
 * and providing access to the stored patterns. It also implements a concrete tie-breaker
 * strategy based on pattern volatility.
 */
#include "PalAst.h"
#include <iostream> // For std::cout, std::cerr

/**
 * @brief Helper function to print a pattern's description to standard output.
 * @param description Shared pointer to the PatternDescription to print.
 */
void printPatternDescription (PatternDescriptionPtr description)
{
  std::cout << "{FILE:" << description->getFileName() << "  Index: " << description->getpatternIndex()
	    << "  Index DATE: " << description->getIndexDate() << "  PL: " << *(description->getPercentLong())
	    << "%  PS: " << *(description->getPercentShort()) << "%  Trades: " << description->numTrades()
	    << "  CL: " << description->numConsecutiveLosses() << " }" << std::endl;
}

/**
 * @brief Helper function to print a full pattern's description to standard output.
 * @param pattern Shared pointer to the PALPatternPtr whose description is to be printed.
 */
void printPattern (PALPatternPtr pattern)
{
  printPatternDescription(pattern->getPatternDescription());
}

/**
 * @brief Initializes and validates the pattern tie-breaker for the system.
 *
 * If `providedTieBreaker` is null:
 * - If `mUseTieBreaker` is true, a warning is issued, and a default `SmallestVolatilityTieBreaker` is created.
 * - If `mUseTieBreaker` is false, a default `SmallestVolatilityTieBreaker` is created silently (for internal consistency).
 * If `providedTieBreaker` is not null, it is used as the system's tie-breaker.
 *
 * @param providedTieBreaker A shared pointer to the `PatternTieBreaker` strategy to use.
 *                           Can be null.
 */
void PriceActionLabSystem::initializeAndValidateTieBreaker(PatternTieBreakerPtr providedTieBreaker)
{
  if (!providedTieBreaker) // If no tie-breaker is provided
    {
      if (mUseTieBreaker) // If the system is configured to use a tie-breaker
	{
	  // User intended to use a tie-breaker, but provided null. Warn and use default.
	  std::cerr << "Warning: useTieBreaker is true, but provided tieBreaker is null. Using default SmallestVolatilityTieBreaker." << std::endl;
	  mPatternTieBreaker = std::make_shared<SmallestVolatilityTieBreaker>();
	}
      else
	{
	  // User did not intend to use tie-breaking logic OR didn't provide one.
	  // Assign a default one anyway for internal consistency, in case mUseTieBreaker is set to true later.
	  // No warning needed here.
	  mPatternTieBreaker = std::make_shared<SmallestVolatilityTieBreaker>();
	}
    }
  else // A valid tieBreaker was provided
    {
        // Use the provided tie-breaker.
        mPatternTieBreaker = providedTieBreaker;
    }
}

/**
 * @brief Constructs a PriceActionLabSystem with a single initial pattern and a tie-breaker.
 * @param pattern The initial PALPatternPtr to add to the system.
 * @param tieBreaker A shared_ptr to the PatternTieBreaker strategy. Can be null.
 * @param useTieBreaker Boolean indicating whether the tie-breaker should be active.
 */
PriceActionLabSystem::PriceActionLabSystem (PALPatternPtr pattern, 
					    PatternTieBreakerPtr tieBreaker,
					    bool useTieBreaker)
  : mLongsPatternMap(),      /**< @brief Map storing long patterns, keyed by hash code. */
    mShortsPatternMap(),     /**< @brief Map storing short patterns, keyed by hash code. */
    mAllPatterns(),          /**< @brief List storing all patterns added to the system. */
    mUseTieBreaker(useTieBreaker) /**< @brief Flag indicating if the tie-breaker logic is active. */
{
  initializeAndValidateTieBreaker(tieBreaker);
  addPattern (pattern); // Add the initial pattern
}

/**
 * @brief Constructs a PriceActionLabSystem with a specified tie-breaker.
 * @param tieBreaker A shared_ptr to the PatternTieBreaker strategy. Can be null.
 * @param useTieBreaker Boolean indicating whether the tie-breaker should be active. Defaults to false.
 */
PriceActionLabSystem::PriceActionLabSystem (PatternTieBreakerPtr tieBreaker, bool useTieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mAllPatterns(),
    mUseTieBreaker(useTieBreaker)
{
  initializeAndValidateTieBreaker(tieBreaker);
}

/**
 * @brief Default constructor for PriceActionLabSystem.
 * Initializes an empty system with a default `SmallestVolatilityTieBreaker` and
 * `mUseTieBreaker` set to false.
 */
PriceActionLabSystem::PriceActionLabSystem ()
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mPatternTieBreaker (std::make_shared<SmallestVolatilityTieBreaker>()), /**< @brief The tie-breaking strategy. Initialized to SmallestVolatilityTieBreaker by default. */
    mAllPatterns(),
    mUseTieBreaker(false)
{}

/**
 * @brief Constructs a PriceActionLabSystem from a list of patterns and with a specified tie-breaker.
 * @param listOfPatterns A list of PALPatternPtr to initialize the system with.
 * @param tieBreaker A shared_ptr to the PatternTieBreaker strategy. Can be null.
 * @param useTieBreaker Boolean indicating whether the tie-breaker should be active. Defaults to false.
 */
PriceActionLabSystem::PriceActionLabSystem (std::list<PALPatternPtr>& listOfPatterns, 
					    PatternTieBreakerPtr tieBreaker,
					    bool useTieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mAllPatterns(),
    mUseTieBreaker(useTieBreaker)
{
  initializeAndValidateTieBreaker(tieBreaker);

  PriceActionLabSystem::ConstPatternIterator it = listOfPatterns.begin();
  PALPatternPtr p;

  // Log the number of patterns being processed
  std::cout << listOfPatterns.size() << " patterns in PAL IR file" << std::endl << std::endl;

  for (; it != listOfPatterns.end(); it++)
    {
      p = *it;
      addPattern (p); // Add each pattern from the list
    }
}

/**
 * @brief Destructor for PriceActionLabSystem.
 * Cleans up resources. Shared_ptrs manage their own memory.
 */
PriceActionLabSystem::~PriceActionLabSystem()
{
}

/**
 * @brief Gets the total number of unique patterns stored in the system (longs + shorts).
 * If tie-breaking is active, this reflects the count after resolving collisions.
 * @return The total number of unique patterns.
 */
unsigned long PriceActionLabSystem::getNumPatterns() const
{
  return mLongsPatternMap.size() + mShortsPatternMap.size();
}

/**
 * @brief Gets the number of unique long patterns stored in the system.
 * If tie-breaking is active, this reflects the count after resolving collisions.
 * @return The number of unique long patterns.
 */
unsigned long PriceActionLabSystem::getNumLongPatterns() const
{
  return mLongsPatternMap.size();
}

/**
 * @brief Gets the number of unique short patterns stored in the system.
 * If tie-breaking is active, this reflects the count after resolving collisions.
 * @return The number of unique short patterns.
 */
unsigned long PriceActionLabSystem::getNumShortPatterns() const
{
  return mShortsPatternMap.size();
}

/**
 * @brief Adds a pattern to the system.
 * The pattern is added to the `mAllPatterns` list and then routed to either
 * `addLongPattern` or `addShortPattern` based on its type.
 * @param pattern The PALPatternPtr to add.
 */
void 
PriceActionLabSystem::addPattern (PALPatternPtr pattern)
{
  mAllPatterns.push_back(pattern); // Add to the comprehensive list first

  if (pattern->isLongPattern())
    return addLongPattern (pattern); // Route to long pattern handling
  else
    return addShortPattern (pattern); // Route to short pattern handling
}

/**
 * @brief Gets a constant iterator to the beginning of the list of all patterns.
 * This list contains all patterns as they were added, before any tie-breaking logic
 * might have filtered them for the `mLongsPatternMap` or `mShortsPatternMap`.
 * @return A constant iterator.
 */
PriceActionLabSystem::ConstPatternIterator PriceActionLabSystem::allPatternsBegin() const
{ return mAllPatterns.begin(); }

/**
 * @brief Gets a constant iterator to the end of the list of all patterns.
 * @return A constant iterator.
 */
PriceActionLabSystem::ConstPatternIterator PriceActionLabSystem::allPatternsEnd() const
{ return mAllPatterns.end(); }

/**
 * @brief Adds a pattern to a specified pattern map (longs or shorts).
 *
 * If `mUseTieBreaker` is false, the pattern is directly inserted into the map.
 * This allows multiple patterns with the same hash code (duplicates by hash).
 *
 * If `mUseTieBreaker` is true:
 * - If no pattern with the same hash code exists, the new pattern is inserted.
 * - If one or more patterns with the same hash code exist, the `mPatternTieBreaker`
 *   is used to decide whether to keep an existing pattern or replace it (and all
 *   other patterns with the same hash) with the new pattern.
 *   Logs information about collisions and tie-breaker decisions to `std::cout`.
 *
 * @param pattern The PALPatternPtr to add.
 * @param patternMap A reference to the `MapType` (either `mLongsPatternMap` or `mShortsPatternMap`)
 *                   to which the pattern should be added.
 * @param mapIdentifier A string identifier (e.g., "addLongPattern") used for logging purposes.
 */
void PriceActionLabSystem::addPatternToMap(PALPatternPtr pattern,
                                           MapType& patternMap,
                                           const std::string& mapIdentifier)
{
  unsigned long long currentHashCode = pattern->hashCode();

  if (!mUseTieBreaker) // If tie-breaker is not active
    {
      // Default behavior: Allow duplicates (by hash), simply insert into the multimap
      patternMap.insert(std::make_pair(currentHashCode, pattern));
    }
  else // Tie-breaker is active
    {
      // Keep only the "best" pattern per hash code
      auto range = patternMap.equal_range(currentHashCode); // Find existing patterns with the same hash

      if (range.first == range.second) { // If no existing pattern with this hash code
	// Insert the new one
	patternMap.insert(std::make_pair(currentHashCode, pattern));
      }
      else // Existing pattern(s) found with the same hash
	{
	  // Use tie-breaker against the first existing pattern found with this hash
	  std::cout << mapIdentifier << " (Tie-Breaker Active): Hash collision detected for code " << currentHashCode << std::endl;
	  std::cout << "  New Pattern:" << std::endl << "    ";
	  printPattern (pattern); // Log new pattern details
	  std::cout << "  Existing Pattern (representative):" << std::endl << "    ";
	  printPattern (range.first->second); // Log details of the first existing pattern in the collision set

	  if (!mPatternTieBreaker) { // Safety check for null tie-breaker
	    std::cerr << "Error: Tie-breaker is null during pattern addition when tie-breaking is enabled. Cannot proceed." << std::endl;
	    return; // Avoid dereferencing null pointer
	  }

	  // Use the tie-breaker to decide which pattern to keep
	  PALPatternPtr patternToKeep =
	    mPatternTieBreaker->getTieBreakerPattern (pattern, range.first->second);

	  // Check if the new pattern won the tie-break
	  if (patternToKeep == pattern)
	    {
	      std::cout << "  => New pattern selected by tie-breaker (" << mapIdentifier << "). Replacing existing entry/entries." << std::endl;
	      // Erase all existing patterns with this hash code from the map
	      patternMap.erase(currentHashCode);
	      // Insert the new winning pattern into the map
	      patternMap.insert(std::make_pair(currentHashCode, patternToKeep));
	    }
	  else // Existing pattern was preferred by the tie-breaker
	    {
	      std::cout << "  => Existing pattern kept by tie-breaker (" << mapIdentifier << "). Discarding new pattern." << std::endl;
	      // Do nothing, the existing pattern(s) remain, the new one is not added.
	    }
	}
    }
}

/**
 * @brief Adds a long pattern to the system's collection of long patterns.
 * Delegates to `addPatternToMap` for the actual insertion logic,
 * targeting the `mLongsPatternMap`.
 * @param pattern The long PALPatternPtr to add.
 */
void 
PriceActionLabSystem::addLongPattern (PALPatternPtr pattern)
{
  addPatternToMap(pattern, mLongsPatternMap, "addLongPattern");
}

/**
 * @brief Adds a short pattern to the system's collection of short patterns.
 * Delegates to `addPatternToMap` for the actual insertion logic,
 * targeting the `mShortsPatternMap`.
 * @param pattern The short PALPatternPtr to add.
 */
void 
PriceActionLabSystem::addShortPattern (PALPatternPtr pattern)
{
  addPatternToMap(pattern, mShortsPatternMap, "addShortPattern");
}

/**
 * @brief Gets a constant iterator to the beginning of the sorted map of long patterns.
 * @return A constant iterator.
 */
PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternLongsBegin() const
{
  return mLongsPatternMap.begin();
}

/**
 * @brief Gets a constant iterator to the end of the sorted map of long patterns.
 * @return A constant iterator.
 */
PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternLongsEnd() const
{
  return mLongsPatternMap.end();
}

/**
 * @brief Gets a non-constant iterator to the beginning of the sorted map of long patterns.
 * @return A non-constant iterator.
 */
PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternLongsBegin()
{
  return mLongsPatternMap.begin();
}

/**
 * @brief Gets a non-constant iterator to the end of the sorted map of long patterns.
 * @return A non-constant iterator.
 */
PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternLongsEnd()
{
  return mLongsPatternMap.end();
}

/**
 * @brief Gets a constant iterator to the beginning of the sorted map of short patterns.
 * @return A constant iterator.
 */
PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternShortsBegin() const
{
  return mShortsPatternMap.begin();
}

/**
 * @brief Gets a constant iterator to the end of the sorted map of short patterns.
 * @return A constant iterator.
 */
PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternShortsEnd() const
{
  return mShortsPatternMap.end();
}

/**
 * @brief Gets a non-constant iterator to the beginning of the sorted map of short patterns.
 * @return A non-constant iterator.
 */
PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternShortsBegin()
{
  return mShortsPatternMap.begin();
}

/**
 * @brief Gets a non-constant iterator to the end of the sorted map of short patterns.
 * @return A non-constant iterator.
 */
PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternShortsEnd()
{
  return mShortsPatternMap.end();
}

// --- SmallestVolatilityTieBreaker ---

/**
 * @brief Determines which of two patterns to keep based on their volatility attribute.
 *
 * The method ranks patterns by volatility: Low (0), Normal (1), High (2), VeryHigh (3), None (4).
 * The pattern with the lower rank (i.e., lower volatility) is preferred.
 * If both patterns have the same volatility rank (or if one is None and the other is also None or worse),
 * `pattern1` (the new pattern being considered) is returned as a default.
 *
 * @param pattern1 The first pattern (typically the new pattern being considered for addition).
 * @param pattern2 The second pattern (typically an existing pattern with a hash collision).
 * @return The `PALPatternPtr` of the pattern that "wins" the tie-break (i.e., should be kept).
 */
PALPatternPtr
SmallestVolatilityTieBreaker::getTieBreakerPattern(PALPatternPtr pattern1, 
						   PALPatternPtr pattern2) const
{
  // Lambda function to rank volatility attributes. Lower rank is better.
  auto volRank = [](const PALPatternPtr &p){
        if (p->isLowVolatilityPattern())       return 0; // Lowest rank
        if (p->isNormalVolatilityPattern())    return 1;
        if (p->isHighVolatilityPattern())      return 2;
        if (p->isVeryHighVolatilityPattern())  return 3;
        // Patterns with VOLATILITY_NONE or any other unhandled state get the highest rank
        return 4;
    };

    int r1 = volRank(pattern1); // Rank of the first pattern
    int r2 = volRank(pattern2); // Rank of the second pattern

    if (r1 < r2) // If pattern1 has lower (better) volatility rank
      return pattern1;

    if (r2 < r1) // If pattern2 has lower (better) volatility rank
      return pattern2;

    // If ranks are equal (or both are rank 4), default to keeping pattern1.
    return pattern1;
}



