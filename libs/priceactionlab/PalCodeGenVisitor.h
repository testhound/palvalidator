#ifndef PAL_CODE_GEN_VISITOR_H
#define PAL_CODE_GEN_VISITOR_H

#include <memory>
#include <fstream>
#include "StopTargetDetail.h"

class PriceBarOpen;
class PriceBarHigh;
class PriceBarLow;
class PriceBarClose;
class VolumeBarReference;
class Roc1BarReference;
class MeanderBarReference;
class MeanderBarReference;
class VChartLowBarReference;
class VChartHighBarReference;
class GreaterThanExpr;
class AndExpr;
class PatternDescription;
class PriceActionLabPattern;
class LongMarketEntryOnOpen;
class ShortMarketEntryOnOpen;
class LongSideProfitTargetInPercent;
class ShortSideProfitTargetInPercent;
class LongSideStopLossInPercent;
class ShortSideStopLossInPercent;
class PriceActionLabSystem;
class IBS1BarReference;
class IBS2BarReference;
class IBS3BarReference;

/**
 * @brief Abstract base class for code generation visitors.
 *
 * This class defines the interface for visiting different nodes of the
 * Price Action Lab Abstract Syntax Tree (AST) and generating code.
 * Concrete derived classes implement code generation for specific target platforms.
 */
class PalCodeGenVisitor
{
public:
  /**
   * @brief Default constructor.
   */
  PalCodeGenVisitor();
  /**
   * @brief Virtual destructor.
   */
  virtual ~PalCodeGenVisitor();
  /**
   * @brief Pure virtual method to initiate the code generation process.
   */
  virtual void generateCode() = 0;

  /** @name Visitor methods for PriceBarReference types */
  ///@{
  virtual void visit (PriceBarOpen *) = 0;
  virtual void visit (PriceBarHigh *) = 0;
  virtual void visit (PriceBarLow *) = 0;
  virtual void visit (PriceBarClose *) = 0;
  virtual void visit (VolumeBarReference *) = 0;
  virtual void visit (Roc1BarReference *) = 0;
  virtual void visit (IBS1BarReference *) = 0;
  virtual void visit (IBS2BarReference *) = 0;
  virtual void visit (IBS3BarReference *) = 0;
  virtual void visit (MeanderBarReference *) = 0;
  virtual void visit (VChartLowBarReference *) = 0;
  virtual void visit (VChartHighBarReference *) = 0;
  ///@}

  /** @name Visitor methods for PatternExpression types */
  ///@{
  virtual void visit (GreaterThanExpr *) = 0;
  virtual void visit (AndExpr *) = 0;
  ///@}

  /** @name Visitor methods for ProfitTargetInPercentExpression types */
  ///@{
  virtual void visit (LongSideProfitTargetInPercent *) = 0;
  virtual void visit (ShortSideProfitTargetInPercent *) = 0;
  ///@}

  /** @name Visitor methods for StopLossInPercentExpression types */
  ///@{
  virtual void visit (LongSideStopLossInPercent *) = 0;
  virtual void visit (ShortSideStopLossInPercent *) = 0;
  ///@}

  /** @name Visitor methods for MarketEntryExpression types */
  ///@{
  virtual void visit (LongMarketEntryOnOpen *) = 0;
  virtual void visit (ShortMarketEntryOnOpen *) = 0;
  ///@}

  /** @name Visitor methods for other AST nodes */
  ///@{
  virtual void visit (PatternDescription *) = 0;
  virtual void visit (PriceActionLabPattern *) = 0;
  ///@}
};

/**
 * @brief Base code generation visitor for TradingBlox platform.
 */
class TradingBloxCodeGenVisitor : public PalCodeGenVisitor
{
public:
  /**
   * @brief Constructs a TradingBloxCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem containing patterns.
   * @param bloxOutfileFileName The name of the output file for the TradingBlox script.
   */
  TradingBloxCodeGenVisitor(PriceActionLabSystem *system, 
			    const std::string& bloxOutfileFileName);
  /**
   * @brief Virtual destructor.
   */
  virtual ~TradingBloxCodeGenVisitor();
  /**
   * @brief Generates the TradingBlox code (entry orders script).
   */
  void generateCode();
  /**
   * @brief Generates TradingBlox code with patterns sorted.
   * @note Currently calls generateCode() which might not implement sorting yet.
   */
  void generatedSortedCode();

protected:
  /**
   * @brief Gets the output file stream for writing the script.
   * @return Pointer to the std::ofstream object.
   */
  std::ofstream * getOutputFileStream();
  /**
   * @brief Pure virtual method to generate code for variables in the entry script.
   *        Must be implemented by derived classes.
   */
  virtual void genCodeForVariablesInEntryScript() = 0;
  /**
   * @brief Checks if a pattern has a high reward-to-risk ratio.
   * @param pattern Pointer to the PriceActionLabPattern to check.
   * @return True if the reward-to-risk ratio is high, false otherwise.
   */
  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);

public:
  /**
   * @brief Generates the entry orders script content.
   */
  void generateEntryOrdersScript();

  /** @name Visitor methods for various AST nodes, specific to TradingBlox */
  ///@{
  void visit (PriceBarOpen *);
  void visit (PriceBarHigh *);
  void visit (PriceBarLow *);
  void visit (PriceBarClose *);
  void visit (VolumeBarReference *);
  void visit (Roc1BarReference *);
  void visit (MeanderBarReference *);
  void visit (VChartLowBarReference *);
  void visit (VChartHighBarReference *);
  void visit (GreaterThanExpr *);
  void visit (AndExpr *);
  void visit (PatternDescription *);
  void visit (PriceActionLabPattern *);
  void visit (LongMarketEntryOnOpen *);
  void visit (ShortMarketEntryOnOpen *);
  void visit (IBS1BarReference *);
  void visit (IBS2BarReference *);
  void visit (IBS3BarReference *);
  ///@}
  
private:
  /**
   * @brief Shared pointer to the trading system patterns.
   */
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  /**
   * @brief Output file stream for the entry orders script.
   */
  std::ofstream mEntryOrdersScriptFile;
};

/**
 * @brief TradingBlox code generator for RAD (Risk Adjusted Dollar) strategies.
 *
 * This class extends TradingBloxCodeGenVisitor to generate scripts
 * specifically for RAD-based trading systems in TradingBlox.
 */
class TradingBloxRADCodeGenVisitor : public TradingBloxCodeGenVisitor
{
public:
  using TradingBloxCodeGenVisitor::visit; // Inherit base class visit methods
  /**
   * @brief Constructs a TradingBloxRADCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param bloxOutfileFileName The name of the output file.
   */
  TradingBloxRADCodeGenVisitor(PriceActionLabSystem *system,
			       const std::string& bloxOutfileFileName);
  /**
   * @brief Destructor.
   */
  ~TradingBloxRADCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to RAD */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}

  /**
   * @brief Generates the script for handling filled entry orders.
   */
  void generateEntryOrderFilledScript();

private:
  /**
   * @brief Generates the script for exit orders.
   */
  void generateExitOrdersScript();
  /**
   * @brief Generates code for variables within the entry script, specific to RAD.
   */
  void genCodeForVariablesInEntryScript();

private:
  /**
   * @brief Output file stream for the entry order filled script.
   */
  std::ofstream mEntryOrderFilledScriptFile;
  /**
   * @brief Output file stream for the exit order script.
   */
  std::ofstream mExitOrderScriptFile;
};

/**
 * @brief TradingBlox code generator for Point Adjusted strategies.
 *
 * This class extends TradingBloxCodeGenVisitor to generate scripts
 * specifically for point-adjusted trading systems in TradingBlox.
 */
class TradingBloxPointAdjustedCodeGenVisitor : public TradingBloxCodeGenVisitor
{
public:
  using TradingBloxCodeGenVisitor::visit; // Inherit base class visit methods
  /**
   * @brief Constructs a TradingBloxPointAdjustedCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param bloxOutfileFileName The name of the output file.
   */
  TradingBloxPointAdjustedCodeGenVisitor(PriceActionLabSystem *system,
					 const std::string& bloxOutfileFileName);
  /**
   * @brief Destructor.
   */
  ~TradingBloxPointAdjustedCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to Point Adjusted */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}
  /**
   * @brief Generates the script for handling filled entry orders.
   */
  void generateEntryOrderFilledScript();

private:
  /**
   * @brief Generates the script for exit orders.
   */
  void generateExitOrdersScript();
  /**
   * @brief Generates code for variables within the entry script, specific to Point Adjusted.
   */
  void genCodeForVariablesInEntryScript();

private:
  /**
   * @brief Output file stream for the entry order filled script.
   */
  std::ofstream mEntryOrderFilledScriptFile;
  /**
   * @brief Output file stream for the exit order script.
   */
  std::ofstream mExitOrderScriptFile;
};

////////////

/**
 * @brief Base code generation visitor for WealthLab platform.
 */
class WealthLabCodeGenVisitor : public PalCodeGenVisitor
{
public:
  /**
   * @brief Constructs a WealthLabCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   */
  WealthLabCodeGenVisitor(PriceActionLabSystem *system);
  /**
   * @brief Constructs a WealthLabCodeGenVisitor with a specific output file name.
   * @param system Pointer to the PriceActionLabSystem.
   * @param outputFileName The name for the output WealthLab script file.
   */
  WealthLabCodeGenVisitor(PriceActionLabSystem *system, 
			  const std::string& outputFileName);
  /**
   * @brief Virtual destructor.
   */
  virtual ~WealthLabCodeGenVisitor();
  /**
   * @brief Generates the WealthLab code.
   */
  void generateCode();
  /**
   * @brief Generates WealthLab code with patterns sorted.
   * @note Currently calls generateCode() which might not implement sorting yet.
   */
  void generatedSortedCode();

protected:
  /**
   * @brief Gets the output file stream for writing the WealthLab script.
   * @return Pointer to the std::ofstream object.
   */
  std::ofstream * getOutputFileStream();
  /**
   * @brief Pure virtual method to generate code for variables in the entry script.
   *        Must be implemented by derived classes.
   */
  virtual void genCodeForVariablesInEntryScript() = 0;
  /**
   * @brief Checks if a pattern has a high reward-to-risk ratio.
   * @param pattern Pointer to the PriceActionLabPattern to check.
   * @return True if the reward-to-risk ratio is high, false otherwise.
   */
  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);

public:
  /**
   * @brief Generates the entry orders script content for WealthLab.
   */
  void generateEntryOrdersScript();

  /** @name Visitor methods for various AST nodes, specific to WealthLab */
  ///@{
  void visit (PriceBarOpen *);
  void visit (PriceBarHigh *);
  void visit (PriceBarLow *);
  void visit (PriceBarClose *);
  void visit (VolumeBarReference *);
  void visit (Roc1BarReference *);
  void visit (MeanderBarReference *);
  void visit (VChartLowBarReference *);
  void visit (VChartHighBarReference *);
  void visit (GreaterThanExpr *);
  void visit (AndExpr *);
  void visit (PatternDescription *);
  void visit (PriceActionLabPattern *);
  void visit (LongMarketEntryOnOpen *);
  void visit (ShortMarketEntryOnOpen *);
  void visit (IBS1BarReference *);
  void visit (IBS2BarReference *);
  void visit (IBS3BarReference *);
  ///@}

private:
  /**
   * @brief Shared pointer to the trading system patterns.
   */
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  /**
   * @brief Output file stream for the WealthLab trading model script.
   */
  std::ofstream mTradingModelFileName;
  /**
   * @brief Flag to track if the first 'if' condition for long patterns has been generated.
   */
  bool mFirstIfForLongsGenerated;
  /**
   * @brief Flag to track if the first 'if' condition for short patterns has been generated.
   */
  bool mFirstIfForShortsGenerated;
};

/**
 * @brief WealthLab code generator for RAD (Risk Adjusted Dollar) strategies.
 */
class WealthLabRADCodeGenVisitor : public WealthLabCodeGenVisitor
{
public:
  using WealthLabCodeGenVisitor::visit; // Inherit base class visit methods

  /**
   * @brief Constructs a WealthLabRADCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   */
  WealthLabRADCodeGenVisitor(PriceActionLabSystem *system);
  /**
   * @brief Constructs a WealthLabRADCodeGenVisitor with a specific output file name.
   * @param system Pointer to the PriceActionLabSystem.
   * @param outputFileName The name for the output WealthLab script file.
   */
  WealthLabRADCodeGenVisitor(PriceActionLabSystem *system, 
			     const std::string& outputFileName);
  /**
   * @brief Destructor.
   */
  ~WealthLabRADCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to RAD */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}
  /**
   * @brief Generates the script for handling filled entry orders in WealthLab for RAD.
   */
  void generateEntryOrderFilledScript();

private:
  /**
   * @brief Generates the script for exit orders in WealthLab for RAD.
   */
  void generateExitOrdersScript();
  /**
   * @brief Generates code for variables within the entry script, specific to RAD.
   */
  void genCodeForVariablesInEntryScript();

};

/**
 * @brief WealthLab code generator for Point Adjusted strategies.
 */
class WealthLabPointAdjustedCodeGenVisitor : public WealthLabCodeGenVisitor
{
public:
  using WealthLabCodeGenVisitor::visit; // Inherit base class visit methods

  /**
   * @brief Constructs a WealthLabPointAdjustedCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   */
  WealthLabPointAdjustedCodeGenVisitor(PriceActionLabSystem *system);
  /**
   * @brief Destructor.
   */
  ~WealthLabPointAdjustedCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to Point Adjusted */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}
  /**
   * @brief Generates the script for handling filled entry orders in WealthLab for Point Adjusted.
   */
  void generateEntryOrderFilledScript();

private:
  /**
   * @brief Generates the script for exit orders in WealthLab for Point Adjusted.
   */
  void generateExitOrdersScript();
  /**
   * @brief Generates code for variables within the entry script, specific to Point Adjusted.
   */
  void genCodeForVariablesInEntryScript();


};

// EasyLanguage Code Generator

/**
 * @brief Base code generation visitor for EasyLanguage platform.
 */
class EasyLanguageCodeGenVisitor : public PalCodeGenVisitor
{
public:
  /**
   * @brief Constructs an EasyLanguageCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param outputFileName The name for the output EasyLanguage file.
   */
  EasyLanguageCodeGenVisitor(std::shared_ptr<PriceActionLabSystem> system,
  	     const std::string& outputFileName);

  /**
   * @brief Virtual destructor.
   */
  virtual ~EasyLanguageCodeGenVisitor();
  
  /**
   * @brief Generates the EasyLanguage code by processing a template file.
   */
  void generateCode();
//  void generatedSortedCode(); // Potentially for sorted generation, currently commented out

private:
//  void genCodeForCommonVariables(); // Commented out utility method
//  void genCodeForCommonEntry(); // Commented out utility method
//  void genCodeToInitVolatility(bool shortSide); // Commented out utility method
//  void genCodeForCommonVariableInit(); // Commented out utility method
  /**
   * @brief Pure virtual method to set stop and target for long trades.
   */
  virtual void setStopTargetLong() = 0;
  /**
   * @brief Pure virtual method to set stop and target for short trades.
   */
  virtual void setStopTargetShort() = 0;
  /**
   * @brief Inserts the generated code for long patterns into the output stream.
   */
  void insertLongPatterns();
  /**
   * @brief Inserts the generated code for short patterns into the output stream.
   */
  void insertShortPatterns();

protected:
//  void genCommonCodeForLongExitPrologue(); // Commented out utility method
//  void genCommonCodeForShortExitPrologue(); // Commented out utility method

  /**
   * @brief Gets the output file stream for EasyLanguage code.
   * @return Pointer to the std::ofstream object.
   */
  std::ofstream * getOutputFileStream();
//  virtual void genCodeForVariablesInEntryScript() = 0; // Commented out pure virtual
//  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern); // Commented out utility
//  virtual void genCodeForEntryExit() = 0; // Commented out pure virtual
//  virtual void genCodeToInitializeVariables() = 0; // Commented out pure virtual

public:
  /**
   * @brief Generates the entry orders script portion for EasyLanguage.
   */
  void generateEntryOrdersScript();

  /** @name Visitor methods for various AST nodes, specific to EasyLanguage */
  ///@{
  void visit (PriceBarOpen *);
  void visit (PriceBarHigh *);
  void visit (PriceBarLow *);
  void visit (PriceBarClose *);
  void visit (VolumeBarReference *);
  void visit (Roc1BarReference *);
  void visit (MeanderBarReference *);
  void visit (VChartLowBarReference *);
  void visit (VChartHighBarReference *);
  void visit (GreaterThanExpr *);
  void visit (AndExpr *);
  void visit (PatternDescription *);
  void visit (PriceActionLabPattern *);
  void visit (LongMarketEntryOnOpen *);
  void visit (ShortMarketEntryOnOpen *);
  void visit (IBS1BarReference *);
  void visit (IBS2BarReference *);
  void visit (IBS3BarReference *);
  ///@}

private:
  /** @brief Marker string in template for inserting long patterns. */
  static const std::string LONG_PATTERNS_MARKER;
  /** @brief Marker string in template for inserting short patterns. */
  static const std::string SHORT_PATTERNS_MARKER;
  /** @brief Marker string in template for inserting long target/stop setters. */
  static const std::string LONG_TARGET_SETTER_MARKER;
  /** @brief Marker string in template for inserting short target/stop setters. */
  static const std::string SHORT_TARGET_SETTER_MARKER;
  /** @brief Shared pointer to the trading system patterns. */
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  /** @brief Input file stream for the EasyLanguage template. */
  std::ifstream mTemplateFile;
  /** @brief Output file stream for the generated EasyLanguage code. */
  std::ofstream mEasyLanguageFileName;
};

/**
 * @brief EasyLanguage code generator for RAD (Risk Adjusted Dollar) strategies.
 */
class EasyLanguageRADCodeGenVisitor : public EasyLanguageCodeGenVisitor
{
public:
  using EasyLanguageCodeGenVisitor::visit; // Inherit base class visit methods

  /**
   * @brief Constructs an EasyLanguageRADCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param outputFileName The name for the output EasyLanguage file.
   */
  EasyLanguageRADCodeGenVisitor(std::shared_ptr<PriceActionLabSystem> system,
                              const std::string& outputFileName);

  /**
   * @brief Destructor.
   */
  ~EasyLanguageRADCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to RAD */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}
//  void generateEntryOrderFilledScript(); // Commented out

private:

//  void generateExitOrdersScript(); // Commented out
//  void genCodeForVariablesInEntryScript(); // Commented out
//  void genCodeForEntryExit(); // Commented out
//  void genCodeToInitializeVariables(); // Commented out
    
private:
  /**
   * @brief Sets stop and target for long trades (RAD specific).
   */
  virtual void setStopTargetLong() override;
  /**
   * @brief Sets stop and target for short trades (RAD specific).
   */
  virtual void setStopTargetShort() override;
//  std::ofstream mEntryOrderFilledScriptFile; // Commented out
//  std::ofstream mExitOrderScriptFile; // Commented out
};

/**
 * @brief EasyLanguage code generator for Point Adjusted strategies.
 */
class EasyLanguagePointAdjustedCodeGenVisitor : public EasyLanguageCodeGenVisitor
{
public:
  using EasyLanguageCodeGenVisitor::visit; // Inherit base class visit methods

  /**
   * @brief Constructs an EasyLanguagePointAdjustedCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param templateFileName The name of the EasyLanguage template file.
   * @param outputFileName The name for the output EasyLanguage file.
   * @param dev1Detail Stop/target details for deviation 1 patterns.
   * @param dev2Detail Stop/target details for deviation 2 patterns.
   */
  EasyLanguagePointAdjustedCodeGenVisitor(std::shared_ptr<PriceActionLabSystem> system,
                                          const std::string& outputFileName);

  /**
   * @brief Destructor.
   */
  ~EasyLanguagePointAdjustedCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to Point Adjusted */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}
//  void generateEntryOrderFilledScript(); // Commented out

private:
//  void generateExitOrdersScript(); // Commented out
//  void genCodeForVariablesInEntryScript(); // Commented out
//  void genCodeForEntryExit(); // Commented out
//  void genCodeToInitializeVariables(); // Commented out

private:
  /**
   * @brief Sets stop and target for long trades (Point Adjusted specific).
   */
  virtual void setStopTargetLong() override;
  /**
   * @brief Sets stop and target for short trades (Point Adjusted specific).
   */
  virtual void setStopTargetShort() override;

//  std::ofstream mEntryOrderFilledScriptFile; // Commented out
//  std::ofstream mExitOrderScriptFile; // Commented out
};

///////////////////////////////////////////////
///
//////////////////////////////////////////////

/**
 * @brief Generic Price Action Lab code generator.
 *
 * This visitor generates a textual representation of the PAL patterns,
 * often used for debugging or a simple pattern language output.
 */
class PalCodeGenerator : public PalCodeGenVisitor
{
public:
  /**
   * @brief Constructs a PalCodeGenerator.
   * @param system Pointer to the PriceActionLabSystem.
   * @param outfileFileName The name of the output file.
   * @param reversePattern If true, generates pattern conditions in reverse order (optional, defaults to false).
   */
  PalCodeGenerator(PriceActionLabSystem *system,
		   const std::string& outfileFileName,
		   bool reversePattern = false);
  /**
   * @brief Destructor.
   */
  ~PalCodeGenerator();
  /**
   * @brief Generates the code (textual representation of patterns).
   */
  void generateCode();

  /** @name Visitor methods for various AST nodes */
  ///@{
  void visit (PriceBarOpen *);
  void visit (PriceBarHigh *);
  void visit (PriceBarLow *);
  void visit (PriceBarClose *);
  void visit (VolumeBarReference *);
  void visit (Roc1BarReference *);
  void visit (MeanderBarReference *);
  void visit (VChartLowBarReference *);
  void visit (VChartHighBarReference *);
  void visit (IBS1BarReference *);
  void visit (IBS2BarReference *);
  void visit (IBS3BarReference *);

  void visit (GreaterThanExpr *);
  void visit (AndExpr *);

  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);

  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  void visit (LongMarketEntryOnOpen *);
  void visit (ShortMarketEntryOnOpen *);
  void visit (PatternDescription *);
  void visit (PriceActionLabPattern *);
  ///@}

private:
  /**
   * @brief Gets the output file stream.
   * @return Pointer to the std::ofstream object.
   */
  std::ofstream * getOutputFileStream();
  /**
   * @brief Prints a separator line between patterns in the output file.
   */
  void printPatternSeperator();

private:
  /**
   * @brief Output file stream.
   */
  std::ofstream mOutFile;
  /**
   * @brief Shared pointer to the trading system patterns.
   */
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  /**
   * @brief Flag indicating whether to reverse pattern conditions.
   */
  bool mReversePattern;
};

// QuantConnect Code Gen Visitor

/**
 * @brief Base code generation visitor for QuantConnect platform.
 */
class QuantConnectCodeGenVisitor : public PalCodeGenVisitor
{
public:
  /**
   * @brief Constructs a QuantConnectCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param oututfileFileName The name for the output QuantConnect script file.
   */
  QuantConnectCodeGenVisitor(PriceActionLabSystem *system, 
			     const std::string& oututfileFileName);
  /**
   * @brief Virtual destructor.
   */
  virtual ~QuantConnectCodeGenVisitor();
  /**
   * @brief Generates the QuantConnect code.
   */
  void generateCode();
  /**
   * @brief Generates QuantConnect code with patterns sorted.
   * @note Currently calls generateCode() which might not implement sorting yet.
   */
  void generatedSortedCode();

private:
  /**
   * @brief Generates code for common variables used in QuantConnect scripts.
   */
  void genCodeForCommonVariables();
  /**
   * @brief Generates code for common entry logic in QuantConnect scripts.
   */
  void genCodeForCommonEntry();
  /**
   * @brief Generates code to initialize volatility related variables.
   * @param shortSide True if generating for short side, false for long side.
   */
  void genCodeToInitVolatility(bool shortSide);
  /**
   * @brief Generates code for common variable initialization.
   */
  void genCodeForCommonVariableInit();


protected:
  /**
   * @brief Generates common prologue code for long exit logic.
   */
  void genCommonCodeForLongExitPrologue();
  /**
   * @brief Generates common prologue code for short exit logic.
   */
  void genCommonCodeForShortExitPrologue();

  /**
   * @brief Gets the output file stream for the QuantConnect script.
   * @return Pointer to the std::ofstream object.
   */
  std::ofstream * getOutputFileStream();
  /**
   * @brief Pure virtual method to generate code for variables in the entry script.
   *        Must be implemented by derived classes.
   */
  virtual void genCodeForVariablesInEntryScript() = 0;
  /**
   * @brief Checks if a pattern has a high reward-to-risk ratio.
   * @param pattern Pointer to the PriceActionLabPattern to check.
   * @return True if the reward-to-risk ratio is high, false otherwise.
   */
  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);
  /**
   * @brief Pure virtual method to generate code for entry and exit logic.
   *        Must be implemented by derived classes.
   */
  virtual void genCodeForEntryExit() = 0;
  /**
   * @brief Pure virtual method to generate code for initializing variables.
   *        Must be implemented by derived classes.
   */
  virtual void genCodeToInitializeVariables() = 0;

public:
  /**
   * @brief Generates the entry orders script content for QuantConnect.
   */
  void generateEntryOrdersScript();

  /** @name Visitor methods for various AST nodes, specific to QuantConnect */
  ///@{
  void visit (PriceBarOpen *);
  void visit (PriceBarHigh *);
  void visit (PriceBarLow *);
  void visit (PriceBarClose *);
  void visit (VolumeBarReference *);
  void visit (Roc1BarReference *);
  void visit (MeanderBarReference *);
  void visit (VChartLowBarReference *);
  void visit (VChartHighBarReference *);
  void visit (GreaterThanExpr *);
  void visit (AndExpr *);
  void visit (PatternDescription *);
  void visit (PriceActionLabPattern *);
  void visit (LongMarketEntryOnOpen *);
  void visit (ShortMarketEntryOnOpen *);
  void visit (IBS1BarReference *);
  void visit (IBS2BarReference *);
  void visit (IBS3BarReference *);
  ///@}

private:
  /**
   * @brief Shared pointer to the trading system patterns.
   */
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  /**
   * @brief Output file stream for the entry orders script.
   */
  std::ofstream mEntryOrdersScriptFile;
};

/**
 * @brief QuantConnect code generator specifically for Equity instruments.
 */
class QuantConnectEquityCodeGenVisitor : public QuantConnectCodeGenVisitor
{
public:
  using QuantConnectCodeGenVisitor::visit; // Inherit base class visit methods

  /**
   * @brief Constructs a QuantConnectEquityCodeGenVisitor.
   * @param system Pointer to the PriceActionLabSystem.
   * @param oututfileFileName The name for the output QuantConnect script file.
   */
  QuantConnectEquityCodeGenVisitor(PriceActionLabSystem *system,
				   const std::string& oututfileFileName);
  /**
   * @brief Destructor.
   */
  ~QuantConnectEquityCodeGenVisitor();

  /** @name Visitor methods for profit target and stop loss, specific to QuantConnect Equity */
  ///@{
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  ///@}
  /**
   * @brief Generates the script for handling filled entry orders for QuantConnect Equity.
   */
  void generateEntryOrderFilledScript();

private:
  /**
   * @brief Generates the script for exit orders for QuantConnect Equity.
   */
  void generateExitOrdersScript();
  /**
   * @brief Generates code for variables within the entry script, specific to QuantConnect Equity.
   */
  void genCodeForVariablesInEntryScript();
  /**
   * @brief Generates code for entry and exit logic, specific to QuantConnect Equity.
   */
  void genCodeForEntryExit();
  /**
   * @brief Generates code for initializing variables, specific to QuantConnect Equity.
   */
  void genCodeToInitializeVariables();
    
private:
  /**
   * @brief Output file stream for the entry order filled script.
   */
  std::ofstream mEntryOrderFilledScriptFile;
  /**
   * @brief Output file stream for the exit order script.
   */
  std::ofstream mExitOrderScriptFile;
};

#endif
