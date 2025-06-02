#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>

extern bool firstSubExpressionVisited;

/**
 * @file EasyLanguageFromTemplateCodeGen.cpp
 * @brief Implements the EasyLanguage code generation visitor using a template file.
 */
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>

extern bool firstSubExpressionVisited;

///////////////////////////////////////
/// class EasyLanguageCodeGenVisitor
//////////////////////////////////////

/** @brief Marker string in template for inserting long entry setups. */
const std::string EasyLanguageCodeGenVisitor::LONG_PATTERNS_MARKER = "////// LONG ENTRY SETUPS";
/** @brief Marker string in template for inserting short entry setups. */
const std::string EasyLanguageCodeGenVisitor::SHORT_PATTERNS_MARKER = "////// SHORT ENTRY SETUPS";
/** @brief Marker string in template for inserting logic for setting long targets. */
const std::string EasyLanguageCodeGenVisitor::LONG_TARGET_SETTER_MARKER = "////// SETTING LONG TARGETS";
/** @brief Marker string in template for inserting logic for setting short targets. */
const std::string EasyLanguageCodeGenVisitor::SHORT_TARGET_SETTER_MARKER = "////// SETTING SHORT TARGETS";

/**
 * @brief Constructs an EasyLanguageCodeGenVisitor.
 * @param system Pointer to the PriceActionLabSystem containing the trading patterns.
 * @param templateFileName Path to the EasyLanguage template file.
 * @param outputFileName Path to the output file where generated EasyLanguage code will be written.
 * @param dev1Detail Stop-loss and profit-target details for "Deviation 1" patterns.
 * @param dev2Detail Stop-loss and profit-target details for "Deviation 2" patterns.
 */
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

/**
 * @brief Destructor for EasyLanguageCodeGenVisitor.
 */
EasyLanguageCodeGenVisitor::~EasyLanguageCodeGenVisitor()
{}

/**
 * @brief Inserts generated EasyLanguage code for long patterns into the output file stream.
 * Iterates through all long patterns in the system and calls `accept` on each to generate its code.
 */
void EasyLanguageCodeGenVisitor::insertLongPatterns()
{

  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  unsigned int numLongPatterns = 0;

  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this); // Polymorphic call to the appropriate visit method
      numLongPatterns++;
    }
  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
}

/**
 * @brief Inserts generated EasyLanguage code for short patterns into the output file stream.
 * Iterates through all short patterns in the system and calls `accept` on each to generate its code.
 */
void EasyLanguageCodeGenVisitor::insertShortPatterns()
{

  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  unsigned int numShortPatterns = 0;
  for (it = mTradingSystemPatterns->patternShortsBegin();
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this); // Polymorphic call to the appropriate visit method
      numShortPatterns++;
     }
    std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}

/**
 * @brief Generates the complete EasyLanguage code by processing the template file.
 * Reads the template file line by line, replacing marker strings with generated code sections.
 * @throws std::runtime_error if any of the required markers are not found in the template file.
 */
void
EasyLanguageCodeGenVisitor::generateCode()
{
  std::ofstream *outFile = getOutputFileStream();
  //line by line parsing
  std::string line;
  bool longInserted = false, shortInserted = false, longTargetsSet = false, shortTargetsSet = false;

  if (!mTemplateFile.is_open()) {
      throw std::runtime_error("EasyLanguage template file not open: " + std::string(strerror(errno)));
  }
  if (!outFile || !outFile->is_open()) {
      throw std::runtime_error("EasyLanguage output file not open: " + std::string(strerror(errno)));
  }

  while (std::getline(mTemplateFile, line))
  {
      if (line.find(LONG_PATTERNS_MARKER) != std::string::npos) {
          insertLongPatterns();
          longInserted = true;
        }
      else if (line.find(SHORT_PATTERNS_MARKER) != std::string::npos) {
          insertShortPatterns();
          shortInserted = true;
        }
      else if (line.find(LONG_TARGET_SETTER_MARKER) != std::string::npos) {
          setStopTargetLong(); // Pure virtual, implemented by derived classes
          longTargetsSet = true;
        }
      else if (line.find(SHORT_TARGET_SETTER_MARKER) != std::string::npos) {
          setStopTargetShort(); // Pure virtual, implemented by derived classes
          shortTargetsSet = true;
        }
      else {
          *outFile << line << std::endl;
        }
  }
  // After processing all lines, check if all markers were found
  if (!(longInserted && shortInserted && longTargetsSet && shortTargetsSet))
    throw std::runtime_error("Invalid EL Template file. Markers missing. Status: longInserted:" + std::to_string(longInserted)
                             + ", shortInserted:" + std::to_string(shortInserted)
                             + ", longTargetsSet:" + std::to_string(longTargetsSet)
                             + ", shortTargetsSet:" + std::to_string(shortTargetsSet));
}

/**
 * @brief Gets the output file stream for writing the generated EasyLanguage code.
 * @return Pointer to the std::ofstream object associated with the output file.
 */
std::ofstream *
EasyLanguageCodeGenVisitor::getOutputFileStream()
{
  return &mEasyLanguageFileName;
}

/**
 * @brief Generates EasyLanguage code for a PriceBarOpen AST node.
 * @param bar Pointer to the PriceBarOpen node.
 */
void
EasyLanguageCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mEasyLanguageFileName << "open[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a PriceBarHigh AST node.
 * @param bar Pointer to the PriceBarHigh node.
 */
void
EasyLanguageCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mEasyLanguageFileName << "high[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a PriceBarLow AST node.
 * @param bar Pointer to the PriceBarLow node.
 */
void
EasyLanguageCodeGenVisitor::visit (PriceBarLow *bar)
{
  mEasyLanguageFileName << "low[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a PriceBarClose AST node.
 * @param bar Pointer to the PriceBarClose node.
 */
void
EasyLanguageCodeGenVisitor::visit (PriceBarClose *bar)
{
  mEasyLanguageFileName << "close[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a VolumeBarReference AST node.
 * @param bar Pointer to the VolumeBarReference node.
 */
void
EasyLanguageCodeGenVisitor::visit (VolumeBarReference *bar)
{
  mEasyLanguageFileName << "volume[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a Roc1BarReference AST node.
 * @param bar Pointer to the Roc1BarReference node.
 */
void
EasyLanguageCodeGenVisitor::visit (Roc1BarReference *bar)
{
  mEasyLanguageFileName << "RateOfChange(Close, 1)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for an IBS1BarReference AST node.
 * @param bar Pointer to the IBS1BarReference node.
 */
void EasyLanguageCodeGenVisitor::visit (IBS1BarReference *bar)
{
  mEasyLanguageFileName << "IBS(1)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for an IBS2BarReference AST node.
 * @param bar Pointer to the IBS2BarReference node.
 */
void EasyLanguageCodeGenVisitor::visit (IBS2BarReference *bar)
{
  mEasyLanguageFileName << "IBS(2)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for an IBS3BarReference AST node.
 * @param bar Pointer to the IBS3BarReference node.
 */
void EasyLanguageCodeGenVisitor::visit (IBS3BarReference *bar)
{
  mEasyLanguageFileName << "IBS(3)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a MeanderBarReference AST node.
 * @param bar Pointer to the MeanderBarReference node.
 */
void
EasyLanguageCodeGenVisitor::visit (MeanderBarReference *bar)
{
  mEasyLanguageFileName << "meanderVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a VChartLowBarReference AST node.
 * @param bar Pointer to the VChartLowBarReference node.
 */
void
EasyLanguageCodeGenVisitor::visit (VChartLowBarReference *bar)
{
  mEasyLanguageFileName << "vchartLowVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a VChartHighBarReference AST node.
 * @param bar Pointer to the VChartHighBarReference node.
 */
void
EasyLanguageCodeGenVisitor::visit (VChartHighBarReference *bar)
{
  mEasyLanguageFileName << "vchartHighVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Generates EasyLanguage code for a GreaterThanExpr AST node.
 * @param expr Pointer to the GreaterThanExpr node.
 */
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

/**
 * @brief Generates EasyLanguage code for an AndExpr AST node.
 * @param expr Pointer to the AndExpr node.
 */
void
EasyLanguageCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mEasyLanguageFileName << " and " << std::endl;
  expr->getRHS()->accept (*this);
}

/**
 * @brief Generates EasyLanguage code comment for a PatternDescription AST node.
 * @param desc Pointer to the PatternDescription node.
 */
void
EasyLanguageCodeGenVisitor::visit (PatternDescription *desc)
{
  mEasyLanguageFileName << "\t\t" << "//FILE:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
                         << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;

}

/**
 * @brief Generates EasyLanguage code for a LongMarketEntryOnOpen AST node.
 * @param entryStatement Pointer to the LongMarketEntryOnOpen node.
 */
void EasyLanguageCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mEasyLanguageFileName << "\t\t\tlongEntryFound = true;" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a ShortMarketEntryOnOpen AST node.
 * @param entryStatement Pointer to the ShortMarketEntryOnOpen node.
 */
void EasyLanguageCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mEasyLanguageFileName << "\t\t\tshortEntryFound = true;" << std::endl;
}

//bool EasyLanguageCodeGenVisitor::isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern)
//{
//  decimal7 threshold(1.25);

//  decimal7 target2 = *(pattern->getProfitTarget()->getProfitTarget());
//  decimal7 stop2 = *(pattern->getStopLoss()->getStopLoss());

//  decimal7 ratio = target2 / stop2;
//  if (ratio >= threshold)
//    return true;
//  else
//    return false;
//}

/**
 * @brief Checks if the given pattern matches the "Deviation 1" stop/target details.
 * @param pattern Pointer to the PriceActionLabPattern to check.
 * @return True if the pattern's stop-loss and profit-target match mDev1Detail, false otherwise.
 */
bool EasyLanguageCodeGenVisitor::isDev1Pattern(PriceActionLabPattern *pattern)
{
  return ((pattern->getStopLossAsDecimal() == mDev1Detail.getStopLoss()) &&
          (pattern->getProfitTargetAsDecimal() == mDev1Detail.getProfitTarget()));
}

/**
 * @brief Checks if the given pattern matches the "Deviation 2" stop/target details.
 * @param pattern Pointer to the PriceActionLabPattern to check.
 * @return True if the pattern's stop-loss and profit-target match mDev2Detail, false otherwise.
 */
bool EasyLanguageCodeGenVisitor::isDev2Pattern(PriceActionLabPattern *pattern)
{
  return ((pattern->getStopLossAsDecimal() == mDev2Detail.getStopLoss()) &&
          (pattern->getProfitTargetAsDecimal() == mDev2Detail.getProfitTarget()));

}

/**
 * @brief Generates EasyLanguage code for a PriceActionLabPattern AST node.
 * This method constructs the main conditional logic for a trading pattern in EasyLanguage.
 * @param pattern Pointer to the PriceActionLabPattern node.
 */
void EasyLanguageCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  pattern->getPatternDescription()->accept (*this); // Generate comment with pattern details
  mEasyLanguageFileName << std::endl;

  // Start the 'if' condition
  if (pattern->isLongPattern())
    {
      mEasyLanguageFileName << "\t\tif (longEntryFound = false) and ";
    }
  else // Short pattern
    {
      mEasyLanguageFileName << "\t\tif (shortEntryFound = false) and ";
    }

  // Add conditions for deviation type (tradeSys1 or tradeSys2)
  if (isDev1Pattern (pattern))
    mEasyLanguageFileName << "(tradeSys1 = true) and ";
  else if (isDev2Pattern (pattern))
    mEasyLanguageFileName << "(tradeSys2 = true) and ";

  // Add conditions for volatility attributes
  if (pattern->hasVolatilityAttribute())
    {
      if (pattern->isLowVolatilityPattern())
        mEasyLanguageFileName << "lowVolatility and ";
      else if (pattern->isHighVolatilityPattern())
        mEasyLanguageFileName << "highVolatility and ";
      else if (pattern->isVeryHighVolatilityPattern())
        mEasyLanguageFileName << "vHighVolatility and ";
    }

  // Add conditions for portfolio attributes (filtering by trade side)
  if (pattern->hasPortfolioAttribute())
    {
      if (pattern->isFilteredLongPattern())
        mEasyLanguageFileName << "tradeLongSide and ";
      else if (pattern->isFilteredShortPattern())
        mEasyLanguageFileName << "tradeShortSide and ";
    }

//  if (isHighRewardToRiskRatioPattern (pattern)) // Commented out code block
//    {
//      mEasyLanguageFileName << "(TradeHighRewardToRiskPatterns = true) and " << std::endl;
//      firstSubExpressionVisited = false;
//    }
//  else
//    firstSubExpressionVisited = true;

  // Generate the core pattern expression (e.g., Close > Open)
  pattern->getPatternExpression()->accept (*this);
  mEasyLanguageFileName << " Then" << std::endl << std::endl; // End of 'if' condition

  // Start the 'begin' block for actions if condition is true
  mEasyLanguageFileName << "\t\tbegin" << std::endl;
  pattern->getStopLoss()->accept (*this);    // Generate stop-loss setting
  pattern->getProfitTarget()->accept (*this); // Generate profit-target setting
  pattern->getMarketEntry()->accept (*this);  // Generate market entry statement

  // Set holding period based on deviation type
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

  mEasyLanguageFileName << "\t\tend;" << std::endl; // End of 'begin' block
}



//////////////////////////////////////////////////////
///// class EasyLanguageRADCodeGenVisitor
//////////////////////////////////////////////////////

/**
 * @brief Constructs an EasyLanguageRADCodeGenVisitor.
 * Inherits from EasyLanguageCodeGenVisitor and specializes for RAD (Risk Adjusted Dollar) strategies.
 * @param system Pointer to the PriceActionLabSystem.
 * @param templateFileName Path to the EasyLanguage template file.
 * @param outputFileName Path to the output EasyLanguage file.
 * @param dev1Detail Stop-loss and profit-target details for "Deviation 1" patterns.
 * @param dev2Detail Stop-loss and profit-target details for "Deviation 2" patterns.
 */
EasyLanguageRADCodeGenVisitor::EasyLanguageRADCodeGenVisitor (PriceActionLabSystem *system,
							      const std::string& templateFileName,
							      const std::string& outputFileName,
							      const StopTargetDetail& dev1Detail,
							      const StopTargetDetail& dev2Detail)

  : EasyLanguageCodeGenVisitor (system, templateFileName, outputFileName, dev1Detail, dev2Detail)
{}

/**
 * @brief Destructor for EasyLanguageRADCodeGenVisitor.
 */
EasyLanguageRADCodeGenVisitor::~EasyLanguageRADCodeGenVisitor()
{}

/**
 * @brief Generates EasyLanguage code for setting long stop-loss and profit-target for RAD strategies.
 * This overrides the pure virtual method in the base class.
 */
void EasyLanguageRADCodeGenVisitor::setStopTargetLong()
{
    std::ofstream *outFile = getOutputFileStream();
    *outFile << "\t\tlongStop_new = Round2Fraction (myEntryPrice * stopPercent_new);" << std::endl;
    *outFile << "\t\tTargPrL = Round2Fraction (myEntryPrice * profitTgtPct_new);" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for setting short stop-loss and profit-target for RAD strategies.
 * This overrides the pure virtual method in the base class.
 */
void EasyLanguageRADCodeGenVisitor::setStopTargetShort()
{
    std::ofstream *outFile = getOutputFileStream();
    *outFile << "\t\tshortStop_new = Round2Fraction (myEntryPrice * stopPercent_new);" << std::endl;
    *outFile << "\t\tTargPrS = Round2Fraction (myEntryPrice * profitTgtPct_new);" << std::endl;
}

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

/**
 * @brief Generates EasyLanguage code for a LongSideStopLossInPercent AST node (RAD specific).
 * @param stopLoss Pointer to the LongSideStopLossInPercent node.
 */
void
EasyLanguageRADCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopPercent = (1.0 - (" << *stop << "/100));" << std::endl;
  *outFile << "\t\t\tlongStop = (Close * stopPercent);" << std::endl;
  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a LongSideProfitTargetInPercent AST node (RAD specific).
 * @param profitTarget Pointer to the LongSideProfitTargetInPercent node.
 */
void EasyLanguageRADCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetPercent = (1.0 + (" << *target << "/100));" << std::endl;
  *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a ShortSideProfitTargetInPercent AST node (RAD specific).
 * @param profitTarget Pointer to the ShortSideProfitTargetInPercent node.
 */
void EasyLanguageRADCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetPercent = (1.0 - (" << *target << "/100));" << std::endl;
  *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a ShortSideStopLossInPercent AST node (RAD specific).
 * @param stopLoss Pointer to the ShortSideStopLossInPercent node.
 */
void
EasyLanguageRADCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopPercent = (1.0 + (" << *stop << "/100));" << std::endl;
  *outFile << "\t\t\tshortStop = (Close * stopPercent);" << std::endl;
  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
}


//////////////////////////////////////////////////////
///// class EasyLanguagePointAdjustedCodeGenVisitor
//////////////////////////////////////////////////////

/**
 * @brief Constructs an EasyLanguagePointAdjustedCodeGenVisitor.
 * Inherits from EasyLanguageCodeGenVisitor and specializes for Point Adjusted strategies.
 * @param system Pointer to the PriceActionLabSystem.
 * @param templateFileName Path to the EasyLanguage template file.
 * @param bloxOutfileFileName Path to the output EasyLanguage file. (Note: parameter name seems like a typo, likely intended as outputFileName)
 * @param dev1Detail Stop-loss and profit-target details for "Deviation 1" patterns.
 * @param dev2Detail Stop-loss and profit-target details for "Deviation 2" patterns.
 */
EasyLanguagePointAdjustedCodeGenVisitor
::EasyLanguagePointAdjustedCodeGenVisitor (PriceActionLabSystem *system,
					   const std::string& templateFileName,
					   const std::string& bloxOutfileFileName, // Likely outputFileName
					   const StopTargetDetail& dev1Detail,
					   const StopTargetDetail& dev2Detail)

  : EasyLanguageCodeGenVisitor (system, templateFileName, bloxOutfileFileName, dev1Detail, dev2Detail)
{}

/**
 * @brief Destructor for EasyLanguagePointAdjustedCodeGenVisitor.
 */
EasyLanguagePointAdjustedCodeGenVisitor::~EasyLanguagePointAdjustedCodeGenVisitor()
{}

/**
 * @brief Generates EasyLanguage code for setting long stop-loss and profit-target for Point Adjusted strategies.
 * This overrides the pure virtual method in the base class.
 */
void EasyLanguagePointAdjustedCodeGenVisitor::setStopTargetLong()
{
    std::ofstream *outFile = getOutputFileStream();
    *outFile << "\t\tUnAdjustedClose = C of Data2;" << std::endl;
    *outFile << "\t\tlongStopDistance_new = Round2Fraction (UnAdjustedClose * stopPercent_new);" << std::endl;
    *outFile << "\t\tlongStop_new = myEntryPrice - longStopDistance_new;" << std::endl;
    *outFile << "\t\tprofitTargetDistance = Round2Fraction (UnAdjustedClose * profitTgtPct_new);" << std::endl;
    *outFile << "\t\tTargPrL = myEntryPrice + profitTargetDistance;" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for setting short stop-loss and profit-target for Point Adjusted strategies.
 * This overrides the pure virtual method in the base class.
 */
void EasyLanguagePointAdjustedCodeGenVisitor::setStopTargetShort()
{
    std::ofstream *outFile = getOutputFileStream();
    *outFile << "\t\tUnAdjustedClose = C of Data2;" << std::endl;
    *outFile << "\t\tshortStopDist_new = Round2Fraction (UnAdjustedClose * stopPercent_new);" << std::endl;
    *outFile << "\t\tshortStop_new = myEntryPrice + shortStopDist_new;" << std::endl;
    *outFile << "\t\tprofitTargetDistance = Round2Fraction (UnAdjustedClose * profitTgtPct_new);" << std::endl;
    *outFile << "\t\tTargPrS = myEntryPrice - profitTargetDistance;" << std::endl;

}

//void EasyLanguagePointAdjustedCodeGenVisitor::genCodeForVariablesInEntryScript()
//{
//  std::ofstream *outFile = getOutputFileStream();

//  *outFile << "vars: shortStopDistance(0.0), longStopDistance(0.0), UnAdjustedClose(0.0);" << std::endl;
//  *outFile << "vars: profitTargetDistance(0.0), unAdjCloseAtEntry(0.0);" << std::endl << std::endl;
//}

/**
 * @brief Generates EasyLanguage code for a LongSideStopLossInPercent AST node (Point Adjusted specific).
 * @param stopLoss Pointer to the LongSideStopLossInPercent node.
 */
void
EasyLanguagePointAdjustedCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopPercent = (" << *stop << "/100);" << std::endl;
  *outFile << "\t\t\tlongStopDistance = Round2Fraction (UnAdjustedClose * stopPercent);"
           << std::endl;
  *outFile << "\t\t\tlongStop = close - longStopDistance;" << std::endl;
  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a LongSideProfitTargetInPercent AST node (Point Adjusted specific).
 * @param profitTarget Pointer to the LongSideProfitTargetInPercent node.
 */
void EasyLanguagePointAdjustedCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetPercent = (" << *target << "/100);" << std::endl;
    *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a ShortSideProfitTargetInPercent AST node (Point Adjusted specific).
 * @param profitTarget Pointer to the ShortSideProfitTargetInPercent node.
 */
void EasyLanguagePointAdjustedCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *target = profitTarget->getProfitTarget();

  *outFile << "\t\t\tprofitTargetPercent = (" << *target << "/100);" << std::endl;
  *outFile << "\t\t\ttargetStr = \"" << *target << "%\";" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for a ShortSideStopLossInPercent AST node (Point Adjusted specific).
 * @param stopLoss Pointer to the ShortSideStopLossInPercent node.
 */
void
EasyLanguagePointAdjustedCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  std::ofstream *outFile = getOutputFileStream();
  decimal7 *stop = stopLoss->getStopLoss();

  *outFile << "\t\t\tstopPercent = (" << *stop << "/100);" << std::endl;
  *outFile << "\t\t\tshortStopDistance = Round2Fraction (UnAdjustedClose * stopPercent);"
           << std::endl;
  *outFile << "\t\t\tshortStop = close + shortStopDistance;" << std::endl;
  *outFile << "\t\t\tstopStr = \"" << *stop << "%\";" << std::endl;
}

//void EasyLanguagePointAdjustedCodeGenVisitor::genCodeToInitializeVariables()
//{
//std::ofstream *outFile = getOutputFileStream();

//  *outFile << "\t\tUnAdjustedClose = C of Data2;" << std::endl << std::endl;
//}

//void EasyLanguagePointAdjustedCodeGenVisitor::genCodeForEntryExit()
//{
//  std::ofstream *outFile = getOutputFileStream();
//const std::string& templateFileName,
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
