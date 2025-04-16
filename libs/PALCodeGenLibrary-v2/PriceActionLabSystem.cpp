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

PriceActionLabSystem::PriceActionLabSystem (PALPatternPtr pattern, 
					    PatternTieBreakerPtr tieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mPatternTieBreaker (tieBreaker),
    mAllPatterns()
{
  addPattern (pattern);
}

PriceActionLabSystem::PriceActionLabSystem (PatternTieBreakerPtr tieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mPatternTieBreaker (tieBreaker),
    mAllPatterns()
{
}

PriceActionLabSystem::PriceActionLabSystem (std::list<PALPatternPtr>& listOfPatterns, 
					    PatternTieBreakerPtr tieBreaker)
  : mLongsPatternMap(),
    mShortsPatternMap(),
    mPatternTieBreaker (tieBreaker),
    mAllPatterns()
{
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

void 
PriceActionLabSystem::addLongPattern (PALPatternPtr pattern)
{
  PriceActionLabSystem::SortedPatternIterator it = mLongsPatternMap.find (pattern->hashCode());
  if (it == patternLongsEnd())
      mLongsPatternMap.insert(std::make_pair(pattern->hashCode(), pattern));
  else
    {
      std::cout << "addLongPattern: equivalent hash codes found: " << pattern->hashCode() << " and " << it->second->hashCode() << std::endl;
      std::cout << "Pattern 1:" << std::endl;
      printPattern (pattern);
      std::cout << "Pattern 2:" << std::endl;
      printPattern (it->second);

      // We don't want the same pattern with different risk reward being generated
      // so use the tiebreaker
      PALPatternPtr patternToKeep = 
	mPatternTieBreaker->getTieBreakerPattern (pattern, it->second);

      mLongsPatternMap.erase (it);
      mLongsPatternMap.insert(std::make_pair(patternToKeep->hashCode(), patternToKeep));
    }
}

void 
PriceActionLabSystem::addShortPattern (PALPatternPtr pattern)
{
  PriceActionLabSystem::SortedPatternIterator it = mShortsPatternMap.find (pattern->hashCode());
  if (it == patternShortsEnd())
      mShortsPatternMap.insert(std::make_pair(pattern->hashCode(), pattern));
  else
    {
      std::cout << "addShortPattern: equivalent hash codes found: " << pattern->hashCode() << " and " << it->second->hashCode() << std::endl;
 

      // We don't want the same pattern with different risk reward being generated
      // so use the tiebreaker
      PALPatternPtr patternToKeep = 
	mPatternTieBreaker->getTieBreakerPattern (pattern, it->second);

      mShortsPatternMap.erase (it);
      mShortsPatternMap.insert(std::make_pair(patternToKeep->hashCode(), patternToKeep));
    }
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
  decimal7 stop1 = pattern1->getStopLossAsDecimal();
  decimal7 stop2 = pattern2->getStopLossAsDecimal();
  decimal7 target1 = pattern1->getProfitTargetAsDecimal();
  decimal7 target2 = pattern2->getProfitTargetAsDecimal();

  if ((stop1 == stop2) && (target1 == target2))
    {
      //      std::cout << "getTiebreakerPattern: stop and profit target are equal" << std::endl;
      return pattern1;
    }

  if (stop1 != stop2)
    {
      if (stop1 < stop2)
	return pattern1;
      else if (stop1 > stop2)
	return pattern2;
    }
  else if (target1 != target2)
    {
      if (target1 < target2)
	return pattern1;
      else if (target1 > target2)
	return pattern2;
    }

  return pattern1;
}



