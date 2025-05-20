#ifndef PALAST_H
#define PALAST_H

#include <memory>
#include <string>
#include <map>
#include <list>
#include <fstream>
#include <algorithm>
#include <exception>
#include "number.h"

using decimal7 = num::DefaultNumber;

typedef std::shared_ptr<decimal7> DecimalPtr;

using std::shared_ptr;

class PalCodeGenVisitor;
class TSApiBackTest;
class PalFileResults;

class PriceBarReference
{
public:
  enum ReferenceType {OPEN, HIGH, LOW, CLOSE, VOLUME, ROC1, MEANDER, VCHARTLOW, VCHARTHIGH,
		      IBS1, IBS2, IBS3, MOMERSIONFILTER};
  
  PriceBarReference (unsigned int barOffset);
  virtual ~PriceBarReference();
  PriceBarReference (const PriceBarReference& rhs);
  PriceBarReference& operator=(const PriceBarReference &rhs);

  unsigned int getBarOffset () const;
  virtual void accept (PalCodeGenVisitor &v) = 0;
  virtual unsigned long long hashCode() = 0;
  virtual PriceBarReference::ReferenceType getReferenceType() = 0;
  virtual int extraBarsNeeded() const = 0;
  
private:
  unsigned int mBarOffset;
};

class PriceBarOpen : public PriceBarReference
{
public:
  PriceBarOpen(unsigned int barOffset);
  PriceBarOpen (const PriceBarOpen& rhs);
  PriceBarOpen& operator=(const PriceBarOpen &rhs);
  ~PriceBarOpen();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long mComputedHash;
};

class PriceBarHigh : public PriceBarReference
{
public:
  PriceBarHigh(unsigned int barOffset);
  PriceBarHigh (const PriceBarHigh& rhs);
  PriceBarHigh& operator=(const PriceBarHigh &rhs);
  ~PriceBarHigh();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
    
private:
    unsigned long long mComputedHash;
};

class PriceBarLow : public PriceBarReference
{
public:
  PriceBarLow(unsigned int barOffset);
  ~PriceBarLow();
  PriceBarLow (const PriceBarLow& rhs);
  PriceBarLow& operator=(const PriceBarLow &rhs);
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class PriceBarClose : public PriceBarReference
{
public:
  PriceBarClose(unsigned int barOffset);
  PriceBarClose (const PriceBarClose& rhs);
  PriceBarClose& operator=(const PriceBarClose &rhs);
  ~PriceBarClose();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class VolumeBarReference : public PriceBarReference
{
public:
  VolumeBarReference(unsigned int barOffset);
  VolumeBarReference (const VolumeBarReference& rhs);
  VolumeBarReference& operator=(const VolumeBarReference &rhs);
  ~VolumeBarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
    
private:
  unsigned long long mComputedHash;
};

class Roc1BarReference : public PriceBarReference
{
public:
  Roc1BarReference(unsigned int barOffset);
  Roc1BarReference (const Roc1BarReference& rhs);
  Roc1BarReference& operator=(const Roc1BarReference &rhs);
  ~Roc1BarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class IBS1BarReference : public PriceBarReference
{
public:
  IBS1BarReference(unsigned int barOffset);
  IBS1BarReference (const IBS1BarReference& rhs);
  IBS1BarReference& operator=(const IBS1BarReference &rhs);
  ~IBS1BarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class IBS2BarReference : public PriceBarReference
{
public:
  IBS2BarReference(unsigned int barOffset);
  IBS2BarReference (const IBS2BarReference& rhs);
  IBS2BarReference& operator=(const IBS2BarReference &rhs);
  ~IBS2BarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class IBS3BarReference : public PriceBarReference
{
public:
  IBS3BarReference(unsigned int barOffset);
  IBS3BarReference (const IBS3BarReference& rhs);
  IBS3BarReference& operator=(const IBS3BarReference &rhs);
  ~IBS3BarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class MeanderBarReference : public PriceBarReference
{
public:
  MeanderBarReference(unsigned int barOffset);
  MeanderBarReference (const MeanderBarReference& rhs);
  MeanderBarReference& operator=(const MeanderBarReference &rhs);
  ~MeanderBarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class VChartHighBarReference : public PriceBarReference
{
public:
  VChartHighBarReference(unsigned int barOffset);
  VChartHighBarReference (const VChartHighBarReference& rhs);
  VChartHighBarReference& operator=(const VChartHighBarReference &rhs);
  ~VChartHighBarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

class VChartLowBarReference : public PriceBarReference
{
public:
  VChartLowBarReference(unsigned int barOffset);
  VChartLowBarReference (const VChartLowBarReference& rhs);
  VChartLowBarReference& operator=(const VChartLowBarReference &rhs);
  ~VChartLowBarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  
private:
  unsigned long long mComputedHash;
};

/*
class MomersionFilterBarReference : public PriceBarReference
{
public:
  MomersionFilterBarReference(unsigned int barOffset, unsigned int period);
  MomersionFilterBarReference (const MomersionFilterBarReference& rhs);
  MomersionFilterBarReference& operator=(const MomersionFilterBarReference &rhs);
  ~MomersionFilterBarReference();
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();
  PriceBarReference::ReferenceType getReferenceType();
  int extraBarsNeeded() const;
  unsigned int getMomersionPeriod() const;
  
private:
  unsigned long long mComputedHash;
  unsigned int mPeriod;
};
*/

typedef std::shared_ptr<PriceBarReference> PriceBarPtr;

//////////////

class PatternExpression : public std::enable_shared_from_this<PatternExpression> {
public:
  PatternExpression();
  PatternExpression (const PatternExpression& rhs);
  PatternExpression& operator=(const PatternExpression &rhs);
  virtual ~PatternExpression();
  virtual void accept (PalCodeGenVisitor &v) = 0;
  virtual unsigned long long hashCode() = 0;
};

typedef std::shared_ptr<PatternExpression> PatternExpressionPtr;

class GreaterThanExpr : public PatternExpression
{
public:
  GreaterThanExpr (PriceBarReference *lhs, PriceBarReference *rhs);
  GreaterThanExpr (const GreaterThanExpr& rhs);
  GreaterThanExpr& operator=(const GreaterThanExpr &rhs);
  ~GreaterThanExpr();

  PriceBarReference * getLHS() const;
  PriceBarReference * getRHS() const;
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();

private:
  PriceBarReference *mLhs;
  PriceBarReference *mRhs;
};

class AndExpr : public PatternExpression
{
public:
  AndExpr(PatternExpressionPtr lhs, PatternExpressionPtr rhs);
  AndExpr (PatternExpression *lhs, PatternExpression *rhs);
  AndExpr (const AndExpr& rhs);
  AndExpr& operator=(const AndExpr &rhs);
  ~AndExpr();

  PatternExpression *getLHS() const;
  PatternExpression *getRHS() const;
  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();

 private:
  PatternExpressionPtr mLeftHandSide;
  PatternExpressionPtr mRightHandSide;
};



//////////////////////

class ProfitTargetInPercentExpression
{
public:
  ProfitTargetInPercentExpression(decimal7 *profitTarget);
  ProfitTargetInPercentExpression (const ProfitTargetInPercentExpression& rhs);
  ProfitTargetInPercentExpression& operator=(const ProfitTargetInPercentExpression &rhs);
  virtual ~ProfitTargetInPercentExpression() = 0;
  decimal7 *getProfitTarget() const;

  virtual void accept (PalCodeGenVisitor &v) = 0;
  unsigned long long hashCode();
  virtual bool isLongSideProfitTarget() const = 0;
  virtual bool isShortSideProfitTarget() const = 0;

 private:
  decimal7 *mProfitTarget;   // Owned by the factory and shared
  unsigned long long mComputedHash;
};


class LongSideProfitTargetInPercent : public ProfitTargetInPercentExpression
{
public:
  LongSideProfitTargetInPercent (decimal7 *profitTarget);
  LongSideProfitTargetInPercent (const LongSideProfitTargetInPercent& rhs);
  LongSideProfitTargetInPercent& operator=(const LongSideProfitTargetInPercent &rhs);
  ~LongSideProfitTargetInPercent();

  void accept (PalCodeGenVisitor &v);

  bool isLongSideProfitTarget() const
  {
    return true;
  }

  bool isShortSideProfitTarget() const
  {
    return false;
  }
};

class ShortSideProfitTargetInPercent : public ProfitTargetInPercentExpression
{
public:
  ShortSideProfitTargetInPercent (decimal7 *profitTarget);
  ShortSideProfitTargetInPercent (const ShortSideProfitTargetInPercent& rhs);
  ShortSideProfitTargetInPercent& operator=(const ShortSideProfitTargetInPercent &rhs);
  ~ShortSideProfitTargetInPercent();

  void accept (PalCodeGenVisitor &v);

  bool isLongSideProfitTarget() const
  {
    return false;
  }

  bool isShortSideProfitTarget() const
  {
    return true;
  }
};

//typedef std::shared_ptr<ProfitTargetInPercentExpression> ProfitTargetInPercentPtr;
typedef ProfitTargetInPercentExpression* ProfitTargetInPercentPtr;

////////////////////////////////

class StopLossInPercentExpression
{
public:
  StopLossInPercentExpression(decimal7 *stopLoss);
  StopLossInPercentExpression (const StopLossInPercentExpression& rhs);
  StopLossInPercentExpression& operator=(const StopLossInPercentExpression &rhs);
  virtual ~StopLossInPercentExpression();
  decimal7 *getStopLoss() const;
  unsigned long long hashCode();
  virtual void accept (PalCodeGenVisitor &v) = 0;
  virtual bool isLongSideStopLoss() const = 0;
  virtual bool isShortSideStopLoss() const = 0;

 private:
  decimal7 *mStopLoss;
  unsigned long long mComputedHash;
};


class LongSideStopLossInPercent : public StopLossInPercentExpression
{
public:
  LongSideStopLossInPercent (decimal7 *stopLoss);
  ~LongSideStopLossInPercent();
  LongSideStopLossInPercent& operator=(const LongSideStopLossInPercent &rhs);
  LongSideStopLossInPercent (const LongSideStopLossInPercent& rhs);
  void accept (PalCodeGenVisitor &v);

  bool isLongSideStopLoss() const
  {
    return true;
  }

  bool isShortSideStopLoss() const
  {
    return false;
  }
};

class ShortSideStopLossInPercent : public StopLossInPercentExpression
{
public:
  ShortSideStopLossInPercent (decimal7 *stopLoss);
  ~ShortSideStopLossInPercent();
  void accept (PalCodeGenVisitor &v);

  ShortSideStopLossInPercent& operator=(const ShortSideStopLossInPercent &rhs);
  ShortSideStopLossInPercent (const ShortSideStopLossInPercent& rhs);

  bool isLongSideStopLoss() const
  {
    return false;
  }

  bool isShortSideStopLoss() const
  {
    return true;
  }
};

//typedef std::shared_ptr<StopLossInPercentExpression> StopLossInPercentPtr;
typedef StopLossInPercentExpression* StopLossInPercentPtr;

/////////////////////

class MarketEntryExpression
{
public:
  MarketEntryExpression();
  virtual ~MarketEntryExpression();
  MarketEntryExpression& operator=(const MarketEntryExpression &rhs);
  MarketEntryExpression (const MarketEntryExpression& rhs);
  virtual void accept (PalCodeGenVisitor &v) = 0;
  virtual bool isLongPattern() const = 0;
  virtual bool isShortPattern() const = 0;
  virtual unsigned long long hashCode() = 0;
};

class MarketEntryOnOpen : public MarketEntryExpression
{
public:
  MarketEntryOnOpen();
  virtual ~MarketEntryOnOpen() = 0;
  MarketEntryOnOpen& operator=(const MarketEntryOnOpen &rhs);
  MarketEntryOnOpen (const MarketEntryOnOpen& rhs);
};

class LongMarketEntryOnOpen : public MarketEntryOnOpen
{
public:
  LongMarketEntryOnOpen();
  ~LongMarketEntryOnOpen();
  LongMarketEntryOnOpen& operator=(const LongMarketEntryOnOpen &rhs);
  LongMarketEntryOnOpen (const LongMarketEntryOnOpen& rhs);
  void accept (PalCodeGenVisitor &v);
  bool isLongPattern() const
  { return true; }
  bool isShortPattern() const
  { return false; }
  unsigned long long hashCode();
};

class ShortMarketEntryOnOpen : public MarketEntryOnOpen
{
public:
  ShortMarketEntryOnOpen();
  ~ShortMarketEntryOnOpen();

  ShortMarketEntryOnOpen& operator=(const ShortMarketEntryOnOpen &rhs);
  ShortMarketEntryOnOpen (const ShortMarketEntryOnOpen& rhs);
  void accept (PalCodeGenVisitor &v);

  bool isLongPattern() const
  { return false; }
  bool isShortPattern() const
  { return true; }
  unsigned long long hashCode();
};

typedef std::shared_ptr<MarketEntryExpression> MarketEntryPtr;

//////////////////////////////

class PatternDescription : public std::enable_shared_from_this<PatternDescription>
{
public:
  PatternDescription(const char *fileName, unsigned int patternIndex,
		     unsigned long indexDate, decimal7* percentLong, decimal7* percentShort,
		     unsigned int numTrades, unsigned int consecutiveLosses);
  PatternDescription (const PatternDescription& rhs);
  PatternDescription& operator=(const PatternDescription &rhs);
  ~PatternDescription();

  const std::string& getFileName() const;
  unsigned int getpatternIndex() const;
  unsigned int getIndexDate() const;
  decimal7* getPercentLong() const;
  decimal7* getPercentShort() const;
  unsigned int numTrades() const;
  unsigned int numConsecutiveLosses() const;

  void accept (PalCodeGenVisitor &v);
  unsigned long long hashCode();

private:
  std::string mFileName;
  unsigned int mPatternIndex;
  unsigned long mIndexDate;
  decimal7* mPercentLong;
  decimal7* mPercentShort;
  unsigned int mNumTrades;
  unsigned int mConsecutiveLosses;
  unsigned long long mComputedHash;
};

typedef std::shared_ptr<PatternDescription> PatternDescriptionPtr;

//////////////////////////////////////

class PalPatternMaxBars
{
public:
    static unsigned int evaluateExpression (PatternExpression *expression)
    {
      if (AndExpr *pAnd = dynamic_cast<AndExpr*>(expression))
	{
	  unsigned int lhsBars = PalPatternMaxBars::evaluateExpression (pAnd->getLHS());
	  unsigned int rhsBars = PalPatternMaxBars::evaluateExpression (pAnd->getRHS());

	  return std::max (lhsBars, rhsBars);
	}
      else if (GreaterThanExpr *pGreaterThan = dynamic_cast<GreaterThanExpr*>(expression))
	{
	  unsigned int lhs = pGreaterThan->getLHS()->getBarOffset () + pGreaterThan->getLHS()->extraBarsNeeded();
	  unsigned int rhs = pGreaterThan->getRHS()->getBarOffset () + pGreaterThan->getRHS()->extraBarsNeeded();;

	  return std::max (lhs, rhs);
	}
      else
	throw std::domain_error ("Unknown derived class of PatternExpression");
    }

};


class PriceActionLabPattern : public std::enable_shared_from_this<PriceActionLabPattern>
{
 public:
  enum PortfolioAttribute {PORTFOLIO_FILTER_LONG, PORTFOLIO_FILTER_SHORT, PORTFOLIO_FILTER_NONE};
  enum VolatilityAttribute {VOLATILITY_VERY_HIGH, VOLATILITY_HIGH, VOLATILITY_LOW, VOLATILITY_NORMAL, VOLATILITY_NONE};

public:
  PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern, 
			 MarketEntryExpression* entry, 
			 ProfitTargetInPercentExpression* profitTarget, 
			 StopLossInPercentExpression* stopLoss);

  PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern, 
			 MarketEntryExpression* entry, 
			 ProfitTargetInPercentExpression* profitTarget, 
			 StopLossInPercentExpression* stopLoss, 
			 VolatilityAttribute volatilityAttribute,
			 PortfolioAttribute portfolioAttribute);

  PriceActionLabPattern (PatternDescriptionPtr description, 
			 PatternExpressionPtr pattern,
			 MarketEntryExpression* entry, 
			 ProfitTargetInPercentExpression* profitTarget, 
			 StopLossInPercentExpression* stopLoss);

  // convenience overload to accept shared_ptr *and* attributes
  PriceActionLabPattern(PatternDescriptionPtr description,
                        PatternExpressionPtr pattern,
                        MarketEntryExpression* entry,
                        ProfitTargetInPercentExpression* profitTarget,
                        StopLossInPercentExpression* stopLoss,
                        VolatilityAttribute volatilityAttribute,
                        PortfolioAttribute portfolioAttribute);
  
  PriceActionLabPattern (const PriceActionLabPattern& rhs);
  PriceActionLabPattern& operator=(const  PriceActionLabPattern &rhs);
  ~PriceActionLabPattern();

  shared_ptr<PriceActionLabPattern> clone (ProfitTargetInPercentExpression* profitTarget, 
					   StopLossInPercentExpression* stopLoss);

  const std::string& getFileName() const;
  const std::string getBaseFileName() const;

  unsigned int getpatternIndex() const;
  unsigned int getIndexDate() const;

  PatternExpressionPtr getPatternExpression() const;
  MarketEntryExpression* getMarketEntry() const;
  ProfitTargetInPercentPtr getProfitTarget() const;
  decimal7 getProfitTargetAsDecimal() const;
  StopLossInPercentPtr getStopLoss() const;
  decimal7 getStopLossAsDecimal() const;
  PatternDescriptionPtr getPatternDescription() const;

  unsigned int getMaxBarsBack() const
  {
    return mMaxBarsBack;
  }

  decimal7 getPayoffRatio() const
  {
    return mPayOffRatio;
  }

  void accept (PalCodeGenVisitor &v);
  bool isLongPattern() const
  { return mEntry->isLongPattern(); }
  bool isShortPattern() const
  { return mEntry->isShortPattern(); }
  unsigned long long hashCode();
  bool hasVolatilityAttribute() const;
  bool isLowVolatilityPattern() const;
  bool isNormalVolatilityPattern() const;
  bool isHighVolatilityPattern() const;
  bool isVeryHighVolatilityPattern() const;

  bool hasPortfolioAttribute() const;
  bool isFilteredLongPattern() const;
  bool isFilteredShortPattern() const;
private:
    unsigned long long getStringHash (const std::string& key);

private:
  PatternExpressionPtr mPattern;
  MarketEntryExpression* mEntry;
  ProfitTargetInPercentExpression *mProfitTarget;
  StopLossInPercentExpression *mStopLoss;
  PatternDescriptionPtr mPatternDescription;
  static std::map<std::string, unsigned long long> mCachedStringHashMap;
  VolatilityAttribute mVolatilityAttribute;
  PortfolioAttribute  mPortfolioAttribute;
  unsigned int mMaxBarsBack;
  decimal7 mPayOffRatio;
};

typedef std::shared_ptr<PriceActionLabPattern> PALPatternPtr;

// When we have the smae pattern with different reward to risk or
// different volatilities (1 standard deviation version two standard deviation)
// volatility we need to chose which pattern to use

class PatternTieBreaker
{
public:
  PatternTieBreaker()
  {}

  virtual ~PatternTieBreaker()
  {}

  virtual PALPatternPtr getTieBreakerPattern(PALPatternPtr pattern1, 
					     PALPatternPtr pattern2) const = 0;
};

class SmallestVolatilityTieBreaker : public PatternTieBreaker
{
public:
  SmallestVolatilityTieBreaker() : PatternTieBreaker()
  {}

  ~SmallestVolatilityTieBreaker()
  {}

  PALPatternPtr getTieBreakerPattern(PALPatternPtr pattern1, 
				     PALPatternPtr pattern2) const;
};

typedef std::shared_ptr<PatternTieBreaker> PatternTieBreakerPtr;

class PriceActionLabSystem
{
 private:
  typedef std::multimap<unsigned long long, PALPatternPtr> MapType;

  public:
  typedef std::list<PALPatternPtr>::const_iterator ConstPatternIterator;
  typedef MapType::iterator SortedPatternIterator;
  typedef MapType::const_iterator ConstSortedPatternIterator;

  PriceActionLabSystem (PALPatternPtr pattern, 
			PatternTieBreakerPtr tieBreaker,
			bool useTieBreaker);
  PriceActionLabSystem (PatternTieBreakerPtr tieBreaker,
			bool useTieBreaker = false);
  PriceActionLabSystem (std::list<PALPatternPtr>& listOfPatterns, 
			PatternTieBreakerPtr tieBreaker,
			bool useTieBreaker = false);
  PriceActionLabSystem();
  ~PriceActionLabSystem();
  ConstSortedPatternIterator patternLongsBegin() const;
  ConstSortedPatternIterator patternLongsEnd() const;

  SortedPatternIterator patternLongsBegin();
  SortedPatternIterator patternLongsEnd();


  ConstSortedPatternIterator patternShortsBegin() const;
  ConstSortedPatternIterator patternShortsEnd() const;

  SortedPatternIterator patternShortsBegin();
  SortedPatternIterator patternShortsEnd();

  ConstPatternIterator allPatternsBegin() const;
  ConstPatternIterator allPatternsEnd() const;

  void addPattern (PALPatternPtr pattern);
  unsigned long getNumPatterns() const;
  unsigned long getNumLongPatterns() const;
  unsigned long getNumShortPatterns() const;

private:
  void addLongPattern (PALPatternPtr pattern);
  void addShortPattern (PALPatternPtr pattern);
  void initializeAndValidateTieBreaker(PatternTieBreakerPtr providedTieBreaker);
  void addPatternToMap(PALPatternPtr pattern,
		       MapType& patternMap, // Pass map by reference
		       const std::string& mapIdentifier); // For logging


private:
  MapType mLongsPatternMap;
  MapType mShortsPatternMap;
  PatternTieBreakerPtr mPatternTieBreaker;
  std::list<PALPatternPtr> mAllPatterns;
  bool mUseTieBreaker;
};

///////////////////////////////



class AstFactory
{
public:
  static const int MaxNumBarOffsets = 15;
  
  AstFactory();
  ~AstFactory();

  PriceBarReference* getPriceOpen (unsigned int barOffset);
  PriceBarReference* getPriceHigh (unsigned int barOffset);
  PriceBarReference* getPriceLow (unsigned int barOffset);
  PriceBarReference* getPriceClose (unsigned int barOffset);
  PriceBarReference* getVolume (unsigned int barOffset);
  PriceBarReference* getRoc1 (unsigned int barOffset);
  PriceBarReference* getIBS1 (unsigned int barOffset);
  PriceBarReference* getIBS2 (unsigned int barOffset);
  PriceBarReference* getIBS3 (unsigned int barOffset);
  PriceBarReference* getMeander (unsigned int barOffset);
  PriceBarReference* getVChartLow (unsigned int barOffset);
  PriceBarReference* getVChartHigh (unsigned int barOffset);
  MarketEntryExpression* getLongMarketEntryOnOpen();
  MarketEntryExpression* getShortMarketEntryOnOpen();
  decimal7 *getDecimalNumber (char *numString);
  decimal7 *getDecimalNumber (int num);
  LongSideProfitTargetInPercent *getLongProfitTarget (decimal7 *profitTarget);
  ShortSideProfitTargetInPercent *getShortProfitTarget (decimal7 *profitTarget);
  LongSideStopLossInPercent *getLongStopLoss(decimal7 *stopLoss);
  ShortSideStopLossInPercent *getShortStopLoss(decimal7 *stopLoss);

private:
  void initializePriceBars();

private:


  PriceBarReference* mPredefinedPriceOpen[MaxNumBarOffsets];
  PriceBarReference* mPredefinedPriceHigh[MaxNumBarOffsets];
  PriceBarReference* mPredefinedPriceLow[MaxNumBarOffsets];
  PriceBarReference* mPredefinedPriceClose[MaxNumBarOffsets];
  PriceBarReference* mPredefinedVolume[MaxNumBarOffsets];
  PriceBarReference* mPredefinedRoc1[MaxNumBarOffsets];
  PriceBarReference* mPredefinedIBS1[MaxNumBarOffsets];
  PriceBarReference* mPredefinedIBS2[MaxNumBarOffsets];
  PriceBarReference* mPredefinedIBS3[MaxNumBarOffsets];
  PriceBarReference* mPredefinedMeander[MaxNumBarOffsets];
  PriceBarReference* mPredefinedVChartLow[MaxNumBarOffsets];
  PriceBarReference* mPredefinedVChartHigh[MaxNumBarOffsets];
  MarketEntryExpression* mLongEntryOnOpen;
  MarketEntryExpression* mShortEntryOnOpen;
  std::map<std::string, DecimalPtr> mDecimalNumMap;
  std::map<int, DecimalPtr> mDecimalNumMap2;
  std::map<decimal7, std::shared_ptr<LongSideProfitTargetInPercent>> mLongsProfitTargets;
  std::map<decimal7, std::shared_ptr<ShortSideProfitTargetInPercent>> mShortsProfitTargets;
  std::map<decimal7, std::shared_ptr<LongSideStopLossInPercent>> mLongsStopLoss;
  std::map<decimal7, std::shared_ptr<ShortSideStopLossInPercent>> mShortsStopLoss;
};



#endif
