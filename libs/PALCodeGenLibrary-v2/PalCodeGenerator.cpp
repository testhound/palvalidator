#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>

extern bool firstSubExpressionVisited;

///////////////////////////////////////
/// class EasyLanguageCodeGenVisitor
//////////////////////////////////////

PalCodeGenerator::PalCodeGenerator(PriceActionLabSystem *system,
				   const std::string& outputFileName)
  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mOutFile(outputFileName)
{}


PalCodeGenerator::~PalCodeGenerator()
{}

void 
PalCodeGenerator::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  mOutFile << "Code For Selected Patterns" << std::endl;
  printPatternSeperator();
  mOutFile << std::endl;

  unsigned int numLongPatterns = 0;

  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numLongPatterns++;
    }

  unsigned int numShortPatterns = 0;  
  for (it = mTradingSystemPatterns->patternShortsBegin(); 
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this);
      numShortPatterns++;
     }


  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
  std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}
	

std::ofstream *
PalCodeGenerator::getOutputFileStream()
{
  return &mOutFile;
}

void
PalCodeGenerator::visit (PriceBarOpen *bar)
{
  mOutFile << "OPEN OF " << bar->getBarOffset() << " BARS AGO";
}

void
PalCodeGenerator::visit (PriceBarHigh *bar)
{
  mOutFile << "HIGH OF " << bar->getBarOffset() << " BARS AGO";
}

void
PalCodeGenerator::visit (PriceBarLow *bar)
{
  mOutFile << "LOW OF " << bar->getBarOffset() << " BARS AGO";
}

void
PalCodeGenerator::visit (PriceBarClose *bar)
{
  mOutFile << "CLOSE OF " << bar->getBarOffset() << " BARS AGO";
}

void
PalCodeGenerator::visit (GreaterThanExpr *expr)
{
  expr->getLHS()->accept (*this);
  mOutFile << " > ";
  expr->getRHS()->accept (*this);
  mOutFile << std::endl;
}

void
PalCodeGenerator::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mOutFile << "AND ";
  expr->getRHS()->accept (*this);
}

void
PalCodeGenerator::visit (PatternDescription *desc)
{
  mOutFile << "{File:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
    

}

void PalCodeGenerator::visit (LongMarketEntryOnOpen *entryStatement)
{
  mOutFile << "THEN BUY NEXT BAR ON THE OPEN WITH" << std::endl;
}

void PalCodeGenerator:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mOutFile << "THEN SELL NEXT BAR ON THE OPEN WITH" << std::endl;
}


void PalCodeGenerator::printPatternSeperator()
{
  mOutFile << "--------------------";
  mOutFile << "--------------------";
  mOutFile << "--------------------";
  mOutFile << "--------------------";
  mOutFile << "--------------------";
  mOutFile << "--------------------";
  mOutFile << "----------" << std::endl;
}

void PalCodeGenerator::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this);
  mOutFile << std::endl;
  
  mOutFile << "IF ";
  
  pattern->getPatternExpression()->accept (*this);
  pattern->getMarketEntry()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getStopLoss()->accept (*this);
  printPatternSeperator();
  mOutFile << std::endl;
}

void 
PalCodeGenerator::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "AND STOP LOSS AT ENTRY PRICE - " << *stop << " %" <<std::endl;
}

void PalCodeGenerator::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "PROFIT TARGET AT ENTRY PRICE + " << *target << " %" << std::endl;
}

void PalCodeGenerator::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "PROFIT TARGET AT ENTRY PRICE - " << *target << " %" << std::endl;
}

void 
PalCodeGenerator::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "AND STOP LOSS AT ENTRY PRICE + " << *stop << " %" <<std::endl;
}

