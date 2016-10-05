#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>

extern bool firstSubExpressionVisited;

///////////////////////////////////////
/// class TradingBloxCodeGenVisitor
//////////////////////////////////////

TradingBloxCodeGenVisitor::TradingBloxCodeGenVisitor(PriceActionLabSystem *system,
						     const std::string& bloxOutFileName)
  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mEntryOrdersScriptFile(bloxOutFileName)
{}

TradingBloxCodeGenVisitor::~TradingBloxCodeGenVisitor()
{}


void 
TradingBloxCodeGenVisitor::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  genCodeForVariablesInEntryScript();

  mEntryOrdersScriptFile << "if (instrument.currentBar > 10) then " << std::endl;
  mEntryOrdersScriptFile << "\tlongEntryFound = 0" << std::endl;
  mEntryOrdersScriptFile << "\tshortEntryFound = 0" << std::endl;

  mEntryOrdersScriptFile << "\tif rankedSimonsVolatility < 50 then" << std::endl;
  mEntryOrdersScriptFile << "\t\tlowVolatility = 1" << std::endl;
  mEntryOrdersScriptFile << "\t\thighVolatility = 0" << std::endl;
  mEntryOrdersScriptFile << "\t\tvHighVolatility = 0" << std::endl;
  mEntryOrdersScriptFile << "\tendif" << std::endl << std::endl;

  mEntryOrdersScriptFile << "\tif (rankedSimonsVolatility >= 50) and (rankedSimonsVolatility <= 80) then" << std::endl;
  mEntryOrdersScriptFile << "\t\tlowVolatility = 0" << std::endl;
  mEntryOrdersScriptFile << "\t\thighVolatility = 1" << std::endl;
  mEntryOrdersScriptFile << "\t\tvHighVolatility = 0" << std::endl;
  mEntryOrdersScriptFile << "\tendif" << std::endl << std::endl;

  mEntryOrdersScriptFile << "\tif rankedSimonsVolatility > 80 then" << std::endl;
  mEntryOrdersScriptFile << "\t\tlowVolatility = 0" << std::endl;
  mEntryOrdersScriptFile << "\t\thighVolatility = 0" << std::endl;
  mEntryOrdersScriptFile << "\t\tvHighVolatility = 1" << std::endl;
  mEntryOrdersScriptFile << "\tendif" << std::endl << std::endl;
  //mEntryOrdersScriptFile << "\tif (instrument.position <> short) then" << std::endl << std::endl;

  unsigned int numLongPatterns = 0;

  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numLongPatterns++;
    }

  //  mEntryOrdersScriptFile << "\tendif" << std::endl << std::endl;

  //mEntryOrdersScriptFile << "\tif (longEntryFound = 0) and (instrument.position <> long) then" << std::endl << std::endl;
    unsigned int numShortPatterns = 0;

  for (it = mTradingSystemPatterns->patternShortsBegin(); 
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numShortPatterns++;
    }

std::ifstream infile("template/blox_entry_order_template");
  std::string line;
  
  // Most of the code for the on_next method is static
  // so read and output from a template file
  
  while (std::getline(infile, line))
    {
      mEntryOrdersScriptFile << line << std::endl;
    }

  mEntryOrdersScriptFile <<  std::endl;

  //mEntryOrdersScriptFile << "\tendif" << std::endl << std::endl;
  mEntryOrdersScriptFile << "endif" << std::endl << std::endl;

  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
  std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}
	

std::ofstream *
TradingBloxCodeGenVisitor::getOutputFileStream()
{
  return &mEntryOrdersScriptFile;
}

void
TradingBloxCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mEntryOrdersScriptFile << "instrument.open[" << bar->getBarOffset() << "]";
}

void
TradingBloxCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mEntryOrdersScriptFile << "instrument.high[" << bar->getBarOffset() << "]";
}

void
TradingBloxCodeGenVisitor::visit (PriceBarLow *bar)
{
  mEntryOrdersScriptFile << "instrument.low[" << bar->getBarOffset() << "]";
}

void
TradingBloxCodeGenVisitor::visit (PriceBarClose *bar)
{
  mEntryOrdersScriptFile << "instrument.close[" << bar->getBarOffset() << "]";
}

void
TradingBloxCodeGenVisitor::visit (GreaterThanExpr *expr)
{
  if (firstSubExpressionVisited == false)
    mEntryOrdersScriptFile << "\t\t\t(";
  else
    {
      mEntryOrdersScriptFile << "(";
      firstSubExpressionVisited = false;
    }

  expr->getLHS()->accept (*this);
  mEntryOrdersScriptFile << " > ";
  expr->getRHS()->accept (*this);
  mEntryOrdersScriptFile << ")";
}

void
TradingBloxCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mEntryOrdersScriptFile << " AND " << std::endl;
  expr->getRHS()->accept (*this);
}

void
TradingBloxCodeGenVisitor::visit (PatternDescription *desc)
{
  mEntryOrdersScriptFile << "\t" << "\'{File:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
    

}

void TradingBloxCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\tlongEntryFound = 1" << std::endl;
  //mEntryOrdersScriptFile << "\t\t\tbroker.EnterLongOnOpen (longStop)" << std::endl;
}

void TradingBloxCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\tshortEntryFound = 1" << std::endl;
  //mEntryOrdersScriptFile << "\t\t\tbroker.EnterShortOnOpen (shortStop)" << std::endl;
}

bool TradingBloxCodeGenVisitor::isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern)
{
  /*decimal7 threshold(1.05);

  decimal7 target2 = *(pattern->getProfitTarget()->getProfitTarget());
  decimal7 stop2 = *(pattern->getStopLoss()->getStopLoss());

  decimal7 ratio = target2 / stop2;
  if (ratio >= threshold)
    return true;
    else */

    return false;
}

void TradingBloxCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this);
  mEntryOrdersScriptFile << std::endl;
  
  if (pattern->isLongPattern())
    mEntryOrdersScriptFile << "\t\tIf (longEntryFound = 0) and ";
  else
    mEntryOrdersScriptFile << "\t\tIf (shortEntryFound = 0) and ";

 if (pattern->hasVolatilityAttribute())
    {
      if (pattern->isLowVolatilityPattern())
	mEntryOrdersScriptFile << "(lowVolatility = 1) and ";
      else if (pattern->isHighVolatilityPattern())
	mEntryOrdersScriptFile << "(highVolatility = 1) and ";
      else if (pattern->isVeryHighVolatilityPattern())
	mEntryOrdersScriptFile << "(vHighVolatility = 1) and ";
    }

  if (isHighRewardToRiskRatioPattern (pattern))
    {
      mEntryOrdersScriptFile << "(TradeHighRewardToRiskPatterns = true) and " << std::endl;
      firstSubExpressionVisited = false;
    }
  else
    firstSubExpressionVisited = true;
  
  pattern->getPatternExpression()->accept (*this);
  mEntryOrdersScriptFile << " Then" << std::endl << std::endl;
  pattern->getStopLoss()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getMarketEntry()->accept (*this);

  mEntryOrdersScriptFile << "\t\tendif" << std::endl;
}

////////////////////////////////////////////////////
/// class TradingBloxRADCodeGenVisitor
////////////////////////////////////////////////////

TradingBloxRADCodeGenVisitor::TradingBloxRADCodeGenVisitor (PriceActionLabSystem *system,
							    const std::string& bloxOutFileName)
  : TradingBloxCodeGenVisitor (system, bloxOutFileName)
{}

TradingBloxRADCodeGenVisitor::~TradingBloxRADCodeGenVisitor()
{}


void TradingBloxRADCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  std::ofstream *outFile = getOutputFileStream();

  *outFile << "VARIABLES: shortStop, longStop TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: longEntryFound, shortEntryFound TYPE: Integer" << std::endl << std::endl;
  *outFile << "VARIABLES: lowVolatility, highVolatility TYPE: Integer" << std::endl << std::endl;
  *outFile << "' NOTE: declare profitTargetInPercentForTrade and stopInPercentForTrade as floating point IPV variables" << std::endl;
}

void 
TradingBloxRADCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (1.0 - (" << *stop << "/100))" << std::endl;
  *outFile << "\t\t\tlongStop = instrument.RoundTick (instrument.close * stopInPercentForTrade)" << std::endl;
}

void TradingBloxRADCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForLongTrade = (1.0 + (" << *target << "/100))" << std::endl;
}

void TradingBloxRADCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForShortTrade = (1.0 - (" << *target << "/100))" << std::endl;
}

void 
TradingBloxRADCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (1.0 + (" << *stop << "/100))" << std::endl;
  *outFile << "\t\t\tshortStop = instrument.RoundTick (instrument.close * stopInPercentForTrade)" << std::endl;
}

////////////////////////////////////////////////////
/// class TradingBloxPointAdjustedCodeGenVisitor
////////////////////////////////////////////////////

TradingBloxPointAdjustedCodeGenVisitor::TradingBloxPointAdjustedCodeGenVisitor (PriceActionLabSystem *system, const std::string& bloxOutFileName)
  : TradingBloxCodeGenVisitor (system, bloxOutFileName)
{}

TradingBloxPointAdjustedCodeGenVisitor::~TradingBloxPointAdjustedCodeGenVisitor()
{}


void TradingBloxPointAdjustedCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  std::ofstream *outFile = getOutputFileStream();

  *outFile << "VARIABLES: shortStop, longStop TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: shortStopDistance, longStopDistance TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: longEntryFound, shortEntryFound TYPE: Integer" << std::endl << std::endl;
  *outFile << "VARIABLES: lowVolatility, highVolatility, vHighVolatility TYPE: Integer" << std::endl << std::endl;
  *outFile << "' NOTE: declare profitTargetInPercentForTrade and stopInPercentForTrade as floating point IPV variables" << std::endl;
}

void 
TradingBloxPointAdjustedCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (" << *stop << "/100)" << std::endl;
  *outFile << "\t\t\tlongStopDistance = instrument.RoundTick (instrument.unadjustedclose * stopInPercentForTrade)" 
	   << std::endl;
  *outFile << "\t\t\tlongStop = instrument.close - longStopDistance" << std::endl;
}

void TradingBloxPointAdjustedCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForLongTrade = (" << *target << "/100)" << std::endl;
}

void TradingBloxPointAdjustedCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForShortTrade = (" << *target << "/100)" << std::endl;
}

void 
TradingBloxPointAdjustedCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (" << *stop << "/100)" << std::endl;
  *outFile << "\t\t\tshortStopDistance = instrument.RoundTick (instrument.unadjustedclose * stopInPercentForTrade)"
	   << std::endl;
  *outFile << "\t\t\tshortStop = instrument.close + shortStopDistance" << std::endl;
}
