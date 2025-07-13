#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <string>


extern bool firstSubExpressionVisited;

/**
 * @file EasyLanguageFromTemplateCodeGen.cpp
 * @brief Implements the EasyLanguage code generation visitor.
 * This version embeds the code structure directly, removing the need for an external template file.
 */

///////////////////////////////////////
/// class EasyLanguageCodeGenVisitor
//////////////////////////////////////

// Note: The marker constants are no longer strictly necessary for the new design,
// but they can be kept for reference or logging if desired.
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
 * @param outputFileName Path to the output file where generated EasyLanguage code will be written.
 * @param dev1Detail Stop-loss and profit-target details for "Deviation 1" patterns.
 * @param dev2Detail Stop-loss and profit-target details for "Deviation 2" patterns.
 */
EasyLanguageCodeGenVisitor::EasyLanguageCodeGenVisitor(std::shared_ptr<PriceActionLabSystem> system,
						       const std::string& outputFileName)
  : PalCodeGenVisitor(),
    mTradingSystemPatterns(system),
    mEasyLanguageFileName(outputFileName)
{
}

/**
 * @brief Destructor for EasyLanguageCodeGenVisitor.
 */
EasyLanguageCodeGenVisitor::~EasyLanguageCodeGenVisitor()
{
}

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
 * @brief Generates the complete EasyLanguage code by writing an embedded template to the output file.
 * This method constructs the code by interleaving static strings with calls to dynamic code generation methods.
 */
void EasyLanguageCodeGenVisitor::generateCode()
{
    std::ofstream *outFile = getOutputFileStream();
    if (!outFile || !outFile->is_open()) {
        throw std::runtime_error("EasyLanguage output file not open: " + std::string(strerror(errno)));
    }

    // --- Start of Embedded Template ---
    *outFile << "// Copyright (C) MKC Associates, LLC - All Rights Reserved" << std::endl;
    *outFile << "// Unauthorized copying of this file, via any medium is strictly prohibited" << std::endl;
    *outFile << "// Proprietary and confidential" << std::endl;
    *outFile << "//" << std::endl << std::endl;
    *outFile << "using elsystem;" << std::endl;
    *outFile << "Inputs:" << std::endl;
    *outFile << "        Int MaxPyramids( 5 )," << std::endl;
    *outFile << "        Int Reverse_Option( 2 {0=ignore, 1=exit, 2=reverse} )," << std::endl;
    *outFile << "       	Int TurnOnEntryBarStop( 0  { 0:OFF  <>0:ON } )," << std::endl;
    *outFile << "        Double EntryBarStopLevel( 0 { <= 0:OFF  > 0: ON } )," << std::endl;
    *outFile << "        PrintDebug( False );" << std::endl;
    *outFile << "// Added by D Cohn on 7/19/2019" << std::endl << std::endl;
    *outFile << "vars: shortStop (0), longStop (0), stopPercent (0);" << std::endl;
    *outFile << "vars: LongEntryFound (false), ShortEntryFound (false), noNextDayOrders(false);" << std::endl;
    *outFile << "vars: oscVChartLow(0.0), oscVChartHigh(0.0);" << std::endl;
    *outFile << "vars: MinHoldPeriod(4.0), MaxHoldPeriod(10.0);" << std::endl;
    *outFile << "vars: profitTargetPercent(0.0), TargPrL(0.0), TargPrS(0.0);" << std::endl;
    *outFile << "vars: shortStopDistance(0.0), longStopDistance(0.0);" << std::endl;
    *outFile << "vars: profitTargetDistance(0.0), unAdjCloseAtEntry(0.0);" << std::endl;
    *outFile << "vars: stopStr(\"\"), targetStr(\"\");" << std::endl;
    *outFile << "Vars:	stopPercent_new(0)," << std::endl;
    *outFile << "                profitTgtPct_new(0)," << std::endl;
    *outFile << "                MinHoldPeriod_new(0)," << std::endl;
    *outFile << "                MaxHoldPeriod_new(0)," << std::endl;
    *outFile << "                longStopDistance_new(0)," << std::endl;
    *outFile << "                longStop_new(0)," << std::endl;
    *outFile << "                shortStopDist_new(0)," << std::endl << "    " << std::endl;
    *outFile << "            shortStop_new(0);" << std::endl;
    *outFile << "Vars:	myBarsSinceEntry(0)," << std::endl;
    *outFile << "                myEntryPrice(0)," << std::endl;
    *outFile << "                AllowEntry(false);" << std::endl;
    *outFile << "Variables:  // Section of code Added by D Cohn on 7/19/2019" << std::endl;
    *outFile << "        Double NumEntries( 0 )," << std::endl;
    *outFile << "        Double MP( 0 )," << std::endl;
    *outFile << "        Double TT( 0 )," << std::endl;
    *outFile << "        Double CS( 0 )," << std::endl;
    *outFile << "        Double AEP( 0 )," << std::endl;
    *outFile << "        Bool ExitSet( False );" << std::endl;
    *outFile << "Once ( BarStatus(1) = 2 ) Begin // Modified by D Cohn on 7/22/2019" << std::endl;
    *outFile << "	SetStopContract;" << std::endl;
    *outFile << "// Added 4/24/2020 by Emerald:  Set all built-in TS stops to be on a per contract basis" << std::endl << std::endl;
    *outFile << "        If PrintDebug Then   // Added by D Cohn on 7/19/2019" << std::endl;
    *outFile << "                ClearPrintLog;" << std::endl;
    *outFile << "End;" << std::endl << std::endl << std::endl;
    *outFile << "\tMP = MarketPosition;" << std::endl;
    *outFile << "\tTT = TotalTrades;" << std::endl;
    *outFile << "\tCS = CurrentShares;    // Added by D Cohn on 7/19/2019" << std::endl;
    *outFile << "\tAEP = AvgEntryPrice;" << std::endl;
    *outFile << "\toscVChartLow  = VChartLow( 5, 0.2 );" << std::endl;
    *outFile << "\toscVChartHigh = VChartHigh( 5, 0.2 );" << std::endl;
    *outFile << "\tLongEntryFound  = false;" << std::endl;
    *outFile << "\tShortEntryFound = false;" << std::endl;
    *outFile << "\tIf ( MP[1] <> MP and MP <> 0 ) or ( CS[1] > 0 and CS > CS[1] ) or ( MP = 0 and TT[1] + 1 < TT ) Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\tMyBarsSinceEntry = 0;" << std::endl;
    *outFile << "\t\tIf MP[1] <> MP and MP <> 0 Then" << std::endl;
    *outFile << "\t\t\tmyEntryPrice = EntryPrice" << std::endl;
    *outFile << "\t\tElse If CS > CS[1] Then" << std::endl;
    *outFile << "\t\t\tmyEntryPrice = ( ( CS * AEP ) - ( CS[1] * AEP[1] ) ) / ( CS - CS[1] );" << std::endl;
    *outFile << "\tEnd;" << std::endl << std::endl;
    *outFile << "\tIf MP = 0 Then" << std::endl;
    *outFile << "\t\tExitSet = False;" << std::endl;
    *outFile << "\tIf PrintDebug Then" << std::endl;
    *outFile << "\t\tPrint( BarDateTime.ToString(), \":NEW , \", MyBarsSinceEntry:0:0, \" , \", MyEntryPrice:0:2, \" , \", ExitPrice(1):0:2, \" , \", MP:0:0, \" , \", NetProfit:0:2 );" << std::endl;
    *outFile << "////////////////////////////////////////////////////////////////////////////////////" << std::endl;
    *outFile << "//////" << std::endl;
    *outFile << "////// LONG ENTRY SETUPS: CODE Simplified and streamlined By D Cohn on 7/22/2019" << std::endl;
    *outFile << "//////" << std::endl;
    *outFile << "////////////////////////////////////////////////////////////////////////////////////" << std::endl;

    // --- Dynamic Section 1: Long Entry Setups ---
    insertLongPatterns();
    *outFile << std::endl << std::endl;

    // --- Resuming Static Template ---
    *outFile << "////////////////////////////////////////////////////////////////////////////////////" << std::endl;
    *outFile << "//////" << std::endl;
    *outFile << "////// SHORT ENTRY SETUPS: CODE Simplified and streamlined By D Cohn on 7/22/2019" << std::endl;
    *outFile << "//////" << std::endl;
    *outFile << "////////////////////////////////////////////////////////////////////////////////////" << std::endl;

    // --- Dynamic Section 2: Short Entry Setups ---
    insertShortPatterns();
    *outFile << std::endl << std::endl;

    // --- Resuming Static Template ---
    *outFile << "\tAllowEntry = MP = 0 or ( MP <> 0 and CurrentEntries <= MaxPyramids and LongEntryFound <> ShortEntryFound );" << std::endl;
    *outFile << "\tIf Reverse_Option = 0 and MP = 1 and ShortEntryFound and AllowEntry Then" << std::endl;
    *outFile << "\t\tAllowEntry = false;" << std::endl;
    *outFile << "\tIf Reverse_Option = 0 and MP = -1 and LongEntryFound and AllowEntry Then" << std::endl;
    *outFile << "\t\t AllowEntry = false;" << std::endl;
    *outFile << "\tIf Reverse_Option = 1 and MP = 1 and ShortEntryFound and (AllowEntry or MaxPyramids = 0) Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\tSell (\"Rev LX\") ALL contracts next bar at market;" << std::endl;
    *outFile << "\t\tAllowEntry = false;" << std::endl;
    *outFile << "\tEnd;" << std::endl << std::endl;
    *outFile << "\tIf Reverse_Option = 1 and MP = -1 and LongEntryFound and (AllowEntry or MaxPyramids = 0) Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\tBuyToCover (\"Rev SX\") ALL contracts next bar at market;" << std::endl;
    *outFile << "\t\tAllowEntry = false;" << std::endl;
    *outFile << "\tEnd;" << std::endl << std::endl;
    *outFile << "\tIf Reverse_Option = 2 and MP = 1 and ShortEntryFound and (AllowEntry or MaxPyramids = 0) Then" << std::endl;
    *outFile << "\t\tAllowEntry = true;" << std::endl;
    *outFile << "\tIf Reverse_Option = 2 and MP = -1 and LongEntryFound and (AllowEntry or MaxPyramids = 0) Then" << std::endl;
    *outFile << "\t\tAllowEntry = true;" << std::endl;
    *outFile << "\tIf AllowEntry Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\tstopPercent_new  = stopPercent;" << std::endl;
    *outFile << "\t\tprofitTgtPct_new = profitTargetPercent;" << std::endl;
    *outFile << "\t\tMinHoldPeriod_new = MinHoldPeriod;" << std::endl;
    *outFile << "\t\tMaxHoldPeriod_new = MaxHoldPeriod;" << std::endl;
    *outFile << "\t\tlongStopDistance_new = longStopDistance;" << std::endl;
    *outFile << "\t\tlongStop_new = longStop;" << std::endl;
    *outFile << "\t\tshortStopDist_new = shortStopDistance;" << std::endl;
    *outFile << "\t\tshortStop_new = shortStop;" << std::endl;
    *outFile << "\tEnd;" << std::endl;
    *outFile << "\tIf MP = 1 Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\tnoNextDayOrders = false;" << std::endl;
    *outFile << "\t\tIf myBarsSinceEntry >= ( MaxHoldPeriod_new - 1 ) and noNextDayOrders = false Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tnoNextDayOrders = true;" << std::endl;
    *outFile << "\t\t\tExitSet = True;" << std::endl;
    *outFile << "\t\t\tSell (\"L Max hold time\") at next bar at Market;" << std::endl << std::endl;
    *outFile << "\t\tEnd;" << std::endl;
    *outFile << "\t\tIf myBarsSinceEntry = 0 Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;

    // --- Dynamic Section 3: Setting Long Targets ---
    setStopTargetLong();
    *outFile << "\t\tEnd;" << std::endl << std::endl;

    // --- Resuming Static Template ---
    *outFile << "\t\tIf myBarsSinceEntry >= ( MinHoldPeriod_new - 1 ) and noNextDayOrders = false Then" << std::endl;
    *outFile << "\t\tBegin"  << std::endl;
    *outFile << "\t\t\tIf oscVChartHigh >= 10 Then" << std::endl;
    *outFile << "\t\t\tBegin" << std::endl;
    *outFile << " \t\t\t\tnoNextDayOrders = true;" << std::endl;
    *outFile << "\t\t\t\tExitSet = True;" << std::endl;
    *outFile << "\t\t\t\tSell (\"OB Exit\") at next bar at Market;" << std::endl;
    *outFile << "\t\t\tEnd;" << std::endl << std::endl;
    *outFile << "\t\tEnd;" << std::endl << std::endl;
    *outFile << "\t\tIf noNextDayOrders = False Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tSell (\"PT LX\") at next bar at TargPrL Limit;" << std::endl;
    *outFile << "\t\t\tSell (\"Stop LX\") at next bar at longStop_new Stop;" << std::endl;
    *outFile << "\t\tEnd;" << std::endl << std::endl;
    *outFile << "\tEnd;" << std::endl << std::endl;
    *outFile << "\tIf MP = -1 Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\tnoNextDayOrders = false;" << std::endl;
    *outFile << "\t\tIf myBarsSinceEntry >= ( MaxHoldPeriod_new - 1 ) and noNextDayOrders = false Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tnoNextDayOrders = true;" << std::endl;
    *outFile << "\t\t\tExitSet = True;" << std::endl;
    *outFile << "\t\t\tBuy to Cover (\"S Max hold time\") at next bar at Market;" << std::endl;
    *outFile << "\t\tEnd;" << std::endl;
    *outFile << "\t\tIf myBarsSinceEntry >= ( MinHoldPeriod_new - 1 ) and noNextDayOrders = false Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tIf oscVChartLow <= -10 Then" << std::endl;
    *outFile << "\t\t\tBegin" << std::endl;
    *outFile << "\t\t\t\tnoNextDayOrders = true;" << std::endl;
    *outFile << "\t\t\t\tExitSet = True;" << std::endl;
    *outFile << "\t\t\t\tBuy to Cover (\"OS Exit\") at next bar at Market;" << std::endl;
    *outFile << "\t\t\tEnd;" << std::endl << std::endl;
    *outFile << "\t\tEnd;" << std::endl;
    *outFile << "\t\tIf myBarsSinceEntry = 0 Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;

    // --- Dynamic Section 4: Setting Short Targets ---
    setStopTargetShort();
    *outFile << "\t\tEnd;" << std::endl;

    // --- Resuming Static Template ---
    *outFile << "\t\tIf noNextDayOrders = False Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tBuy to Cover (\"PT SX\") at next bar at TargPrS Limit;" << std::endl;
    *outFile << "\t\t\tBuy to Cover (\"Stop SX\") at next bar at shortStop_new Stop;" << std::endl;
    *outFile << "\t\tEnd;" << std::endl << std::endl;
    *outFile << "\tEnd;" << std::endl << std::endl;
    *outFile << "\t//Code moved to after exit calculations to block Entries after Market exits have been set" << std::endl;
    *outFile << "\tIf AllowEntry Then" << std::endl;
    *outFile << "\tBegin" << std::endl;
    *outFile << "\t\t// Allow reversal from Long to Short and Short to Long even if ExitSet is true, " << std::endl;
    *outFile << "\t\t//but do not allow pyramid in same direction if an Exit has been set." << std::endl;
    *outFile << "\t\tIf LongEntryFound and ShortEntryFound = False and ( MP < 0 or ( MP >= 0 and ( ExitSet = False ) ) ) Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tCommentary (\"Manual stop = open of next bar - \", stopStr, NewLine);" << std::endl;
    *outFile << "\t\t\tCommentary (\"Manual profit target = open of next bar + \", targetStr, NewLine);" << std::endl;
    *outFile << "\t\t\tBuy (\"LE1\") at next bar at Market;" << std::endl << std::endl;
    *outFile << "\t\t\t// Added 4/24/2020 by Emerald: If the Entry Bar Stop enabled and the MP" << std::endl;
    *outFile << "\t\t\t//  is not Long then enable the stop on the entry bar only" << std::endl;
    *outFile << "\t\t\tIf TurnOnEntryBarStop <> 0 and EntryBarStopLevel > 0.0 and MP <= 0 Then  " << std::endl;
    *outFile << "\t\t\t\tSetStopLoss( EntryBarStopLevel * stopPercent * Close);" << std::endl;
    *outFile << "\t\tEnd;" << std::endl;
    *outFile << "\t\tElse If ShortEntryFound and LongEntryFound = False and ( MP > 0 or ( MP <= 0 and ( ExitSet = False ) ) ) Then" << std::endl;
    *outFile << "\t\tBegin" << std::endl;
    *outFile << "\t\t\tCommentary (\"Manual stop = open of next bar + \", stopStr, NewLine);" << std::endl;
    *outFile << "\t\t\tCommentary (\"Manual profit target = open of next bar - \", targetStr, NewLine);" << std::endl;
    *outFile << "\t\t\tSell short (\"SE1\") at next bar at Market;" << std::endl << std::endl;
    *outFile << "\t\t\t// Added 4/24/2020 by Emerald: If the Entry Bar Stop enabled and the MP is not" << std::endl;
    *outFile << "\t\t\t//  Long then enable the stop on the entry bar only" << std::endl;
    *outFile << "\t\t\tIf TurnOnEntryBarStop <> 0 and EntryBarStopLevel > 0.0 and MP >= 0 Then " << std::endl;
    *outFile << "\t\t\t\tSetStopLoss( EntryBarStopLevel * stopPercent * Close);" << std::endl;
    *outFile << "\t\tEnd; " << std::endl << std::endl;
    *outFile << "\tEnd;" << std::endl << std::endl;
    *outFile << "//End of code" << std::endl;
    *outFile << "If BarStatus(1) = 2 Then" << std::endl;
    *outFile << "        myBarsSinceEntry = myBarsSinceEntry + 1;" << std::endl;
    // --- End of Embedded Template ---
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

  // Generate the core pattern expression (e.g., Close > Open)
  pattern->getPatternExpression()->accept (*this);
  mEasyLanguageFileName << " Then" << std::endl << std::endl; // End of 'if' condition

  // Start the 'begin' block for actions if condition is true
  mEasyLanguageFileName << "\t\tbegin" << std::endl;
  pattern->getStopLoss()->accept (*this);    // Generate stop-loss setting
  pattern->getProfitTarget()->accept (*this); // Generate profit-target setting
  pattern->getMarketEntry()->accept (*this);  // Generate market entry statement

  mEasyLanguageFileName << "\t\tend;" << std::endl; // End of 'begin' block
}



//////////////////////////////////////////////////////
///// class EasyLanguageRADCodeGenVisitor
//////////////////////////////////////////////////////

/**
 * @brief Constructs an EasyLanguageRADCodeGenVisitor.
 * Inherits from EasyLanguageCodeGenVisitor and specializes for RAD (Risk Adjusted Dollar) strategies.
 * @param system Pointer to the PriceActionLabSystem.
 * @param outputFileName Path to the output EasyLanguage file.
 */
EasyLanguageRADCodeGenVisitor::EasyLanguageRADCodeGenVisitor (std::shared_ptr<PriceActionLabSystem> system,
							      const std::string& outputFileName)
  : EasyLanguageCodeGenVisitor (system, outputFileName)
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
    *outFile << "\t\t\tlongStop_new = Round2Fraction (myEntryPrice * stopPercent_new);" << std::endl;
    *outFile << "\t\t\tTargPrL = Round2Fraction (myEntryPrice * profitTgtPct_new);" << std::endl;
}

/**
 * @brief Generates EasyLanguage code for setting short stop-loss and profit-target for RAD strategies.
 * This overrides the pure virtual method in the base class.
 */
void EasyLanguageRADCodeGenVisitor::setStopTargetShort()
{
    std::ofstream *outFile = getOutputFileStream();
    *outFile << "\t\t\tshortStop_new = Round2Fraction (myEntryPrice * stopPercent_new);" << std::endl;
    *outFile << "\t\t\tTargPrS = Round2Fraction (myEntryPrice * profitTgtPct_new);" << std::endl;
}


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
 * @param outputFileName Path to the output EasyLanguage file.
 */
EasyLanguagePointAdjustedCodeGenVisitor
::EasyLanguagePointAdjustedCodeGenVisitor (std::shared_ptr<PriceActionLabSystem> system,
					   const std::string& outputFileName)

  : EasyLanguageCodeGenVisitor (system, outputFileName)
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
