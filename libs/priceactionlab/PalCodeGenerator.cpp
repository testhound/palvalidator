/**
 * @file PalCodeGenerator.cpp
 * @brief Implements the PalCodeGenerator class, a visitor for generating
 *        a textual representation of Price Action Lab (PAL) patterns.
 *
 * This generator outputs a human-readable string format that describes
 * the conditions, entry logic, stop-loss, and profit target for each pattern.
 * It supports an option to reverse the pattern logic (e.g., long to short).
 */
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream> // For std::cout for pattern counts

/**
 * @var firstSubExpressionVisited
 * @brief External global flag (defined elsewhere, likely PalCodeGenVisitor.cpp)
 *        used to track if the first sub-expression has been visited.
 *
 * This variable aids some visitor implementations in formatting expressions,
 * particularly with parentheses for nested conditions.
 */
extern bool firstSubExpressionVisited;

// PalCodeGenerator class implementation

/**
 * @brief Constructs a PalCodeGenerator.
 * @param system Pointer to the PriceActionLabSystem containing the patterns to be generated.
 * @param outputFileName The name of the file where the generated code will be written.
 * @param reversePattern A boolean flag; if true, the generated pattern logic is reversed
 *                       (e.g., long entries become short entries, profit targets and stop losses adjusted accordingly).
 */
PalCodeGenerator::PalCodeGenerator(PriceActionLabSystem *system,
				   const std::string& outputFileName,
				   bool reversePattern)
  : PalCodeGenVisitor(), // Call base class constructor
    mOutFile(outputFileName), // Initialize output file stream
    mTradingSystemPatterns(system), // Store pointer to the system of patterns
    mReversePattern(reversePattern) // Store the reverse pattern flag
{}

/**
 * @brief Destructor for PalCodeGenerator.
 * Closes the output file stream if it's open.
 */
PalCodeGenerator::~PalCodeGenerator()
{
  if (mOutFile.is_open())
  {
    mOutFile.close();
  }
}

/**
 * @brief Generates the textual representation for all patterns in the system.
 * Iterates through both long and short patterns stored in `mTradingSystemPatterns`,
 * calling the `accept` method on each to trigger the corresponding `visit` methods
 * for code generation. It also prints a header and counts of long/short patterns.
 */
void 
PalCodeGenerator::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  mOutFile << "Code For Selected Patterns" << std::endl;
  printPatternSeperator();
  mOutFile << std::endl;

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

  // Output counts to standard output
  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
  std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}
	
/**
 * @brief Gets the output file stream used by the generator.
 * @return Pointer to the std::ofstream object.
 */
std::ofstream *
PalCodeGenerator::getOutputFileStream()
{
  return &mOutFile;
}

/**
 * @brief Visits a PriceBarOpen node.
 * Generates a string like "OPEN OF N BARS AGO".
 * @param bar Pointer to the PriceBarOpen node.
 */
void
PalCodeGenerator::visit (PriceBarOpen *bar)
{
  mOutFile << "OPEN OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a PriceBarHigh node.
 * Generates a string like "HIGH OF N BARS AGO".
 * @param bar Pointer to the PriceBarHigh node.
 */
void
PalCodeGenerator::visit (PriceBarHigh *bar)
{
  mOutFile << "HIGH OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a PriceBarLow node.
 * Generates a string like "LOW OF N BARS AGO".
 * @param bar Pointer to the PriceBarLow node.
 */
void
PalCodeGenerator::visit (PriceBarLow *bar)
{
  mOutFile << "LOW OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a PriceBarClose node.
 * Generates a string like "CLOSE OF N BARS AGO".
 * @param bar Pointer to the PriceBarClose node.
 */
void
PalCodeGenerator::visit (PriceBarClose *bar)
{
  mOutFile << "CLOSE OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a VolumeBarReference node.
 * Generates a string like "VOLUME OF N BARS AGO".
 * @param bar Pointer to the VolumeBarReference node.
 */
void
PalCodeGenerator::visit (VolumeBarReference *bar)
{
  mOutFile << "VOLUME OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a Roc1BarReference node (1-period Rate of Change).
 * Generates a string like "ROC1 OF N BARS AGO".
 * @param bar Pointer to the Roc1BarReference node.
 */
void
PalCodeGenerator::visit (Roc1BarReference *bar)
{
  mOutFile << "ROC1 OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits an IBS1BarReference node (Internal Bar Strength Type 1).
 * Generates a string like "IBS1 OF N BARS AGO".
 * @param bar Pointer to the IBS1BarReference node.
 */
void PalCodeGenerator::visit (IBS1BarReference *bar)
{
  mOutFile << "IBS1 OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits an IBS2BarReference node (Internal Bar Strength Type 2).
 * Generates a string like "IBS2 OF N BARS AGO".
 * @param bar Pointer to the IBS2BarReference node.
 */
void PalCodeGenerator::visit (IBS2BarReference *bar)
{
  mOutFile << "IBS2 OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits an IBS3BarReference node (Internal Bar Strength Type 3).
 * Generates a string like "IBS3 OF N BARS AGO".
 * @param bar Pointer to the IBS3BarReference node.
 */
void PalCodeGenerator::visit (IBS3BarReference *bar)
{
  mOutFile << "IBS3 OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a MeanderBarReference node.
 * Generates a string like "MEANDER OF N BARS AGO".
 * @param bar Pointer to the MeanderBarReference node.
 */
void
PalCodeGenerator::visit (MeanderBarReference *bar)
{
  mOutFile << "MEANDER OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a VChartLowBarReference node.
 * Generates a string like "VCHARTLOW OF N BARS AGO".
 * @param bar Pointer to the VChartLowBarReference node.
 */
void
PalCodeGenerator::visit (VChartLowBarReference *bar)
{
  mOutFile << "VCHARTLOW OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a VChartHighBarReference node.
 * Generates a string like "VCHARTHIGH OF N BARS AGO".
 * @param bar Pointer to the VChartHighBarReference node.
 */
void
PalCodeGenerator::visit (VChartHighBarReference *bar)
{
  mOutFile << "VCHARTHIGH OF " << bar->getBarOffset() << " BARS AGO";
}

/**
 * @brief Visits a GreaterThanExpr node.
 * Generates a string representation of "LHS > RHS".
 * @param expr Pointer to the GreaterThanExpr node.
 */
void
PalCodeGenerator::visit (GreaterThanExpr *expr)
{
  expr->getLHS()->accept (*this); // Generate LHS
  mOutFile << " > ";
  expr->getRHS()->accept (*this); // Generate RHS
  mOutFile << std::endl;
}

/**
 * @brief Visits an AndExpr node.
 * Generates a string representation of "LHS AND RHS".
 * @param expr Pointer to the AndExpr node.
 */
void
PalCodeGenerator::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this); // Generate LHS
  mOutFile << "AND ";
  expr->getRHS()->accept (*this); // Generate RHS
}

/**
 * @brief Visits a PatternDescription node.
 * Generates a comment-like string containing the pattern's metadata.
 * Example: "{File:filename  Index: N  Index DATE: YYYYMMDD  PL: X%  PS: Y%  Trades: Z  CL: W }"
 * @param desc Pointer to the PatternDescription node.
 */
void
PalCodeGenerator::visit (PatternDescription *desc)
{
  mOutFile << "{File:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
}

/**
 * @brief Visits a LongMarketEntryOnOpen node.
 * Generates "THEN BUY NEXT BAR ON THE OPEN WITH" or "THEN SELL NEXT BAR ON THE OPEN WITH"
 * if `mReversePattern` is true.
 * @param entryStatement Pointer to the LongMarketEntryOnOpen node.
 */
void PalCodeGenerator::visit (LongMarketEntryOnOpen *entryStatement)
{
  if (!mReversePattern) // Standard long entry
    mOutFile << "THEN BUY NEXT BAR ON THE OPEN WITH" << std::endl;
  else // Reversed logic: long entry becomes short
    mOutFile << "THEN SELL NEXT BAR ON THE OPEN WITH" << std::endl;
}

/**
 * @brief Visits a ShortMarketEntryOnOpen node.
 * Generates "THEN SELL NEXT BAR ON THE OPEN WITH" or "THEN BUY NEXT BAR ON THE OPEN WITH"
 * if `mReversePattern` is true.
 * @param entryStatement Pointer to the ShortMarketEntryOnOpen node.
 */
void PalCodeGenerator:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  if (!mReversePattern) // Standard short entry
    mOutFile << "THEN SELL NEXT BAR ON THE OPEN WITH" << std::endl;
  else // Reversed logic: short entry becomes long
    mOutFile << "THEN BUY NEXT BAR ON THE OPEN WITH" << std::endl;
}

/**
 * @brief Prints a separator line to the output file.
 * Used to visually distinguish between different patterns in the generated output.
 */
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

/**
 * @brief Visits a PriceActionLabPattern node.
 * This is the main entry point for generating code for a complete pattern.
 * It orchestrates visits to the pattern's description, expression, market entry,
 * profit target, and stop loss components.
 * @param pattern Pointer to the PriceActionLabPattern node.
 */
void PalCodeGenerator::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this); // Visit description first
  mOutFile << std::endl;
  
  mOutFile << "IF "; // Start of the pattern condition
  
  pattern->getPatternExpression()->accept (*this); // Visit the core pattern expression
  pattern->getMarketEntry()->accept (*this);       // Visit market entry logic
  pattern->getProfitTarget()->accept (*this);    // Visit profit target logic
  pattern->getStopLoss()->accept (*this);          // Visit stop loss logic

  printPatternSeperator(); // Print separator after the pattern
  mOutFile << std::endl;
}

/**
 * @brief Visits a LongSideStopLossInPercent node.
 * Generates "AND STOP LOSS AT ENTRY PRICE - X %" or "AND STOP LOSS AT ENTRY PRICE + X %"
 * if `mReversePattern` is true (stop direction flips for reversed trade).
 * @param stopLoss Pointer to the LongSideStopLossInPercent node.
 */
void 
PalCodeGenerator::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  if (!mReversePattern) // Standard long stop loss
    *outFile << "AND STOP LOSS AT ENTRY PRICE - " << *stop << " %" <<std::endl;
  else // Reversed logic: long becomes short, so stop is above entry
    *outFile << "AND STOP LOSS AT ENTRY PRICE + " << *stop << " %" <<std::endl;
}

/**
 * @brief Visits a LongSideProfitTargetInPercent node.
 * Generates "PROFIT TARGET AT ENTRY PRICE + X %" or "PROFIT TARGET AT ENTRY PRICE - X %"
 * if `mReversePattern` is true (target direction flips for reversed trade).
 * @param profitTarget Pointer to the LongSideProfitTargetInPercent node.
 */
void PalCodeGenerator::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  if (!mReversePattern) // Standard long profit target
    *outFile << "PROFIT TARGET AT ENTRY PRICE + " << *target << " %" << std::endl;
  else // Reversed logic: long becomes short, so target is below entry
    *outFile << "PROFIT TARGET AT ENTRY PRICE - " << *target << " %" << std::endl;
}

/**
 * @brief Visits a ShortSideProfitTargetInPercent node.
 * Generates "PROFIT TARGET AT ENTRY PRICE - X %" or "PROFIT TARGET AT ENTRY PRICE + X %"
 * if `mReversePattern` is true (target direction flips for reversed trade).
 * @param profitTarget Pointer to the ShortSideProfitTargetInPercent node.
 */
void PalCodeGenerator::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  if (!mReversePattern) // Standard short profit target
    *outFile << "PROFIT TARGET AT ENTRY PRICE - " << *target << " %" << std::endl;
  else // Reversed logic: short becomes long, so target is above entry
    *outFile << "PROFIT TARGET AT ENTRY PRICE + " << *target << " %" << std::endl;
}

/**
 * @brief Visits a ShortSideStopLossInPercent node.
 * Generates "AND STOP LOSS AT ENTRY PRICE + X %" or "AND STOP LOSS AT ENTRY PRICE - X %"
 * if `mReversePattern` is true (stop direction flips for reversed trade).
 * @param stopLoss Pointer to the ShortSideStopLossInPercent node.
 */
void 
PalCodeGenerator::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  if (!mReversePattern) // Standard short stop loss
    *outFile << "AND STOP LOSS AT ENTRY PRICE + " << *stop << " %" <<std::endl;
  else // Reversed logic: short becomes long, so stop is below entry
    *outFile << "AND STOP LOSS AT ENTRY PRICE - " << *stop << " %" <<std::endl;
}

