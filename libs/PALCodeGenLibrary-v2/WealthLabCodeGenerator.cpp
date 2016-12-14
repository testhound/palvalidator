#include "PalAst.h"
#include "PalCodeGenVisitor.h"

extern bool firstSubExpressionVisited;

///////////////////////////////////////
/// class WealthLabCodeGenVisitor
//////////////////////////////////////

WealthLabCodeGenVisitor::WealthLabCodeGenVisitor(PriceActionLabSystem *system)
  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mTradingModelFileName(std::string("WlModel.txt")),
    mFirstIfForLongsGenerated (false),
    mFirstIfForShortsGenerated (false)
{}

WealthLabCodeGenVisitor::WealthLabCodeGenVisitor(PriceActionLabSystem *system,
						 const std::string& outputFileName)
  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mTradingModelFileName(outputFileName),
    mFirstIfForLongsGenerated (false),
    mFirstIfForShortsGenerated (false)
{}

WealthLabCodeGenVisitor::~WealthLabCodeGenVisitor()
{}


void 
WealthLabCodeGenVisitor::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  genCodeForVariablesInEntryScript();

  mTradingModelFileName << "for Bar := 10 to BarCount - 1 do " << std::endl << "begin" << std::endl;
  mTradingModelFileName << "\tif not LastPositionActive then" << std::endl;
  mTradingModelFileName << "\t{ Entry Rules }" << std::endl;
  mTradingModelFileName << "\tbegin" << std::endl;
  mTradingModelFileName << "\t\tlongEntryFound := false;" << std::endl;
  mTradingModelFileName << "\t\tshortEntryFound := false;" << std::endl << std::endl;

  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
    }

  for (it = mTradingSystemPatterns->patternShortsBegin(); 
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
    }

mTradingModelFileName <<  std::endl;
  mTradingModelFileName << "\t\tif (longEntryFound = true) and (shortEntryFound = false) then" << std::endl;
  mTradingModelFileName << "\t\tbegin" << std::endl;
  mTradingModelFileName << "\t\t\tSetRiskStopLevel (longStop);"  << std::endl;
  mTradingModelFileName << "\t\t\tBuyAtMarket (Bar + 1, \'\')"  << std::endl;
  mTradingModelFileName << "\t\tend"  << std::endl;
  mTradingModelFileName << "\t\telse if (longEntryFound = false) and (shortEntryFound = true) then" << std::endl;
  mTradingModelFileName << "\t\tbegin"  << std::endl;
  mTradingModelFileName << "\t\t\tSetRiskStopLevel (shortStop);"  << std::endl;
  mTradingModelFileName << "\t\t\tShortAtMarket (Bar + 1, \'\');"  << std::endl;
  mTradingModelFileName << "\t\tend;"  << std::endl;

  mTradingModelFileName <<  std::endl;
  mTradingModelFileName << "\tend  // end for if not LastPositionActive"  << std::endl;
  mTradingModelFileName << "\telse" << std::endl;
  mTradingModelFileName << "\tbegin" << std::endl;
  mTradingModelFileName << "\t\tif PositionLong (LastPosition) then" << std::endl;
  mTradingModelFileName << "\t\t\tProcessLongPosition (Bar)" << std::endl;
  mTradingModelFileName << "\t\telse" << std::endl;
  mTradingModelFileName << "\t\t\tProcessshortPosition (Bar);" << std::endl;
  mTradingModelFileName << "\tend;" << std::endl << std::endl;
  mTradingModelFileName << "end;" << std::endl << std::endl;

  
}
	

std::ofstream *
WealthLabCodeGenVisitor::getOutputFileStream()
{
  return &mTradingModelFileName;
}

void
WealthLabCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mTradingModelFileName << "PriceOpen(Bar - " << bar->getBarOffset() << ")";
}

void
WealthLabCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mTradingModelFileName << "PriceHigh(Bar - " << bar->getBarOffset() << ")";
}

void
WealthLabCodeGenVisitor::visit (PriceBarLow *bar)
{
  mTradingModelFileName << "PriceLow(Bar - " << bar->getBarOffset() << ")";
}

void
WealthLabCodeGenVisitor::visit (PriceBarClose *bar)
{
  mTradingModelFileName << "PriceClose(Bar - " << bar->getBarOffset() << ")";
}

void
WealthLabCodeGenVisitor::visit (VolumeBarReference *bar)
{
  mTradingModelFileName << "Volume(Bar - " << bar->getBarOffset() << ")";
}

void
WealthLabCodeGenVisitor::visit (Roc1BarReference *bar)
{
  mTradingModelFileName << "RateOfChange(Bar - " << bar->getBarOffset() << ", Close, 1)";
}

void
WealthLabCodeGenVisitor::visit (MeanderBarReference *bar)
{
  mTradingModelFileName << "meanderVar(Bar - " << bar->getBarOffset() << ", Close, 1)";
}

void
WealthLabCodeGenVisitor::visit (VChartLowBarReference *bar)
{
  mTradingModelFileName << "vchartLowVar(Bar - " << bar->getBarOffset() << ", Close, 1)";
}

void
WealthLabCodeGenVisitor::visit (VChartHighBarReference *bar)
{
  mTradingModelFileName << "vchartHighVar(Bar - " << bar->getBarOffset() << ", Close, 1)";
}


void
WealthLabCodeGenVisitor::visit (GreaterThanExpr *expr)
{
  if (firstSubExpressionVisited == false)
    mTradingModelFileName << "\t\t\t(";
  else
    {
      mTradingModelFileName << "(";
      firstSubExpressionVisited = false;
    }

  expr->getLHS()->accept (*this);
  mTradingModelFileName << " > ";
  expr->getRHS()->accept (*this);
  mTradingModelFileName << ")";
}

void
WealthLabCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mTradingModelFileName << " and " << std::endl;
  expr->getRHS()->accept (*this);
}

void
WealthLabCodeGenVisitor::visit (PatternDescription *desc)
{
  mTradingModelFileName << "\t\t" << "\{FILE:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
    

}

void WealthLabCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mTradingModelFileName << "\t\t\tlongEntryFound := true;" << std::endl;
}

void WealthLabCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mTradingModelFileName << "\t\t\tshortEntryFound := true;" << std::endl;
}

bool WealthLabCodeGenVisitor::isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern)
{
  decimal7 threshold(1.05);

  decimal7 target2 = *(pattern->getProfitTarget()->getProfitTarget());
  decimal7 stop2 = *(pattern->getStopLoss()->getStopLoss());

  decimal7 ratio = target2 / stop2;
  if (ratio >= threshold)
    return true;
  else
    return false;
}

void WealthLabCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this);
  mTradingModelFileName << std::endl;
  
  if (pattern->isLongPattern())
    {
      if (mFirstIfForLongsGenerated == true)
	mTradingModelFileName << "\t\telse if ";
      else
	{
	  mTradingModelFileName << "\t\tif ";
	  mFirstIfForLongsGenerated = true;
	}
    }
  else
    {
      if (mFirstIfForShortsGenerated == true)
	mTradingModelFileName << "\t\telse if ";
      else
	{
	  mTradingModelFileName << "\t\tif ";
	  mFirstIfForShortsGenerated = true;
	}
    }

  if (isHighRewardToRiskRatioPattern (pattern))
    {
      mTradingModelFileName << "(TradeHighRewardToRiskPatterns = true) and " << std::endl;
      firstSubExpressionVisited = false;
    }
  else
    firstSubExpressionVisited = true;
  
  pattern->getPatternExpression()->accept (*this);
  mTradingModelFileName << " Then" << std::endl << std::endl;
  mTradingModelFileName << "\t\tbegin" << std::endl;
  pattern->getStopLoss()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getMarketEntry()->accept (*this);

  mTradingModelFileName << "\t\tend" << std::endl;
}

////////////////////////////////////////////////////
/// class WealthLabRADCodeGenVisitor
////////////////////////////////////////////////////

WealthLabRADCodeGenVisitor::WealthLabRADCodeGenVisitor (PriceActionLabSystem *system)
  : WealthLabCodeGenVisitor (system)
{}

WealthLabRADCodeGenVisitor::WealthLabRADCodeGenVisitor (PriceActionLabSystem *system, const std::string& outputFileName)
  : WealthLabCodeGenVisitor (system, outputFileName)
{}

WealthLabRADCodeGenVisitor::~WealthLabRADCodeGenVisitor()
{}


void WealthLabRADCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  std::ofstream *outFile = getOutputFileStream();

  *outFile << "var Bar : integer;" << std::endl;
  *outFile << "var shortStop, longStop, stopInPercentForTrade : float;" << std::endl;
  *outFile << "var profitTargetInPercentForLongTrade, profitTargetInPercentForShortTrade : float;" << std::endl;
  *outFile << "var longEntryFound, shortEntryFound : boolean;" << std::endl << std::endl;
  *outFile << "var stopForPosition, targetForPosition : float;" << std::endl;

}

void 
WealthLabRADCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade := (1.0 - (" << *stop << "/100));" << std::endl;
  *outFile << "\t\t\tlongStop := (PriceClose (Bar) * stopInPercentForTrade);" << std::endl;
}

void WealthLabRADCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForLongTrade := (1.0 + (" << *target << "/100));" << std::endl;
}

void WealthLabRADCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForShortTrade := (1.0 - (" << *target << "/100));" << std::endl;
}

void 
WealthLabRADCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade := (1.0 + (" << *stop << "/100));" << std::endl;
  *outFile << "\t\t\tshortStop := (PriceClose (Bar) * stopInPercentForTrade);" << std::endl;
}

////////////////////////////////////////////////////
/// class WealthLabPointAdjustedCodeGenVisitor
////////////////////////////////////////////////////

WealthLabPointAdjustedCodeGenVisitor::WealthLabPointAdjustedCodeGenVisitor (PriceActionLabSystem *system)
  : WealthLabCodeGenVisitor (system)
{}

WealthLabPointAdjustedCodeGenVisitor::~WealthLabPointAdjustedCodeGenVisitor()
{}


void WealthLabPointAdjustedCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  std::ofstream *outFile = getOutputFileStream();

  *outFile << "VARIABLES: shortStop, longStop TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: shortStopDistance, longStopDistance TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: longEntryFound, shortEntryFound TYPE: Integer" << std::endl << std::endl;
  *outFile << "' NOTE: declare profitTargetInPercentForTrade and stopInPercentForTrade as floating point IPV variables" << std::endl;
}

void 
WealthLabPointAdjustedCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (" << *stop << "/100)" << std::endl;
  *outFile << "\t\t\tlongStopDistance = instrument.RoundTick (instrument.unadjustedclose * stopInPercentForTrade)" 
	   << std::endl;
  *outFile << "\t\t\tlongStop = instrument.close - longStopDistance" << std::endl;
}

void WealthLabPointAdjustedCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForLongTrade = (" << *target << "/100)" << std::endl;
}

void WealthLabPointAdjustedCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForShortTrade = (" << *target << "/100)" << std::endl;
}

void 
WealthLabPointAdjustedCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (" << *stop << "/100)" << std::endl;
  *outFile << "\t\t\tshortStopDistance = instrument.RoundTick (instrument.unadjustedclose * stopInPercentForTrade)"
	   << std::endl;
  *outFile << "\t\t\tshortStop = instrument.close + shortStopDistance" << std::endl;
}
