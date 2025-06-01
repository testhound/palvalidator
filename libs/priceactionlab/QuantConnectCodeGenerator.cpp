/**
 * @file QuantConnectCodeGenerator.cpp
 * @brief Implements the QuantConnectCodeGenVisitor and QuantConnectEquityCodeGenVisitor classes.
 *
 * These classes are responsible for generating C# code compatible with the QuantConnect
 * trading platform from Price Action Lab AST nodes.
 */
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <iostream> // For std::cout (pattern counts)

/**
 * @var firstSubExpressionVisited
 * @brief External global flag used to manage formatting for nested expressions,
 *        particularly for parentheses placement.
 */
extern bool firstSubExpressionVisited;

// --- QuantConnectCodeGenVisitor ---

/**
 * @brief Constructs a QuantConnectCodeGenVisitor.
 * @param system Pointer to the PriceActionLabSystem containing the patterns.
 * @param bloxOutFileName The name of the output file for the generated QuantConnect C# code.
 *        Note: The parameter name `bloxOutFileName` might be a misnomer if it's for QuantConnect.
 */
QuantConnectCodeGenVisitor::QuantConnectCodeGenVisitor(PriceActionLabSystem *system,
						       const std::string& bloxOutFileName)
  : PalCodeGenVisitor(), // Call base class constructor
    mTradingSystemPatterns(system), /**< @brief Shared pointer to the trading system patterns. */
    mEntryOrdersScriptFile(bloxOutFileName) /**< @brief Output file stream for the generated script. */
{}

/**
 * @brief Destructor for QuantConnectCodeGenVisitor.
 * Closes the output file stream if it's open.
 */
QuantConnectCodeGenVisitor::~QuantConnectCodeGenVisitor()
{
    if (mEntryOrdersScriptFile.is_open()) {
        mEntryOrdersScriptFile.close();
    }
}

/**
 * @brief Generates code for common variables (stub).
 * @note This method is currently a stub and does not generate any code.
 */
void QuantConnectCodeGenVisitor::genCodeForCommonVariables()
{
  //std::ofstream *outFile = getOutputFileStream();
}

/**
 * @brief Generates code to initialize volatility variables (stub).
 * @param shortSide Boolean indicating if for short side (not used in current stub).
 * @note This method is currently a stub and does not generate any code.
 */
void QuantConnectCodeGenVisitor::genCodeToInitVolatility(bool shortSide)
{
  //std::ofstream *outFile = getOutputFileStream();
}

/**
 * @brief Generates code for common entry logic (stub).
 * @note This method is currently a stub and does not generate any code.
 */
void QuantConnectCodeGenVisitor::genCodeForCommonEntry()
{
  //std::ofstream *outFile = getOutputFileStream();
}

/**
 * @brief Generates common prologue code for long exit logic (stub).
 * @note This method is currently a stub and does not generate any code.
 */
void QuantConnectCodeGenVisitor::genCommonCodeForLongExitPrologue()
{
  //std::ofstream *outFile = getOutputFileStream();
}

/**
 * @brief Generates common prologue code for short exit logic (stub).
 * @note This method is currently a stub and does not generate any code.
 */
void QuantConnectCodeGenVisitor::genCommonCodeForShortExitPrologue()
{
  //std::ofstream *outFile = getOutputFileStream();
}

/**
 * @brief Generates code for common variable initialization (stub).
 * @note This method is currently a stub and does not generate any code.
 */
void QuantConnectCodeGenVisitor::genCodeForCommonVariableInit()
{
  //std::ofstream *outFile = getOutputFileStream();
}

/**
 * @brief Generates the main C# code for QuantConnect strategies.
 *
 * This method orchestrates the code generation by:
 * 1. Calling stub/virtual methods for common variable and initialization code.
 * 2. Generating an `isLongEntry` C# method by iterating through long patterns.
 * 3. Generating an `isShortEntry` C# method by iterating through short patterns.
 * 4. Calling stub/virtual methods for common entry/exit logic.
 * It outputs the counts of long and short patterns processed to `std::cout`.
 */
void 
QuantConnectCodeGenVisitor::generateCode()
{
  PriceActionLabSystem::ConstSortedPatternIterator it;
  PALPatternPtr p;

  // Call stub/virtual methods for pre-generation setup
  genCodeForCommonVariables();
  genCodeForVariablesInEntryScript(); // Pure virtual, to be implemented by derived class
  genCodeForCommonVariableInit();
  genCodeToInitializeVariables();    // Pure virtual, to be implemented by derived class

  unsigned int numLongPatterns = 0;

  // Generate the isLongEntry method
  mEntryOrdersScriptFile << "\tpublic bool isLongEntry (decimal [] open, decimal [] high, ";
  mEntryOrdersScriptFile << "decimal [] low, decimal [] close)" << std::endl;
  mEntryOrdersScriptFile << "\t{" << std::endl;
  
  for (it = mTradingSystemPatterns->patternLongsBegin(); it != mTradingSystemPatterns->patternLongsEnd(); it++)
    {
      p = it->second;
      p->accept (*this); // Generate code for each long pattern
      numLongPatterns++;
    }

  mEntryOrdersScriptFile << std::endl;
  mEntryOrdersScriptFile << "\t\treturn false;" << std::endl; // Default return if no pattern matches
  mEntryOrdersScriptFile << "\t}" << std::endl;
  mEntryOrdersScriptFile << std::endl;

  
  unsigned int numShortPatterns = 0;  
  // Generate the isShortEntry method
  mEntryOrdersScriptFile << "\tpublic bool isShortEntry (decimal [] open, decimal [] high, ";
  mEntryOrdersScriptFile << "decimal [] low, decimal [] close)" << std::endl;
  mEntryOrdersScriptFile << "\t{" << std::endl;

  for (it = mTradingSystemPatterns->patternShortsBegin(); 
       it != mTradingSystemPatterns->patternShortsEnd(); it++)
    {
      p = it->second;
      p->accept (*this); // Generate code for each short pattern
      numShortPatterns++;
     }

  mEntryOrdersScriptFile << std::endl;
  mEntryOrdersScriptFile << "\t\treturn false;" << std::endl; // Default return if no pattern matches
  mEntryOrdersScriptFile << "\t}" << std::endl << std::endl;
  
  // Call stub/virtual methods for post-generation actions
  genCodeForCommonEntry();
  genCodeForEntryExit();  // Pure virtual, to be implemented by derived class

  std::cout << "Num long patterns = " << numLongPatterns << std::endl;
  std::cout << "Num short patterns = " << numShortPatterns << std::endl;
}
	
/**
 * @brief Gets the output file stream.
 * @return Pointer to the std::ofstream object used for writing the generated code.
 */
std::ofstream *
QuantConnectCodeGenVisitor::getOutputFileStream()
{
  return &mEntryOrdersScriptFile;
}

/**
 * @brief Visits a PriceBarOpen node.
 * Generates C# code like `open[barOffset]`.
 * @param bar Pointer to the PriceBarOpen node.
 */
void
QuantConnectCodeGenVisitor::visit (PriceBarOpen *bar)
{
  mEntryOrdersScriptFile << "open[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a PriceBarHigh node.
 * Generates C# code like `high[barOffset]`.
 * @param bar Pointer to the PriceBarHigh node.
 */
void
QuantConnectCodeGenVisitor::visit (PriceBarHigh *bar)
{
  mEntryOrdersScriptFile << "high[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a PriceBarLow node.
 * Generates C# code like `low[barOffset]`.
 * @param bar Pointer to the PriceBarLow node.
 */
void
QuantConnectCodeGenVisitor::visit (PriceBarLow *bar)
{
  mEntryOrdersScriptFile << "low[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a PriceBarClose node.
 * Generates C# code like `close[barOffset]`.
 * @param bar Pointer to the PriceBarClose node.
 */
void
QuantConnectCodeGenVisitor::visit (PriceBarClose *bar)
{
  mEntryOrdersScriptFile << "close[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a VolumeBarReference node.
 * Generates C# code like `volume[barOffset]`.
 * @param bar Pointer to the VolumeBarReference node.
 */
void
QuantConnectCodeGenVisitor::visit (VolumeBarReference *bar)
{
  mEntryOrdersScriptFile << "volume[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a Roc1BarReference node.
 * Generates C# code like `RateOfChange(Close, 1)[barOffset]`.
 * @param bar Pointer to the Roc1BarReference node.
 */
void
QuantConnectCodeGenVisitor::visit (Roc1BarReference *bar)
{
  // Note: Assumes a RateOfChange helper method or equivalent exists in the QuantConnect context.
  mEntryOrdersScriptFile << "RateOfChange(Close, 1)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits an IBS1BarReference node.
 * Generates C# code like `IBS(1)[barOffset]`.
 * @param bar Pointer to the IBS1BarReference node.
 */
void QuantConnectCodeGenVisitor::visit (IBS1BarReference *bar)
{
  // Note: Assumes an IBS helper method or equivalent exists in the QuantConnect context.
  mEntryOrdersScriptFile << "IBS(1)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits an IBS2BarReference node.
 * Generates C# code like `IBS(2)[barOffset]`.
 * @param bar Pointer to the IBS2BarReference node.
 */
void QuantConnectCodeGenVisitor::visit (IBS2BarReference *bar)
{
  // Note: Assumes an IBS helper method or equivalent exists in the QuantConnect context.
  mEntryOrdersScriptFile << "IBS(2)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits an IBS3BarReference node.
 * Generates C# code like `IBS(3)[barOffset]`.
 * @param bar Pointer to the IBS3BarReference node.
 */
void QuantConnectCodeGenVisitor::visit (IBS3BarReference *bar)
{
  // Note: Assumes an IBS helper method or equivalent exists in the QuantConnect context.
  mEntryOrdersScriptFile << "IBS(3)[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a MeanderBarReference node.
 * Generates C# code like `meanderVar[barOffset]`.
 * @param bar Pointer to the MeanderBarReference node.
 */
void
QuantConnectCodeGenVisitor::visit (MeanderBarReference *bar)
{
  // Note: Assumes 'meanderVar' is a pre-calculated array/list in the QuantConnect context.
  mEntryOrdersScriptFile << "meanderVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a VChartLowBarReference node.
 * Generates C# code like `vchartLowVar[barOffset]`.
 * @param bar Pointer to the VChartLowBarReference node.
 */
void
QuantConnectCodeGenVisitor::visit (VChartLowBarReference *bar)
{
  // Note: Assumes 'vchartLowVar' is a pre-calculated array/list in the QuantConnect context.
  mEntryOrdersScriptFile << "vchartLowVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a VChartHighBarReference node.
 * Generates C# code like `vchartHighVar[barOffset]`.
 * @param bar Pointer to the VChartHighBarReference node.
 */
void
QuantConnectCodeGenVisitor::visit (VChartHighBarReference *bar)
{
  // Note: Assumes 'vchartHighVar' is a pre-calculated array/list in the QuantConnect context.
  mEntryOrdersScriptFile << "vchartHighVar[" << bar->getBarOffset() << "]";
}

/**
 * @brief Visits a GreaterThanExpr node.
 * Generates C# code for a greater than comparison `(LHS > RHS)`.
 * Uses `firstSubExpressionVisited` to manage parentheses for nesting.
 * @param expr Pointer to the GreaterThanExpr node.
 */
void
QuantConnectCodeGenVisitor::visit (GreaterThanExpr *expr)
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
 * Generates C# code for a logical AND `LHS & RHS`.
 * Note the use of `&` (bitwise AND in C# for bools can act as logical AND without short-circuiting,
 * or `&&` for logical AND with short-circuiting. Current output is `&`).
 * @param expr Pointer to the AndExpr node.
 */
void
QuantConnectCodeGenVisitor::visit (AndExpr *expr)
{
  expr->getLHS()->accept (*this);
  mEntryOrdersScriptFile << " & " << std::endl; // C# logical AND is &&, bitwise & also works for bool
  expr->getRHS()->accept (*this);
}

/**
 * @brief Visits a PatternDescription node.
 * Generates a C# comment line with the pattern's metadata.
 * @param desc Pointer to the PatternDescription node.
 */
void
QuantConnectCodeGenVisitor::visit (PatternDescription *desc)
{
  mEntryOrdersScriptFile << "\t\t" << "//FILE:" << desc->getFileName() << "  Index: " << desc->getpatternIndex()
			 << "  Index DATE: " << desc->getIndexDate() << "  PL: " << *(desc->getPercentLong())
                         << "%  PS: " << *(desc->getPercentShort()) << "%  Trades: " << desc->numTrades()
                         << "  CL: " << desc->numConsecutiveLosses() << " }" << std::endl;
}

/**
 * @brief Visits a LongMarketEntryOnOpen node.
 * Generates C# code `return true;` indicating a long entry condition is met.
 * @param entryStatement Pointer to the LongMarketEntryOnOpen node.
 */
void QuantConnectCodeGenVisitor::visit (LongMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\treturn true;" << std::endl;
}

/**
 * @brief Visits a ShortMarketEntryOnOpen node.
 * Generates C# code `return true;` indicating a short entry condition is met.
 * @param entryStatement Pointer to the ShortMarketEntryOnOpen node.
 */
void QuantConnectCodeGenVisitor:: visit (ShortMarketEntryOnOpen *entryStatement)
{
  mEntryOrdersScriptFile << "\t\t\treturn true;" << std::endl;
}

/**
 * @brief Visits a PriceActionLabPattern node to generate its C# representation.
 *
 * Generates an `if` or `else if` block in C# for the pattern's conditions.
 * Includes conditions for volatility and portfolio filters if present.
 * The core pattern expression is visited, followed by setting stop-loss, profit-target,
 * and market entry (which typically results in `return true;`).
 * Uses static counters `numLongPatterns` and `numShortPatterns` to format `if`/`else if`.
 *
 * @param pattern Pointer to the PriceActionLabPattern node.
 */
void QuantConnectCodeGenVisitor::visit (PriceActionLabPattern *pattern)
{
  static int numLongPatterns = 0; // Counter for long patterns to manage if/else if
  static int numShortPatterns = 0; // Counter for short patterns
  
  pattern->getPatternDescription()->accept (*this); // Add description as a comment
  mEntryOrdersScriptFile << std::endl;
  
  // Determine if it's a long or short pattern and format if/else if accordingly
  if (pattern->isLongPattern())
    {
      numLongPatterns++;
      if (numLongPatterns > 1)
	mEntryOrdersScriptFile << "\t\telse if (";
      else
	mEntryOrdersScriptFile << "\t\tif (";
    }
  else // Short pattern
    {
      numShortPatterns++;
      if (numShortPatterns > 1)
	mEntryOrdersScriptFile << "\t\telse if (";
      else
	mEntryOrdersScriptFile << "\t\tif (";
    }

  // Add volatility conditions if present
  if (pattern->hasVolatilityAttribute())
    {
      if (pattern->isLowVolatilityPattern())
	mEntryOrdersScriptFile << "lowVolatility & "; // Assuming 'lowVolatility' is a bool variable
      else if (pattern->isHighVolatilityPattern())
	mEntryOrdersScriptFile << "highVolatility & "; // Assuming 'highVolatility' is a bool variable
      else if (pattern->isVeryHighVolatilityPattern())
	mEntryOrdersScriptFile << "vHighVolatility & "; // Assuming 'vHighVolatility' is a bool variable
    }

  // Add portfolio filter conditions if present
  if (pattern->hasPortfolioAttribute())
    {
      if (pattern->isFilteredLongPattern())
	mEntryOrdersScriptFile << "tradeLongSide & "; // Assuming 'tradeLongSide' is a bool variable
      else if (pattern->isFilteredShortPattern())
	mEntryOrdersScriptFile << "tradeShortSide & "; // Assuming 'tradeShortSide' is a bool variable
    }

  firstSubExpressionVisited = true; // Flag for formatting nested expressions

  pattern->getPatternExpression()->accept (*this); // Generate the core pattern expression
  mEntryOrdersScriptFile << ")" << std::endl;      // Close the if condition
  mEntryOrdersScriptFile << "\t\t{" << std::endl;    // Start block for actions
  
  // Visit stop loss, profit target, and market entry.
  // Note: For QuantConnect, these might set properties or prepare for an action,
  // rather than directly generating executable lines here. The market entry visit
  // currently generates `return true;`.
  pattern->getStopLoss()->accept (*this);
  pattern->getProfitTarget()->accept (*this);
  pattern->getMarketEntry()->accept (*this);

  mEntryOrdersScriptFile << "\t\t}" << std::endl; // End block
}

// --- QuantConnectEquityCodeGenVisitor ---

/**
 * @brief Constructs a QuantConnectEquityCodeGenVisitor.
 * Specializes QuantConnectCodeGenVisitor for equity instruments.
 * @param system Pointer to the PriceActionLabSystem.
 * @param outputFileName The name of the output file for the generated C# code.
 */
QuantConnectEquityCodeGenVisitor::QuantConnectEquityCodeGenVisitor (PriceActionLabSystem *system, 
							      const std::string& outputFileName)
  : QuantConnectCodeGenVisitor (system, outputFileName) // Call base constructor
{}

/**
 * @brief Destructor for QuantConnectEquityCodeGenVisitor.
 */
QuantConnectEquityCodeGenVisitor::~QuantConnectEquityCodeGenVisitor()
{}

/**
 * @brief Generates code to initialize variables (stub for Equity visitor).
 * @note This method is currently a stub and does not generate any code.
 *       It overrides a pure virtual method from the base class.
 */
void QuantConnectEquityCodeGenVisitor::genCodeToInitializeVariables()
{
  // Stub: Specific initialization for equity strategies could go here.
}

/**
 * @brief Generates code for entry and exit logic (stub for Equity visitor).
 * @note This method is currently a stub and does not generate any code.
 *       It overrides a pure virtual method from the base class.
 */
void QuantConnectEquityCodeGenVisitor::genCodeForEntryExit()
{
  std::ofstream *outFile = getOutputFileStream();
  // Stub: Equity-specific entry/exit wrappers or calls could be generated here.
  *outFile <<  std::endl; // Placeholder
}

/**
 * @brief Generates code for variables in the entry script (stub for Equity visitor).
 * @note This method is currently a stub and does not generate any code.
 *       It overrides a pure virtual method from the base class.
 */
void QuantConnectEquityCodeGenVisitor::genCodeForVariablesInEntryScript()
{
  //  std::ofstream *outFile = getOutputFileStream();
  // Stub: Equity-specific variables could be declared here.
}

/**
 * @brief Visits a LongSideStopLossInPercent node (stub for Equity visitor).
 * @param stopLoss Pointer to the LongSideStopLossInPercent node.
 * @note This method is currently empty. For a functional generator, it would
 *       set stop loss parameters or generate stop market order logic for QuantConnect.
 */
void 
QuantConnectEquityCodeGenVisitor::visit (LongSideStopLossInPercent *stopLoss)
{
  // Stub: Generate C# code to set long stop loss (e.g., _stopMarketTicket = StopMarketOrder(symbol, quantity, price);)
}

/**
 * @brief Visits a LongSideProfitTargetInPercent node (stub for Equity visitor).
 * @param profitTarget Pointer to the LongSideProfitTargetInPercent node.
 * @note This method is currently empty. For a functional generator, it would
 *       set profit target parameters or generate limit order logic for QuantConnect.
 */
void QuantConnectEquityCodeGenVisitor::visit (LongSideProfitTargetInPercent *profitTarget)
{
  // Stub: Generate C# code to set long profit target (e.g., _limitMarketTicket = LimitOrder(symbol, quantity, price);)
}

/**
 * @brief Visits a ShortSideProfitTargetInPercent node (stub for Equity visitor).
 * @param profitTarget Pointer to the ShortSideProfitTargetInPercent node.
 * @note This method is currently empty.
 */
void QuantConnectEquityCodeGenVisitor::visit (ShortSideProfitTargetInPercent *profitTarget)
{
  // Stub: Generate C# code to set short profit target
}

/**
 * @brief Visits a ShortSideStopLossInPercent node (stub for Equity visitor).
 * @param stopLoss Pointer to the ShortSideStopLossInPercent node.
 * @note This method is currently empty.
 */
void 
QuantConnectEquityCodeGenVisitor::visit (ShortSideStopLossInPercent *stopLoss)
{
  // Stub: Generate C# code to set short stop loss
}
