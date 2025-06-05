#include <catch2/catch_test_macros.hpp>
#include "ValidatorConfiguration.h"
#include "TestUtils.h"
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace mkc_timeseries;
using namespace boost::gregorian;
using namespace boost::posix_time;


TEST_CASE ("Security operations", "[Security]")
{
  ValidatorConfigurationFileReader reader("QQQ_config.txt");

  
  std::string symbol("QQQ");
    DecimalType qqqBigPointValue(createDecimal("1.0"));
  DecimalType qqqTickValue(createDecimal("0.01"));

  auto oosFirstDate = createDate("20210820");
  auto oosLastDate = createDate("20250331");
  
  std::shared_ptr<ValidatorConfiguration<DecimalType>> configuration = reader.readConfigurationFile();
  auto aSecurity = configuration->getSecurity();
  REQUIRE (aSecurity->getSymbol() == symbol);
    REQUIRE (aSecurity->getBigPointValue() == qqqBigPointValue);
  REQUIRE (aSecurity->getTick() == qqqTickValue);
  REQUIRE (aSecurity->getTimeSeries()->getFirstDate() == createDate("20070402"));
  REQUIRE (aSecurity->getTimeSeries()->getLastDate() == createDate("20250331"));
  REQUIRE (aSecurity->isEquitySecurity());
  REQUIRE (aSecurity->getTimeSeries()->getTimeFrame() == TimeFrame::DAILY);

  REQUIRE (configuration->getPricePatterns()->getNumPatterns() == 7);
  DateRange inSampleDateRange = configuration->getInsampleDateRange();
  DateRange ooSampleDateRange = configuration->getOosDateRange();

  
  REQUIRE (inSampleDateRange.getFirstDate() == createDate("20070402"));
  REQUIRE (inSampleDateRange.getLastDate() == createDate("20210819"));
  REQUIRE (ooSampleDateRange.getFirstDate() == oosFirstDate);
  REQUIRE (ooSampleDateRange.getLastDate() == oosLastDate);

  auto oosBackTester = configuration->getBackTester();
  REQUIRE (oosBackTester->numBackTestRanges() == 1);
  REQUIRE (oosBackTester->getStartDate() == oosFirstDate);
  REQUIRE (oosBackTester->getEndDate() == oosLastDate);
}

TEST_CASE("Intraday date parsing - ptime format", "[ValidatorConfiguration][Intraday]")
{
    SECTION("Parse intraday ptime dates correctly")
    {
        // Create a temporary config file with intraday ptime format dates
        std::string configContent =
            "Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame\n"
            "AAPL,./test_ir.txt,./test_data.txt,INTRADAY::TRADESTATION,20210415T093000,20240604T160000,20240605T093000,20250320T160000,Intraday\n";
        
        std::ofstream configFile("test_intraday_config.csv");
        configFile << configContent;
        configFile.close();
        
        // Create minimal test files to satisfy file existence checks
        std::ofstream irFile("./test_ir.txt");
        irFile << "Code For Selected Patterns\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile << "\n";
        irFile << "{File:test_data.txt  Index:1  Index Date:20210415  PL:80.00%  PS:20%  Trades:10  CL:1}\n";
        irFile << "\n";
        irFile << "IF CLOSE OF 0 BARS AGO > OPEN OF 0 BARS AGO\n";
        irFile << "THEN BUY NEXT BAR ON THE OPEN WITH\n";
        irFile << "PROFIT TARGET AT ENTRY PRICE + 1.0 %\n";
        irFile << "AND STOP LOSS AT ENTRY PRICE - 1.0 %\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile.close();
        
        std::ofstream dataFile("./test_data.txt");
        dataFile << "Date,Time,Open,High,Low,Close,Up,Down\n";
        dataFile << "04/15/2021,09:30:00,130.00,131.00,129.50,130.50,1000,500\n";
        dataFile << "04/15/2021,09:31:00,130.50,131.25,130.00,131.00,1500,800\n";
        dataFile.close();
        
        ValidatorConfigurationFileReader reader("test_intraday_config.csv");
        
        REQUIRE_NOTHROW([&]() {
            auto config = reader.readConfigurationFile();
            
            // Verify date ranges are parsed correctly from ptime format
            DateRange inSampleRange = config->getInsampleDateRange();
            DateRange oosRange = config->getOosDateRange();
            
            // Expected dates extracted from ptime strings
            date expectedISStart = date(2021, 4, 15);  // From 20210415T093000
            date expectedISEnd = date(2024, 6, 4);     // From 20240604T160000
            date expectedOOSStart = date(2024, 6, 5);  // From 20240605T093000
            date expectedOOSEnd = date(2025, 3, 20);   // From 20250320T160000
            
            REQUIRE(inSampleRange.getFirstDate() == expectedISStart);
            REQUIRE(inSampleRange.getLastDate() == expectedISEnd);
            REQUIRE(oosRange.getFirstDate() == expectedOOSStart);
            REQUIRE(oosRange.getLastDate() == expectedOOSEnd);
            
            // Verify no overlap between in-sample and out-of-sample
            REQUIRE(oosRange.getFirstDate() > inSampleRange.getLastDate());
        }());
        
        // Cleanup
        std::remove("test_intraday_config.csv");
        std::remove("./test_ir.txt");
        std::remove("./test_data.txt");
    }
}

TEST_CASE("EOD date parsing - gregorian format", "[ValidatorConfiguration][EOD]")
{
    SECTION("Parse EOD gregorian dates correctly")
    {
        // Create a temporary config file with EOD gregorian format dates
        std::string configContent =
            "Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame\n"
            "SPY,./test_ir.txt,./test_data.txt,PAL,20210415,20240604,20240605,20250320,Daily\n";
        
        std::ofstream configFile("test_eod_config.csv");
        configFile << configContent;
        configFile.close();
        
        // Create minimal test files
        std::ofstream irFile("./test_ir.txt");
        irFile << "Code For Selected Patterns\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile << "\n";
        irFile << "{File:test_data.txt  Index:1  Index Date:20210415  PL:80.00%  PS:20%  Trades:10  CL:1}\n";
        irFile << "\n";
        irFile << "IF CLOSE OF 0 BARS AGO > OPEN OF 0 BARS AGO\n";
        irFile << "THEN BUY NEXT BAR ON THE OPEN WITH\n";
        irFile << "PROFIT TARGET AT ENTRY PRICE + 1.0 %\n";
        irFile << "AND STOP LOSS AT ENTRY PRICE - 1.0 %\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile.close();
        
        std::ofstream dataFile("./test_data.txt");
        dataFile << "20210415,400.0000000,401.0000000,399.5000000,400.5000000\n";
        dataFile << "20210416,400.5000000,402.0000000,400.0000000,401.2500000\n";
        dataFile.close();
        
        ValidatorConfigurationFileReader reader("test_eod_config.csv");
        
        REQUIRE_NOTHROW([&]() {
            auto config = reader.readConfigurationFile();
            
            // Verify date ranges are parsed correctly from gregorian format
            DateRange inSampleRange = config->getInsampleDateRange();
            DateRange oosRange = config->getOosDateRange();
            
            // Expected dates from gregorian strings
            date expectedISStart = date(2021, 4, 15);  // From 20210415
            date expectedISEnd = date(2024, 6, 4);     // From 20240604
            date expectedOOSStart = date(2024, 6, 5);  // From 20240605
            date expectedOOSEnd = date(2025, 3, 20);   // From 20250320
            
            REQUIRE(inSampleRange.getFirstDate() == expectedISStart);
            REQUIRE(inSampleRange.getLastDate() == expectedISEnd);
            REQUIRE(oosRange.getFirstDate() == expectedOOSStart);
            REQUIRE(oosRange.getLastDate() == expectedOOSEnd);
            
            // Verify no overlap between in-sample and out-of-sample
            REQUIRE(oosRange.getFirstDate() > inSampleRange.getLastDate());
        }());
        
        // Cleanup
        std::remove("test_eod_config.csv");
        std::remove("./test_ir.txt");
        std::remove("./test_data.txt");
    }
}

TEST_CASE("Intraday format reader selection", "[ValidatorConfiguration][FormatReader]")
{
    SECTION("INTRADAY::TRADESTATION format creates correct reader")
    {
        // Create a temporary config file with intraday format
        std::string configContent =
            "Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame\n"
            "QQQ,./test_ir.txt,./test_data.txt,INTRADAY::TRADESTATION,20210415T093000,20240604T160000,20240605T093000,20250320T160000,Intraday\n";
        
        std::ofstream configFile("test_format_config.csv");
        configFile << configContent;
        configFile.close();
        
        // Create minimal test files
        std::ofstream irFile("./test_ir.txt");
        irFile << "Code For Selected Patterns\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile << "\n";
        irFile << "{File:test_data.txt  Index:1  Index Date:20210415  PL:80.00%  PS:20%  Trades:10  CL:1}\n";
        irFile << "\n";
        irFile << "IF CLOSE OF 0 BARS AGO > OPEN OF 0 BARS AGO\n";
        irFile << "THEN BUY NEXT BAR ON THE OPEN WITH\n";
        irFile << "PROFIT TARGET AT ENTRY PRICE + 1.0 %\n";
        irFile << "AND STOP LOSS AT ENTRY PRICE - 1.0 %\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile.close();
        
        std::ofstream dataFile("./test_data.txt");
        dataFile << "Date,Time,Open,High,Low,Close,Up,Down\n";
        dataFile << "04/15/2021,09:30:00,350.00,351.00,349.50,350.50,10000,5000\n";
        dataFile << "04/15/2021,09:31:00,350.50,351.25,350.00,351.00,15000,8000\n";
        dataFile.close();
        
        ValidatorConfigurationFileReader reader("test_format_config.csv");
        
        REQUIRE_NOTHROW([&]() {
            auto config = reader.readConfigurationFile();
            
            // Verify the configuration was created successfully
            REQUIRE(config != nullptr);
            
            auto security = config->getSecurity();
            REQUIRE(security != nullptr);
            REQUIRE(security->getSymbol() == "QQQ");
            
            // Verify time frame is set correctly for intraday
            REQUIRE(security->getTimeSeries()->getTimeFrame() == TimeFrame::INTRADAY);
        }());
        
        // Cleanup
        std::remove("test_format_config.csv");
        std::remove("./test_ir.txt");
        std::remove("./test_data.txt");
    }
}

TEST_CASE("Date overlap validation", "[ValidatorConfiguration][Validation]")
{
    SECTION("Throws exception when OOS start date overlaps with in-sample end date")
    {
        // Create config with overlapping dates (same date for IS end and OOS start)
        std::string configContent =
            "Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame\n"
            "MSFT,./test_ir.txt,./test_data.txt,INTRADAY::TRADESTATION,20210415T093000,20240604T160000,20240604T093000,20250320T160000,Intraday\n";
        
        std::ofstream configFile("test_overlap_config.csv");
        configFile << configContent;
        configFile.close();
        
        // Create minimal test files
        std::ofstream irFile("./test_ir.txt");
        irFile << "Code For Selected Patterns\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile << "\n";
        irFile << "{File:test_data.txt  Index:1  Index Date:20210415  PL:80.00%  PS:20%  Trades:10  CL:1}\n";
        irFile << "\n";
        irFile << "IF CLOSE OF 0 BARS AGO > OPEN OF 0 BARS AGO\n";
        irFile << "THEN BUY NEXT BAR ON THE OPEN WITH\n";
        irFile << "PROFIT TARGET AT ENTRY PRICE + 1.0 %\n";
        irFile << "AND STOP LOSS AT ENTRY PRICE - 1.0 %\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile.close();
        
        std::ofstream dataFile("./test_data.txt");
        dataFile << "Date,Time,Open,High,Low,Close,Up,Down\n";
        dataFile << "04/15/2021,09:30:00,250.00,251.00,249.50,250.50,5000,2500\n";
        dataFile.close();
        
        ValidatorConfigurationFileReader reader("test_overlap_config.csv");
        
        // Should throw exception due to overlapping dates
        REQUIRE_THROWS_AS(reader.readConfigurationFile(), ValidatorConfigurationException);
        
        // Cleanup
        std::remove("test_overlap_config.csv");
        std::remove("./test_ir.txt");
        std::remove("./test_data.txt");
    }
}

TEST_CASE("Date format consistency validation", "[ValidatorConfiguration][FormatConsistency]")
{
    SECTION("Throws exception when date formats are inconsistent")
    {
        // Create config with mixed formats (ptime for in-sample, gregorian for out-of-sample)
        std::string configContent =
            "Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame\n"
            "TSLA,./test_ir.txt,./test_data.txt,INTRADAY::TRADESTATION,20210415T093000,20240604T160000,20240605,20250320,Intraday\n";
        
        std::ofstream configFile("test_inconsistent_config.csv");
        configFile << configContent;
        configFile.close();
        
        // Create minimal test files
        std::ofstream irFile("./test_ir.txt");
        irFile << "Code For Selected Patterns\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile << "\n";
        irFile << "{File:test_data.txt  Index:1  Index Date:20210415  PL:80.00%  PS:20%  Trades:10  CL:1}\n";
        irFile << "\n";
        irFile << "IF CLOSE OF 0 BARS AGO > OPEN OF 0 BARS AGO\n";
        irFile << "THEN BUY NEXT BAR ON THE OPEN WITH\n";
        irFile << "PROFIT TARGET AT ENTRY PRICE + 1.0 %\n";
        irFile << "AND STOP LOSS AT ENTRY PRICE - 1.0 %\n";
        irFile << "----------------------------------------------------------------------------------------------------------------------------------\n";
        irFile.close();
        
        std::ofstream dataFile("./test_data.txt");
        dataFile << "Date,Time,Open,High,Low,Close,Up,Down\n";
        dataFile << "04/15/2021,09:30:00,700.00,701.00,699.50,700.50,8000,4000\n";
        dataFile.close();
        
        ValidatorConfigurationFileReader reader("test_inconsistent_config.csv");
        
        // Should throw exception due to inconsistent date formats
        REQUIRE_THROWS_AS(reader.readConfigurationFile(), ValidatorConfigurationException);
        
        // Cleanup
        std::remove("test_inconsistent_config.csv");
        std::remove("./test_ir.txt");
        std::remove("./test_data.txt");
    }
}
