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


class PalCodeGenVisitor
{
public:
  PalCodeGenVisitor();
  virtual ~PalCodeGenVisitor();
  virtual void generateCode() = 0;

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
  virtual void visit (GreaterThanExpr *) = 0;
  virtual void visit (AndExpr *) = 0;

  virtual void visit (LongSideProfitTargetInPercent *) = 0;
  virtual void visit (ShortSideProfitTargetInPercent *) = 0;

  virtual void visit (LongSideStopLossInPercent *) = 0;
  virtual void visit (ShortSideStopLossInPercent *) = 0;
  virtual void visit (LongMarketEntryOnOpen *) = 0;
  virtual void visit (ShortMarketEntryOnOpen *) = 0;
  virtual void visit (PatternDescription *) = 0;
  virtual void visit (PriceActionLabPattern *) = 0;

};

class TradingBloxCodeGenVisitor : public PalCodeGenVisitor
{
public:
  TradingBloxCodeGenVisitor(PriceActionLabSystem *system, 
			    const std::string& bloxOutfileFileName);
  virtual ~TradingBloxCodeGenVisitor();
  void generateCode();
  void generatedSortedCode();

protected:
  std::ofstream * getOutputFileStream();
  virtual void genCodeForVariablesInEntryScript() = 0;
  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);

public:
  void generateEntryOrdersScript();
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
  
private:
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  std::ofstream mEntryOrdersScriptFile;
};

class TradingBloxRADCodeGenVisitor : public TradingBloxCodeGenVisitor
{
public:
  TradingBloxRADCodeGenVisitor(PriceActionLabSystem *system,
			       const std::string& bloxOutfileFileName);
  ~TradingBloxRADCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  void generateEntryOrderFilledScript();

private:
  void generateExitOrdersScript();
  void genCodeForVariablesInEntryScript();

private:
  std::ofstream mEntryOrderFilledScriptFile;
  std::ofstream mExitOrderScriptFile;
};


class TradingBloxPointAdjustedCodeGenVisitor : public TradingBloxCodeGenVisitor
{
public:
  TradingBloxPointAdjustedCodeGenVisitor(PriceActionLabSystem *system,
					 const std::string& bloxOutfileFileName);
  ~TradingBloxPointAdjustedCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  void generateEntryOrderFilledScript();

private:
  void generateExitOrdersScript();
  void genCodeForVariablesInEntryScript();

private:
  std::ofstream mEntryOrderFilledScriptFile;
  std::ofstream mExitOrderScriptFile;
};

////////////


class WealthLabCodeGenVisitor : public PalCodeGenVisitor
{
public:
  WealthLabCodeGenVisitor(PriceActionLabSystem *system);
  WealthLabCodeGenVisitor(PriceActionLabSystem *system, 
			  const std::string& outputFileName);
  virtual ~WealthLabCodeGenVisitor();
  void generateCode();
  void generatedSortedCode();

protected:
  std::ofstream * getOutputFileStream();
  virtual void genCodeForVariablesInEntryScript() = 0;
  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);

public:
  void generateEntryOrdersScript();
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

private:
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  std::ofstream mTradingModelFileName;
  bool mFirstIfForLongsGenerated;
  bool mFirstIfForShortsGenerated;
};

class WealthLabRADCodeGenVisitor : public WealthLabCodeGenVisitor
{
public:
  WealthLabRADCodeGenVisitor(PriceActionLabSystem *system);
  WealthLabRADCodeGenVisitor(PriceActionLabSystem *system, 
			     const std::string& outputFileName);
  ~WealthLabRADCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  void generateEntryOrderFilledScript();

private:
  void generateExitOrdersScript();
  void genCodeForVariablesInEntryScript();

};


class WealthLabPointAdjustedCodeGenVisitor : public WealthLabCodeGenVisitor
{
public:
  WealthLabPointAdjustedCodeGenVisitor(PriceActionLabSystem *system);
  ~WealthLabPointAdjustedCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  void generateEntryOrderFilledScript();

private:
  void generateExitOrdersScript();
  void genCodeForVariablesInEntryScript();


};

// EasyLanguage Code Generator

class EasyLanguageCodeGenVisitor : public PalCodeGenVisitor
{
public:
  EasyLanguageCodeGenVisitor(PriceActionLabSystem *system, 
			     const std::string& outputFileName,
			     const StopTargetDetail& dev1Detail,
			     const StopTargetDetail& dev2Detail);

  EasyLanguageCodeGenVisitor(PriceActionLabSystem *system,
                             const std::string& templateFileName,
                             const std::string& outputFileName,
                             const StopTargetDetail& dev1Detail,
                             const StopTargetDetail& dev2Detail);

  virtual ~EasyLanguageCodeGenVisitor();
  bool isDev1Pattern(PriceActionLabPattern *pattern);
  bool isDev2Pattern(PriceActionLabPattern *pattern);
  
  void generateCode();
//  void generatedSortedCode();

private:
//  void genCodeForCommonVariables();
//  void genCodeForCommonEntry();
//  void genCodeToInitVolatility(bool shortSide);
//  void genCodeForCommonVariableInit();
  virtual void setStopTargetLong() = 0;
  virtual void setStopTargetShort() = 0;
  void insertLongPatterns();
  void insertShortPatterns();

protected:
//  void genCommonCodeForLongExitPrologue();
//  void genCommonCodeForShortExitPrologue();

  std::ofstream * getOutputFileStream();
//  virtual void genCodeForVariablesInEntryScript() = 0;
//  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);
//  virtual void genCodeForEntryExit() = 0;
//  virtual void genCodeToInitializeVariables() = 0;

public:
  void generateEntryOrdersScript();
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

private:
  static const std::string LONG_PATTERNS_MARKER;
  static const std::string SHORT_PATTERNS_MARKER;
  static const std::string LONG_TARGET_SETTER_MARKER;
  static const std::string SHORT_TARGET_SETTER_MARKER;
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  std::ifstream mTemplateFile;
  std::ofstream mEasyLanguageFileName;
  StopTargetDetail mDev1Detail;
  StopTargetDetail mDev2Detail;
};


class EasyLanguageRADCodeGenVisitor : public EasyLanguageCodeGenVisitor
{
public:
  EasyLanguageRADCodeGenVisitor(PriceActionLabSystem *system,
                             const std::string& templateFileName,
                             const std::string& outputFileName,
                             const StopTargetDetail& dev1Detail,
                             const StopTargetDetail& dev2Detail);

  ~EasyLanguageRADCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
//  void generateEntryOrderFilledScript();

private:

//  void generateExitOrdersScript();
//  void genCodeForVariablesInEntryScript();
//  void genCodeForEntryExit();
//  void genCodeToInitializeVariables();
    
private:
  virtual void setStopTargetLong() override;
  virtual void setStopTargetShort() override;
//  std::ofstream mEntryOrderFilledScriptFile;
//  std::ofstream mExitOrderScriptFile;
};


class EasyLanguagePointAdjustedCodeGenVisitor : public EasyLanguageCodeGenVisitor
{
public:
  EasyLanguagePointAdjustedCodeGenVisitor(PriceActionLabSystem *system,
                                          const std::string& templateFileName,
                                          const std::string& outputFileName,
                                          const StopTargetDetail& dev1Detail,
                                          const StopTargetDetail& dev2Detail);

  ~EasyLanguagePointAdjustedCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
//  void generateEntryOrderFilledScript();

private:
//  void generateExitOrdersScript();
//  void genCodeForVariablesInEntryScript();
//  void genCodeForEntryExit();
//  void genCodeToInitializeVariables();

private:
  virtual void setStopTargetLong() override;
  virtual void setStopTargetShort() override;

//  std::ofstream mEntryOrderFilledScriptFile;
//  std::ofstream mExitOrderScriptFile;
};

///////////////////////////////////////////////
///
//////////////////////////////////////////////

class PalCodeGenerator : public PalCodeGenVisitor
{
public:
  PalCodeGenerator(PriceActionLabSystem *system,
		    const std::string& outfileFileName);
  ~PalCodeGenerator();
  void generateCode();

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

private:
  std::ofstream * getOutputFileStream();
  void printPatternSeperator();

private:
  std::ofstream mOutFile;
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
};

// QuantConnect Code Gen Visitor

class QuantConnectCodeGenVisitor : public PalCodeGenVisitor
{
public:
  QuantConnectCodeGenVisitor(PriceActionLabSystem *system, 
			     const std::string& oututfileFileName);
  virtual ~QuantConnectCodeGenVisitor();
  void generateCode();
  void generatedSortedCode();

private:
  void genCodeForCommonVariables();
  void genCodeForCommonEntry();
  void genCodeToInitVolatility(bool shortSide);
  void genCodeForCommonVariableInit();


protected:
  void genCommonCodeForLongExitPrologue();
  void genCommonCodeForShortExitPrologue();

  std::ofstream * getOutputFileStream();
  virtual void genCodeForVariablesInEntryScript() = 0;
  bool isHighRewardToRiskRatioPattern (PriceActionLabPattern *pattern);
  virtual void genCodeForEntryExit() = 0;
  virtual void genCodeToInitializeVariables() = 0;

public:
  void generateEntryOrdersScript();
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

private:
  std::shared_ptr<PriceActionLabSystem> mTradingSystemPatterns;
  std::ofstream mEntryOrdersScriptFile;
};

class QuantConnectEquityCodeGenVisitor : public QuantConnectCodeGenVisitor
{
public:
  QuantConnectEquityCodeGenVisitor(PriceActionLabSystem *system,
				   const std::string& oututfileFileName);
  ~QuantConnectEquityCodeGenVisitor();
  void visit (LongSideProfitTargetInPercent *);
  void visit (ShortSideProfitTargetInPercent *);
  void visit (LongSideStopLossInPercent *);
  void visit (ShortSideStopLossInPercent *);
  void generateEntryOrderFilledScript();

private:
  void generateExitOrdersScript();
  void genCodeForVariablesInEntryScript();
  void genCodeForEntryExit();
  void genCodeToInitializeVariables();
    
private:
  std::ofstream mEntryOrderFilledScriptFile;
  std::ofstream mExitOrderScriptFile;
};

#endif
