#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>

extern bool firstSubExpressionVisited;

///////////////////////////////////////
/// class EasyLanguageCodeGenVisitor
//////////////////////////////////////

const std::string EasyLanguageCodeGenVisitor::LONG_PATTERNS_MARKER = "////// LONG ENTRY SETUPS";
const std::string EasyLanguageCodeGenVisitor::SHORT_PATTERNS_MARKER = "////// SHORT ENTRY SETUPS";

EasyLanguageCodeGenVisitor::EasyLanguageCodeGenVisitor(PriceActionLabSystem *system,
						       const std::string& templateFileName,
						       const std::string& outputFileName,
						       const StopTargetDetail& dev1Detail,
						       const StopTargetDetail& dev2Detail)

  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mTemplateFile(templateFileName),
    mEasyLanguageFileName(outputFileName),
    mDev1Detail (dev1Detail),
    mDev2Detail (dev2Detail)
{}


EasyLanguageCodeGenVisitor::~EasyLanguageCodeGenVisitor()
{}

void EasyLanguageCodeGenVisitor::insertLongPatterns()
{

  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  unsigned int numLongPatterns = 0;

  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numLongPatterns++;
    }
  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
}

void EasyLanguageCodeGenVisitor::insertShortPatterns()
{

  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  unsigned int numShortPatterns = 0;
  for (it = mTradingSystemPatterns->patternShortsBegin();
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numShortPatterns++;
     }
    std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}

void
EasyLanguageCodeGenVisitor::generateCode()
{
  std::ofstream *outFile = getOutputFileStream();
  //line by line parsing
  std::string line;

  while (std::getline(mTemplateFile, line))
  {
      if (line.find(LONG_PATTERNS_MARKER) != std::string::npos) {
          insertLongPatterns();
        }
      else if (line.find(SHORT_PATTERNS_MARKER) != std::string::npos) {
          insertShortPatterns();
        }
      else {
          *outFile << line << std::endl;
        }
  }
}


std::ofstream *
EasyLanguageCodeGenVisitor::getOutputFileStream()
{
  return &mEasyLanguageFileName;
}

void
EasyLanguageCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mEasyLanguageFileName << "open[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mEasyLanguageFileName << "high[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (PriceBarLow *bar)
{
  mEasyLanguageFileName << "low[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (PriceBarClose *bar)
{
  mEasyLanguageFileName << "close[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (VolumeBarReference *bar)
{
  mEasyLanguageFileName << "volume[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (Roc1BarReference *bar)
{
  mEasyLanguageFileName << "RateOfChange(Close, 1)[" << bar->getBarOffset() << "]";
}

void EasyLanguageCodeGenVisitor::visit (IBS1BarReference *bar)
{
  mEasyLanguageFileName << "IBS(1)[" << bar->getBarOffset() << "]";
}

void EasyLanguageCodeGenVisitor::visit (IBS2BarReference *bar)
{
  mEasyLanguageFileName << "IBS(2)[" << bar->getBarOffset() << "]";
}

void EasyLanguageCodeGenVisitor::visit (IBS3BarReference *bar)
{
  mEasyLanguageFileName << "IBS(3)[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (MeanderBarReference *bar)
{
  mEasyLanguageFileName << "meanderVar[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (VChartLowBarReference *bar)
{
  mEasyLanguageFileName << "vchartLowVar[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (VChartHighBarReference *bar)
{
  mEasyLanguageFileName << "vchartHighVar[" << bar->getBarOffset() << "]";
}

void
EasyLanguageCodeGenVisitor::visit (GreaterThanExpr *expr)
{
  if (firstSubExpressionVisited == false)
    mEasyLanguageFileName << "\t\t\t(";
  else
    {
      mEasyLanguageFileName << "(";
      firstSubExpressionVisited = false;
    }

  expr->getLHS()->accept (*this);
  mEasyLanguageFileName << " > ";
  expr->getRHS()->accept (*this);
  mEasyLanguageFileName << ")";
}

void
EasyLanguageCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mEasyLanguageFileName << " and " << std::endl;
  expr->getRHS()->accept (*this);
}

void
EasyLanguageCodeGenVisitor::visit (PatternDescription *desc)
{
  mEasyLanguageFileName << "\t\t" << "//FILE:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
                         << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;


}

void EasyLanguageCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mEasyLanguageFileName << "\t\t\tlongEntryFound = true;" << std::endl;
}

void EasyLanguageCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mEasyLanguageFileName << "\t\t\tshortEntryFound = true;" << std::endl;
}

bool EasyLanguageCodeGenVisitor::isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern)
{
  decimal7 threshold(1.25);

  decimal7 target2 = *(pattern->getProfitTarget()->getProfitTarget());
  decimal7 stop2 = *(pattern->getStopLoss()->getStopLoss());

  decimal7 ratio = target2 / stop2;
  if (ratio >= threshold)
    return true;
  else
    return false;
}

bool EasyLanguageCodeGenVisitor::isDev1Pattern(PriceActionLabPattern *pattern)
{
  return ((pattern->getStopLossAsDecimal() == mDev1Detail.getStopLoss()) &&
          (pattern->getProfitTargetAsDecimal() == mDev1Detail.getProfitTarget()));
}

bool EasyLanguageCodeGenVisitor::isDev2Pattern(PriceActionLabPattern *pattern)
{
  return ((pattern->getStopLossAsDecimal() == mDev2Detail.getStopLoss()) &&
          (pattern->getProfitTargetAsDecimal() == mDev2Detail.getProfitTarget()));

}


void EasyLanguageCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this);
  mEasyLanguageFileName << std::endl;

  if (pattern->isLongPattern())
    {
      mEasyLanguageFileName << "\t\tif (longEntryFound = false) and ";


    }
  else
    {
      mEasyLanguageFileName << "\t\tif (shortEntryFound = false) and ";
    }

  if (isDev1Pattern (pattern))
    mEasyLanguageFileName << "(tradeSys1 = true) and ";
  else if (isDev2Pattern (pattern))
    mEasyLanguageFileName << "(tradeSys2 = true) and ";

  if (pattern->hasVolatilityAttribute())
    {
      if (pattern->isLowVolatilityPattern())
        mEasyLanguageFileName << "lowVolatility and ";
      else if (pattern->isHighVolatilityPattern())
        mEasyLanguageFileName << "highVolatility and ";
      else if (pattern->isVeryHighVolatilityPattern())
        mEasyLanguageFileName << "vHighVolatility and ";
    }

  if (pattern->hasPortfolioAttribute())
    {
      if (pattern->isFilteredLongPattern())
        mEasyLanguageFileName << "tradeLongSide and ";
      else if (pattern->isFilteredShortPattern())
        mEasyLanguageFileName << "tradeShortSide and ";
    }

  if (isHighRewardToRiskRatioPattern (pattern))
    {
      mEasyLanguageFileName << "(TradeHighRewardToRiskPatterns = true) and " << std::endl;
      firstSubExpressionVisited = false;
    }
  else
    firstSubExpressionVisited = true;

  pattern->getPatternExpression()->accept (*this);
  mEasyLanguageFileName << " Then" << std::endl << std::endl;
  mEasyLanguageFileName << "\t\tbegin" << std::endl;
  pattern->getStopLoss()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getMarketEntry()->accept (*this);

    if (isDev1Pattern (pattern))
    {
      mEasyLanguageFileName << "\t\t\tMinHoldPeriod = MinDev1HoldPeriod;" << std::endl;
      mEasyLanguageFileName << "\t\t\tMaxHoldPeriod = MaxDev1HoldPeriod;" << std::endl;
    }
  else if (isDev2Pattern (pattern))
    {
      mEasyLanguageFileName << "\t\t\tMinHoldPeriod = MinDev2HoldPeriod;" << std::endl;
      mEasyLanguageFileName << "\t\t\tMaxHoldPeriod = MaxDev2HoldPeriod;" << std::endl;

    }

  mEasyLanguageFileName << "\t\tend;" << std::endl;
}






//////////////////////////////////////////////////////
///// class EasyLanguageRADCodeGenVisitor
//////////////////////////////////////////////////////

//EasyLanguageRADCodeGenVisitor::EasyLanguageRADCodeGenVisitor (PriceActionLabSystem *system,
//							      const std::string& outputFileName,
//							      const StopTargetDetail& dev1Detail,
//							      const StopTargetDetail& dev2Detail)

//  : EasyLanguageCodeGenVisitor (system, outputFileName, dev1Detail, dev2Detail)
//{}

//EasyLanguageRADCodeGenVisitor::~EasyLanguageRADCodeGenVisitor()
//{}

//void EasyLanguageRADCodeGenVisitor::genCodeToInitializeVariables()
//{



//}

//void EasyLanguageRADCodeGenVisitor::genCodeForEntryExit()
//{
//std::ofstream *outFile = getOutputFileStream();

// *outFile <<  std::endl;
// *outFile << "\t\tif (longEntryFound = true) and (shortEntryFound = false) then" << std::endl;
// *outFile << "\t\tbegin" << std::endl;
// *outFile << "\t\t\tbuy next bar at market;"  << std::endl;
// *outFile << "\t\tend;"  << std::endl;
// *outFile << "\t\tif (longEntryFound = false) and (shortEntryFound = true) then" << std::endl;
// *outFile << "\t\tbegin"  << std::endl;
// *outFile << "\t\t\tsell short next bar at market;"  << std::endl;
// *outFile << "\t\tend;"  << std::endl;

// *outFile <<  std::endl;
// *outFile << "\tend"  << std::endl;
// *outFile << "\telse" << std::endl;
// *outFile << "\tbegin" << std::endl;
// *outFile << "\t\tif marketposition = 1 then begin" << std::endl;
// *outFile << "\t\t\tif BarsSinceEntry = 0 then" << std::endl;
// *outFile << "\t\t\tbegin" << std::endl;
// *outFile << "\t\t\t\tlongStop = Round2Fraction (EntryPrice * stopPercent);" << std::endl;
// *outFile << "\t\t\t\tTargPrL = Round2Fraction (EntryPrice * profitTargetPercent);" << std::endl;
// *outFile << "\t\t\tend;" << std::endl << std::endl;

// *outFile << "\t\t\tsell next bar at TargPrL limit;" << std::endl;
// *outFile << "\t\t\tsell next bar at longStop stop;" << std::endl;
// *outFile << "\t\tend;" << std::endl;

// *outFile << "\t\tif marketposition = -1 then begin" << std::endl;
//*outFile << "\t\t\tif BarsSinceEntry = 0 then" << std::endl;
// *outFile << "\t\t\tbegin" << std::endl;
// *outFile << "\t\t\t\tshortStop = Round2Fraction (EntryPrice * stopPercent);" << std::endl;
// *outFile << "\t\t\t\tTargPrS = Round2Fraction (EntryPrice * profitTargetPercent);" << std::endl;
//*outFile << "\t\t\tend;" << std::endl << std::endl;
// *outFile << "\t\t\tbuy to cover next bar at TargPrS limit;" << std::endl;
// *outFile << "\t\t\tbuy to cover next bar at shortStop stop;" << std::endl;
// *outFile << "\t\tend;" << std::endl;

// *outFile << "\tend;" << std::endl << std::endl;
//}

//void EasyLanguageRADCodeGenVisitor::genCodeForVariablesInEntryScript()
//{
//  //  std::ofstream *outFile = getOutputFileStream();



//}

//void
//EasyLanguageRADCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *stop = stopLoss->getStopLoss();

//  *outFile << "\t\t\tstopPercent = (1.0 - (" << *stop << "/100));" << std::endl;
//  *outFile << "\t\t\tlongStop = (Close * stopPercent);" << std::endl;
//  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
//}

//void EasyLanguageRADCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *target = profitTarget->getProfitTarget();

//  *outFile << "\t\t\tprofitTargetPercent = (1.0 + (" << *target << "/100));" << std::endl;
//  *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
//}

//void EasyLanguageRADCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *target = profitTarget->getProfitTarget();

//  *outFile << "\t\t\tprofitTargetPercent = (1.0 - (" << *target << "/100));" << std::endl;
//  *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
//}

//void
//EasyLanguageRADCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *stop = stopLoss->getStopLoss();

//  *outFile << "\t\t\tstopPercent = (1.0 + (" << *stop << "/100));" << std::endl;
//  *outFile << "\t\t\tshortStop = (Close * stopPercent);" << std::endl;
//  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
//}

//////////////////////////////////////////////////////
///// class EasyLanguagePointAdjustedCodeGenVisitor
//////////////////////////////////////////////////////

//EasyLanguagePointAdjustedCodeGenVisitor
//::EasyLanguagePointAdjustedCodeGenVisitor (PriceActionLabSystem *system,
//					   const std::string& bloxOutfileFileName,
//					   const StopTargetDetail& dev1Detail,
//					   const StopTargetDetail& dev2Detail)

//  : EasyLanguageCodeGenVisitor (system, bloxOutfileFileName, dev1Detail, dev2Detail)
//{}

//EasyLanguagePointAdjustedCodeGenVisitor::~EasyLanguagePointAdjustedCodeGenVisitor()
//{}


//void EasyLanguagePointAdjustedCodeGenVisitor::genCodeForVariablesInEntryScript()
//{
//  std::ofstream *outFile = getOutputFileStream();

//  *outFile << "vars: shortStopDistance(0.0), longStopDistance(0.0), UnAdjustedClose(0.0);" << std::endl;
//  *outFile << "vars: profitTargetDistance(0.0), unAdjCloseAtEntry(0.0);" << std::endl << std::endl;
//}

//void
//EasyLanguagePointAdjustedCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *stop = stopLoss->getStopLoss();

//  *outFile << "\t\t\tstopPercent = (" << *stop << "/100);" << std::endl;
//  *outFile << "\t\t\tlongStopDistance = Round2Fraction (UnAdjustedClose * stopPercent);"
//           << std::endl;
//  *outFile << "\t\t\tlongStop = close - longStopDistance;" << std::endl;
//  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
//}

//void EasyLanguagePointAdjustedCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *target = profitTarget->getProfitTarget();

//  *outFile << "\t\t\tprofitTargetPercent = (" << *target << "/100);" << std::endl;
//    *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
//}

//void EasyLanguagePointAdjustedCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *target = profitTarget->getProfitTarget();

//  *outFile << "\t\t\tprofitTargetPercent = (" << *target << "/100);" << std::endl;
//  *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
//}

//void
//EasyLanguagePointAdjustedCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
//{
//  std::ofstream *outFile = getOutputFileStream();
//  decimal7 *stop = stopLoss->getStopLoss();

//  *outFile << "\t\t\tstopPercent = (" << *stop << "/100);" << std::endl;
//  *outFile << "\t\t\tshortStopDistance = Round2Fraction (UnAdjustedClose * stopPercent);"
//           << std::endl;
//  *outFile << "\t\t\tshortStop = close + shortStopDistance;" << std::endl;
//  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
//}

//void EasyLanguagePointAdjustedCodeGenVisitor::genCodeToInitializeVariables()
//{
//std::ofstream *outFile = getOutputFileStream();

//  *outFile << "\t\tUnAdjustedClose = C of Data2;" << std::endl << std::endl;
//}

//void EasyLanguagePointAdjustedCodeGenVisitor::genCodeForEntryExit()
//{
//  std::ofstream *outFile = getOutputFileStream();

// *outFile << "\telse" << std::endl;
// *outFile << "\tbegin" << std::endl;
// *outFile << "\t\tif marketposition = 1 then begin" << std::endl;

// genCommonCodeForLongExitPrologue();

// *outFile << "\t\t\tif BarsSinceEntry = 0 then" << std::endl;
// *outFile << "\t\t\tbegin" << std::endl;
// *outFile << "\t\t\t\tUnAdjustedClose = C of Data2;" << std::endl;
// *outFile << "\t\t\t\tlongStopDistance = Round2Fraction (UnAdjustedClose * stopPercent);" << std::endl;
// *outFile << "\t\t\t\tlongStop = EntryPrice - longStopDistance;" << std::endl;
// *outFile << "\t\t\t\tprofitTargetDistance = Round2Fraction (UnAdjustedClose * profitTargetPercent);" << std::endl;
// *outFile << "\t\t\t\tTargPrL = EntryPrice + profitTargetDistance;" << std::endl;
// *outFile << "\t\t\t\tunAdjCloseAtEntry = UnAdjustedClose;" << std::endl;
// *outFile << "\t\t\t\tIf Close > open then" << std::endl;
// *outFile << "\t\t\t\t\thighestPosChange = ((UnAdjustedClose/UnadjustedClose[1]) - 1) * 100.0;" << std::endl;

// *outFile << "\t\t\tend;" << std::endl << std::endl;

// *outFile << "\t\t\tIf Barssinceentry > 0 then" << std::endl;
// *outFile << "\t\t\tBegin" << std::endl;
// *outFile << "\t\t\t\tValue1 = ((UnAdjustedClose / unAdjCloseAtEntry) - 1) * 100;" << std::endl;
// *outFile << "\t\t\t\t\thighestPosChange = Maxlist (highestPosChange,value1 );" << std::endl;
// *outFile << "\t\t\tend;" << std::endl;

// *outFile << "\t\t\tif noNextDayOrders = False then" << std::endl;
// *outFile << "\t\t\tbegin" << std::endl;
// *outFile << "\t\t\t\tsell next bar at TargPrL limit;" << std::endl;
// *outFile << "\t\t\t\tsell next bar at longStop stop;" << std::endl;
// *outFile << "\t\t\tend;" << std::endl;
// *outFile << "\t\tend;" << std::endl;

// *outFile << "\t\tif marketposition = -1 then begin" << std::endl;

// genCommonCodeForShortExitPrologue();


//*outFile << "\t\t\tif BarsSinceEntry = 0 then" << std::endl;
// *outFile << "\t\t\tbegin" << std::endl;
// *outFile << "\t\t\t\tUnAdjustedClose = C of Data2;" << std::endl;
// *outFile << "\t\t\t\tshortStopDistance = Round2Fraction (UnAdjustedClose * stopPercent);" << std::endl;
// *outFile << "\t\t\t\tshortStop = EntryPrice + shortStopDistance;" << std::endl;
//*outFile << "\t\t\t\tprofitTargetDistance = Round2Fraction (UnAdjustedClose * profitTargetPercent);" << std::endl;
// *outFile << "\t\t\t\tTargPrS = EntryPrice - profitTargetDistance;" << std::endl;
// *outFile << "\t\t\tend;" << std::endl << std::endl;
//*outFile << "\t\t\tif noNextDayOrders = False then" << std::endl;
// *outFile << "\t\t\tbegin" << std::endl;
// *outFile << "\t\t\t\tbuy to cover next bar at TargPrS limit;" << std::endl;
// *outFile << "\t\t\t\tbuy to cover next bar at shortStop stop;" << std::endl;
// *outFile << "\t\t\tend;" << std::endl;
// *outFile << "\t\tend;" << std::endl;

// *outFile << "\tend;" << std::endl << std::endl;
// //*outFile << "end;" << std::endl << std::endl;

//}
