#include <catch2/catch_test_macros.hpp>
#include <set>
#include "Portfolio.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;

TEST_CASE ("Portfolio operations", "[Portfolio]")
{
  auto entry0 = createEquityEntry ("20160106", "198.34", "200.06", "197.60","198.82",
				   142662900);

  auto entry1 = createEquityEntry ("20160105", "201.40", "201.90", "200.05","201.36",
				   105999900);

  auto entry2 = createEquityEntry ("20160104", "200.49", "201.03", "198.59","201.02",
				   222353400);

  auto entry3 = createEquityEntry ("20151231", "205.13", "205.89", "203.87","203.87",
				   114877900);

  auto entry4 = createEquityEntry ("20151230", "207.11", "207.21", "205.76","205.93",
				   63317700);

  auto entry5 = createEquityEntry ("20151229", "206.51", "207.79", "206.47","207.40",
				   92640700);

  auto entry6 = createEquityEntry ("20151228", "204.86", "205.26", "203.94","205.21",
				   65899900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);

  spySeries->addEntry (*entry4);
  spySeries->addEntry (*entry6);
  spySeries->addEntry (*entry2);
  spySeries->addEntry (*entry3);
  spySeries->addEntry (*entry1);
  spySeries->addEntry (*entry5);
  spySeries->addEntry (*entry0);

  std::string equitySymbol("SPY");
  std::string equityName("SPDR S&P 500 ETF");

  EquitySecurity<DecimalType> spy (equitySymbol, equityName, spySeries);



  // Futures security

  std::string futuresSymbol("C2");
  std::string futuresName("Corn futures");
  DecimalType cornBigPointValue(createDecimal("50.0"));
  DecimalType cornTickValue(createDecimal("0.25"));

  auto futuresEntry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068",0);

  auto futuresEntry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", 0);

  auto futuresEntry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125",0);

  auto futuresEntry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563",0);

  auto futuresEntry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", 0);

  auto futuresEntry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", 0);

  auto futuresEntry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", 0);
  auto futuresEntry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", 0);
  auto futuresEntry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", 0);
  auto futuresEntry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", 0);
  auto futuresEntry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", 0);
  auto futuresEntry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875",0);

  auto cornSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  cornSeries->addEntry(*futuresEntry0);
  cornSeries->addEntry(*futuresEntry1);
  cornSeries->addEntry(*futuresEntry2);
  cornSeries->addEntry(*futuresEntry3);
  cornSeries->addEntry(*futuresEntry4);
  cornSeries->addEntry(*futuresEntry5);
  cornSeries->addEntry(*futuresEntry6);
  cornSeries->addEntry(*futuresEntry7);
  cornSeries->addEntry(*futuresEntry8);
  cornSeries->addEntry(*futuresEntry9);
  cornSeries->addEntry(*futuresEntry10);
  cornSeries->addEntry(*futuresEntry11);

  FuturesSecurity<DecimalType> corn (futuresSymbol, futuresName, cornBigPointValue,
			   cornTickValue, cornSeries);

  std::string portName("Test Portfolio");

  Portfolio<DecimalType> aPortfolio(portName);
  auto cornPtr = std::make_shared<FuturesSecurity<DecimalType>>(futuresSymbol,
						     futuresName,
						     cornBigPointValue,
						     cornTickValue, cornSeries);
  auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>(equitySymbol, equityName, spySeries);

  aPortfolio.addSecurity(cornPtr);
  aPortfolio.addSecurity(spyPtr);

  REQUIRE (aPortfolio.getNumSecurities() == 2);
  REQUIRE (aPortfolio.getPortfolioName() == portName);
  Portfolio<DecimalType>::ConstPortfolioIterator it = aPortfolio.beginPortfolio();
  REQUIRE (it->second->getSymbol() == futuresSymbol);
  it++;
  REQUIRE (it->second->getSymbol() == equitySymbol);


  SECTION ("Portfolio find test", "[Portfolio find]")
    {
      Portfolio<DecimalType>::ConstPortfolioIterator it = aPortfolio.findSecurity(equitySymbol);
      REQUIRE (it != aPortfolio.endPortfolio());
      REQUIRE (it->second->getSymbol() == equitySymbol);

      it = aPortfolio.findSecurity(futuresSymbol);
      REQUIRE (it != aPortfolio.endPortfolio());
      REQUIRE (it->second->getSymbol() == futuresSymbol);
    }
}

TEST_CASE("Portfolio: throws on null security", "[Portfolio][exception]") {
  // Test that Portfolio properly rejects null securities at the addSecurity level
  std::string portName("Test Portfolio");
  Portfolio<DecimalType> portfolio(portName);
  
  REQUIRE_THROWS_AS(
    portfolio.addSecurity(nullptr),
    PortfolioException
  );
}

TEST_CASE("Portfolio Copy Constructor and Assignment", "[Portfolio][CopySemantics]")
{
  // Setup test data
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  SECTION("Copy Constructor - Empty Portfolio")
  {
    Portfolio<DecimalType> original("Original Portfolio");
    Portfolio<DecimalType> copy(original);
    
    REQUIRE(copy.getPortfolioName() == original.getPortfolioName());
    REQUIRE(copy.getNumSecurities() == 0);
    REQUIRE(copy.getNumSecurities() == original.getNumSecurities());
  }
  
  SECTION("Copy Constructor - Portfolio with Securities")
  {
    Portfolio<DecimalType> original("Original Portfolio");
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    original.addSecurity(spyPtr);
    original.addSecurity(aaplPtr);
    
    Portfolio<DecimalType> copy(original);
    
    // Verify portfolio name and size
    REQUIRE(copy.getPortfolioName() == original.getPortfolioName());
    REQUIRE(copy.getNumSecurities() == 2);
    REQUIRE(copy.getNumSecurities() == original.getNumSecurities());
    
    // Verify securities are present
    auto it = copy.findSecurity("SPY");
    REQUIRE(it != copy.endPortfolio());
    REQUIRE(it->second->getSymbol() == "SPY");
    
    it = copy.findSecurity("AAPL");
    REQUIRE(it != copy.endPortfolio());
    REQUIRE(it->second->getSymbol() == "AAPL");
    
    // Verify shared_ptr semantics - same securities are shared
    auto origIt = original.findSecurity("SPY");
    auto copyIt = copy.findSecurity("SPY");
    REQUIRE(origIt->second == copyIt->second); // Same shared_ptr
  }
  
  SECTION("Copy Assignment Operator - Empty to Empty")
  {
    Portfolio<DecimalType> portfolio1("Portfolio 1");
    Portfolio<DecimalType> portfolio2("Portfolio 2");
    
    portfolio2 = portfolio1;
    
    REQUIRE(portfolio2.getPortfolioName() == "Portfolio 1");
    REQUIRE(portfolio2.getNumSecurities() == 0);
  }
  
  SECTION("Copy Assignment Operator - Non-empty to Empty")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    source.addSecurity(spyPtr);
    
    Portfolio<DecimalType> target("Target Portfolio");
    
    target = source;
    
    REQUIRE(target.getPortfolioName() == "Source Portfolio");
    REQUIRE(target.getNumSecurities() == 1);
    
    auto it = target.findSecurity("SPY");
    REQUIRE(it != target.endPortfolio());
    REQUIRE(it->second->getSymbol() == "SPY");
  }
  
  SECTION("Copy Assignment Operator - Non-empty to Non-empty")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    source.addSecurity(spyPtr);
    
    Portfolio<DecimalType> target("Target Portfolio");
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    target.addSecurity(aaplPtr);
    
    // Before assignment
    REQUIRE(target.getNumSecurities() == 1);
    REQUIRE(target.findSecurity("AAPL") != target.endPortfolio());
    
    target = source;
    
    // After assignment - target should have source's contents
    REQUIRE(target.getPortfolioName() == "Source Portfolio");
    REQUIRE(target.getNumSecurities() == 1);
    REQUIRE(target.findSecurity("SPY") != target.endPortfolio());
    REQUIRE(target.findSecurity("AAPL") == target.endPortfolio()); // Old contents gone
  }
  
  SECTION("Self-Assignment")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    portfolio = portfolio; // Self-assignment
    
    // Portfolio should still be valid
    REQUIRE(portfolio.getPortfolioName() == "Test Portfolio");
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.findSecurity("SPY") != portfolio.endPortfolio());
  }
}

TEST_CASE("Portfolio clone() Method", "[Portfolio][Clone]")
{
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry);
  
  SECTION("Clone empty portfolio")
  {
    Portfolio<DecimalType> original("Test Portfolio");
    auto cloned = original.clone();
    
    // Should have same name
    REQUIRE(cloned->getPortfolioName() == original.getPortfolioName());
    
    // Should be empty
    REQUIRE(cloned->getNumSecurities() == 0);
    
    // Should be independent object
    REQUIRE(cloned.get() != &original);
  }
  
  SECTION("Clone portfolio with securities - clone should be empty")
  {
    Portfolio<DecimalType> original("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    original.addSecurity(spyPtr);
    
    auto cloned = original.clone();
    
    // Should have same name
    REQUIRE(cloned->getPortfolioName() == original.getPortfolioName());
    
    // Clone should be empty (as per documentation - for thread-safe backtesting)
    REQUIRE(cloned->getNumSecurities() == 0);
    REQUIRE(original.getNumSecurities() == 1);
    
    // Should be independent - adding to clone doesn't affect original
    auto aaplEntry = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
    auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    aaplSeries->addEntry(*aaplEntry);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    cloned->addSecurity(aaplPtr);
    
    REQUIRE(cloned->getNumSecurities() == 1);
    REQUIRE(original.getNumSecurities() == 1);
    REQUIRE(cloned->findSecurity("AAPL") != cloned->endPortfolio());
    REQUIRE(original.findSecurity("AAPL") == original.endPortfolio());
  }
}

TEST_CASE("Portfolio removeSecurity()", "[Portfolio][Remove]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  SECTION("Remove existing security")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 2);
    
    // Remove SPY
    portfolio.removeSecurity("SPY");
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.findSecurity("SPY") == portfolio.endPortfolio());
    REQUIRE(portfolio.findSecurity("AAPL") != portfolio.endPortfolio());
  }
  
  SECTION("Remove non-existent security - should be no-op")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    
    // Remove non-existent security - should not throw
    portfolio.removeSecurity("NONEXISTENT");
    
    // Portfolio should be unchanged
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.findSecurity("SPY") != portfolio.endPortfolio());
  }
  
  SECTION("Remove from empty portfolio - should be no-op")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE(portfolio.getNumSecurities() == 0);
    
    // Should not throw
    portfolio.removeSecurity("ANYTHING");
    
    REQUIRE(portfolio.getNumSecurities() == 0);
  }
  
  SECTION("Remove all securities")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    
    portfolio.removeSecurity("SPY");
    portfolio.removeSecurity("AAPL");
    
    REQUIRE(portfolio.getNumSecurities() == 0);
    REQUIRE(portfolio.beginPortfolio() == portfolio.endPortfolio());
  }
}

TEST_CASE("Portfolio replaceSecurity()", "[Portfolio][Replace]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries1->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160107", "199.00", "201.00", "198.00", "200.50", 150000000);
  auto spySeries2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries2->addEntry(*entry2);
  
  SECTION("Replace existing security with single-argument version")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto spyPtr1 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.addSecurity(spyPtr1);
    
    // Verify original
    auto it = portfolio.findSecurity("SPY");
    REQUIRE(it != portfolio.endPortfolio());
    REQUIRE(it->second->getTimeSeries() == spySeries1);
    
    // Replace with new security
    auto spyPtr2 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries2);
    portfolio.replaceSecurity(spyPtr2);
    
    // Verify replacement
    REQUIRE(portfolio.getNumSecurities() == 1); // Still just one security
    it = portfolio.findSecurity("SPY");
    REQUIRE(it != portfolio.endPortfolio());
    REQUIRE(it->second->getTimeSeries() == spySeries2); // Different time series
  }
  
  SECTION("Replace inserts new security if not present - single-argument version")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE(portfolio.getNumSecurities() == 0);
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.replaceSecurity(spyPtr);
    
    // Should have inserted
    REQUIRE(portfolio.getNumSecurities() == 1);
    auto it = portfolio.findSecurity("SPY");
    REQUIRE(it != portfolio.endPortfolio());
  }
  
  SECTION("Replace existing security with two-argument version")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto spyPtr1 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.addSecurity(spyPtr1);
    
    auto spyPtr2 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries2);
    portfolio.replaceSecurity("SPY", spyPtr2);
    
    // Verify replacement
    REQUIRE(portfolio.getNumSecurities() == 1);
    auto it = portfolio.findSecurity("SPY");
    REQUIRE(it != portfolio.endPortfolio());
    REQUIRE(it->second->getTimeSeries() == spySeries2);
  }
  
  SECTION("Replace inserts new security if not present - two-argument version")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.replaceSecurity("SPY", spyPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.findSecurity("SPY") != portfolio.endPortfolio());
  }
  
  SECTION("Replace with null pointer should throw - single-argument version")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    // This currently CRASHES - should be fixed to throw
    // Once fixed, this test should pass
    REQUIRE_THROWS_AS(
      portfolio.replaceSecurity(nullptr),
      PortfolioException
    );
  }
  
  SECTION("Replace with null pointer should throw - two-argument version")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    // This currently CRASHES - should be fixed to throw
    // Once fixed, this test should pass
    REQUIRE_THROWS_AS(
      portfolio.replaceSecurity("SPY", nullptr),
      PortfolioException
    );
  }
  
  SECTION("Symbol mismatch in two-argument version")
  {
    // This is a potential bug - the two-argument version allows
    // storing a security under a different symbol than it actually has
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    
    // Store SPY security under "WRONG" key
    portfolio.replaceSecurity("WRONG", spyPtr);
    
    // This creates inconsistent state
    auto it = portfolio.findSecurity("WRONG");
    REQUIRE(it != portfolio.endPortfolio());
    REQUIRE(it->second->getSymbol() == "SPY"); // Symbol doesn't match key!
    
    // Cannot find by actual symbol
    REQUIRE(portfolio.findSecurity("SPY") == portfolio.endPortfolio());
  }
}

TEST_CASE("Portfolio addSecurity() Exception Handling", "[Portfolio][Exceptions]")
{
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry);
  
  SECTION("Adding duplicate security throws exception")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto spyPtr1 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr1);
    
    auto spyPtr2 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    REQUIRE_THROWS_AS(
      portfolio.addSecurity(spyPtr2),
      PortfolioException
    );
    
    // Verify error message
    try {
      portfolio.addSecurity(spyPtr2);
      FAIL("Expected PortfolioException");
    } catch (const PortfolioException& e) {
      std::string msg(e.what());
      REQUIRE(msg.find("SPY") != std::string::npos);
      REQUIRE(msg.find("already exists") != std::string::npos);
    }
    
    // Portfolio should still have just one security
    REQUIRE(portfolio.getNumSecurities() == 1);
  }
  
  SECTION("Null security pointer throws exception")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE_THROWS_AS(
      portfolio.addSecurity(nullptr),
      PortfolioException
    );
    
    // Verify error message
    try {
      portfolio.addSecurity(nullptr);
      FAIL("Expected PortfolioException");
    } catch (const PortfolioException& e) {
      std::string msg(e.what());
      REQUIRE(msg.find("cannot be null") != std::string::npos);
    }
  }
}

TEST_CASE("Portfolio Empty Portfolio Behavior", "[Portfolio][Empty]")
{
  SECTION("Newly created portfolio is empty")
  {
    Portfolio<DecimalType> portfolio("Empty Portfolio");
    
    REQUIRE(portfolio.getNumSecurities() == 0);
    REQUIRE(portfolio.beginPortfolio() == portfolio.endPortfolio());
  }
  
  SECTION("Finding security in empty portfolio returns end iterator")
  {
    Portfolio<DecimalType> portfolio("Empty Portfolio");
    
    auto it = portfolio.findSecurity("ANYTHING");
    REQUIRE(it == portfolio.endPortfolio());
  }
  
  SECTION("Removing from empty portfolio is no-op")
  {
    Portfolio<DecimalType> portfolio("Empty Portfolio");
    
    // Should not throw
    portfolio.removeSecurity("ANYTHING");
    REQUIRE(portfolio.getNumSecurities() == 0);
  }
}

TEST_CASE("Portfolio findSecurity() Edge Cases", "[Portfolio][Find]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  SECTION("Find non-existent security returns end iterator")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    auto it = portfolio.findSecurity("NONEXISTENT");
    REQUIRE(it == portfolio.endPortfolio());
  }
  
  SECTION("Find is case-sensitive")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    // Exact match works
    auto it = portfolio.findSecurity("SPY");
    REQUIRE(it != portfolio.endPortfolio());
    
    // Different case doesn't match
    it = portfolio.findSecurity("spy");
    REQUIRE(it == portfolio.endPortfolio());
    
    it = portfolio.findSecurity("Spy");
    REQUIRE(it == portfolio.endPortfolio());
  }
  
  SECTION("Find empty string")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto it = portfolio.findSecurity("");
    REQUIRE(it == portfolio.endPortfolio());
  }
}

TEST_CASE("Portfolio Iterator Operations", "[Portfolio][Iterators]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  auto entry3 = createEquityEntry("20160106", "120.00", "125.00", "119.00", "123.00", 30000000);
  auto googSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  googSeries->addEntry(*entry3);
  
  SECTION("Iterate through empty portfolio")
  {
    Portfolio<DecimalType> portfolio("Empty Portfolio");
    
    int count = 0;
    for (auto it = portfolio.beginPortfolio(); it != portfolio.endPortfolio(); ++it) {
      count++;
    }
    REQUIRE(count == 0);
  }
  
  SECTION("Iterate through portfolio with one security")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    int count = 0;
    for (auto it = portfolio.beginPortfolio(); it != portfolio.endPortfolio(); ++it) {
      REQUIRE(it->first == "SPY");
      REQUIRE(it->second->getSymbol() == "SPY");
      count++;
    }
    REQUIRE(count == 1);
  }
  
  SECTION("Iterate through portfolio with multiple securities")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    auto googPtr = std::make_shared<EquitySecurity<DecimalType>>("GOOG", "Google", googSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    portfolio.addSecurity(googPtr);
    
    int count = 0;
    std::set<std::string> symbols;
    
    for (auto it = portfolio.beginPortfolio(); it != portfolio.endPortfolio(); ++it) {
      symbols.insert(it->first);
      REQUIRE(it->first == it->second->getSymbol());
      count++;
    }
    
    REQUIRE(count == 3);
    REQUIRE(symbols.size() == 3);
    REQUIRE(symbols.count("SPY") == 1);
    REQUIRE(symbols.count("AAPL") == 1);
    REQUIRE(symbols.count("GOOG") == 1);
  }
  
  SECTION("Iterators are sorted by symbol (std::map property)")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    // Add in non-alphabetical order
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    auto googPtr = std::make_shared<EquitySecurity<DecimalType>>("GOOG", "Google", googSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    portfolio.addSecurity(googPtr);
    
    // Iteration should be alphabetical
    auto it = portfolio.beginPortfolio();
    REQUIRE(it->first == "AAPL");
    ++it;
    REQUIRE(it->first == "GOOG");
    ++it;
    REQUIRE(it->first == "SPY");
    ++it;
    REQUIRE(it == portfolio.endPortfolio());
  }
  
  SECTION("Const-correctness of iterators")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    const Portfolio<DecimalType>& constRef = portfolio;
    
    // Should compile - const portfolio returns const iterators
    Portfolio<DecimalType>::ConstPortfolioIterator it = constRef.beginPortfolio();
    REQUIRE(it != constRef.endPortfolio());
    REQUIRE(it->first == "SPY");
  }
}

TEST_CASE("Portfolio Large Portfolio Operations", "[Portfolio][Performance]")
{
  SECTION("Add many securities")
  {
    Portfolio<DecimalType> portfolio("Large Portfolio");
    
    const int numSecurities = 100;
    std::vector<std::shared_ptr<EquitySecurity<DecimalType>>> securities;
    
    for (int i = 0; i < numSecurities; ++i) {
      auto entry = createEquityEntry("20160106", "100.00", "101.00", "99.00", "100.50", 1000000);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      std::string symbol = "SYM" + std::to_string(i);
      auto sec = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Security " + std::to_string(i), series);
      
      portfolio.addSecurity(sec);
      securities.push_back(sec);
    }
    
    REQUIRE(portfolio.getNumSecurities() == numSecurities);
    
    // Verify all can be found
    for (int i = 0; i < numSecurities; ++i) {
      std::string symbol = "SYM" + std::to_string(i);
      auto it = portfolio.findSecurity(symbol);
      REQUIRE(it != portfolio.endPortfolio());
    }
  }
  
  SECTION("Remove many securities")
  {
    Portfolio<DecimalType> portfolio("Large Portfolio");
    
    const int numSecurities = 50;
    
    for (int i = 0; i < numSecurities; ++i) {
      auto entry = createEquityEntry("20160106", "100.00", "101.00", "99.00", "100.50", 1000000);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      std::string symbol = "SYM" + std::to_string(i);
      auto sec = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Security " + std::to_string(i), series);
      portfolio.addSecurity(sec);
    }
    
    REQUIRE(portfolio.getNumSecurities() == numSecurities);
    
    // Remove every other security
    for (int i = 0; i < numSecurities; i += 2) {
      std::string symbol = "SYM" + std::to_string(i);
      portfolio.removeSecurity(symbol);
    }
    
    REQUIRE(portfolio.getNumSecurities() == numSecurities / 2);
    
    // Verify correct ones were removed
    for (int i = 0; i < numSecurities; ++i) {
      std::string symbol = "SYM" + std::to_string(i);
      auto it = portfolio.findSecurity(symbol);
      
      if (i % 2 == 0) {
        REQUIRE(it == portfolio.endPortfolio()); // Should be removed
      } else {
        REQUIRE(it != portfolio.endPortfolio()); // Should still exist
      }
    }
  }
}

TEST_CASE("Portfolio Mixed Security Types", "[Portfolio][MixedTypes]")
{
  auto equityEntry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto equitySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  equitySeries->addEntry(*equityEntry);
  
  auto futuresEntry = createTimeSeriesEntry("19851118", "3664.51025", "3687.58178", 
                                            "3656.81982", "3672.20068", 50000);
  auto futuresSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::CONTRACTS);
  futuresSeries->addEntry(*futuresEntry);
  
  SECTION("Portfolio can hold both equity and futures securities")
  {
    Portfolio<DecimalType> portfolio("Mixed Portfolio");
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", equitySeries);
    auto esPtr = std::make_shared<FuturesSecurity<DecimalType>>(
      "ES", "E-mini S&P 500",
      createDecimal("50.0"), createDecimal("0.25"),
      futuresSeries
    );
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(esPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 2);
    
    // Verify equity
    auto it = portfolio.findSecurity("SPY");
    REQUIRE(it != portfolio.endPortfolio());
    REQUIRE(it->second->isEquitySecurity());
    REQUIRE_FALSE(it->second->isFuturesSecurity());
    
    // Verify futures
    it = portfolio.findSecurity("ES");
    REQUIRE(it != portfolio.endPortfolio());
    REQUIRE_FALSE(it->second->isEquitySecurity());
    REQUIRE(it->second->isFuturesSecurity());
  }
}

TEST_CASE("Portfolio Name Handling", "[Portfolio][Name]")
{
  SECTION("Portfolio name is stored correctly")
  {
    Portfolio<DecimalType> portfolio("My Test Portfolio");
    REQUIRE(portfolio.getPortfolioName() == "My Test Portfolio");
  }
  
  SECTION("Empty portfolio name")
  {
    Portfolio<DecimalType> portfolio("");
    REQUIRE(portfolio.getPortfolioName() == "");
  }
  
  SECTION("Portfolio name with special characters")
  {
    Portfolio<DecimalType> portfolio("Portfolio-2024_Q1 (Test)");
    REQUIRE(portfolio.getPortfolioName() == "Portfolio-2024_Q1 (Test)");
  }
  
  SECTION("Portfolio name is copied correctly")
  {
    Portfolio<DecimalType> original("Original Name");
    Portfolio<DecimalType> copy(original);
    
    REQUIRE(copy.getPortfolioName() == "Original Name");
    REQUIRE(copy.getPortfolioName() == original.getPortfolioName());
  }
  
  SECTION("Portfolio name is assigned correctly")
  {
    Portfolio<DecimalType> portfolio1("Portfolio 1");
    Portfolio<DecimalType> portfolio2("Portfolio 2");
    
    portfolio2 = portfolio1;
    
    REQUIRE(portfolio2.getPortfolioName() == "Portfolio 1");
  }
}

TEST_CASE("Portfolio Shared Pointer Semantics", "[Portfolio][SharedPtr]")
{
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry);
  
  SECTION("Multiple portfolios can share the same security")
  {
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    Portfolio<DecimalType> portfolio1("Portfolio 1");
    Portfolio<DecimalType> portfolio2("Portfolio 2");
    
    portfolio1.addSecurity(spyPtr);
    portfolio2.addSecurity(spyPtr);
    
    // Both portfolios should have the same shared_ptr
    auto it1 = portfolio1.findSecurity("SPY");
    auto it2 = portfolio2.findSecurity("SPY");
    
    REQUIRE(it1->second == it2->second); // Same shared_ptr
    REQUIRE(it1->second.get() == it2->second.get()); // Same raw pointer
  }
  
  SECTION("Copying portfolio shares securities (shallow copy of shared_ptr)")
  {
    Portfolio<DecimalType> original("Original");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    original.addSecurity(spyPtr);
    
    Portfolio<DecimalType> copy(original);
    
    auto origIt = original.findSecurity("SPY");
    auto copyIt = copy.findSecurity("SPY");
    
    // Should be the same shared_ptr
    REQUIRE(origIt->second == copyIt->second);
  }
  
  SECTION("Removing from one portfolio doesn't affect others")
  {
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    Portfolio<DecimalType> portfolio1("Portfolio 1");
    Portfolio<DecimalType> portfolio2("Portfolio 2");
    
    portfolio1.addSecurity(spyPtr);
    portfolio2.addSecurity(spyPtr);
    
    // Remove from portfolio1
    portfolio1.removeSecurity("SPY");
    
    REQUIRE(portfolio1.getNumSecurities() == 0);
    REQUIRE(portfolio2.getNumSecurities() == 1);
    REQUIRE(portfolio2.findSecurity("SPY") != portfolio2.endPortfolio());
  }
}

TEST_CASE("Portfolio Move Constructor", "[Portfolio][MoveSemantics]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  SECTION("Move empty portfolio")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    
    Portfolio<DecimalType> destination(std::move(source));
    
    REQUIRE(destination.getPortfolioName() == "Source Portfolio");
    REQUIRE(destination.getNumSecurities() == 0);
    REQUIRE(destination.empty());
    
    // Source should be in valid but unspecified state
    // We can still call methods on it safely
    REQUIRE(source.getNumSecurities() == 0);
  }
  
  SECTION("Move portfolio with securities")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    source.addSecurity(spyPtr);
    source.addSecurity(aaplPtr);
    
    REQUIRE(source.getNumSecurities() == 2);
    
    // Move construct
    Portfolio<DecimalType> destination(std::move(source));
    
    // Destination should have the securities
    REQUIRE(destination.getPortfolioName() == "Source Portfolio");
    REQUIRE(destination.getNumSecurities() == 2);
    REQUIRE_FALSE(destination.empty());
    REQUIRE(destination.contains("SPY"));
    REQUIRE(destination.contains("AAPL"));
    
    auto it = destination.findSecurity("SPY");
    REQUIRE(it != destination.endPortfolio());
    REQUIRE(it->second->getSymbol() == "SPY");
    
    it = destination.findSecurity("AAPL");
    REQUIRE(it != destination.endPortfolio());
    REQUIRE(it->second->getSymbol() == "AAPL");
    
    // Source should be in valid but unspecified state
    // After move, it should be empty or have no securities
    REQUIRE(source.getNumSecurities() == 0);
  }
  
  SECTION("Move portfolio with many securities")
  {
    Portfolio<DecimalType> source("Large Portfolio");
    
    const int numSecurities = 50;
    for (int i = 0; i < numSecurities; ++i) {
      auto entry = createEquityEntry("20160106", "100.00", "101.00", "99.00", "100.50", 1000000);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      std::string symbol = "SYM" + std::to_string(i);
      auto sec = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Security " + std::to_string(i), series);
      source.addSecurity(sec);
    }
    
    REQUIRE(source.getNumSecurities() == numSecurities);
    
    Portfolio<DecimalType> destination(std::move(source));
    
    REQUIRE(destination.getNumSecurities() == numSecurities);
    REQUIRE_FALSE(destination.empty());
    
    // Verify all securities are present
    for (int i = 0; i < numSecurities; ++i) {
      std::string symbol = "SYM" + std::to_string(i);
      REQUIRE(destination.contains(symbol));
    }
  }
}

TEST_CASE("Portfolio Move Assignment Operator", "[Portfolio][MoveSemantics]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  auto entry3 = createEquityEntry("20160106", "120.00", "125.00", "119.00", "123.00", 30000000);
  auto googSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  googSeries->addEntry(*entry3);
  
  SECTION("Move assign empty to empty")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    Portfolio<DecimalType> destination("Destination Portfolio");
    
    destination = std::move(source);
    
    REQUIRE(destination.getPortfolioName() == "Source Portfolio");
    REQUIRE(destination.getNumSecurities() == 0);
    REQUIRE(destination.empty());
  }
  
  SECTION("Move assign non-empty to empty")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    source.addSecurity(spyPtr);
    source.addSecurity(aaplPtr);
    
    Portfolio<DecimalType> destination("Destination Portfolio");
    
    destination = std::move(source);
    
    REQUIRE(destination.getPortfolioName() == "Source Portfolio");
    REQUIRE(destination.getNumSecurities() == 2);
    REQUIRE(destination.contains("SPY"));
    REQUIRE(destination.contains("AAPL"));
  }
  
  SECTION("Move assign non-empty to non-empty")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    source.addSecurity(spyPtr);
    
    Portfolio<DecimalType> destination("Destination Portfolio");
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    auto googPtr = std::make_shared<EquitySecurity<DecimalType>>("GOOG", "Google", googSeries);
    destination.addSecurity(aaplPtr);
    destination.addSecurity(googPtr);
    
    REQUIRE(destination.getNumSecurities() == 2);
    
    destination = std::move(source);
    
    // Destination should now have source's contents
    REQUIRE(destination.getPortfolioName() == "Source Portfolio");
    REQUIRE(destination.getNumSecurities() == 1);
    REQUIRE(destination.contains("SPY"));
    REQUIRE_FALSE(destination.contains("AAPL")); // Old contents replaced
    REQUIRE_FALSE(destination.contains("GOOG")); // Old contents replaced
  }
  
  SECTION("Move assign empty to non-empty")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    
    Portfolio<DecimalType> destination("Destination Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    destination.addSecurity(spyPtr);
    
    REQUIRE(destination.getNumSecurities() == 1);
    
    destination = std::move(source);
    
    REQUIRE(destination.getPortfolioName() == "Source Portfolio");
    REQUIRE(destination.getNumSecurities() == 0);
    REQUIRE(destination.empty());
  }
  
  SECTION("Move assignment preserves data correctly")
  {
    Portfolio<DecimalType> source("Source Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    source.addSecurity(spyPtr);
    
    Portfolio<DecimalType> destination("Destination Portfolio");
    
    // Store original data for verification
    auto originalName = source.getPortfolioName();
    auto originalCount = source.getNumSecurities();
    
    // Perform move assignment
    destination = std::move(source);
    
    // Verify destination has correct data
    REQUIRE(destination.getPortfolioName() == originalName);
    REQUIRE(destination.getNumSecurities() == originalCount);
    REQUIRE(destination.contains("SPY"));
    
    // Verify destination is usable
    REQUIRE_NOTHROW(destination.getPortfolioName());
    REQUIRE_NOTHROW(destination.getNumSecurities());
    REQUIRE_NOTHROW(destination.beginPortfolio());
  }
  SECTION("Move assignment with large portfolio")
  {
    Portfolio<DecimalType> source("Large Source");
    
    const int numSecurities = 100;
    for (int i = 0; i < numSecurities; ++i) {
      auto entry = createEquityEntry("20160106", "100.00", "101.00", "99.00", "100.50", 1000000);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      std::string symbol = "SRC" + std::to_string(i);
      auto sec = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Security " + std::to_string(i), series);
      source.addSecurity(sec);
    }
    
    Portfolio<DecimalType> destination("Small Destination");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    destination.addSecurity(spyPtr);
    
    destination = std::move(source);
    
    REQUIRE(destination.getNumSecurities() == numSecurities);
    REQUIRE_FALSE(destination.contains("SPY")); // Old content gone
    
    for (int i = 0; i < numSecurities; ++i) {
      std::string symbol = "SRC" + std::to_string(i);
      REQUIRE(destination.contains(symbol));
    }
  }
}

TEST_CASE("Portfolio empty() method", "[Portfolio][Empty]")
{
  auto entry = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry);
  
  SECTION("Newly created portfolio is empty")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 0);
  }
  
  SECTION("Portfolio with securities is not empty")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    
    REQUIRE_FALSE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 1);
  }
  
  SECTION("Portfolio becomes empty after removing all securities")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    REQUIRE_FALSE(portfolio.empty());
    
    portfolio.removeSecurity("SPY");
    REQUIRE(portfolio.empty());
  }
  
  SECTION("Portfolio becomes empty after clear()")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    REQUIRE_FALSE(portfolio.empty());
    
    portfolio.clear();
    REQUIRE(portfolio.empty());
  }
  
  SECTION("empty() consistent with getNumSecurities()")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE(portfolio.empty() == (portfolio.getNumSecurities() == 0));
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    REQUIRE(portfolio.empty() == (portfolio.getNumSecurities() == 0));
  }
}

TEST_CASE("Portfolio clear() method", "[Portfolio][Clear]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  auto entry3 = createEquityEntry("20160106", "120.00", "125.00", "119.00", "123.00", 30000000);
  auto googSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  googSeries->addEntry(*entry3);
  
  SECTION("Clear empty portfolio")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE(portfolio.empty());
    
    portfolio.clear();
    
    REQUIRE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 0);
  }
  
  SECTION("Clear portfolio with one security")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    REQUIRE(portfolio.getNumSecurities() == 1);
    
    portfolio.clear();
    
    REQUIRE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 0);
    REQUIRE_FALSE(portfolio.contains("SPY"));
    REQUIRE(portfolio.findSecurity("SPY") == portfolio.endPortfolio());
  }
  
  SECTION("Clear portfolio with multiple securities")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    auto googPtr = std::make_shared<EquitySecurity<DecimalType>>("GOOG", "Google", googSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    portfolio.addSecurity(googPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 3);
    
    portfolio.clear();
    
    REQUIRE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 0);
    REQUIRE_FALSE(portfolio.contains("SPY"));
    REQUIRE_FALSE(portfolio.contains("AAPL"));
    REQUIRE_FALSE(portfolio.contains("GOOG"));
  }
  
  SECTION("Clear and re-add securities")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.clear();
    
    REQUIRE(portfolio.empty());
    
    // Should be able to add securities again
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    portfolio.addSecurity(aaplPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.contains("AAPL"));
  }
  
  SECTION("Clear portfolio multiple times")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.clear();
    portfolio.clear(); // Second clear should be safe
    portfolio.clear(); // Third clear should be safe
    
    REQUIRE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 0);
  }
  
  SECTION("Clear large portfolio")
  {
    Portfolio<DecimalType> portfolio("Large Portfolio");
    
    const int numSecurities = 100;
    for (int i = 0; i < numSecurities; ++i) {
      auto entry = createEquityEntry("20160106", "100.00", "101.00", "99.00", "100.50", 1000000);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      std::string symbol = "SYM" + std::to_string(i);
      auto sec = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Security " + std::to_string(i), series);
      portfolio.addSecurity(sec);
    }
    
    REQUIRE(portfolio.getNumSecurities() == numSecurities);
    
    portfolio.clear();
    
    REQUIRE(portfolio.empty());
    REQUIRE(portfolio.getNumSecurities() == 0);
  }
  
  SECTION("Iterators after clear")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.clear();
    
    // Iterators should be equal (empty range)
    REQUIRE(portfolio.beginPortfolio() == portfolio.endPortfolio());
  }
}

TEST_CASE("Portfolio contains() method", "[Portfolio][Contains]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  SECTION("Empty portfolio contains nothing")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE_FALSE(portfolio.contains("SPY"));
    REQUIRE_FALSE(portfolio.contains("AAPL"));
    REQUIRE_FALSE(portfolio.contains("ANYTHING"));
    REQUIRE_FALSE(portfolio.contains(""));
  }
  
  SECTION("Portfolio contains added security")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    
    REQUIRE(portfolio.contains("SPY"));
    REQUIRE_FALSE(portfolio.contains("AAPL"));
  }
  
  SECTION("Portfolio contains multiple securities")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    
    REQUIRE(portfolio.contains("SPY"));
    REQUIRE(portfolio.contains("AAPL"));
    REQUIRE_FALSE(portfolio.contains("GOOG"));
  }
  
  SECTION("contains() is case-sensitive")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    
    REQUIRE(portfolio.contains("SPY"));
    REQUIRE_FALSE(portfolio.contains("spy"));
    REQUIRE_FALSE(portfolio.contains("Spy"));
    REQUIRE_FALSE(portfolio.contains("sPy"));
  }
  
  SECTION("contains() after removeSecurity")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    portfolio.addSecurity(spyPtr);
    portfolio.addSecurity(aaplPtr);
    
    REQUIRE(portfolio.contains("SPY"));
    REQUIRE(portfolio.contains("AAPL"));
    
    portfolio.removeSecurity("SPY");
    
    REQUIRE_FALSE(portfolio.contains("SPY"));
    REQUIRE(portfolio.contains("AAPL"));
  }
  
  SECTION("contains() after clear")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    REQUIRE(portfolio.contains("SPY"));
    
    portfolio.clear();
    REQUIRE_FALSE(portfolio.contains("SPY"));
  }
  
  SECTION("contains() consistent with findSecurity()")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    
    // contains() should be consistent with findSecurity()
    REQUIRE(portfolio.contains("SPY") == (portfolio.findSecurity("SPY") != portfolio.endPortfolio()));
    REQUIRE(portfolio.contains("AAPL") == (portfolio.findSecurity("AAPL") != portfolio.endPortfolio()));
  }
  
  SECTION("contains() with empty string")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE_FALSE(portfolio.contains(""));
  }
  
  SECTION("contains() performance with many securities")
  {
    Portfolio<DecimalType> portfolio("Large Portfolio");
    
    const int numSecurities = 100;
    for (int i = 0; i < numSecurities; ++i) {
      auto entry = createEquityEntry("20160106", "100.00", "101.00", "99.00", "100.50", 1000000);
      auto series = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
      series->addEntry(*entry);
      
      std::string symbol = "SYM" + std::to_string(i);
      auto sec = std::make_shared<EquitySecurity<DecimalType>>(symbol, "Security " + std::to_string(i), series);
      portfolio.addSecurity(sec);
    }
    
    // Test contains for all securities
    for (int i = 0; i < numSecurities; ++i) {
      std::string symbol = "SYM" + std::to_string(i);
      REQUIRE(portfolio.contains(symbol));
    }
    
    // Test non-existent securities
    REQUIRE_FALSE(portfolio.contains("NONEXISTENT"));
    REQUIRE_FALSE(portfolio.contains("SYM999"));
  }
}

TEST_CASE("Portfolio replaceSecurity() with null pointer validation", "[Portfolio][Replace][NullCheck]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries1 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries1->addEntry(*entry1);
  
  SECTION("replaceSecurity(security) throws on null pointer - single argument")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE_THROWS_AS(
      portfolio.replaceSecurity(nullptr),
      PortfolioException
    );
    
    // Verify error message
    try {
      portfolio.replaceSecurity(nullptr);
      FAIL("Expected PortfolioException");
    } catch (const PortfolioException& e) {
      std::string msg(e.what());
      REQUIRE(msg.find("cannot be null") != std::string::npos);
    }
  }
  
  SECTION("replaceSecurity(symbol, security) throws on null pointer - two arguments")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    REQUIRE_THROWS_AS(
      portfolio.replaceSecurity("SPY", nullptr),
      PortfolioException
    );
    
    // Verify error message
    try {
      portfolio.replaceSecurity("SPY", nullptr);
      FAIL("Expected PortfolioException");
    } catch (const PortfolioException& e) {
      std::string msg(e.what());
      REQUIRE(msg.find("cannot be null") != std::string::npos);
    }
  }
  
  SECTION("replaceSecurity with null doesn't modify portfolio")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.addSecurity(spyPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    
    // Attempt to replace with null
    try {
      portfolio.replaceSecurity(nullptr);
      FAIL("Should have thrown");
    } catch (const PortfolioException&) {
      // Expected
    }
    
    // Portfolio should be unchanged
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.contains("SPY"));
  }
  
  SECTION("replaceSecurity two-arg with null doesn't modify portfolio")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.addSecurity(spyPtr);
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    
    // Attempt to replace with null
    try {
      portfolio.replaceSecurity("AAPL", nullptr);
      FAIL("Should have thrown");
    } catch (const PortfolioException&) {
      // Expected
    }
    
    // Portfolio should be unchanged
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.contains("SPY"));
    REQUIRE_FALSE(portfolio.contains("AAPL"));
  }
  
  SECTION("Valid replaceSecurity still works after fix")
  {
    Portfolio<DecimalType> portfolio("Test Portfolio");
    
    auto entry2 = createEquityEntry("20160107", "199.00", "201.00", "198.00", "200.50", 150000000);
    auto spySeries2 = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
    spySeries2->addEntry(*entry2);
    
    auto spyPtr1 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries1);
    portfolio.addSecurity(spyPtr1);
    
    // Valid replace should still work
    auto spyPtr2 = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries2);
    REQUIRE_NOTHROW(portfolio.replaceSecurity(spyPtr2));
    
    REQUIRE(portfolio.getNumSecurities() == 1);
    REQUIRE(portfolio.contains("SPY"));
    
    auto it = portfolio.findSecurity("SPY");
    REQUIRE(it->second->getTimeSeries() == spySeries2);
  }
}

TEST_CASE("Portfolio Integration Tests - New Methods Combined", "[Portfolio][Integration]")
{
  auto entry1 = createEquityEntry("20160106", "198.34", "200.06", "197.60", "198.82", 142662900);
  auto spySeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  spySeries->addEntry(*entry1);
  
  auto entry2 = createEquityEntry("20160106", "90.12", "91.50", "89.80", "90.75", 50000000);
  auto aaplSeries = std::make_shared<OHLCTimeSeries<DecimalType>>(TimeFrame::DAILY, TradingVolume::SHARES);
  aaplSeries->addEntry(*entry2);
  
  SECTION("Move, contains, and clear work together")
  {
    Portfolio<DecimalType> source("Source");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    
    source.addSecurity(spyPtr);
    source.addSecurity(aaplPtr);
    
    REQUIRE(source.contains("SPY"));
    REQUIRE(source.contains("AAPL"));
    
    Portfolio<DecimalType> destination(std::move(source));
    
    REQUIRE(destination.contains("SPY"));
    REQUIRE(destination.contains("AAPL"));
    REQUIRE_FALSE(destination.empty());
    
    destination.clear();
    
    REQUIRE_FALSE(destination.contains("SPY"));
    REQUIRE_FALSE(destination.contains("AAPL"));
    REQUIRE(destination.empty());
  }
  
  SECTION("empty() and contains() consistency")
  {
    Portfolio<DecimalType> portfolio("Test");
    
    REQUIRE(portfolio.empty());
    REQUIRE_FALSE(portfolio.contains("SPY"));
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    REQUIRE_FALSE(portfolio.empty());
    REQUIRE(portfolio.contains("SPY"));
    
    portfolio.removeSecurity("SPY");
    
    REQUIRE(portfolio.empty());
    REQUIRE_FALSE(portfolio.contains("SPY"));
  }
  
  SECTION("Replace with validation, then move")
  {
    Portfolio<DecimalType> portfolio("Test");
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    
    portfolio.addSecurity(spyPtr);
    
    // Try to replace with null - should throw
    REQUIRE_THROWS_AS(portfolio.replaceSecurity(nullptr), PortfolioException);
    
    // Portfolio should still be valid
    REQUIRE(portfolio.contains("SPY"));
    REQUIRE_FALSE(portfolio.empty());
    
    // Now move it
    Portfolio<DecimalType> destination(std::move(portfolio));
    
    REQUIRE(destination.contains("SPY"));
    REQUIRE_FALSE(destination.empty());
  }
  
  SECTION("Build, clear, rebuild workflow")
  {
    Portfolio<DecimalType> portfolio("Test");
    
    REQUIRE(portfolio.empty());
    
    auto spyPtr = std::make_shared<EquitySecurity<DecimalType>>("SPY", "SPDR S&P 500 ETF", spySeries);
    portfolio.addSecurity(spyPtr);
    
    REQUIRE_FALSE(portfolio.empty());
    REQUIRE(portfolio.contains("SPY"));
    
    portfolio.clear();
    
    REQUIRE(portfolio.empty());
    REQUIRE_FALSE(portfolio.contains("SPY"));
    
    auto aaplPtr = std::make_shared<EquitySecurity<DecimalType>>("AAPL", "Apple Inc.", aaplSeries);
    portfolio.addSecurity(aaplPtr);
    
    REQUIRE_FALSE(portfolio.empty());
    REQUIRE(portfolio.contains("AAPL"));
    REQUIRE_FALSE(portfolio.contains("SPY"));
  }
}
