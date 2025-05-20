#include "PalAst.h"
#include <iostream>

void printPatternDescription (PatternDescriptionPtr description)
{
  std::cout << "{FILE:" << description->getFileName() << "  Index: " << description->getpatternIndex()
	    << "  Index DATE: " << description->getIndexDate() << "  PL: " << *(description->getPercentLong())
	    << "%  PS: " << *(description->getPercentShort()) << "%  Trades: " << description->numTrades()
	    << "  CL: " << description->numConsecutiveLosses() << " }" << std::endl;
}

void printPattern (PALPatternPtr pattern)
{
  printPatternDescription(pattern->getPatternDescription());
}

void PriceActionLabSystem::initializeAndValidateTieBreaker(PatternTieBreakerPtr providedTieBreaker)
{
  if (!providedTieBreaker)
    {
      if (mUseTieBreaker)
	{
	  // User intended to use a tie-breaker, but provided null. Warn and use default.
	  std::cerr << "Warning: useTieBreaker is true, but provided tieBreaker is null. Using default SmallestVolatilityTieBreaker." << std::endl;
	  mPatternTieBreaker = std::make_shared<SmallestVolatilityTieBreaker>();
	}
      else
	{
	  // User did not intend to use tie-breaking logic OR didn't provide one.
	  // Assign a default one anyway for internal consistency, in case setUseTieBreaker(true) is called later.
	  // No warning needed here.
	  mPatternTieBreaker = std::make_shared<SmallestVolatilityTieBreaker>();
	}
    }
  else
    {
        // A valid tieBreaker was provided, use it.
        mPatternTieBreaker = providedTieBreaker;
    }
}

PriceActionLabSystem::PriceActionLabSystem (PALPatternPtr pattern, 
					    PatternTieBreakerPtr tieBreaker,
					    bool useTieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mAllPatterns(),
    mUseTieBreaker(useTieBreaker)
{
  initializeAndValidateTieBreaker(tieBreaker);
  addPattern (pattern);
}

PriceActionLabSystem::PriceActionLabSystem (PatternTieBreakerPtr tieBreaker, bool useTieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mAllPatterns(),
    mUseTieBreaker(useTieBreaker)
{
  initializeAndValidateTieBreaker(tieBreaker);
}

PriceActionLabSystem::PriceActionLabSystem ()
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mPatternTieBreaker (std::make_shared<SmallestVolatilityTieBreaker>()),
    mAllPatterns(),
    mUseTieBreaker(false)
{}

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

  std::cout << listOfPatterns.size() << " patterns in PAL IR file" << std::endl << std::endl;

  for (; it != listOfPatterns.end(); it++)
    {
      p = *it;
      addPattern (p);
    }
}

PriceActionLabSystem::~PriceActionLabSystem()
{}

unsigned long PriceActionLabSystem::getNumPatterns() const
{
  return mLongsPatternMap.size() + mShortsPatternMap.size();
}

unsigned long PriceActionLabSystem::getNumLongPatterns() const
{
  return mLongsPatternMap.size();
}

unsigned long PriceActionLabSystem::getNumShortPatterns() const
{
  return mShortsPatternMap.size();
}

void 
PriceActionLabSystem::addPattern (PALPatternPtr pattern)
{
  mAllPatterns.push_back(pattern);

  if (pattern->isLongPattern())
    return addLongPattern (pattern);
  else
    return addShortPattern (pattern);
}


PriceActionLabSystem::ConstPatternIterator PriceActionLabSystem::allPatternsBegin() const
{ return mAllPatterns.begin(); }

PriceActionLabSystem::ConstPatternIterator PriceActionLabSystem::allPatternsEnd() const
{ return mAllPatterns.end(); }

void PriceActionLabSystem::addPatternToMap(PALPatternPtr pattern,
                                           MapType& patternMap, // Pass map by reference
                                           const std::string& mapIdentifier) // For logging
{
  unsigned long long currentHashCode = pattern->hashCode();

  if (!mUseTieBreaker)
    {
      // Default behavior: Allow duplicates, simply insert
      patternMap.insert(std::make_pair(currentHashCode, pattern));
    }
  else
    {
      // Tie-breaker enabled: Keep only the "best" pattern per hash code
      auto range = patternMap.equal_range(currentHashCode); // Use the passed-in map

      if (range.first == range.second) {
	// No existing pattern with this hash code, insert the new one
	patternMap.insert(std::make_pair(currentHashCode, pattern));
      }
      else
	{
	  // Existing pattern(s) found, use tie-breaker against the first one found
	  std::cout << mapIdentifier << " (Tie-Breaker Active): Hash collision detected for code " << currentHashCode << std::endl;
	  std::cout << "  New Pattern:" << std::endl << "    ";
	  printPattern (pattern);
	  std::cout << "  Existing Pattern (representative):" << std::endl << "    ";
	  printPattern (range.first->second); // Compare against the first existing one

	  if (!mPatternTieBreaker) {
	    std::cerr << "Error: Tie-breaker is null during pattern addition when tie-breaking is enabled. Cannot proceed." << std::endl;
	    // Or potentially throw an exception
	    return; // Avoid dereferencing null pointer
	  }

	  PALPatternPtr patternToKeep =
	    mPatternTieBreaker->getTieBreakerPattern (pattern, range.first->second);

	  // Check if the new pattern won the tie-break
	  if (patternToKeep == pattern)
	    {
	      std::cout << "  => New pattern selected by tie-breaker (" << mapIdentifier << "). Replacing existing entry/entries." << std::endl;
	      // Erase all existing patterns with this hash code from the specific map
	      patternMap.erase(currentHashCode);
	      // Insert the new winning pattern into the specific map
	      patternMap.insert(std::make_pair(currentHashCode, patternToKeep));
	    }
	  else
	    {
	      std::cout << "  => Existing pattern kept by tie-breaker (" << mapIdentifier << "). Discarding new pattern." << std::endl;
	      // Do nothing, the existing pattern(s) remain, the new one is not added to the map
	    }
	}
    }
}

void 
PriceActionLabSystem::addLongPattern (PALPatternPtr pattern)
{
  addPatternToMap(pattern, mLongsPatternMap, "addLongPattern");
}

void 
PriceActionLabSystem::addShortPattern (PALPatternPtr pattern)
{
  addPatternToMap(pattern, mShortsPatternMap, "addShortPattern");
}

PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternLongsBegin() const
{
  return mLongsPatternMap.begin();
}

PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternLongsEnd() const
{
  return mLongsPatternMap.end();
}

PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternLongsBegin()
{
  return mLongsPatternMap.begin();
}

PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternLongsEnd()
{
  return mLongsPatternMap.end();
}

///////////////////////

PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternShortsBegin() const
{
  return mShortsPatternMap.begin();
}

PriceActionLabSystem::ConstSortedPatternIterator 
PriceActionLabSystem::patternShortsEnd() const
{
  return mShortsPatternMap.end();
}

PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternShortsBegin()
{
  return mShortsPatternMap.begin();
}

PriceActionLabSystem::SortedPatternIterator 
PriceActionLabSystem::patternShortsEnd()
{
  return mShortsPatternMap.end();
}



//////////////////////////////////
// class SmallestVolatilityTieBreaker
/////////////////////////////////////

PALPatternPtr
SmallestVolatilityTieBreaker::getTieBreakerPattern(PALPatternPtr pattern1, 
						   PALPatternPtr pattern2) const
{
  auto volRank = [](const PALPatternPtr &p){
        if (p->isLowVolatilityPattern())       return 0;
        if (p->isNormalVolatilityPattern())    return 1;
        if (p->isHighVolatilityPattern())      return 2;
        if (p->isVeryHighVolatilityPattern())  return 3;
        // everything else (including VOLATILITY_NONE) → highest rank
        return 4;
    };

    int r1 = volRank(pattern1);
    int r2 = volRank(pattern2);

    if (r1 < r2)
      return pattern1;

    if (r2 < r1)
      return pattern2;

    return pattern1;  // tie → pick the first
}



