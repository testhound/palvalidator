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
    static SecurityAttributesFactory<Decimal>& instance()
    {
      static SecurityAttributesFactory<Decimal> theOne;
      return theOne;
    }

    SecurityAttributesIterator getSecurityAttributes(const std::string securitySymbol)
    {
      return mSecurityAttributes.find(securitySymbol);
    }

    SecurityAttributesIterator endSecurityAttributes() const
    {
      return mSecurityAttributes.end();
    }
 
  private:
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
      initializeMeatFuturesAttributes();
      initialize1XLeveragedETFs();
      addFuturesAttributes (std::string("@VX"), std::string("VIX Futures"),
			    createDecimal("1000.00"), createDecimal("0.05"));

    }

    void initializeCommonStockAttributes()
    {
      addCommonStock (std::string("TXN"), std::string("Texas Instruments"),
		      boost::gregorian::from_undelimited_string("19531001"));
      addCommonStock (std::string("BA"), std::string("Boeing"),
		      boost::gregorian::from_undelimited_string("19620102"));
      addCommonStock (std::string("LOW"), std::string("Lowes"),
		      boost::gregorian::from_undelimited_string("19611010"));
      addCommonStock (std::string("HD"), std::string("Home Depot"),
		      boost::gregorian::from_undelimited_string("19810922"));
      addCommonStock (std::string("LMT"), std::string("Lockeed Martin"),
		      boost::gregorian::from_undelimited_string("19950315"));
      addCommonStock (std::string("RTX"), std::string("Raytheon"),
		      boost::gregorian::from_undelimited_string("19520915"));

      addCommonStock (std::string("BBY"), std::string("Best Buy"),
		      boost::gregorian::from_undelimited_string("19870720"));
      addCommonStock (std::string("TSCO"), std::string("Tractor Supply"),
		      boost::gregorian::from_undelimited_string("19940217"));
      addCommonStock (std::string("AMAT"), std::string("Applied Materials"),
		      boost::gregorian::from_undelimited_string("19721005"));
      addCommonStock (std::string("TSM"), std::string("Taiwan Semiconductor"),
		      boost::gregorian::from_undelimited_string("19971008"));
      addCommonStock (std::string("NXPI"), std::string("NXP Semiconductors"),
		      boost::gregorian::from_undelimited_string("20100806"));
      addCommonStock (std::string("ON"), std::string("On Semiconductor"),
		      boost::gregorian::from_undelimited_string("20000501"));
      addCommonStock (std::string("NEM"), std::string("Nemont Mining"),
		      boost::gregorian::from_undelimited_string("19830406"));
      addCommonStock (std::string("AMZN"), std::string("Amazon"),
		      boost::gregorian::from_undelimited_string("19970515"));
      addCommonStock (std::string("GOOGL"), std::string("Google"),
		      boost::gregorian::from_undelimited_string("20040814"));
      addCommonStock (std::string("META"), std::string("Meta"),
		      boost::gregorian::from_undelimited_string("20120518"));
      addCommonStock (std::string("NFLX"), std::string("Netflix"),
		      boost::gregorian::from_undelimited_string("20020523"));
      addCommonStock (std::string("XOM"), std::string("Exxon Mobil"),
		      boost::gregorian::from_undelimited_string("19700102"));
      addCommonStock (std::string("MSFT"), std::string("Microsoft"),
		      boost::gregorian::from_undelimited_string("19860313"));
      addCommonStock (std::string("INTC"), std::string("Intel"),
		      boost::gregorian::from_undelimited_string("19711013"));
      addCommonStock (std::string("NTAP"), std::string("NetApp"),
		      boost::gregorian::from_undelimited_string("19951121"));

      addCommonStock (std::string("AMD"), std::string("Advanced Micro Devices"),
		      boost::gregorian::from_undelimited_string("19830321"));
      addCommonStock (std::string("MCHP"), std::string("Microchip"),
		      boost::gregorian::from_undelimited_string("19930319"));
      addCommonStock (std::string("MU"), std::string("Micron"),
		      boost::gregorian::from_undelimited_string("19840604"));
      addCommonStock (std::string("AAPL"), std::string("Apple"),
		      boost::gregorian::from_undelimited_string("19801212"));
      addCommonStock (std::string("NVDA"), std::string("Nvidia"),
		      boost::gregorian::from_undelimited_string("19990122"));
      addCommonStock (std::string("NOW"), std::string("ServiceNow"),
		      boost::gregorian::from_undelimited_string("20120629"));
      addCommonStock (std::string("SQ"), std::string("Square"),
		      boost::gregorian::from_undelimited_string("20151119"));
      addCommonStock (std::string("ZM"), std::string("Zoom"),
		      boost::gregorian::from_undelimited_string("20190418"));
      addCommonStock (std::string("TSLA"), std::string("Tesla"),
		      boost::gregorian::from_undelimited_string("20100629"));
      addCommonStock (std::string("PINS"), std::string("Pinterest"),
		      boost::gregorian::from_undelimited_string("20190418"));
      addCommonStock (std::string("TEAM"), std::string("Atlassian"),
		      boost::gregorian::from_undelimited_string("20151210"));
      addCommonStock (std::string("ETSY"), std::string("Etsy"),
		      boost::gregorian::from_undelimited_string("20150416"));
      addCommonStock (std::string("OKTA"), std::string("Okta"),
		      boost::gregorian::from_undelimited_string("20170407"));
      addCommonStock (std::string("SHOP"), std::string("Shopify"),
		      boost::gregorian::from_undelimited_string("20150520"));
      addCommonStock (std::string("NIO"), std::string("NIO"),
		      boost::gregorian::from_undelimited_string("20180912"));
      addCommonStock (std::string("SNAP"), std::string("Snapchat"),
		      boost::gregorian::from_undelimited_string("20170302"));
      addCommonStock (std::string("PYPL"), std::string("PayPal"),
		      boost::gregorian::from_undelimited_string("20020215"));
      addCommonStock (std::string("MA"), std::string("Mastercard"),
		      boost::gregorian::from_undelimited_string("20060525"));
      addCommonStock (std::string("V"), std::string("Visa"),
		      boost::gregorian::from_undelimited_string("20080318"));

      addCommonStock (std::string("ADBE"), std::string("Adobe"),
		      boost::gregorian::from_undelimited_string("19860820"));
      addCommonStock (std::string("CRM"), std::string("Salesforce"),
		      boost::gregorian::from_undelimited_string("20040623"));
      addCommonStock (std::string("INTU"), std::string("Intuit"),
		      boost::gregorian::from_undelimited_string("19930312"));
      addCommonStock (std::string("POOL"), std::string("Pool"),
		      boost::gregorian::from_undelimited_string("19951012"));
      addCommonStock (std::string("ROKU"), std::string("Roku"),
		      boost::gregorian::from_undelimited_string("20170928"));
      addCommonStock (std::string("CMG"), std::string("Chipotle"),
		      boost::gregorian::from_undelimited_string("20060126"));
      addCommonStock (std::string("QCOM"), std::string("Qualcomm"),
		      boost::gregorian::from_undelimited_string("19911220"));
      addCommonStock (std::string("ADI"), std::string("Analog Devices"),
		      boost::gregorian::from_undelimited_string("19800107"));
      addCommonStock (std::string("BLK"), std::string("Blackrock"),
		      boost::gregorian::from_undelimited_string("19991001"));
      addCommonStock (std::string("DPZ"), std::string("Dominos Pizza"),
		      boost::gregorian::from_undelimited_string("20040713"));
      addCommonStock (std::string("BMY"), std::string("Bristol Myers"),
		      boost::gregorian::from_undelimited_string("20000103"));
      addCommonStock (std::string("PANW"), std::string("Palo Alto Networks"),
		      boost::gregorian::from_undelimited_string("20120720"));
      addCommonStock (std::string("DIS"), std::string("Walt Disney"),
		      boost::gregorian::from_undelimited_string("19900108"));
      addCommonStock (std::string("SWKS"), std::string("Skyworks"),
		      boost::gregorian::from_undelimited_string("20030912"));
      addCommonStock (std::string("DE"), std::string("John Deere"),
		      boost::gregorian::from_undelimited_string("19780113"));


      addCommonStock (std::string("FCX"), std::string("Freeport-McMoRan"),
		      boost::gregorian::from_undelimited_string("19950901"));
      addCommonStock (std::string("GS"), std::string("Goldman Sachs"),
		      boost::gregorian::from_undelimited_string("19990504"));
      addCommonStock (std::string("JPM"), std::string("JP Morgan"),
		      boost::gregorian::from_undelimited_string("20000103"));
      addCommonStock (std::string("UPS"), std::string("United Parcel Service"),
		      boost::gregorian::from_undelimited_string("19991110"));
      addCommonStock (std::string("FDX"), std::string("Federall Express"),
		      boost::gregorian::from_undelimited_string("19780412"));
      addCommonStock (std::string("DKNG"), std::string("Draft Kings"),
		      boost::gregorian::from_undelimited_string("20201007"));
      addCommonStock (std::string("LEN"), std::string("Lennar"),
		      boost::gregorian::from_undelimited_string("19720103"));
      addCommonStock (std::string("DHI"), std::string("D.R. Horton"),
		      boost::gregorian::from_undelimited_string("19920701"));
      addCommonStock (std::string("ORLY"), std::string("O'Reilly Automotive"),
		      boost::gregorian::from_undelimited_string("19930422"));
      addCommonStock (std::string("AVGO"), std::string("Avago Technologies"),
		      boost::gregorian::from_undelimited_string("19880417"));
      addCommonStock (std::string("ASML"), std::string("ASML Holding N.V."),
		      boost::gregorian::from_undelimited_string("19950301"));

      addCommonStock (std::string("CSCO"), std::string("Cisco"),
		      boost::gregorian::from_undelimited_string("19900216"));
      addCommonStock (std::string("MRVL"), std::string("Marvell"),
		      boost::gregorian::from_undelimited_string("20000627"));
      addCommonStock (std::string("LRCX"), std::string("Lam Research"),
		      boost::gregorian::from_undelimited_string("19840511"));


      addCommonStock (std::string("PFE"), std::string("Pfizer"),
		      boost::gregorian::from_undelimited_string("20000103"));
      addCommonStock (std::string("CVX"), std::string("Chevron"),
		      boost::gregorian::from_undelimited_string("20011009"));
      addCommonStock (std::string("SNPS"), std::string("Synopsys"),
		      boost::gregorian::from_undelimited_string("19930104"));
      addCommonStock (std::string("COST"), std::string("Costco"),
		      boost::gregorian::from_undelimited_string("19851205"));

    }

    void initializeETFAttributes()
    {
      initialize2XLeveragedETFs();
      initialize3XLeveragedETFs();
      initializeSectorETFs();
      initializeInternationalETFs();
      initializeBondETFs();
      initializeCommodityETFs();
      initializeIndustryGroupETFs();
      initializeCurrencyETFs();

      addUnLeveragedETF (std::string("SPY"),
			 std::string("SPDR S&P 500 ETF"),
			 createDecimal("0.09"),
			 boost::gregorian::from_undelimited_string("19930122"));

      addUnLeveragedETF (std::string("QQQ"),
			 std::string("PowerShares QQQ ETF"),
			 createDecimal("0.20"),
			 boost::gregorian::from_undelimited_string("19990310"));

      addUnLeveragedETF (std::string("DIA"),
			 std::string("SPDR Dow Jones Industrial Average ETF"),
			 createDecimal("0.17"),
			 boost::gregorian::from_undelimited_string("19980114"));

      addUnLeveragedETF (std::string("IWM"),
			 std::string("iShares Russell 2000 ETF"),
			 createDecimal("0.17"),
			 boost::gregorian::from_undelimited_string("20000522"));
    }

    void initializeCurrencyETFs ()
    {
      addUnLeveragedETF (std::string("UUP"),
			 std::string(" Invesco DB US Dollar Index Bullish Fund"),
			 createDecimal("0.75"),
			 boost::gregorian::from_undelimited_string("20070220"));

      addUnLeveragedETF (std::string("UDN"),
			 std::string("Invesco DB US Dollar Index Bearish Fund "),
			 createDecimal("0.75"),
			 boost::gregorian::from_undelimited_string("20070220"));

      addUnLeveragedETF (std::string("FXE"),
			 std::string("Invesco CurrencyShares® Euro Currency Trust "),
			 createDecimal("0.40"),
			 boost::gregorian::from_undelimited_string("20051209"));

      addUnLeveragedETF (std::string("FXY"),
			 std::string("Invesco CurrencyShares® Japanese Yen Currency Trust "),
			 createDecimal("0.40"),
			 boost::gregorian::from_undelimited_string("20070212"));

      addUnLeveragedETF (std::string("FXB"),
			 std::string("Invesco CurrencyShares® British Pound Currency Trust "),
			 createDecimal("0.40"),
			 boost::gregorian::from_undelimited_string("20060621"));

      addLeveragedETF (std::string("EUO"),
		       std::string("ProShares UltraShort Euro"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20081125"));

      addLeveragedETF (std::string("YCS"),
		       std::string("ProShares UltraShort Yen"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20081125"));
    }

    void initializeCommodityETFs ()
    {
      addUnLeveragedETF (std::string("GLD"),
			 std::string("SPDR Gold Trust"),
			 createDecimal("0.4"),
			 boost::gregorian::from_undelimited_string("20041118"));

      addUnLeveragedETF (std::string("SLV"),
			 std::string("iShares Silver Trust"),
			 createDecimal("0.5"),
			 boost::gregorian::from_undelimited_string("20060428"));

      addUnLeveragedETF (std::string("PPLT"),
			 std::string("Aberdeen Standard Platinum Shares ETF "),
			 createDecimal("0.6"),
			 boost::gregorian::from_undelimited_string("20100106"));

      addUnLeveragedETF (std::string("USO"),
			 std::string("United State Oil Fund"),
			 createDecimal("0.4"),
			 boost::gregorian::from_undelimited_string("20060410"));

      addUnLeveragedETF (std::string("BNO"),
			 std::string("United States Brent Oil Fund"),
			 createDecimal("0.90"),
			 boost::gregorian::from_undelimited_string("20100602"));

      addUnLeveragedETF (std::string("UNG"),
			 std::string("United State Natural Gas Fund"),
			 createDecimal("1.33"),
			 boost::gregorian::from_undelimited_string("20070418"));

      addUnLeveragedETF (std::string("UGA"),
			 std::string("United State Gasoline Fund"),
			 createDecimal("0.9"),
			 boost::gregorian::from_undelimited_string("20080226"));

      addUnLeveragedETF (std::string("DBA"),
			 std::string("Invesco DB Agriculture Fund"),
			 createDecimal("0.85"),
			 boost::gregorian::from_undelimited_string("20070105"));

      addUnLeveragedETF (std::string("WEAT"),
			 std::string("Teucrium Wheat Fund"),
			 createDecimal("1.0"),
			 boost::gregorian::from_undelimited_string("20110919"));

      addUnLeveragedETF (std::string("CANE"),
			 std::string("Teucrium Sugar Fund"),
			 createDecimal("1.14"),
			 boost::gregorian::from_undelimited_string("20110919"));

      addUnLeveragedETF (std::string("CORN"),
			 std::string("Teucrium Corn Fund"),
			 createDecimal("1.11"),
			 boost::gregorian::from_undelimited_string("20100609"));

      addUnLeveragedETF (std::string("SOYB"),
			 std::string("Teucrium Soybeans"),
			 createDecimal("1.15"),
			 boost::gregorian::from_undelimited_string("20110919"));

      addUnLeveragedETF (std::string("CPER"),
			 std::string("United States Copper Index Fund "),
			 createDecimal("0.76"),
			 boost::gregorian::from_undelimited_string("20111115"));
    }

    void initializeBondETFs ()
    {
      addUnLeveragedETF (std::string("AGG"),
			 std::string("iShares Core U.S. Aggregate Bond ETF"),
			 createDecimal("0.03"),
			 boost::gregorian::from_undelimited_string("20030922"));

      addUnLeveragedETF (std::string("BWX"),
			 std::string("SPDR Bloomberg International Treasury Bond ETF"),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20071002"));

      addUnLeveragedETF (std::string("EDV"),
			 std::string("Vanguard Extended Duration Treasury ETF"),
			 createDecimal("0.06"),
			 boost::gregorian::from_undelimited_string("20071206"));

      addUnLeveragedETF (std::string("IEI"),
			 std::string("iShares 3-7 Year Treasury Bond ETF"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20070105"));

      addUnLeveragedETF (std::string("IEF"),
			 std::string("iShares 7-10 Year Treasury Bond ETF"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20020722"));

      addUnLeveragedETF (std::string("SHY"),
			 std::string("iShares 1-3 Year Treasury Bond ETF"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20020722"));

      addUnLeveragedETF (std::string("TLH"),
			 std::string("iShares 10-20 Year Treasury Bond ETF"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20070105"));

      addUnLeveragedETF (std::string("TLT"),
			 std::string("iShares 20+ Year Treasury Bond ETF"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20020722"));

      addUnLeveragedETF (std::string("ZROZ"),
			 std::string("PIMCO 25+ Year Zero Coupon US Treasury Index ETF"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20091030"));

      addUnLeveragedETF (std::string("BND"),
			 std::string("Vanguard Total Bond Market ETF"),
			 createDecimal("0.03"),
			 boost::gregorian::from_undelimited_string("20070403"));

      addUnLeveragedETF (std::string("LQD"),
			 std::string("iShares US Corporate Bond"),
			 createDecimal("0.15"),
			 boost::gregorian::from_undelimited_string("20020722"));

      addUnLeveragedETF (std::string("HYG"),
			 std::string("iShares US High Yield Bond"),
			 createDecimal("0.49"),
			 boost::gregorian::from_undelimited_string("20070404"));

      addUnLeveragedETF (std::string("JNK"),
			 std::string("SPDR Bloomberg High Yield Bond ETF "),
			 createDecimal("0.40"),
			 boost::gregorian::from_undelimited_string("20071128"));

      addUnLeveragedETF (std::string("EMB"),
			 std::string("iShares J.P. Morgan USD Emerging Markets Bond ETF"),
			 createDecimal("0.39"),
			 boost::gregorian::from_undelimited_string("20071217"));

      addUnLeveragedETF (std::string("MBB"),
			 std::string("iShares MBS Bond ETF"),
			 createDecimal("0.06"),
			 boost::gregorian::from_undelimited_string("20070316"));

      addUnLeveragedETF (std::string("BKLN"),
			 std::string("Invesco Senior Loan ETF "),
			 createDecimal("0.65"),
			 boost::gregorian::from_undelimited_string("20110303"));

      addLeveragedETF (std::string("SJB"),
		       std::string("ProShares Short High Yield"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusOne,
		       boost::gregorian::from_undelimited_string("20110321"));


      addLeveragedETF (std::string("TBT"),
		       std::string("ProShares UltraShort 20+ Year Treasury"),
		       createDecimal("0.89"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20080501"));

      addLeveragedETF (std::string("PST"),
		       std::string("ProShares UltraShort 7-10 Year Treasury"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20080501"));

      addLeveragedETF (std::string("TMF"),
		       std::string("Direxion Daily 20-Year Treasury Bull 3X"),
		       createDecimal("1.09"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20090416"));

      addLeveragedETF (std::string("TMV"),
		       std::string("Direxion Daily 20-Year Treasury Bear 3X"),
		       createDecimal("1.02"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20090416"));

      addLeveragedETF (std::string("TTT"),
		       std::string("ProShares UltraPro Short 20+ Year Treasury"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20120327"));

    }

    void initializeInternationalETFs ()
    {
      addUnLeveragedETF (std::string("FXI"),
			 std::string("iShares China Large-Cap ETF"),
			 createDecimal("0.4"),
			 boost::gregorian::from_undelimited_string("20041005"));

      addUnLeveragedETF (std::string("KWEB"),
			 std::string("KraneShares CSI China Internet ETF"),
			 createDecimal("0.69"),
			 boost::gregorian::from_undelimited_string("20130731"));

      addUnLeveragedETF (std::string("EWJ"),
			 std::string("iShares MSCI Japan ETF"),
			 createDecimal("0.48"),
			 boost::gregorian::from_undelimited_string("19960312"));

      addUnLeveragedETF (std::string("EWZ"),
			 std::string("iShares MSCI Brazil ETF"),
			 createDecimal("0.59"),
			 boost::gregorian::from_undelimited_string("20000710"));

      addUnLeveragedETF (std::string("EWH"),
			 std::string("iShares MSCI Hong Kong ETF"),
			 createDecimal("0.51"),
			 boost::gregorian::from_undelimited_string("19960312"));

      addUnLeveragedETF (std::string("EWG"),
			 std::string("iShares MSCI German ETF"),
			 createDecimal("0.51"),
			 boost::gregorian::from_undelimited_string("19960312"));

      addUnLeveragedETF (std::string("EWA"),
			 std::string("iShares MSCI Australia ETF"),
			 createDecimal("0.51"),
			 boost::gregorian::from_undelimited_string("19960318"));

      addUnLeveragedETF (std::string("EWT"),
			 std::string("iShares MSCI Taiwan ETF"),
			 createDecimal("0.59"),
			 boost::gregorian::from_undelimited_string("20000620"));

      addUnLeveragedETF (std::string("EWS"),
			 std::string("iShares MSCI Singapore ETF"),
			 createDecimal("0.51"),
			 boost::gregorian::from_undelimited_string("19960312"));

      addUnLeveragedETF (std::string("EEM"),
			 std::string("iShares MSCI Emerging Markets ETF"),
			 createDecimal("0.67"),
			 boost::gregorian::from_undelimited_string("20030407"));

      addUnLeveragedETF (std::string("RSX"),
			 std::string("VanEck Vectors Russia ETF"),
			 createDecimal("0.67"),
			 boost::gregorian::from_undelimited_string("20070430"));

    }

    void initializeIndustryGroupETFs ()
    {
      addUnLeveragedETF (std::string("KRE"),
			 std::string("S&P Regional Banking ETF"),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("20060619"));

      addUnLeveragedETF (std::string("XHB"),
			 std::string("SPDR S&P Homebuilders ETF"),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20060206"));

      addUnLeveragedETF (std::string("VNQ"),
			 std::string("Vanguard Real Estate Index Fund"),
			 createDecimal("0.12"),
			 boost::gregorian::from_undelimited_string("20040923"));

      addUnLeveragedETF (std::string("SMH"),
			 std::string("VanEck Vectors Semiconductor ETF "),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20111220"));

      addUnLeveragedETF (std::string("GDX"),
			 std::string("Van Eck Gold Miners ETF"),
			 createDecimal("0.52"),
			 boost::gregorian::from_undelimited_string("20060522"));

      addUnLeveragedETF (std::string("GDXJ"),
			 std::string("Van Eck Junior Gold Miners ETF"),
			 createDecimal("0.54"),
			 boost::gregorian::from_undelimited_string("20091110"));

      addUnLeveragedETF (std::string("IBB"),
			 std::string("iShares Nasdaq Biotechnology ETF"),
			 createDecimal("0.48"),
			 boost::gregorian::from_undelimited_string("20010205"));

      addUnLeveragedETF (std::string("IYT"),
			 std::string("iShares U.S. Transportation ETF"),
			 createDecimal("0.48"),
			 boost::gregorian::from_undelimited_string("20031006"));

      addUnLeveragedETF (std::string("XBI"),
			 std::string("SPDR S&P Biotech ETF"),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20060206"));

      addUnLeveragedETF (std::string("OIH"),
			 std::string("VanEck Oil Services ETF"),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20111220"));

      addUnLeveragedETF (std::string("TAN"),
			 std::string("Invesco Solar ETF"),
			 createDecimal("0.69"),
			 boost::gregorian::from_undelimited_string("20080415"));

      addUnLeveragedETF (std::string("XOP"),
			 std::string("SPDR S&P Oil & Gas Exploration & Production ETF"),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20060619"));

      addLeveragedETF (std::string("REK"),
		       std::string("ProShares Short Real Estate"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusOne,
		       boost::gregorian::from_undelimited_string("20100318"));
    }

    void initializeSectorETFs ()
    {
      addUnLeveragedETF (std::string("XLE"),
			 std::string("Energy Select Sector SPDR Fund"),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLF"),
			 std::string("Financial Select Sector SPDR Fund "),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLB"),
			 std::string("Materials Select Sector SPDR ETF "),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLK"),
			 std::string("Technology Select Sector SPDR Fund "),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLI"),
			 std::string("Industrial Select Sector SPDR Fund"),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLV"),
			 std::string("Health Care Select Sector SPDR Fund"),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLU"),
			 std::string("Utilities Select Sector SPDR Fund "),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLP"),
			 std::string("Consumer Staples Select Sector SPDR Fund "),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XLY"),
			 std::string("Consumer Discretionary Select Sector SPDR Fund"),
			 createDecimal("0.13"),
			 boost::gregorian::from_undelimited_string("19981216"));

      addUnLeveragedETF (std::string("XRT"),
			 std::string("SPDR S&P Retail ETF"),
			 createDecimal("0.35"),
			 boost::gregorian::from_undelimited_string("20060619"));
    }

    void initialize1XLeveragedETFs()
    {

    }

    void initialize2XLeveragedETFs()
    {
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

      addLeveragedETF (std::string("ERY"),
		       std::string("Direxion Daily Energy Bear 2X Shares"),
		       createDecimal("0.99"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20081106"));

      addLeveragedETF (std::string("DDM"),
		       std::string("ProShares Ultra Dow30"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20060619"));

      addLeveragedETF (std::string("DXD"),
		       std::string("ProShares UltraShort Dow30"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20060711"));

      addLeveragedETF (std::string("UWM"),
		       std::string("ProShares Ultra Russell2000"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20060619"));

      addLeveragedETF (std::string("TWM"),
		       std::string("ProShares UltraShort Russell2000"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20060711"));

      addLeveragedETF (std::string("SCO"),
		       std::string("ProShares UltraShort Bloomberg Crude Oil"),
		       createDecimal("1.25"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20081124"));

      addLeveragedETF (std::string("UCO"),
		       std::string("ProShares Ultra Bloomberg Crude Oil"),
		       createDecimal("1.27"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20081125"));

      addLeveragedETF (std::string("BOIL"),
		       std::string("ProShares Ultra Bloomberg Natural Gas"),
		       createDecimal("1.31"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20111004"));

      addLeveragedETF (std::string("KOLD"),
		       std::string("ProShares UltraShort Bloomberg Natural Gas "),
		       createDecimal("1.34"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20111004"));

      addLeveragedETF (std::string("ZSL"),
		       std::string("ProShares UltraShort Silver"),
		       createDecimal("1.39"),
		       DecimalConstants<Decimal>::DecimalMinusTwo,
		       boost::gregorian::from_undelimited_string("20081201"));

      addLeveragedETF (std::string("AGQ"),
		       std::string("ProShares Ultra Silver"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalTwo,
		       boost::gregorian::from_undelimited_string("20081201"));

    }
    
    void initialize3XLeveragedETFs()
    {
      addLeveragedETF (std::string("TNA"),
		       std::string("Direxion Daily Small Cap Bull 3x Shares"),
		       createDecimal("1.14"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20081105"));

      addLeveragedETF (std::string("TZA"),
		       std::string("Direxion Daily Small Cap Bear 3X Shares"),
		       createDecimal("1.11"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20081105"));

      addLeveragedETF (std::string("URTY"),
		       std::string("ProShares UltraPro Russell 2000"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20100209"));

      addLeveragedETF (std::string("SRTY"),
		       std::string("ProShares UltraPro Short Russell 2000 "),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20100209"));

     addLeveragedETF (std::string("UDOW"),
		       std::string("ProShares UltraPro Dow30"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20100211"));

     addLeveragedETF (std::string("SDOW"),
		       std::string("ProShares UltraPro Short Dow30"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20100211"));


      addLeveragedETF (std::string("NUGT"),
		       std::string("Direxion Daily Gold Miners Index Bull 3X Shares"),
		       createDecimal("1.23"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20101208"));

      addLeveragedETF (std::string("DUST"),
		       std::string("Direxion Daily Gold Miners Index Bear 3x Shares"),
		       createDecimal("1.05"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20101208"));

      addLeveragedETF (std::string("SPXL"),
		       std::string("Direxion Daily S&P 500 Bull 3X Shares"),
		       createDecimal("1.02"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20081105"));

      addLeveragedETF (std::string("SPXS"),
		       std::string("Direxion Daily S&P 500 Bear 3X Shares"),
		       createDecimal("1.08"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20081105"));

      addLeveragedETF (std::string("SOXL"),
		       std::string("Direxion Daily Semiconductor Bull 3X Shares"),
		       createDecimal("0.99"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20100311"));

      addLeveragedETF (std::string("SOXS"),
		       std::string("Direxion Daily Semiconductor Bear 3X Shares"),
		       createDecimal("1.08"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20100311"));

      addLeveragedETF (std::string("TQQQ"),
		       std::string("ProShares UltraPro QQQ"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20100209"));

      addLeveragedETF (std::string("SQQQ"),
		       std::string("ProShares UltraPro Short QQQ"),
		       createDecimal("0.95"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20100209"));

      addLeveragedETF (std::string("LABD"),
		       std::string("Direxion Daily S&P Biotech Bear 3X Shares"),
		       createDecimal("1.11"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20150528"));

      addLeveragedETF (std::string("LABU"),
		       std::string("Direxion Daily S&P Biotech Bull 3X Shares"),
		       createDecimal("1.12"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20150528"));

      addLeveragedETF (std::string("FAS"),
		       std::string("Direxion Daily Financial Bull 3x Shares"),
		       createDecimal("1.00"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20081106"));

      addLeveragedETF (std::string("FAZ"),
		       std::string("Direxion Daily Financial Bear 3X Shares"),
		       createDecimal("1.07"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20081106"));

      addLeveragedETF (std::string("YINN"),
		       std::string("Direxion Daily FTSE China Bull 3X Shares"),
		       createDecimal("1.52"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20091203"));

      addLeveragedETF (std::string("YANG"),
		       std::string("Direxion Daily FTSE China Bear 3X Shares"),
		       createDecimal("1.08"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20091203"));

      addLeveragedETF (std::string("GASL"),
		       std::string("Direxion Daily Natural Gas Related Bull 3X Shares"),
		       createDecimal("1.04"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20100714"));

      addLeveragedETF (std::string("GUSH"),
		       std::string("Direxion Daily S&P Oil & Gas Exp. & Prod. Bull 3X Shares"),
		       createDecimal("1.17"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20150528"));

      addLeveragedETF (std::string("BRZU"),
		       std::string("Direxion Daily Brazil Bull 3X Shares"),
		       createDecimal("1.36"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20130410"));

      addLeveragedETF (std::string("ERX"),
		       std::string("Direxion Daily Energy Bull 3X Shares"),
		       createDecimal("1.09"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20081106"));

      addLeveragedETF (std::string("ERY"),
		       std::string("Direxion Daily Energy Bear 3X Shares"),
		       createDecimal("1.09"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20081106"));

      addLeveragedETF (std::string("TECL"),
		       std::string("Direxion Daily Technology Bull 3X Shares"),
		       createDecimal("1.08"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20081217"));

      addLeveragedETF (std::string("TECS"),
		       std::string("Direxion Daily Technology Bear 3X Shares"),
		       createDecimal("1.10"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20081217"));

      addLeveragedETF (std::string("UWTI"),
		       std::string("VelocityShares 3x Long Crude ETN"),
		       createDecimal("1.35"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20120207"));

      addLeveragedETF (std::string("DWTI"),
		       std::string("VelocityShares 3x Inverse Crude ETN"),
		       createDecimal("1.35"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20120207"));

      addLeveragedETF (std::string("UGAZ"),
		       std::string("VelocityShares 3x Long Natural Gas"),
		       createDecimal("1.65"),
		       DecimalConstants<Decimal>::DecimalThree,
		       boost::gregorian::from_undelimited_string("20120207"));

      addLeveragedETF (std::string("DGAZ"),
		       std::string("VelocityShares 3x Inverse Natural Gas"),
		       createDecimal("1.65"),
		       DecimalConstants<Decimal>::DecimalMinusThree,
		       boost::gregorian::from_undelimited_string("20120207"));

    }

    void initializeStockIndexFuturesAttributes()
    {
      std::string miniNasdaq100FuturesSymbol("@NQ");
      std::string miniSP500FuturesSymbol("@ES");
      std::string miniRussell2000FuturesSymbol("@TF");
      std::string miniDowFuturesSymbol("@YM");

      addFuturesAttributes (miniNasdaq100FuturesSymbol,
			    "Emini Nasdaq 100 Futures",
			    createDecimal("20.0"),
			    createDecimal("0.25"));

      addFuturesAttributes (miniSP500FuturesSymbol,
			     "Emini S&P 500 Futures",
			     createDecimal("50.0"),
			     createDecimal("0.25"));

      addFuturesAttributes (miniRussell2000FuturesSymbol,
			    "Russell 2000 Futures",
			    createDecimal("100.0"),
			    createDecimal("0.10"));

      addFuturesAttributes (miniDowFuturesSymbol,
			     "Mini Dow Futures",
			     createDecimal("5.0"),
			     createDecimal("1.0"));
    }

    void initializeSoftsFuturesAttributes()
    {
      std::string cottonFuturesSymbol("@CT");
      std::string milkFuturesSymbol("@DA");
      std::string coffeeFuturesSymbol("@KC");
      std::string sugarFuturesSymbol("@SB");
      std::string cocoaFuturesSymbol("@CC");

      addFuturesAttributes (cottonFuturesSymbol,"Cotton Futures",
			    createDecimal("500.0"),
			    createDecimal("0.01"));

      addFuturesAttributes (milkFuturesSymbol, "Milk Futures",
			    createDecimal("2000.0"),
			    createDecimal("0.01"));

      addFuturesAttributes (coffeeFuturesSymbol, "Coffee Futures",
			    createDecimal("375.0"),
			    createDecimal("0.05"));

      addFuturesAttributes (sugarFuturesSymbol, "Sugar Futures",
			    createDecimal("1120.0"),
			    createDecimal("0.01"));

      addFuturesAttributes (cocoaFuturesSymbol, "Cocoa Futures",
			    createDecimal("10.0"),
			    createDecimal("1.0"));
    }

    void initializeBondFuturesAttributes()
    {
      std::string tenYearFuturesSymbol("@TY");
      std::string thirtyYearFuturesSymbol("@US");
      std::string fiveYearFuturesSymbol("@FV");

      addFuturesAttributes (fiveYearFuturesSymbol,"5-Year Note Futures",
			    createDecimal("1000.0"),
			    createDecimal("0.0078125"));

     addFuturesAttributes (tenYearFuturesSymbol,"10-Year Note Futures",
			   createDecimal("1000.0"),
			   createDecimal("0.015625"));

     addFuturesAttributes (thirtyYearFuturesSymbol, "30-Year Note Futures",
			   createDecimal("1000.0"),
			   createDecimal("0.03125"));
    }

    void initializeCurrencyFuturesAttributes()
    {

      addFuturesAttributes (std::string("@DX"), std::string("Dollar Index Futures"),
			    createDecimal("1000.00"), createDecimal("0.005"));

      addFuturesAttributes (std::string("@JY"), std::string("Japanese Yen Futures"),
			    createDecimal("125000.00"), createDecimal("0.0001"));

      addFuturesAttributes (std::string("@EC"), std::string("Euro FX"),
			    createDecimal("125000.00"), createDecimal("0.00005"));

      addFuturesAttributes (std::string("@BP"), std::string("British Pound Futures"),
			    createDecimal("62500.00"), createDecimal("0.0001"));

      addFuturesAttributes (std::string("@SF"), std::string("Swiss Franc"),
			    createDecimal("125000.00"), createDecimal("0.0001"));

      addFuturesAttributes (std::string("@AD"), std::string("Australian Dollar"),
			    createDecimal("100000.00"), createDecimal("0.0001"));

    }

    
    void initializeGrainFuturesAttributes()
    {
      std::string cornFuturesSymbol("@C");
      std::string soyBeanMealFuturesSymbol("@SM");

      addFuturesAttributes (cornFuturesSymbol,
			    "Corn Futures",
			    createDecimal("50.0"),
			    createDecimal("0.25"));

      addFuturesAttributes (std::string("@S"), std::string("Soybean Futures"),
			    createDecimal("50.00"), createDecimal("0.25"));

      addFuturesAttributes (std::string("@W"), std::string("Wheat"),
			    createDecimal("50.00"), createDecimal("0.25"));

      addFuturesAttributes (soyBeanMealFuturesSymbol,
			    "SoyBean Meal Futures",
			    createDecimal("100.0"),
			    createDecimal("0.1"));
    }

    void initializeMeatFuturesAttributes()
    {
      std::string feederCattleFuturesSymbol("@FC");

      addFuturesAttributes (feederCattleFuturesSymbol, "Feeder Cattle Futures",
			    createDecimal("500.0"),
			    createDecimal("0.025"));
    }

	
    void initializeMetalsFuturesAttributes()
    {
      std::string goldFuturesSymbol("@GC");
      std::string copperFuturesSymbol("@HG");
      std::string platinumFuturesSymbol("@PL");

      addFuturesAttributes (goldFuturesSymbol, "Gold Futures",
			    createDecimal("100.0"), createDecimal("0.10"));

      addFuturesAttributes (copperFuturesSymbol, "Copper Futures",
			    createDecimal("25000.0"), createDecimal("0.0005"));

      addFuturesAttributes (std::string("@PL"), "Platinum Futures",
			    createDecimal("50.0"), createDecimal("0.10"));

      addFuturesAttributes (std::string("@SI"), std::string("Silver"),
			    createDecimal("5000.00"), createDecimal("0.005"));

    }

    void initializeEnergyFuturesAttributes()
    {
      addFuturesAttributes (std::string("@CL"), std::string("Crude Oil Futures"),
			    createDecimal("1000.0"),
			    createDecimal("0.01"));

      addFuturesAttributes (std::string("@NG"), std::string("Natural Gas Futures"),
			    createDecimal("10000.00"), createDecimal("0.001"));

      addFuturesAttributes (std::string("@RB"), std::string("RBOB Gasoline"),
			    createDecimal("42000.00"), createDecimal("0.0001"));
      
    }

 private:
    SecurityAttributesFactory ()
    {
      initializeEquityAttributes();
      initializeFuturesAttributes();
    }

    ~SecurityAttributesFactory ()
    {}

    // disable copy/move so there’s truly only one
    SecurityAttributesFactory(const SecurityAttributesFactory&) = delete;
    SecurityAttributesFactory& operator=(const SecurityAttributesFactory&) = delete;

    
    Decimal createDecimal(const std::string& valueString)
    {
      return num::fromString<Decimal>(valueString);
    }

    void addFuturesAttributes(const std::string& symbol, 
			      const std::string& futuresName,
			      const Decimal& bigPointValue,
			      const Decimal& tickValue)
    {
      // Create a fake/temporary inception date for all futures symbols
      auto inceptionDate = boost::gregorian::from_undelimited_string("20000103");
      auto attributes =
	std::make_shared<FuturesSecurityAttributes<Decimal>>(symbol, futuresName,
							     bigPointValue, tickValue,
							     inceptionDate);
      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addFuturesAttributes(const std::string& symbol,
			      const std::string& futuresName,
			      const Decimal& bigPointValue,
			      const Decimal& tickValue,
			      const boost::gregorian::date& inceptionDate)
    {
      auto attributes = 
	std::make_shared<FuturesSecurityAttributes<Decimal>>(symbol, futuresName,
							     bigPointValue, tickValue,
							     inceptionDate);
      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addUnLeveragedETF (const std::string& symbol, const std::string& etfName, 
			    const Decimal& expenseRatio, 
			    const boost::gregorian::date& inceptionDate)
    {
      LeverageAttributes<Decimal> noLeverage(DecimalConstants<Decimal>::DecimalZero);

      FundAttributes<Decimal> etfAttributes(expenseRatio, noLeverage);
      auto attributes = 
	std::make_shared<ETFSecurityAttributes<Decimal>>(symbol, 
							 etfName,
							 etfAttributes,
							 inceptionDate);

      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addLeveragedETF (const std::string& symbol, const std::string& etfName, 
			  const Decimal& expenseRatio,
			  const Decimal& leverage,
			  const boost::gregorian::date& inceptionDate)
    {
      LeverageAttributes<Decimal> leverageForFund(leverage);

      FundAttributes<Decimal> etfAttributes(expenseRatio, leverageForFund);
      auto attributes = 
	std::make_shared<ETFSecurityAttributes<Decimal>>(symbol, 
							 etfName,
							 etfAttributes,
							 inceptionDate);

      mSecurityAttributes.insert(std::make_pair(symbol,
						attributes));
    }

    void addCommonStock (const std::string& symbol, const std::string& stockName,
			 const boost::gregorian::date& inceptionDate)
    {
      auto attributes = 
	std::make_shared<CommonStockSecurityAttributes<Decimal>>(symbol, stockName, inceptionDate);

      mSecurityAttributes.insert(std::make_pair(symbol, attributes));
    }

  private:
    std::map<std::string, std::shared_ptr<SecurityAttributes<Decimal>>> mSecurityAttributes;
  };

  template <class Decimal>
  std::shared_ptr<SecurityAttributes<Decimal>> getSecurityAttributes (const std::string &symbol)
  {
    auto& factory = SecurityAttributesFactory<Decimal>::instance();
    
    auto it = factory.getSecurityAttributes (symbol);

    if (it != factory.endSecurityAttributes())
      return it->second;
    else
      throw SecurtyAttributesFactoryException("getSecurityAttributes - ticker symbol " +symbol +" is unkown");
  }
}
#endif
