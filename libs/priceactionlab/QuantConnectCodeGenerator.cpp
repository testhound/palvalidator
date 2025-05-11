#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>

extern bool firstSubExpressionVisited;

///////////////////////////////////////
/// class QuantConnectCodeGenVisitor
//////////////////////////////////////

QuantConnectCodeGenVisitor::QuantConnectCodeGenVisitor(PriceActionLabSystem *system,
						       const std::string& bloxOutFileName)
  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mEntryOrdersScriptFile(bloxOutFileName)
{}


QuantConnectCodeGenVisitor::~QuantConnectCodeGenVisitor()
{}

void QuantConnectCodeGenVisitor::genCodeForCommonVariables()
{
  //std::ofstream *outFile = getOutputFileStream();
}

void QuantConnectCodeGenVisitor::genCodeToInitVolatility(bool shortSide)
{
  //std::ofstream *outFile = getOutputFileStream();
}

void QuantConnectCodeGenVisitor::genCodeForCommonEntry()
{
  //std::ofstream *outFile = getOutputFileStream();

}

void QuantConnectCodeGenVisitor::genCommonCodeForLongExitPrologue()
{
  //std::ofstream *outFile = getOutputFileStream();

}

void QuantConnectCodeGenVisitor::genCommonCodeForShortExitPrologue()
{
  //std::ofstream *outFile = getOutputFileStream();

}

void QuantConnectCodeGenVisitor::genCodeForCommonVariableInit()
{
  //std::ofstream *outFile = getOutputFileStream();

}

void 
QuantConnectCodeGenVisitor::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  genCodeForCommonVariables();
  genCodeForVariablesInEntryScript();

  genCodeForCommonVariableInit();
  genCodeToInitializeVariables();

  unsigned int numLongPatterns = 0;

  mEntryOrdersScriptFile << "\tpublic bool isLongEntry (decimal [] open, decimal [] high, ";
  mEntryOrdersScriptFile << "decimal [] low, decimal [] close)" << std::endl;
  mEntryOrdersScriptFile << "\t{" << std::endl;
  
  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numLongPatterns++;
    }

  mEntryOrdersScriptFile << std::endl;
  mEntryOrdersScriptFile << "\t\treturn false;" << std::endl;
  mEntryOrdersScriptFile << "\t}" << std::endl;
  mEntryOrdersScriptFile << std::endl;

  
  unsigned int numShortPatterns = 0;  
  mEntryOrdersScriptFile << "\tpublic bool isShortEntry (decimal [] open, decimal [] high, ";
  mEntryOrdersScriptFile << "decimal [] low, decimal [] close)" << std::endl;
  mEntryOrdersScriptFile << "\t{" << std::endl;

  for (it = mTradingSystemPatterns->patternShortsBegin(); 
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numShortPatterns++;
     }

  mEntryOrdersScriptFile << std::endl;
  mEntryOrdersScriptFile << "\t\treturn false;" << std::endl;
  mEntryOrdersScriptFile << "\t}" << std::endl << std::endl;
  
  genCodeForCommonEntry();
  genCodeForEntryExit();  

  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
  std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}
	

std::ofstream *
QuantConnectCodeGenVisitor::getOutputFileStream()
{
  return &mEntryOrdersScriptFile;
}

void
QuantConnectCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mEntryOrdersScriptFile << "open[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mEntryOrdersScriptFile << "high[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (PriceBarLow *bar)
{
  mEntryOrdersScriptFile << "low[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (PriceBarClose *bar)
{
  mEntryOrdersScriptFile << "close[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (VolumeBarReference *bar)
{
  mEntryOrdersScriptFile << "volume[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (Roc1BarReference *bar)
{
  mEntryOrdersScriptFile << "RateOfChange(Close, 1)[" << bar->getBarOffset() << "]";
}

void QuantConnectCodeGenVisitor::visit (IBS1BarReference *bar)
{
  mEntryOrdersScriptFile << "IBS(1)[" << bar->getBarOffset() << "]";
}

void QuantConnectCodeGenVisitor::visit (IBS2BarReference *bar)
{
  mEntryOrdersScriptFile << "IBS(2)[" << bar->getBarOffset() << "]";
}

void QuantConnectCodeGenVisitor::visit (IBS3BarReference *bar)
{
  mEntryOrdersScriptFile << "IBS(3)[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (MeanderBarReference *bar)
{
  mEntryOrdersScriptFile << "meanderVar[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (VChartLowBarReference *bar)
{
  mEntryOrdersScriptFile << "vchartLowVar[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (VChartHighBarReference *bar)
{
  mEntryOrdersScriptFile << "vchartHighVar[" << bar->getBarOffset() << "]";
}

void
QuantConnectCodeGenVisitor::visit (GreaterThanExpr *expr)
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
QuantConnectCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mEntryOrdersScriptFile << " & " << std::endl;
  expr->getRHS()->accept (*this);
}

void
QuantConnectCodeGenVisitor::visit (PatternDescription *desc)
{
  mEntryOrdersScriptFile << "\t\t" << "//FILE:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
    

}

void QuantConnectCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\treturn true;" << std::endl;
}

void QuantConnectCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\treturn true;" << std::endl;
}


void QuantConnectCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  static int numLongPatterns = 0;
  static int numShortPatterns = 0;
  
  pattern->getPatternDescription()->accept (*this);
  mEntryOrdersScriptFile << std::endl;
  
  if (pattern->isLongPattern())
    {
      numLongPatterns++;

      if (numLongPatterns > 1)
	mEntryOrdersScriptFile << "\t\telse if (";
      else
	mEntryOrdersScriptFile << "\t\tif (";
    }
  else
    {
      numShortPatterns++;

      if (numShortPatterns > 1)
	mEntryOrdersScriptFile << "\t\telse if (";
      else
	mEntryOrdersScriptFile << "\t\tif (";
    }

  if (pattern->hasVolatilityAttribute())
    {
      if (pattern->isLowVolatilityPattern())
	mEntryOrdersScriptFile << "lowVolatility & ";
      else if (pattern->isHighVolatilityPattern())
	mEntryOrdersScriptFile << "highVolatility & ";
      else if (pattern->isVeryHighVolatilityPattern())
	mEntryOrdersScriptFile << "vHighVolatility & ";
    }

  if (pattern->hasPortfolioAttribute())
    {
      if (pattern->isFilteredLongPattern())
	mEntryOrdersScriptFile << "tradeLongSide & ";
      else if (pattern->isFilteredShortPattern())
	mEntryOrdersScriptFile << "tradeShortSide & ";
    }

  firstSubExpressionVisited = true;
  
  pattern->getPatternExpression()->accept (*this);
  mEntryOrdersScriptFile << ")" << std::endl;
  mEntryOrdersScriptFile << "\t\t{" << std::endl;
  pattern->getStopLoss()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getMarketEntry()->accept (*this);

  mEntryOrdersScriptFile << "\t\t}" << std::endl;
}

////////////////////////////////////////////////////
/// class QuantConnectEquityCodeGenVisitor
////////////////////////////////////////////////////

QuantConnectEquityCodeGenVisitor::QuantConnectEquityCodeGenVisitor (PriceActionLabSystem *system, 
							      const std::string& outputFileName)
  : QuantConnectCodeGenVisitor (system, outputFileName)
{}

QuantConnectEquityCodeGenVisitor::~QuantConnectEquityCodeGenVisitor()
{}

void QuantConnectEquityCodeGenVisitor::genCodeToInitializeVariables()
{


 
}

void QuantConnectEquityCodeGenVisitor::genCodeForEntryExit()
{
std::ofstream *outFile = getOutputFileStream();

 *outFile <<  std::endl;
}

void QuantConnectEquityCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  //  std::ofstream *outFile = getOutputFileStream();



}

void 
QuantConnectEquityCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
}

void QuantConnectEquityCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
}

void QuantConnectEquityCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
}

void 
QuantConnectEquityCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
}
