/**
 * @file TradingBloxCodeGenerator.cpp
 * @brief Implements visitor classes for generating TradingBlox™ specific script code.
 *
 * This file contains the definitions for `TradingBloxCodeGenVisitor` and its derived
 * classes `TradingBloxRADCodeGenVisitor` and `TradingBloxPointAdjustedCodeGenVisitor`.
 * These visitors traverse a Price Action Lab AST (Abstract Syntax Tree) and output
 * script code compatible with the TradingBlox™ platform.
 */
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream> // For std::cout, std::cerr
#include <fstream>  // For std::ifstream

/**
 * @var firstSubExpressionVisited
 * @brief External global flag used to manage formatting for nested expressions,
 *        particularly for parentheses placement during code generation.
 */
extern bool firstSubExpressionVisited;

// --- TradingBloxCodeGenVisitor ---

/**
 * @brief Constructs a TradingBloxCodeGenVisitor.
 * @param system Pointer to the PriceActionLabSystem containing the patterns.
 * @param bloxOutFileName The name of the output file for the generated TradingBlox script.
 */
TradingBloxCodeGenVisitor::TradingBloxCodeGenVisitor(PriceActionLabSystem *system,
						     const std::string& bloxOutFileName)
  : PalCodeGenVisitor(), // Call base class constructor
    mTradingSystemPatterns(system), /**< @brief Shared pointer to the trading system patterns. */
    mEntryOrdersScriptFile(bloxOutFileName) /**< @brief Output file stream for the entry orders script. */
{}

/**
 * @brief Destructor for TradingBloxCodeGenVisitor.
 * Closes the output file stream if it's open.
 */
TradingBloxCodeGenVisitor::~TradingBloxCodeGenVisitor()
{
    if (mEntryOrdersScriptFile.is_open()) {
        mEntryOrdersScriptFile.close();
    }
}

/**
 * @brief Generates the TradingBlox script code for the provided patterns.
 *
 * The process involves:
 * 1. Calling `genCodeForVariablesInEntryScript()` (a pure virtual method implemented by derived classes)
 *    to declare necessary script variables.
 * 2. Setting up initial script logic, including a check for `instrument.currentBar > 10`
 *    and resetting `longEntryFound` and `shortEntryFound` flags.
 * 3. Generating code to set `lowVolatility`, `highVolatility`, and `vHighVolatility`
 *    variables based on `rankedSimonsVolatility` (assumed to be an existing TradingBlox variable).
 * 4. Iterating through all long patterns and then all short patterns from `mTradingSystemPatterns`,
 *    calling `accept()` on each to trigger their specific `visit()` methods for code generation.
 * 5. Appending content from a template file named "template/blox_entry_order_template".
 *    This template likely contains common TradingBlox script structures for order execution.
 * 6. Printing the counts of long and short patterns processed to `std::cout`.
 */
void 
TradingBloxCodeGenVisitor::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  genCodeForVariablesInEntryScript(); // Pure virtual, implemented by derived classes

  // Initial script setup
  mEntryOrdersScriptFile << "if (instrument.currentBar > 10) then " << std::endl;
  mEntryOrdersScriptFile << "\tlongEntryFound = 0" << std::endl;
  mEntryOrdersScriptFile << "\tshortEntryFound = 0" << std::endl;

  // Volatility setup based on 'rankedSimonsVolatility'
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

  unsigned int numLongPatterns = 0;
  // Generate code for long patterns
  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this); // Dispatch to visit(PriceActionLabPattern*)
      numLongPatterns++;
    }

  unsigned int numShortPatterns = 0;
  // Generate code for short patterns
  for (it = mTradingSystemPatterns->patternShortsBegin(); 
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this); // Dispatch to visit(PriceActionLabPattern*)
      numShortPatterns++;
    }

  // Append content from template file
  std::ifstream infile("template/blox_entry_order_template");
  std::string line;
  if (infile.is_open()) {
    while (std::getline(infile, line))
      {
        mEntryOrdersScriptFile << line << std::endl;
      }
    infile.close();
  } else {
    std::cerr << "Warning: Could not open template file template/blox_entry_order_template" << std::endl;
  }
  
  mEntryOrdersScriptFile <<  std::endl;
  mEntryOrdersScriptFile << "endif" << std::endl << std::endl; // Matches "if (instrument.currentBar > 10) then"

  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
  std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}
	
/**
 * @brief Gets the output file stream.
 * @return Pointer to the std::ofstream object used for writing the TradingBlox script.
 */
std::ofstream *
TradingBloxCodeGenVisitor::getOutputFileStream()
{
  return &mEntryOrdersScriptFile;
}

/**
 * @brief Visits a PriceBarOpen node.
 * Generates TradingBlox script: `instrument.open[barOffset]`.
 * @param bar Pointer to the PriceBarOpen node.
 */
void
TradingBloxCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mEntryOrdersScriptFile << "instrument.open[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a PriceBarHigh node.
 * Generates TradingBlox script: `instrument.high[barOffset]`.
 * @param bar Pointer to the PriceBarHigh node.
 */
void
TradingBloxCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mEntryOrdersScriptFile << "instrument.high[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a PriceBarLow node.
 * Generates TradingBlox script: `instrument.low[barOffset]`.
 * @param bar Pointer to the PriceBarLow node.
 */
void
TradingBloxCodeGenVisitor::visit (PriceBarLow *bar)
{
  mEntryOrdersScriptFile << "instrument.low[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a PriceBarClose node.
 * Generates TradingBlox script: `instrument.close[barOffset]`.
 * @param bar Pointer to the PriceBarClose node.
 */
void
TradingBloxCodeGenVisitor::visit (PriceBarClose *bar)
{
  mEntryOrdersScriptFile << "instrument.close[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a VolumeBarReference node.
 * Generates TradingBlox script: `instrument.volume[barOffset]`.
 * @param bar Pointer to the VolumeBarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (VolumeBarReference *bar)
{
  mEntryOrdersScriptFile << "instrument.volume[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a Roc1BarReference node.
 * Generates TradingBlox script: `roc1[barOffset]`.
 * Assumes `roc1` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the Roc1BarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (Roc1BarReference *bar)
{
  mEntryOrdersScriptFile << "roc1[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits an IBS1BarReference node.
 * Generates TradingBlox script: `IBS1[barOffset]`.
 * Assumes `IBS1` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the IBS1BarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (IBS1BarReference *bar)
{
  mEntryOrdersScriptFile << "IBS1[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits an IBS2BarReference node.
 * Generates TradingBlox script: `IBS2[barOffset]`.
 * Assumes `IBS2` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the IBS2BarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (IBS2BarReference *bar)
{
  mEntryOrdersScriptFile << "IBS2[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits an IBS3BarReference node.
 * Generates TradingBlox script: `IBS3[barOffset]`.
 * Assumes `IBS3` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the IBS3BarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (IBS3BarReference *bar)
{
  mEntryOrdersScriptFile << "IBS3[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a MeanderBarReference node.
 * Generates TradingBlox script: `meanderVar[barOffset]`.
 * Assumes `meanderVar` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the MeanderBarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (MeanderBarReference *bar)
{
  mEntryOrdersScriptFile << "meanderVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a VChartLowBarReference node.
 * Generates TradingBlox script: `vchartLowVar[barOffset]`.
 * Assumes `vchartLowVar` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the VChartLowBarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (VChartLowBarReference *bar)
{
  mEntryOrdersScriptFile << "vchartLowVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a VChartHighBarReference node.
 * Generates TradingBlox script: `vchartHighVar[barOffset]`.
 * Assumes `vchartHighVar` is a pre-calculated array or series in TradingBlox.
 * @param bar Pointer to the VChartHighBarReference node.
 */
void
TradingBloxCodeGenVisitor::visit (VChartHighBarReference *bar)
{
  mEntryOrdersScriptFile << "vchartHighVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a GreaterThanExpr node.
 * Generates TradingBlox script for a greater than comparison: `(LHS > RHS)`.
 * Uses `firstSubExpressionVisited` to manage parentheses for nesting.
 * @param expr Pointer to the GreaterThanExpr node.
 */
void
TradingBloxCodeGenVisitor::visit (GreaterThanExpr *expr)
{
  if (firstSubExpressionVisited == false)
    mEntryOrdersScriptFile << "\t\t\t("; // Indent for the start of a main condition line
  else
    {
      mEntryOrdersScriptFile << "("; // Nested condition
      firstSubExpressionVisited = false;
    }

  expr->getLHS()->accept (*this);
  mEntryOrdersScriptFile << " > ";
  expr->getRHS()->accept (*this);
  mEntryOrdersScriptFile << ")";
}

/**
 * @brief Visits an AndExpr node.
 * Generates TradingBlox script for a logical AND: `LHS AND RHS`.
 * @param expr Pointer to the AndExpr node.
 */
void
TradingBloxCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mEntryOrdersScriptFile << " AND " << std::endl; // TradingBlox uses "AND"
  expr->getRHS()->accept (*this);
}

/**
 * @brief Visits a PatternDescription node.
 * Generates a TradingBlox comment line with the pattern's metadata.
 * Example: `'{File:filename  Index: N  Index DATE: YYYYMMDD  PL: X%  PS: Y%  Trades: Z  CL: W }`
 * @param desc Pointer to the PatternDescription node.
 */
void
TradingBloxCodeGenVisitor::visit (PatternDescription *desc)
{
  mEntryOrdersScriptFile << "\t" << "\'{File:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
}

/**
 * @brief Visits a LongMarketEntryOnOpen node.
 * Generates TradingBlox script: `longEntryFound = 1`.
 * @param entryStatement Pointer to the LongMarketEntryOnOpen node.
 */
void TradingBloxCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\tlongEntryFound = 1" << std::endl;
  // Original commented-out line for direct broker call:
  //mEntryOrdersScriptFile << "\t\t\tbroker.EnterLongOnOpen (longStop)" << std::endl;
}

/**
 * @brief Visits a ShortMarketEntryOnOpen node.
 * Generates TradingBlox script: `shortEntryFound = 1`.
 * @param entryStatement Pointer to the ShortMarketEntryOnOpen node.
 */
void TradingBloxCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\tshortEntryFound = 1" << std::endl;
  // Original commented-out line for direct broker call:
  //mEntryOrdersScriptFile << "\t\t\tbroker.EnterShortOnOpen (shortStop)" << std::endl;
}

/**
 * @brief Checks if a pattern has a high reward-to-risk ratio.
 * @param pattern Pointer to the PriceActionLabPattern to check.
 * @return Currently always returns `false` as the actual logic is commented out.
 * @note The original logic for calculating and checking the ratio is commented out.
 */
bool TradingBloxCodeGenVisitor::isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern)
{
  /*decimal7 threshold(1.05); // Example threshold

  decimal7 target2 = *(pattern->getProfitTarget()->getProfitTarget());
  decimal7 stop2 = *(pattern->getStopLoss()->getStopLoss());

  decimal7 ratio = target2 / stop2;
  if (ratio >= threshold)
    return true;
  else */
    return false; // Currently hardcoded to false
}

/**
 * @brief Visits a PriceActionLabPattern node to generate its TradingBlox script representation.
 *
 * Generates an `If ... Then ... endif` block in TradingBlox script.
 * The `If` condition includes checks for `longEntryFound`/`shortEntryFound`,
 * volatility attributes (`lowVolatility`, `highVolatility`, `vHighVolatility`),
 * and potentially high reward-to-risk patterns (though this part is currently disabled).
 * The core pattern expression is generated, followed by calls to visit stop-loss,
 * profit-target, and market entry nodes.
 *
 * @param pattern Pointer to the PriceActionLabPattern node.
 */
void TradingBloxCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this); // Generate pattern description comment
  mEntryOrdersScriptFile << std::endl;
  
  // Start the main 'If' condition for the pattern
  if (pattern->isLongPattern())
    mEntryOrdersScriptFile << "\t\tIf (longEntryFound = 0) and ";
  else
    mEntryOrdersScriptFile << "\t\tIf (shortEntryFound = 0) and ";

  // Add volatility conditions
 if (pattern->hasVolatilityAttribute())
    {
      if (pattern->isLowVolatilityPattern())
	mEntryOrdersScriptFile << "(lowVolatility = 1) and ";
      else if (pattern->isHighVolatilityPattern())
	mEntryOrdersScriptFile << "(highVolatility = 1) and ";
      else if (pattern->isVeryHighVolatilityPattern())
	mEntryOrdersScriptFile << "(vHighVolatility = 1) and ";
    }

  // Add high reward-to-risk condition (currently disabled)
  if (isHighRewardToRiskRatioPattern (pattern))
    {
      mEntryOrdersScriptFile << "(TradeHighRewardToRiskPatterns = true) and " << std::endl;
      firstSubExpressionVisited = false; // Reset for correct parenthesis in pattern expression
    }
  else
    firstSubExpressionVisited = true; // Assume it's not the very first part of a complex condition
  
  pattern->getPatternExpression()->accept (*this); // Generate the core pattern logic
  mEntryOrdersScriptFile << " Then" << std::endl << std::endl; // End of 'If' condition

  // Generate actions: stop loss, profit target, market entry
  pattern->getStopLoss()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getMarketEntry()->accept (*this);

  mEntryOrdersScriptFile << "\t\tendif" << std::endl; // End of 'If' block
}

// --- TradingBloxRADCodeGenVisitor ---

/**
 * @brief Constructs a TradingBloxRADCodeGenVisitor.
 * Specializes `TradingBloxCodeGenVisitor` for Risk-Adjusted Dollar (RAD) strategies.
 * @param system Pointer to the PriceActionLabSystem.
 * @param bloxOutFileName The name of the output file for the TradingBlox script.
 */
TradingBloxRADCodeGenVisitor::TradingBloxRADCodeGenVisitor (PriceActionLabSystem *system,
							    const std::string& bloxOutFileName)
  : TradingBloxCodeGenVisitor (system, bloxOutFileName) // Call base constructor
{}

/**
 * @brief Destructor for TradingBloxRADCodeGenVisitor.
 */
TradingBloxRADCodeGenVisitor::~TradingBloxRADCodeGenVisitor()
{}

/**
 * @brief Generates TradingBlox script code for declaring variables specific to RAD strategies.
 * This includes `shortStop`, `longStop`, entry flags, volatility flags, and a note
 * about IPV (Instrument Property Variables) for profit target and stop percentages.
 */
void TradingBloxRADCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  std::ofstream *outFile = getOutputFileStream();

  *outFile << "VARIABLES: shortStop, longStop TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: longEntryFound, shortEntryFound TYPE: Integer" << std::endl << std::endl;
  *outFile << "VARIABLES: lowVolatility, highVolatility TYPE: Integer" << std::endl << std::endl; // Note: vHighVolatility not declared here, unlike PointAdjusted
  *outFile << "' NOTE: declare profitTargetInPercentForTrade and stopInPercentForTrade as floating point IPV variables" << std::endl;
}

/**
 * @brief Visits a LongSideStopLossInPercent node for RAD strategies.
 * Generates TradingBlox script to set `stopInPercentForTrade` (as 1 - stop%/100)
 * and `longStop` (Close * stopInPercentForTrade, rounded to tick).
 * @param stopLoss Pointer to the LongSideStopLossInPercent node.
 */
void 
TradingBloxRADCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (1.0 - (" << *stop << "/100))" << std::endl;
  *outFile << "\t\t\tlongStop = instrument.RoundTick (instrument.close * stopInPercentForTrade)" << std::endl;
}

/**
 * @brief Visits a LongSideProfitTargetInPercent node for RAD strategies.
 * Generates TradingBlox script to set `profitTargetInPercentForLongTrade` (as 1 + target%/100).
 * @param profitTarget Pointer to the LongSideProfitTargetInPercent node.
 */
void TradingBloxRADCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForLongTrade = (1.0 + (" << *target << "/100))" << std::endl;
}

/**
 * @brief Visits a ShortSideProfitTargetInPercent node for RAD strategies.
 * Generates TradingBlox script to set `profitTargetInPercentForShortTrade` (as 1 - target%/100).
 * @param profitTarget Pointer to the ShortSideProfitTargetInPercent node.
 */
void TradingBloxRADCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForShortTrade = (1.0 - (" << *target << "/100))" << std::endl;
}

/**
 * @brief Visits a ShortSideStopLossInPercent node for RAD strategies.
 * Generates TradingBlox script to set `stopInPercentForTrade` (as 1 + stop%/100)
 * and `shortStop` (Close * stopInPercentForTrade, rounded to tick).
 * @param stopLoss Pointer to the ShortSideStopLossInPercent node.
 */
void 
TradingBloxRADCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopInPercentForTrade = (1.0 + (" << *stop << "/100))" << std::endl;
  *outFile << "\t\t\tshortStop = instrument.RoundTick (instrument.close * stopInPercentForTrade)" << std::endl;
}

// --- TradingBloxPointAdjustedCodeGenVisitor ---

/**
 * @brief Constructs a TradingBloxPointAdjustedCodeGenVisitor.
 * Specializes `TradingBloxCodeGenVisitor` for point-adjusted strategies.
 * @param system Pointer to the PriceActionLabSystem.
 * @param bloxOutFileName The name of the output file for the TradingBlox script.
 */
TradingBloxPointAdjustedCodeGenVisitor::TradingBloxPointAdjustedCodeGenVisitor (PriceActionLabSystem *system, const std::string& bloxOutFileName)
  : TradingBloxCodeGenVisitor (system, bloxOutFileName) // Call base constructor
{}

/**
 * @brief Destructor for TradingBloxPointAdjustedCodeGenVisitor.
 */
TradingBloxPointAdjustedCodeGenVisitor::~TradingBloxPointAdjustedCodeGenVisitor()
{}

/**
 * @brief Generates TradingBlox script code for declaring variables specific to point-adjusted strategies.
 * This includes stops, stop distances, entry flags, volatility flags (including `vHighVolatility`),
 * and a note about IPV variables.
 */
void TradingBloxPointAdjustedCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  std::ofstream *outFile = getOutputFileStream();

  *outFile << "VARIABLES: shortStop, longStop TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: shortStopDistance, longStopDistance TYPE: Floating" << std::endl;
  *outFile << "VARIABLES: longEntryFound, shortEntryFound TYPE: Integer" << std::endl << std::endl;
  *outFile << "VARIABLES: lowVolatility, highVolatility, vHighVolatility TYPE: Integer" << std::endl << std::endl;
  *outFile << "' NOTE: declare profitTargetInPercentForTrade and stopInPercentForTrade as floating point IPV variables" << std::endl;
}

/**
 * @brief Visits a LongSideStopLossInPercent node for point-adjusted strategies.
 * Generates TradingBlox script to set `stopInPercentForTrade` (as stop%/100),
 * calculates `longStopDistance` based on `instrument.unadjustedclose`,
 * and then sets `longStop` as `instrument.close - longStopDistance`.
 * @param stopLoss Pointer to the LongSideStopLossInPercent node.
 */
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

/**
 * @brief Visits a LongSideProfitTargetInPercent node for point-adjusted strategies.
 * Generates TradingBlox script to set `profitTargetInPercentForLongTrade` (as target%/100).
 * @param profitTarget Pointer to the LongSideProfitTargetInPercent node.
 */
void TradingBloxPointAdjustedCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForLongTrade = (" << *target << "/100)" << std::endl;
}

/**
 * @brief Visits a ShortSideProfitTargetInPercent node for point-adjusted strategies.
 * Generates TradingBlox script to set `profitTargetInPercentForShortTrade` (as target%/100).
 * @param profitTarget Pointer to the ShortSideProfitTargetInPercent node.
 */
void TradingBloxPointAdjustedCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetInPercentForShortTrade = (" << *target << "/100)" << std::endl;
}

/**
 * @brief Visits a ShortSideStopLossInPercent node for point-adjusted strategies.
 * Generates TradingBlox script to set `stopInPercentForTrade` (as stop%/100),
 * calculates `shortStopDistance` based on `instrument.unadjustedclose`,
 * and then sets `shortStop` as `instrument.close + shortStopDistance`.
 * @param stopLoss Pointer to the ShortSideStopLossInPercent node.
 */
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
