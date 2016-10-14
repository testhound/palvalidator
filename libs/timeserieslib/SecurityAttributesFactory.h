// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SECURITY_ATTRIBUTES_FACTORY_H
#define __SECURITY_ATTRIBUTES_FACTORY_H 1

#include <string>
#include <map>
#include <memory>
#include "SecurityAttributes.h"

using dec::decimal;
using std::string;

namespace mkc_timeseries
{
  //
  // class SecurtyAttributesFactoryException
  //

  class SecurtyAttributesFactoryException : public std::domain_error
  {
  public:
    SecurtyAttributesFactoryException(const std::string msg) 
      : std::domain_error(msg)
    {}
    
    ~SecurtyAttributesFactoryException()
    {}
    
  };

  template <class Decimal>
  class SecurityAttributesFactory
  {
  public:
    typedef typename std::map<std::string, std::shared_ptr<SecurityAttributes<Decimal>>>::const_iterator SecurityAttributesIterator;

  public:
    SecurityAttributesFactory ()
    {
      initializeEquityAttributes();
      initializeFuturesAttributes();
    }

    ~SecurityAttributesFactory ()
    {}

    SecurityAttributesIterator getSecurityAttributes(const std::string securitySymbol)
    {
      return mSecurityAttributes.find(securitySymbol);
    }

    SecurityAttributesIterator endSecurityAttributes() const
    {
      return mSecurityAttributes.end();
    }

 

    void initializeEquityAttributes()
    {
      initializeETFAttributes();
      initializeCommonStockAttributes();
    }

    void initializeFuturesAttributes()
    {
      initializeGrainFuturesAttributes();
      initializeSoftsFuturesAttributes();
      initializeBondFuturesAttributes();
      initializeStockIndexFuturesAttributes();
      initializeCurrencyFuturesAttributes();
      initializeMetalsFuturesAttributes();
      initializeEnergyFuturesAttributes();
    }

    void initializeCommonStockAttributes()
    {
      addCommonStock (std::string("BA"), std::string("Boeing"));
      addCommonStock (std::string("NEM"), std::string("Nemont Mining"));
      addCommonStock (std::string("AMZN"), std::string("Amazon"));
      addCommonStock (std::string("GOOGL"), std::string("Google"));
      addCommonStock (std::string("FB"), std::string("Facebook"));
      addCommonStock (std::string("NFLX"), std::string("Netflix"));

    }

    void initializeETFAttributes()
    {
      addUnLeveragedETF (std::string("SPY"), 
			 std::string("SPDR S&P 500 ETF"),
			 createDecimal("0.09"),
			 boost::gregorian::from_undelimited_string("19930122"));

      addUnLeveragedETF (std::string("QQQ"), 
			 std::string("PowerShares QQQ ETF"),
			 createDecimal("0.20"),
			 boost::gregorian::from_undelimited_string("19990310"));

      addUnLeveragedETF (std::string("EWJ"), 
			 std::string("iShares MSCI Japan ETF"),
			 createDecimal("0.48"),
			 boost::gregorian::from_undelimited_string("19960312"));


      addLeveragedETF (std::string("SSO"), 
		       std::string("ProShares Ultra S&P 500"), 
		       createDecimal("0.89"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20060619"));

      addLeveragedETF (std::string("SDS"), 
		       std::string("ProShares UltraShort S&P 500"), 
		       createDecimal("0.91"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20060711"));

      addLeveragedETF (std::string("QLD"), 
		       std::string("ProShares UltraPro QQQ"), 
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20060619"));

      addLeveragedETF (std::string("QID"), 
		       std::string("ProShares UltraShort QQQ"), 
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20060711"));

      addUnLeveragedETF (std::string("GDX"), 
			 std::string("Van Eck Gold Miners ETF"),
			 createDecimal("0.52"),
			 boost::gregorian::from_undelimited_string("20060522"));

      addUnLeveragedETF (std::string("IBB"), 
			 std::string("iShares Nasdaq Biotechnology ETF"),
			 createDecimal("0.48"),
			 boost::gregorian::from_undelimited_string("20010205"));
    }

    

    void initializeStockIndexFuturesAttributes()
    {
      std::string miniNasdaq100FuturesSymbol("@NQ");
      std::string miniSP500FuturesSymbol("@ES");
      std::string miniRussell2000FuturesSymbol("@TF");
      std::string miniDowFuturesSymbol("@YM");

      auto miniNasdaq100Attributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(miniNasdaq100FuturesSymbol, 
						     "Emini Nasdaq 100 Futures",
						     createDecimal("20.0"),
						     createDecimal("0.25"));
      mSecurityAttributes.insert(std::make_pair(miniNasdaq100FuturesSymbol,
						miniNasdaq100Attributes));

      auto miniSP500Attributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(miniSP500FuturesSymbol, 
						     "Emini S&P 500 Futures",
						     createDecimal("50.0"),
						     createDecimal("0.25"));
      mSecurityAttributes.insert(std::make_pair(miniSP500FuturesSymbol,
						miniSP500Attributes));

      auto miniRussell2000Attributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(miniRussell2000FuturesSymbol, 
						     "Russell 2000 Futures",
						     createDecimal("100.0"),
						     createDecimal("0.10"));
      mSecurityAttributes.insert(std::make_pair(miniRussell2000FuturesSymbol,
						miniRussell2000Attributes));

      auto miniDowAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(miniDowFuturesSymbol, 
							  "Mini Dow Futures",
							  createDecimal("5.0"),
							  createDecimal("1.0"));
      mSecurityAttributes.insert(std::make_pair(miniDowFuturesSymbol,
						miniDowAttributes));
    }

    void initializeSoftsFuturesAttributes()
    {
      std::string cottonFuturesSymbol("@CT");
      std::string milkFuturesSymbol("@DA");

      auto cottonAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(cottonFuturesSymbol, 
							  "Cotton Futures",
							  createDecimal("500.0"),
							  createDecimal("0.01"));

      mSecurityAttributes.insert(std::make_pair(cottonFuturesSymbol, 
						cottonAttributes));

      auto milkAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(milkFuturesSymbol, 
							  "Milk Futures",
							  createDecimal("2000.0"),
							  createDecimal("0.01"));
      mSecurityAttributes.insert(std::make_pair(milkFuturesSymbol, 
						milkAttributes));

    }

    void initializeBondFuturesAttributes()
    {
      std::string tenYearFuturesSymbol("@TY");
      std::string thirtyYearFuturesSymbol("@US");
      std::string fiveYearFuturesSymbol("@FV");

      auto fiveYearAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(fiveYearFuturesSymbol, 
						     "5-Year Note Futures",
						     createDecimal("1000.0"),
						     createDecimal("0.0078125"));

      auto tenYearAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(tenYearFuturesSymbol, 
						     "10-Year Note Futures",
						     createDecimal("1000.0"),
						     createDecimal("0.015625"));

      auto thirtyYearAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(thirtyYearFuturesSymbol, 
							  "30-Year Note Futures",
							  createDecimal("1000.0"),
							  createDecimal("0.03125"));

      mSecurityAttributes.insert(std::make_pair(fiveYearFuturesSymbol,
						fiveYearAttributes));

      mSecurityAttributes.insert(std::make_pair(tenYearFuturesSymbol,
						tenYearAttributes));

      mSecurityAttributes.insert(std::make_pair(thirtyYearFuturesSymbol,
						thirtyYearAttributes));
    }

    void initializeCurrencyFuturesAttributes()
    {

      addFuturesAttributes (std::string("@DX"), std::string("Dollar Index Futures"),
			    createDecimal("1000.00"), createDecimal("0.001"));

      addFuturesAttributes (std::string("@JY"), std::string("Japanese Yen Futures"),
			    createDecimal("125000.00"), createDecimal("0.0001"));

      addFuturesAttributes (std::string("@BP"), std::string("British Pound Futures"),
			    createDecimal("125000.00"), createDecimal("0.0001"));
    }

    
    void initializeGrainFuturesAttributes()
    {
      std::string cornFuturesSymbol("@C");

      auto cornAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(cornFuturesSymbol, 
						     "Corn Futures",
						     createDecimal("50.0"),
						     createDecimal("0.25"));
      mSecurityAttributes.insert(std::make_pair(cornFuturesSymbol,
						cornAttributes));
    }

    void initializeMetalsFuturesAttributes()
    {
      std::string goldFuturesSymbol("@GC");

      auto goldAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(goldFuturesSymbol, 
						     "Gold Futures",
						     createDecimal("100.0"),
						     createDecimal("0.10"));
      mSecurityAttributes.insert(std::make_pair(goldFuturesSymbol,
						goldAttributes));
    }

    void initializeEnergyFuturesAttributes()
    {
      std::string crudeOilFuturesSymbol("@CL");

      auto crudeOilAttributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(crudeOilFuturesSymbol, 
							  "Crude Oil Futures",
							  createDecimal("1000.0"),
							  createDecimal("0.01"));
      mSecurityAttributes.insert(std::make_pair(crudeOilFuturesSymbol,
						crudeOilAttributes));
    }

 private:
    Decimal createDecimal(const std::string& valueString)
    {
      return dec::fromString<Decimal>(valueString);
    }

    void addFuturesAttributes(const std::string& symbol, 
			      const std::string& futuresName,
			      const Decimal& bigPointValue,
			      const Decimal& tickValue)
    {
      auto attributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(symbol, futuresName,
							  bigPointValue, tickValue);
      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addUnLeveragedETF (const std::string& symbol, const std::string& etfName, 
			    const Decimal& expenseRatio, 
			    const boost::gregorian::date& inceptionDate)
    {
      LeverageAttributes<Decimal> noLeverage(DecimalConstants<Decimal>::DecimalZero);

      FundAttributes<Decimal> etfAttributes(inceptionDate, expenseRatio, noLeverage);
      auto attributes = 
	std::make_shared<ETFSecurityAttributes<Decimal>>(symbol, 
						      etfName,
						      etfAttributes);

      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addLeveragedETF (const std::string& symbol, const std::string& etfName, 
			  const Decimal& expenseRatio,
			  const Decimal& leverage,
			  const boost::gregorian::date& inceptionDate)
    {
      LeverageAttributes<Decimal> leverageForFund(leverage);

      FundAttributes<Decimal> etfAttributes(inceptionDate, expenseRatio, leverageForFund);
      auto attributes = 
	std::make_shared<ETFSecurityAttributes<Decimal>>(symbol, 
						      etfName,
						      etfAttributes);

      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addCommonStock (const std::string& symbol, const std::string& stockName)
    {
      auto attributes = 
	std::make_shared<CommonStockSecurityAttributes<Decimal>>(symbol, stockName);

      mSecurityAttributes.insert(std::make_pair(symbol, attributes));
    }

  private:
    std::map<std::string, std::shared_ptr<SecurityAttributes<Decimal>>> mSecurityAttributes;

  };

  template <class Decimal>
  std::shared_ptr<SecurityAttributes<Decimal>> getSecurityAttributes (const std::string &symbol)
  {
    SecurityAttributesFactory<Decimal> factory;
    typename SecurityAttributesFactory<Decimal>::SecurityAttributesIterator it = factory.getSecurityAttributes (symbol);

    if (it != factory.endSecurityAttributes())
      return it->second;
    else
      throw SecurtyAttributesFactoryException("getSecurityAttributes - ticker symbol " +symbol +" is unkown");
  }
}
#endif
