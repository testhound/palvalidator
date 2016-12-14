#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <stdio.h>

const int AstFactory::MaxNumBarOffsets;

unsigned long long hash_str(const char* s);

unsigned long long hash_str(const char* s)
{
   unsigned long long h = 31 /* also prime */;
   while (*s) {
     h = (h * 54059) ^ (s[0] * 76963);
     s++;
   }
   return h; // or return h % C;
}

std::string GetBaseFilename(const char *filename)
{
    std::string fName(filename);
    size_t pos = fName.rfind(".");
    if(pos == std::string::npos)  //No extension.
        return fName;

    if(pos == 0)    //. is at the front. Not an extension.
        return fName;

    return fName.substr(0, pos);
}


PriceBarReference::PriceBarReference(unsigned int barOffset) : mBarOffset(barOffset)
{}



PriceBarReference::~PriceBarReference()
{}

PriceBarReference::PriceBarReference (const PriceBarReference& rhs) 
  : mBarOffset (rhs.mBarOffset) 
{}

PriceBarReference& 
PriceBarReference::operator=(const PriceBarReference &rhs)
{
  if (this == &rhs)
    return *this;

  mBarOffset = rhs.mBarOffset;
  return *this;
}

unsigned int
PriceBarReference::getBarOffset() const
{
  return mBarOffset;
}


//////////

PriceBarOpen :: PriceBarOpen(unsigned int barOffset) : 
  PriceBarReference(barOffset),
  mComputedHash(0)
{}

PriceBarOpen::PriceBarOpen (const PriceBarOpen& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

PriceBarOpen& 
PriceBarOpen::operator=(const PriceBarOpen &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}


PriceBarOpen :: ~PriceBarOpen()
{}


void PriceBarOpen::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long PriceBarOpen::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 17;
      result = 53 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType PriceBarOpen::getReferenceType()
{
  return PriceBarReference::OPEN;
}

int PriceBarOpen::extraBarsNeeded() const
{
  return 0;
}

////////////////

PriceBarHigh::PriceBarHigh(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash(0)
{}

PriceBarHigh::~PriceBarHigh()
{}

PriceBarHigh::PriceBarHigh (const PriceBarHigh& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

PriceBarHigh& 
PriceBarHigh::operator=(const PriceBarHigh &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

void PriceBarHigh::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long PriceBarHigh::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 19;
      result = 59 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType PriceBarHigh::getReferenceType()
{
  return PriceBarReference::HIGH;
}

int PriceBarHigh::extraBarsNeeded() const
{
  return 0;
}


////////////////////

PriceBarLow :: PriceBarLow(unsigned int barOffset) : 
  PriceBarReference(barOffset),
  mComputedHash (0)
{}

PriceBarLow::PriceBarLow (const PriceBarLow& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

PriceBarLow& 
PriceBarLow::operator=(const PriceBarLow &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

PriceBarLow :: ~PriceBarLow()
{}

void PriceBarLow::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long PriceBarLow::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 23;
      result = 61 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType PriceBarLow::getReferenceType()
{
  return PriceBarReference::LOW;
}

int PriceBarLow::extraBarsNeeded() const
{
  return 0;
}

/////////////////////


PriceBarClose::PriceBarClose(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0)
{}

PriceBarClose::PriceBarClose (const PriceBarClose& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

PriceBarClose& 
PriceBarClose::operator=(const PriceBarClose &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

PriceBarClose::~PriceBarClose()
{}

void 
PriceBarClose::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long PriceBarClose::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 29;
      result = 67 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType PriceBarClose::getReferenceType()
{
  return PriceBarReference::CLOSE;
}

int PriceBarClose::extraBarsNeeded() const
{
  return 0;
}

//////////////////////////
// Volume

VolumeBarReference::VolumeBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0)
{}

VolumeBarReference::VolumeBarReference (const VolumeBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

VolumeBarReference& 
VolumeBarReference::operator=(const VolumeBarReference &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

VolumeBarReference::~VolumeBarReference()
{}

void 
VolumeBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long VolumeBarReference::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 37;
      result = 73 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType VolumeBarReference::getReferenceType()
{
  return PriceBarReference::VOLUME;
}

int VolumeBarReference::extraBarsNeeded() const
{
  return 0;
}


//////////////////////////
////ROC1


Roc1BarReference::Roc1BarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0)
{}

Roc1BarReference::Roc1BarReference (const Roc1BarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

Roc1BarReference& 
Roc1BarReference::operator=(const Roc1BarReference &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

Roc1BarReference::~Roc1BarReference()
{}

void 
Roc1BarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long Roc1BarReference::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 41;
      result = 79 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType Roc1BarReference::getReferenceType()
{
  return PriceBarReference::ROC1;
}

int Roc1BarReference::extraBarsNeeded() const
{
  return 1;
}

//////////////////////////

//////////////////////////
//// Meander


MeanderBarReference::MeanderBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0)
{}

MeanderBarReference::MeanderBarReference (const MeanderBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

MeanderBarReference& 
MeanderBarReference::operator=(const MeanderBarReference &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

MeanderBarReference::~MeanderBarReference()
{}

void 
MeanderBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long MeanderBarReference::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 43;
      result = 83 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType MeanderBarReference::getReferenceType()
{
  return PriceBarReference::MEANDER;
}

int MeanderBarReference::extraBarsNeeded() const
{
  return 5;
}

/////////////////////////
//// VChartLow
/////////////////////////

VChartLowBarReference::VChartLowBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0)
{}

VChartLowBarReference::VChartLowBarReference (const VChartLowBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

VChartLowBarReference& 
VChartLowBarReference::operator=(const VChartLowBarReference &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

VChartLowBarReference::~VChartLowBarReference()
{}

void 
VChartLowBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long VChartLowBarReference::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 47;
      result = 89 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType VChartLowBarReference::getReferenceType()
{
  return PriceBarReference::VCHARTLOW;
}

int VChartLowBarReference::extraBarsNeeded() const
{
  return 6;
}

/////////////////////////

/////////////////////////
//// VChartHigh
/////////////////////////

VChartHighBarReference::VChartHighBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0)
{}

VChartHighBarReference::VChartHighBarReference (const VChartHighBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash)
{}

VChartHighBarReference& 
VChartHighBarReference::operator=(const VChartHighBarReference &rhs)
{
  if (this == &rhs)
    return *this;

  PriceBarReference::operator=(rhs);
  mComputedHash = rhs.mComputedHash;
  return *this;
}

VChartHighBarReference::~VChartHighBarReference()
{}

void 
VChartHighBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long VChartHighBarReference::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 53;
      result = 97 * result + getBarOffset();
      mComputedHash = result;
    }

  return result;
}

PriceBarReference::ReferenceType VChartHighBarReference::getReferenceType()
{
  return PriceBarReference::VCHARTHIGH;
}

int VChartHighBarReference::extraBarsNeeded() const
{
  return 6;
}

PatternExpression::PatternExpression()
{}

PatternExpression::PatternExpression (const PatternExpression& rhs)
{}

PatternExpression& PatternExpression::operator=(const PatternExpression &rhs)
{
  if (this == &rhs)
    return *this;

  return *this;
}

PatternExpression::~PatternExpression()
{}

/////////////

GreaterThanExpr::GreaterThanExpr (PriceBarReference *lhs, PriceBarReference *rhs)
  : PatternExpression(),
    mLhs(lhs),
    mRhs(rhs)
{}

GreaterThanExpr::GreaterThanExpr (const GreaterThanExpr& rhs)
  : PatternExpression(rhs),
  mLhs(rhs.mLhs),
  mRhs(rhs.mRhs)
{}

GreaterThanExpr& 
GreaterThanExpr::operator=(const GreaterThanExpr &rhs)
{
  if (this == &rhs)
    return *this;

  PatternExpression::operator=(rhs);
  mLhs = rhs.mLhs;
  mRhs = rhs.mRhs;
  return *this;
}

// Don't delete PriceBarReference objects as there are shared and owned by AstFactory
GreaterThanExpr::~GreaterThanExpr()
{}

PriceBarReference *GreaterThanExpr::getLHS() const
{
  return mLhs;
}

PriceBarReference *GreaterThanExpr::getRHS() const
{
  return mRhs;
}

void 
GreaterThanExpr::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long GreaterThanExpr::hashCode()
{
  unsigned long long result;

  result = 37;
  result = 71 * result + getRHS()->hashCode();
  result = 71 * result + getLHS()->hashCode();

  return result;
}

//////////////////////
// Class AndExpr
////////////////////////

AndExpr::AndExpr (PatternExpression *lhs, PatternExpression *rhs)
  : mLeftHandSide (lhs),
    mRightHandSide (rhs)
{}

AndExpr::AndExpr (const AndExpr& rhs)
  : PatternExpression(rhs),
  mLeftHandSide(rhs.mLeftHandSide),
  mRightHandSide(rhs.mRightHandSide)
{}

AndExpr& 
AndExpr::operator=(const AndExpr &rhs)
{
  if (this == &rhs)
    return *this;

  PatternExpression::operator=(rhs);
  mLeftHandSide = rhs.mLeftHandSide;
  mRightHandSide = rhs.mRightHandSide;
  return *this;
}

AndExpr::~AndExpr()
{}

PatternExpression* 
AndExpr::getLHS() const
{
  return mLeftHandSide.get();
}


PatternExpression* 
AndExpr::getRHS() const
{
  return mRightHandSide.get();
}

void 
AndExpr::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long AndExpr::hashCode()
{
  unsigned long long result;

  result = 41;
  result = 79 * result + getRHS()->hashCode();
  result = 79 * result + getLHS()->hashCode();

  return result;
}

////////////////////////////////////////
// class ProfitTargetInPercentExpression
///////////////////////////////////////

ProfitTargetInPercentExpression::ProfitTargetInPercentExpression(decimal7 *profitTarget)
  : mProfitTarget (profitTarget),
    mComputedHash(0)
{}

ProfitTargetInPercentExpression::ProfitTargetInPercentExpression (const ProfitTargetInPercentExpression& rhs) 
  : mProfitTarget (rhs.mProfitTarget),
    mComputedHash (rhs.mComputedHash)
{}

ProfitTargetInPercentExpression& 
ProfitTargetInPercentExpression::operator=(const ProfitTargetInPercentExpression &rhs)
{
  if (this == &rhs)
    return *this;

  mProfitTarget = rhs.mProfitTarget;
  mComputedHash = rhs.mComputedHash;
  return *this;
}


ProfitTargetInPercentExpression::~ProfitTargetInPercentExpression()
{}

decimal7 *ProfitTargetInPercentExpression::getProfitTarget() const
{
  return mProfitTarget;
}

unsigned long long 
ProfitTargetInPercentExpression::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      unsigned long long strHashVal = 
	hash_str (num::toString (*mProfitTarget).c_str());

      result = 43;
      result = 97 * result + strHashVal;
      mComputedHash = result;
    }

  return result;

}

//////////////////////////////////////////
// class LongSideProfitTargetInPercent
/////////////////////////////////////////

LongSideProfitTargetInPercent::LongSideProfitTargetInPercent (decimal7 *profitTarget)
  : ProfitTargetInPercentExpression (profitTarget)
{}

LongSideProfitTargetInPercent::LongSideProfitTargetInPercent (const LongSideProfitTargetInPercent& rhs) :
  ProfitTargetInPercentExpression (rhs)
{}

LongSideProfitTargetInPercent& 
LongSideProfitTargetInPercent::operator=(const LongSideProfitTargetInPercent &rhs)
{
 if (this == &rhs)
    return *this;

  ProfitTargetInPercentExpression::operator=(rhs);
  return *this;
}

LongSideProfitTargetInPercent::~LongSideProfitTargetInPercent()
{}

void 
LongSideProfitTargetInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

/////////////////////////////////////////
//  class ShortSideProfitTargetInPercent
////////////////////////////////////////

ShortSideProfitTargetInPercent::ShortSideProfitTargetInPercent (decimal7 *profitTarget)
  : ProfitTargetInPercentExpression (profitTarget)
{}

ShortSideProfitTargetInPercent::ShortSideProfitTargetInPercent (const ShortSideProfitTargetInPercent& rhs) :
  ProfitTargetInPercentExpression (rhs)
{}

ShortSideProfitTargetInPercent& 
ShortSideProfitTargetInPercent::operator=(const ShortSideProfitTargetInPercent &rhs)
{
 if (this == &rhs)
    return *this;

  ProfitTargetInPercentExpression::operator=(rhs);
  return *this;
}

ShortSideProfitTargetInPercent::~ShortSideProfitTargetInPercent()
{}

void 
ShortSideProfitTargetInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

///////////////////////////////////////
/// class StopLossInPercentExpression
//////////////////////////////////////

StopLossInPercentExpression::StopLossInPercentExpression(decimal7 *stopLoss) : 
  mStopLoss (stopLoss),
  mComputedHash(0)
{}

StopLossInPercentExpression::StopLossInPercentExpression (const StopLossInPercentExpression& rhs) 
  : mStopLoss (rhs.mStopLoss),
    mComputedHash (rhs.mComputedHash)
{}

StopLossInPercentExpression& 
StopLossInPercentExpression::operator=(const StopLossInPercentExpression &rhs)
{
  if (this == &rhs)
    return *this;

  mStopLoss = rhs.mStopLoss;
  mComputedHash = rhs.mComputedHash;
  return *this;
}

StopLossInPercentExpression::~StopLossInPercentExpression()
{}

decimal7 *StopLossInPercentExpression::getStopLoss() const
{
  return mStopLoss;
}

unsigned long long 
StopLossInPercentExpression::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      unsigned long long strHashVal = 
	hash_str (num::toString (*mStopLoss).c_str());

      result = 47;
      result = 101 * result + strHashVal;
      mComputedHash = result;
    }

  return result;

}
////////////////////////////////////////
/// class LongSideStopLossInPercent
////////////////////////////////////////

LongSideStopLossInPercent::LongSideStopLossInPercent(decimal7 *stopLoss) 
  : StopLossInPercentExpression (stopLoss)
{}

LongSideStopLossInPercent::LongSideStopLossInPercent (const LongSideStopLossInPercent& rhs) 
  : StopLossInPercentExpression (rhs)
{}

LongSideStopLossInPercent& 
LongSideStopLossInPercent::operator=(const LongSideStopLossInPercent &rhs)
{
  if (this == &rhs)
    return *this;

  StopLossInPercentExpression::operator=(rhs);
  return *this;
}

LongSideStopLossInPercent::~LongSideStopLossInPercent()
{}

void 
LongSideStopLossInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}
///////////////////////////////////////
/// class ShortSideStopLossInPercent
///////////////////////////////////////

ShortSideStopLossInPercent::ShortSideStopLossInPercent(decimal7 *stopLoss) 
  : StopLossInPercentExpression (stopLoss)
{}

ShortSideStopLossInPercent::ShortSideStopLossInPercent (const ShortSideStopLossInPercent& rhs) 
  : StopLossInPercentExpression (rhs)
{}

ShortSideStopLossInPercent& 
ShortSideStopLossInPercent::operator=(const ShortSideStopLossInPercent &rhs)
{
  if (this == &rhs)
    return *this;

  StopLossInPercentExpression::operator=(rhs);
  return *this;
}

ShortSideStopLossInPercent::~ShortSideStopLossInPercent()
{}

void 
ShortSideStopLossInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}
/////////////////////////////////////////////////////////
/// class MarketEntryExpression
////////////////////////////////////////////////////////

MarketEntryExpression::MarketEntryExpression()
{}

MarketEntryExpression::~MarketEntryExpression()
{}

MarketEntryExpression& 
MarketEntryExpression::operator=(const MarketEntryExpression &rhs)
{
  return *this;
}

MarketEntryExpression::MarketEntryExpression (const MarketEntryExpression& rhs)
{}

/////////////////////////////////////////////////////////
/// class MarketEntryOnOpen
////////////////////////////////////////////////////////

MarketEntryOnOpen::MarketEntryOnOpen() 
  : MarketEntryExpression()
{}

MarketEntryOnOpen::~MarketEntryOnOpen()
{}

MarketEntryOnOpen& 
MarketEntryOnOpen::operator=(const MarketEntryOnOpen &rhs)
{
 if (this == &rhs)
    return *this;

  MarketEntryExpression::operator=(rhs);
  return *this;
}

MarketEntryOnOpen::MarketEntryOnOpen (const MarketEntryOnOpen& rhs)
  : MarketEntryExpression (rhs)
{}

/////////////////////////////////////////////////////////
/// class LongMarketEntryOnOpen
////////////////////////////////////////////////////////

LongMarketEntryOnOpen::LongMarketEntryOnOpen() 
  : MarketEntryOnOpen()
{}

LongMarketEntryOnOpen::~LongMarketEntryOnOpen()
{}

LongMarketEntryOnOpen& 
LongMarketEntryOnOpen::operator=(const LongMarketEntryOnOpen &rhs)
{
 if (this == &rhs)
    return *this;

  MarketEntryOnOpen::operator=(rhs);
  return *this;
}

LongMarketEntryOnOpen::LongMarketEntryOnOpen (const LongMarketEntryOnOpen& rhs)
  : MarketEntryOnOpen (rhs)
{}

void 
LongMarketEntryOnOpen::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long 
LongMarketEntryOnOpen::hashCode()
{
  return 53;
}

/////////////////////////////////////////////////////////
/// class ShortMarketEntryOnOpen
/////////////////////////////////////////////////////////

ShortMarketEntryOnOpen::ShortMarketEntryOnOpen() 
  : MarketEntryOnOpen()
{}

ShortMarketEntryOnOpen::~ShortMarketEntryOnOpen()
{}

ShortMarketEntryOnOpen& 
ShortMarketEntryOnOpen::operator=(const ShortMarketEntryOnOpen &rhs)
{
 if (this == &rhs)
    return *this;

  MarketEntryOnOpen::operator=(rhs);
  return *this;
}

ShortMarketEntryOnOpen::ShortMarketEntryOnOpen (const ShortMarketEntryOnOpen& rhs)
  : MarketEntryOnOpen (rhs)
{}

void 
ShortMarketEntryOnOpen::accept (PalCodeGenVisitor &v)
{
  v.visit(this);
}

unsigned long long 
ShortMarketEntryOnOpen::hashCode()
{
  return 59;
}

/////////////////////////////////////////////////////////
/// class PatternDescription
////////////////////////////////////////////////////////

PatternDescription::PatternDescription(char *fileName, unsigned int patternIndex, 
		     unsigned long indexDate, decimal7* percentLong, decimal7* percentShort,
		     unsigned int numTrades, unsigned int consecutiveLosses)
  : mFileName (fileName),
    mPatternIndex (patternIndex),
    mIndexDate (indexDate),
    mPercentLong (percentLong),
    mPercentShort (percentShort),
    mNumTrades (numTrades),
    mConsecutiveLosses (consecutiveLosses),
    mComputedHash(0)
{}

PatternDescription::PatternDescription (const PatternDescription& rhs)
  : mFileName (rhs.mFileName),
    mPatternIndex (rhs.mPatternIndex),
    mIndexDate (rhs.mIndexDate),
    mPercentLong (rhs.mPercentLong),
    mPercentShort (rhs.mPercentShort),
    mNumTrades (rhs.mNumTrades),
    mConsecutiveLosses (rhs.mConsecutiveLosses),
    mComputedHash(rhs.mComputedHash)
{}

PatternDescription& 
PatternDescription::operator=(const PatternDescription &rhs)
{
  if (this == &rhs)
    return *this;

  mFileName = rhs.mFileName;
  mPatternIndex = rhs.mPatternIndex;
  mIndexDate = rhs.mIndexDate;
  mPercentLong = rhs.mPercentLong;
  mPercentShort = rhs.mPercentShort;
  mNumTrades = rhs.mNumTrades;
  mConsecutiveLosses = rhs.mConsecutiveLosses;
  mComputedHash = rhs.mComputedHash;

  return *this;
}


PatternDescription::~PatternDescription()
{}

const std::string& 
PatternDescription::getFileName() const
{
  return mFileName;
}

unsigned int 
PatternDescription::getpatternIndex() const
{
  return mPatternIndex;
}

unsigned int 
PatternDescription::getIndexDate() const
{
  return mIndexDate;
}

decimal7* 
PatternDescription::getPercentLong() const
{
  return mPercentLong;
}

decimal7* 
PatternDescription::getPercentShort() const
{
  return mPercentShort;
}

unsigned int 
PatternDescription::numTrades() const
{
  return mNumTrades;
}

unsigned int 
PatternDescription::numConsecutiveLosses() const
{
  return mConsecutiveLosses;
}

unsigned long long 
PatternDescription::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0)
    {
      result = 17;
      result = 31 * result + hash_str (mFileName.c_str());
      result = 31 * result + mPatternIndex;
      result = 31 * result + mIndexDate;
      result = 31 * result + hash_str (num::toString (*mPercentLong).c_str());
      result = 31 * result + hash_str (num::toString (*mPercentShort).c_str());
      result = 31 * result + mNumTrades;
      result = 31 * result + mConsecutiveLosses;
      mComputedHash = result;
    }

  return result;
}

void PatternDescription::accept (PalCodeGenVisitor &v)
{
  v.visit (this);
}


/////////////////////////////////////////////////////////
/// class PriceActionLabPattern
/////////////////////////////////////////////////////////
std::map<std::string, unsigned long long> PriceActionLabPattern:: mCachedStringHashMap;

PriceActionLabPattern::PriceActionLabPattern (PatternDescription* description, 
					      PatternExpression* pattern, 
					      MarketEntryExpression* entry, 
					      ProfitTargetInPercentExpression* profitTarget, 
					      StopLossInPercentExpression* stopLoss)
  : PriceActionLabPattern (description, pattern, entry, 
			   profitTarget, stopLoss, 
			   VOLATILITY_NONE, PORTFOLIO_FILTER_NONE)
{}

shared_ptr<PriceActionLabPattern> 
PriceActionLabPattern::clone (ProfitTargetInPercentExpression* profitTarget, 
			      StopLossInPercentExpression* stopLoss)
{
  return std::make_shared<PriceActionLabPattern>(getPatternDescription(),
						  getPatternExpression(),
						  getMarketEntry(),
						  profitTarget,
						  stopLoss);
}

PriceActionLabPattern::PriceActionLabPattern(PatternDescriptionPtr description,
					      PatternExpressionPtr pattern,
					      MarketEntryExpression* entry, 
					      ProfitTargetInPercentExpression* profitTarget, 
					      StopLossInPercentExpression* stopLoss)
  : mPattern (pattern),
    mEntry (entry),
    mProfitTarget (profitTarget),
    mStopLoss (stopLoss),
    mPatternDescription (description),
    mVolatilityAttribute(VOLATILITY_NONE), 
    mPortfolioAttribute (PORTFOLIO_FILTER_NONE),
    mMaxBarsBack(0),
    mPayOffRatio()
{
  mMaxBarsBack = PalPatternMaxBars::evaluateExpression (mPattern.get());
  mPayOffRatio = getProfitTargetAsDecimal() / getStopLossAsDecimal();
}

PriceActionLabPattern::PriceActionLabPattern (PatternDescription* description, 
					      PatternExpression* pattern, 
					      MarketEntryExpression* entry, 
					      ProfitTargetInPercentExpression* profitTarget, 
					      StopLossInPercentExpression* stopLoss, 
					      VolatilityAttribute volatilityAttribute,
					      PortfolioAttribute portfolioAttribute)
  : mPattern (pattern),
    mEntry (entry),
    mProfitTarget (profitTarget),
    mStopLoss (stopLoss),
    mPatternDescription (description),
    mVolatilityAttribute (volatilityAttribute),
  mPortfolioAttribute (portfolioAttribute),
  mMaxBarsBack(0),
  mPayOffRatio()
{
  mMaxBarsBack = PalPatternMaxBars::evaluateExpression (mPattern.get());
  mPayOffRatio = getProfitTargetAsDecimal() / getStopLossAsDecimal();
}

PriceActionLabPattern::PriceActionLabPattern (const PriceActionLabPattern& rhs)
  : mPattern (rhs.mPattern),
    mEntry (rhs.mEntry),
    mProfitTarget (rhs.mProfitTarget),
    mStopLoss (rhs.mStopLoss),
    mPatternDescription (rhs.mPatternDescription),
    mVolatilityAttribute (rhs.mVolatilityAttribute),
    mPortfolioAttribute (rhs.mPortfolioAttribute),
    mMaxBarsBack(rhs.mMaxBarsBack),
    mPayOffRatio(rhs.mPayOffRatio)
{}

PriceActionLabPattern& 
PriceActionLabPattern::operator=(const  PriceActionLabPattern &rhs)
{
  if (this == &rhs)
    return *this;

  mPattern = rhs.mPattern;
  mEntry = rhs.mEntry;
  mProfitTarget = rhs.mProfitTarget;
  mStopLoss = rhs.mStopLoss;
  mPatternDescription = rhs.mPatternDescription;
  mVolatilityAttribute = rhs.mVolatilityAttribute;
  mPortfolioAttribute = rhs.mPortfolioAttribute;
  mMaxBarsBack = rhs.mMaxBarsBack;
  mPayOffRatio = rhs.mPayOffRatio;

  return *this;
}

// Do not delete MarketEntryExpression because it is owned by factory
PriceActionLabPattern::~PriceActionLabPattern()
{}

PatternExpressionPtr 
PriceActionLabPattern::getPatternExpression() const
{
  return mPattern;
}

const std::string& 
PriceActionLabPattern::getFileName() const
{
  return mPatternDescription->getFileName();
}

const std::string PriceActionLabPattern::getBaseFileName() const
{
  return  GetBaseFilename (mPatternDescription->getFileName().c_str());

}

unsigned int 
PriceActionLabPattern::getpatternIndex() const
{
  return mPatternDescription->getpatternIndex();
}


unsigned int 
PriceActionLabPattern::getIndexDate() const
{
  return mPatternDescription->getIndexDate();
}


MarketEntryExpression* 
PriceActionLabPattern::getMarketEntry() const
{
  return mEntry;
}

ProfitTargetInPercentPtr 
PriceActionLabPattern::getProfitTarget() const
{
  return mProfitTarget;
}

decimal7
PriceActionLabPattern::getProfitTargetAsDecimal() const
{
  decimal7 target2 = *(getProfitTarget()->getProfitTarget());
  decimal7 target = target2;

  return target;
}

StopLossInPercentPtr 
PriceActionLabPattern::getStopLoss() const
{
  return mStopLoss;
}

decimal7 
PriceActionLabPattern::getStopLossAsDecimal() const
{
  decimal7 stop2 = *(getStopLoss()->getStopLoss());
  decimal7 stop = stop2;

  return stop;
}

PatternDescriptionPtr 
PriceActionLabPattern::getPatternDescription() const
{
  return mPatternDescription;
}

bool 
PriceActionLabPattern::hasVolatilityAttribute() const
{
  return isLowVolatilityPattern() || isHighVolatilityPattern() || isVeryHighVolatilityPattern();
}

bool 
PriceActionLabPattern::isLowVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_LOW);
}

bool 
PriceActionLabPattern::isNormalVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_NORMAL);
}

bool 
PriceActionLabPattern::isHighVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_HIGH);
}

bool 
PriceActionLabPattern::isVeryHighVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_VERY_HIGH);
}


bool 
PriceActionLabPattern::hasPortfolioAttribute() const
{
  return (isFilteredLongPattern() || isFilteredShortPattern());
}

bool 
PriceActionLabPattern::isFilteredLongPattern() const
{
  return (mPortfolioAttribute == PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
}

bool 
PriceActionLabPattern::isFilteredShortPattern() const
{
  return (mPortfolioAttribute == PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);
}

void 
PriceActionLabPattern::accept (PalCodeGenVisitor &v)
{
  v.visit (this);
}



unsigned long long
PriceActionLabPattern::getStringHash (const std::string& key)
{
  std::map<std::string, unsigned long long>::iterator pos;

  pos = mCachedStringHashMap.find (key);
  if (pos != mCachedStringHashMap.end())
    return (pos->second);
  else
    {
      unsigned long long hashVal = hash_str (key.c_str());

      mCachedStringHashMap.insert (std::make_pair(key, hashVal));
      return hashVal;
    }
}

unsigned long long
PriceActionLabPattern::hashCode()
{
  unsigned long long result = 181;
  result = 31 * result + getStringHash (getBaseFileName());
  result = 31 * result + getPatternExpression()->hashCode();
  result = 31 * result + getPatternDescription()->hashCode();
  result = 31 * result + getMarketEntry()->hashCode();
  result = 31 * result + getProfitTarget()->hashCode();
  result = 31 * result + getStopLoss()->hashCode();

  return result;

}

/////////////////////////////////////////////////////////

AstFactory::AstFactory() : 
  mLongEntryOnOpen (new LongMarketEntryOnOpen ()),
  mShortEntryOnOpen (new ShortMarketEntryOnOpen ()),
  mDecimalNumMap(),
  mDecimalNumMap2(),
  mLongsProfitTargets(),
  mShortsProfitTargets(),
  mLongsStopLoss(),
  mShortsStopLoss()

{
  initializePriceBars();
}

AstFactory::~AstFactory()
{
  //printf ("Deleting mLongEntryOnOpen\n");
  delete mLongEntryOnOpen;
  //printf ("Deleting mShortEntryOnOpen\n");
  delete mShortEntryOnOpen;

  unsigned int i;

  //printf ("Deleting predefined ohlc arrays\n");

  for (i = 0; i < AstFactory::MaxNumBarOffsets; i++)
    {
      //printf ("Iteration %d\n", i);
      delete mPredefinedPriceOpen[i];
      delete mPredefinedPriceHigh[i];
      delete mPredefinedPriceLow[i];
      delete mPredefinedPriceClose[i];
      delete mPredefinedVolume[i];
      delete mPredefinedRoc1[i];
      delete mPredefinedMeander[i];
      delete mPredefinedVChartLow[i];
      delete mPredefinedVChartHigh[i];
    }

  //printf ("AstFactory destructor complete\n");
  // printf ("Finished deleting predefined ohlc arrays\n");
}

LongSideProfitTargetInPercent *AstFactory::getLongProfitTarget (decimal7 *profitTarget)
{
  std::map<decimal7, std::shared_ptr<LongSideProfitTargetInPercent>>::const_iterator pos;

  pos = mLongsProfitTargets.find (*profitTarget);
  if (pos != mLongsProfitTargets.end())
    return (pos->second.get());
  else
    {
      auto p = std::make_shared<LongSideProfitTargetInPercent>(profitTarget);

      mLongsProfitTargets.insert (std::make_pair(*profitTarget, p));
      return p.get();
    }
}

ShortSideProfitTargetInPercent *AstFactory::getShortProfitTarget (decimal7 *profitTarget)
{
  std::map<decimal7, std::shared_ptr<ShortSideProfitTargetInPercent>>::const_iterator pos;

  pos = mShortsProfitTargets.find (*profitTarget);
  if (pos != mShortsProfitTargets.end())
    return (pos->second.get());
  else
    {
      auto p = std::make_shared<ShortSideProfitTargetInPercent>(profitTarget);

      mShortsProfitTargets.insert (std::make_pair(*profitTarget, p));
      return p.get();
    }
}

LongSideStopLossInPercent *AstFactory::getLongStopLoss(decimal7 *stopLoss)
{
  std::map<decimal7, std::shared_ptr<LongSideStopLossInPercent>>::const_iterator pos;

  pos = mLongsStopLoss.find (*stopLoss);
  if (pos != mLongsStopLoss.end())
    return (pos->second.get());
  else
    {
      auto p = std::make_shared<LongSideStopLossInPercent>(stopLoss);

      mLongsStopLoss.insert (std::make_pair(*stopLoss, p));
      return p.get();
    }
}

ShortSideStopLossInPercent *AstFactory::getShortStopLoss(decimal7 *stopLoss)
{
  std::map<decimal7, std::shared_ptr<ShortSideStopLossInPercent>>::const_iterator pos;

  pos = mShortsStopLoss.find (*stopLoss);
  if (pos != mShortsStopLoss.end())
    return (pos->second.get());
  else
    {
      auto p = std::make_shared<ShortSideStopLossInPercent>(stopLoss);

      mShortsStopLoss.insert (std::make_pair(*stopLoss, p));
      return p.get();
    }
}

MarketEntryExpression* AstFactory::getLongMarketEntryOnOpen()
{
  return mLongEntryOnOpen;
}

MarketEntryExpression* AstFactory::getShortMarketEntryOnOpen()
{
    return mShortEntryOnOpen;
}


void AstFactory::initializePriceBars()
{
  unsigned int i;

  for (i = 0; i < AstFactory::MaxNumBarOffsets; i++)
    {
      mPredefinedPriceOpen[i] = new PriceBarOpen (i);
      mPredefinedPriceHigh[i] = new PriceBarHigh (i);
      mPredefinedPriceLow[i] = new PriceBarLow (i);
      mPredefinedPriceClose[i] = new PriceBarClose (i);
      mPredefinedVolume[i] = new VolumeBarReference (i);
      mPredefinedRoc1[i] = new Roc1BarReference (i);
      mPredefinedMeander[i] = new MeanderBarReference (i);
      mPredefinedVChartLow[i] = new VChartLowBarReference (i);
      mPredefinedVChartHigh[i] = new VChartHighBarReference (i);
    }
}

PriceBarReference* AstFactory::getPriceOpen (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceOpen[barOffset];
  else
    return new PriceBarOpen (barOffset);
}

PriceBarReference* AstFactory::getPriceHigh (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceHigh[barOffset];
  else
    return new PriceBarHigh (barOffset);
}

PriceBarReference* AstFactory::getPriceLow (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceLow[barOffset];
  else
    return new PriceBarLow (barOffset);
}

PriceBarReference* AstFactory::getPriceClose (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceClose[barOffset];
  else
    return new PriceBarClose (barOffset);
}

PriceBarReference* AstFactory::getVolume (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedVolume[barOffset];
  else
    return new VolumeBarReference (barOffset);
}

PriceBarReference* AstFactory::getRoc1 (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedRoc1[barOffset];
  else
    return new Roc1BarReference (barOffset);
}

PriceBarReference* AstFactory::getMeander (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedMeander[barOffset];
  else
    return new MeanderBarReference (barOffset);
}

PriceBarReference* AstFactory::getVChartLow (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedVChartLow[barOffset];
  else
    return new VChartLowBarReference (barOffset);
}

PriceBarReference* AstFactory::getVChartHigh (unsigned int barOffset)
{
  if (barOffset <= AstFactory::MaxNumBarOffsets)
    return mPredefinedVChartHigh[barOffset];
  else
    return new VChartHighBarReference (barOffset);
}

decimal7 * AstFactory::getDecimalNumber (char *numString)
{
  std::string key(numString);
  std::map<std::string, DecimalPtr>::iterator pos;

  pos = mDecimalNumMap.find (key);
  if (pos != mDecimalNumMap.end())
    return (pos->second.get());
  else
    {
      decimal7 num = num::fromString<decimal7 >(numString);
      DecimalPtr p(new decimal7 (num));
	
      mDecimalNumMap.insert (std::make_pair(key, p));
      return p.get();
    }

}

decimal7 * AstFactory::getDecimalNumber (int num)
{
  int key = num;
  std::map<int, DecimalPtr>::iterator pos;

  pos = mDecimalNumMap2.find (key);
  if (pos != mDecimalNumMap2.end())
    return (pos->second.get());
  else
    {
      DecimalPtr p(new decimal7 (num));
	
      mDecimalNumMap2.insert (std::make_pair(key, p));
      return p.get();
    }

}

