#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <limits>
#include <cctype>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp> 

#include "TimeSeriesCsvReader.h"
#include "TimeSeriesCsvWriter.h"
#include "TimeSeriesIndicators.h"
#include "DecimalConstants.h"
#include "TimeFrameUtility.h"

using namespace mkc_timeseries;
using std::shared_ptr;
using namespace boost::gregorian;
namespace fs = std::filesystem;

using Num = num::DefaultNumber;

// Writes a CSV configuration file for permutation testing into the given output directory
void writeConfigFile(const std::string& outputDir,
                     const std::string& tickerSymbol,
                     const OHLCTimeSeries<Num>& insampleSeries,
                     const OHLCTimeSeries<Num>& outOfSampleSeries,
                     const std::string& timeFrame)
{
  fs::path configFileName = fs::path(outputDir) / (tickerSymbol + "_config.csv");
  std::ofstream configFile(configFileName);
  if (!configFile.is_open())
    {
      std::cerr << "Error: Unable to open config file " << configFileName << std::endl;
      return;
    }

  std::string irPath     = "./" + tickerSymbol + "_IR.txt";
  std::string dataPath   = "./" + tickerSymbol + "_ALL.txt";
  std::string fileFormat = (timeFrame == "Intraday") ? "INTRADAY::TRADESTATION" : "PAL";

  // Dates in YYYYMMDD format - use DateTime for intraday to preserve time information
  std::string isDateStart, isDateEnd, oosDateStart, oosDateEnd;

  if (timeFrame == "Intraday")
    {
      // For intraday data, use full DateTime to avoid overlapping date ranges
      // Format as YYYYMMDDTHHMMSS for ptime
      isDateStart  = boost::posix_time::to_iso_string(insampleSeries.getFirstDateTime());
      isDateEnd    = boost::posix_time::to_iso_string(insampleSeries.getLastDateTime());
      oosDateStart = boost::posix_time::to_iso_string(outOfSampleSeries.getFirstDateTime());
      oosDateEnd   = boost::posix_time::to_iso_string(outOfSampleSeries.getLastDateTime());
    }
  else
    {
      // For EOD data, use Date methods as before
      isDateStart  = to_iso_string(insampleSeries.getFirstDate());
      isDateEnd    = to_iso_string(insampleSeries.getLastDate());
      oosDateStart = to_iso_string(outOfSampleSeries.getFirstDate());
      oosDateEnd   = to_iso_string(outOfSampleSeries.getLastDate());
    }

  // Write CSV line: Symbol,IRPath,DataPath,FileFormat,ISDateStart,ISDateEnd,OOSDateStart,OOSDateEnd,TimeFrame
  configFile << tickerSymbol << ","
	     << irPath       << ","
	     << dataPath     << ","
	     << fileFormat   << ","
	     << isDateStart  << ","
	     << isDateEnd    << ","
	     << oosDateStart << ","
	     << oosDateEnd   << ","
	     << timeFrame    << std::endl;

  configFile.close();
  std::cout << "Configuration file written: " << configFileName << std::endl;
}

// Generate timeframe directory name based on timeframe and minutes
std::string createTimeFrameDirectoryName(const std::string& timeFrameStr, int intradayMinutes = 0)
{
  if (timeFrameStr == "Intraday") {
    return "Intraday_" + std::to_string(intradayMinutes);
  }
  return timeFrameStr;
}

// Factory for CSV readers using specified time frame
shared_ptr< TimeSeriesCsvReader<Num> >
createTimeSeriesReader(int fileType,
                       const std::string& fileName,
                       const Num& tick,
                       TimeFrame::Duration timeFrame)
{
  switch (fileType)
    {
    case 1:
      return std::make_shared<CSIErrorCheckingFuturesCsvReader<Num>>(fileName,
								     timeFrame,
								     TradingVolume::SHARES,
								     tick);
    case 2:
      return std::make_shared<CSIErrorCheckingExtendedFuturesCsvReader<Num>>(fileName,
									     timeFrame,
									     TradingVolume::SHARES,
									     tick);
    case 3:
      // Use TradeStationFormatCsvReader for intraday, TradeStationErrorCheckingFormatCsvReader for others
      if (timeFrame == TimeFrame::INTRADAY)
        {
	  return std::make_shared<TradeStationFormatCsvReader<Num>>(fileName,
								    timeFrame,
								    TradingVolume::SHARES,
								    tick);
        }
      else
        {
	  return std::make_shared<TradeStationErrorCheckingFormatCsvReader<Num>>(fileName,
										 timeFrame,
										 TradingVolume::SHARES,
										 tick);
        }
    case 4:
      return std::make_shared<PinnacleErrorCheckingFormatCsvReader<Num>>(fileName,
									 timeFrame,
									 TradingVolume::SHARES,
									 tick);
    case 5:
      return std::make_shared<PALFormatCsvReader<Num>>(fileName,
						       timeFrame,
						       TradingVolume::SHARES,
						       tick);
    default:
      throw std::out_of_range("Invalid file type");
    }
}

int main(int argc, char** argv)
{
  // Parse command line arguments for -indicator flag
  bool indicatorMode = false;
  std::vector<std::string> args;
  
  // Separate flag from positional arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = std::string(argv[i]);
    if (arg == "-indicator" || arg == "--indicator") {
      indicatorMode = true;
    } else {
      args.push_back(arg);
    }
  }
  
  if ((args.size() == 2) || (args.size() == 3))
    {
      // Command-line args: datafile, file-type, [securityTick]
      std::string historicDataFileName = args[0];
      int fileType = std::stoi(args[1]);
      Num securityTick(DecimalConstants<Num>::EquityTick);
      if (args.size() == 3)
	securityTick = Num(std::stof(args[2]));

      // 1. Extract default ticker symbol from filename and read ticker symbol
      std::string defaultTicker;
      fs::path filePath(historicDataFileName);
      std::string baseName = filePath.stem().string(); // Gets filename without extension
      size_t dotPos = baseName.find('.');
      if (dotPos != std::string::npos)
        {
	  defaultTicker = baseName.substr(0, dotPos);
        }
      else
        {
	  defaultTicker = baseName;
        }

      std::string tickerSymbol;
      std::cout << "Enter ticker symbol [default " << defaultTicker << "]: ";
      std::getline(std::cin, tickerSymbol);
      if (tickerSymbol.empty())
        {
	  tickerSymbol = defaultTicker;
        }

      // 2. Read and parse time frame (default Daily)
      std::string timeFrameStr;
      TimeFrame::Duration timeFrame;
      bool validFrame = false;
      while (!validFrame)
        {
	  std::cout << "Enter time frame ([D]aily, [W]eekly, [M]onthly, [I]ntraday) [default D]: ";
	  std::string tfInput;
	  std::getline(std::cin, tfInput);
	  if (tfInput.empty()) tfInput = "D";
	  char c = std::toupper(tfInput[0]);
	  switch (c)
            {
	    case 'D': timeFrameStr = "Daily"; validFrame = true; break;
	    case 'W': timeFrameStr = "Weekly"; validFrame = true; break;
	    case 'M': timeFrameStr = "Monthly"; validFrame = true; break;
	    case 'I': timeFrameStr = "Intraday"; validFrame = true; break;
	    default:
	      std::cerr << "Invalid time frame. Please enter D, W, M, or I." << std::endl;
            }
        }
      timeFrame = getTimeFrameFromString(timeFrameStr);

      // Handle intraday minutes input
      int intradayMinutes = 90; // default
      if (timeFrameStr == "Intraday") {
        std::cout << "Enter number of minutes for intraday timeframe (1-1440, default 90): ";
        std::string minutesInput;
        std::getline(std::cin, minutesInput);
        if (!minutesInput.empty()) {
          try {
            intradayMinutes = std::stoi(minutesInput);
            intradayMinutes = std::clamp(intradayMinutes, 1, 1440);
          } catch (...) {
            std::cerr << "Invalid input for minutes. Using default 90." << std::endl;
            intradayMinutes = 90;
          }
        }
      }

      // 3. Handle indicator selection if in indicator mode
      std::string selectedIndicator;
      if (indicatorMode) {
        std::cout << "Select indicator ([I]BS - Internal Bar Strength): ";
        std::string indicatorChoice;
        std::getline(std::cin, indicatorChoice);
        if (indicatorChoice.empty()) indicatorChoice = "I";
        
        char c = std::toupper(indicatorChoice[0]);
        switch (c) {
          case 'I':
            selectedIndicator = "IBS";
            std::cout << "Selected: Internal Bar Strength (IBS)" << std::endl;
            break;
          default:
            std::cerr << "Invalid indicator selection. Defaulting to IBS." << std::endl;
            selectedIndicator = "IBS";
        }
      }

      // 4. Read and parse data split percentages with validation
      double insamplePercent, outOfSamplePercent, reservedPercent;
      bool validPercentages = false;
      
      while (!validPercentages)
        {
          // Get in-sample percentage (default 80%)
          insamplePercent = 60.0;
          std::cout << "Enter percent of data for in-sample (0-100, default 60%): ";
          std::string insamplePercentStr;
          std::getline(std::cin, insamplePercentStr);
          if (!insamplePercentStr.empty())
            {
              try
                {
                  insamplePercent = std::stod(insamplePercentStr);
                }
              catch (...)
                {
                  std::cerr << "Invalid input for in-sample percent. Using default 80%." << std::endl;
                  insamplePercent = 80.0;
                }
            }
          insamplePercent = std::clamp(insamplePercent, 0.0, 100.0);

          // Get out-of-sample percentage
          outOfSamplePercent = 40.0;
          std::cout << "Enter percent of data for out-of-sample (0-100, default 40%): ";
          std::string outOfSamplePercentStr;
          std::getline(std::cin, outOfSamplePercentStr);
          if (!outOfSamplePercentStr.empty())
            {
              try
                {
                  outOfSamplePercent = std::stod(outOfSamplePercentStr);
                }
              catch (...)
                {
                  std::cerr << "Invalid input for out-of-sample percent. Using 0%." << std::endl;
                  outOfSamplePercent = 0.0;
                }
            }
          outOfSamplePercent = std::clamp(outOfSamplePercent, 0.0, 100.0);

          // Get reserved percentage (default 0%)
          reservedPercent = 0.0;
          std::cout << "Enter percent of data to reserve (0-100, default 0%): ";
          std::string reservedPercentStr;
          std::getline(std::cin, reservedPercentStr);
          if (!reservedPercentStr.empty())
            {
              try
                {
                  reservedPercent = std::stod(reservedPercentStr);
                }
              catch (...)
                {
                  std::cerr << "Invalid input for reserved percent. Using default 5%." << std::endl;
                  reservedPercent = 5.0;
                }
            }
          reservedPercent = std::clamp(reservedPercent, 0.0, 100.0);

          // Validate that total doesn't exceed 100%
          double totalPercent = insamplePercent + outOfSamplePercent + reservedPercent;
          if (totalPercent <= 100.0)
            {
              validPercentages = true;
            }
          else
            {
              std::cerr << "Error: Total percentage (" << totalPercent
                        << "%) exceeds 100%. Please enter the percentages again." << std::endl;
            }
        }

      // 4. Prepare output directories with timeframe differentiation
      // Holding period input
      int holdingPeriod = 1;
      std::cout << "Enter holding period (integer, default 1): ";
      std::string holdingPeriodStr;
      std::getline(std::cin, holdingPeriodStr);
      if (!holdingPeriodStr.empty()) {
          try {
              holdingPeriod = std::stoi(holdingPeriodStr);
              holdingPeriod = std::max(1, holdingPeriod);
          } catch (...) {
              std::cerr << "Invalid input for holding period. Using default 1." << std::endl;
              holdingPeriod = 1;
          }
      }
  
      fs::path baseDir = tickerSymbol + "_Validation";
      // Preserve existing directories and files; do not remove baseDir if it already exists
      
      // Create timeframe-specific subdirectory
      std::string timeFrameDirName = createTimeFrameDirectoryName(timeFrameStr, intradayMinutes);
      fs::path timeFrameDir = baseDir / timeFrameDirName;
      // Create Roc<holdingPeriod> subdirectory
      fs::path rocDir = timeFrameDir / ("Roc" + std::to_string(holdingPeriod));
      fs::path palDir = rocDir / "PAL_Files";
      fs::path valDir = rocDir / "Validation_Files";
      fs::create_directories(palDir);
      fs::create_directories(valDir);

      // Create risk-reward subdirectories within validation directory
      fs::path riskReward05Dir = valDir / "Risk_Reward_0_5";
      fs::path riskReward11Dir = valDir / "Risk_Reward_1_1";
      fs::path riskReward21Dir = valDir / "Risk_Reward_2_1";
      fs::create_directories(riskReward05Dir);
      fs::create_directories(riskReward11Dir);
      fs::create_directories(riskReward21Dir);

      // Create 8 subdirectories under palDir for parallel processing
      std::vector<fs::path> palSubDirs;
      for (int i = 1; i <= 8; ++i)
        {
          fs::path subDir = palDir / ("pal_" + std::to_string(i));
          fs::create_directories(subDir);
          palSubDirs.push_back(subDir);
        }

      // 5. Create and read time series
      shared_ptr<TimeSeriesCsvReader<Num>> reader;
      if (fileType >= 1 && fileType <= 5)
        {
	  reader = createTimeSeriesReader(fileType,
					  historicDataFileName,
					  securityTick,
					  timeFrame);
        }
      else
        {
	  throw std::out_of_range("Invalid file type");
        }

      try
        {
	  reader->readFile();
        }
      catch (const TimeSeriesException& e)
        {
	  std::cerr << "ERROR: Data file contains duplicate timestamps." << std::endl;
	  std::cerr << "Details: " << e.what() << std::endl;
	  std::cerr << "Action: Please clean the data file and remove any duplicate entries." << std::endl;
	  std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
	  return 1;
        }
      catch (const TimeSeriesEntryException& e)
        {
	  std::cerr << "ERROR: Data file contains invalid OHLC price relationships." << std::endl;
	  std::cerr << "Details: " << e.what() << std::endl;
	  std::cerr << "Action: Please check and correct the data file for invalid price entries." << std::endl;
	  std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
	  return 1;
        }
      auto aTimeSeries = reader->getTimeSeries();

      // 6. Calculate indicator if in indicator mode
      NumericTimeSeries<Num> indicatorSeries(aTimeSeries->getTimeFrame());
      if (indicatorMode && selectedIndicator == "IBS") {
        std::cout << "Calculating Internal Bar Strength (IBS) indicator..." << std::endl;
        indicatorSeries = IBS1Series(*aTimeSeries);
        std::cout << "IBS calculation complete. Generated " << indicatorSeries.getNumEntries() << " indicator values." << std::endl;
      }

      // 7. Split into insample, out-of-sample, and reserved (last)
      size_t totalSize    = aTimeSeries->getNumEntries();
      size_t insampleSize = static_cast<size_t>(totalSize * (insamplePercent / 100.0));
      size_t oosSize      = static_cast<size_t>(totalSize * (outOfSamplePercent / 100.0));
      size_t reservedSize = static_cast<size_t>(totalSize * (reservedPercent / 100.0));

      OHLCTimeSeries<Num> reservedSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());
      OHLCTimeSeries<Num> insampleSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());
      OHLCTimeSeries<Num> outOfSampleSeries(aTimeSeries->getTimeFrame(), aTimeSeries->getVolumeUnits());

      size_t count = 0;
      for (auto it = aTimeSeries->beginSortedAccess(); it != aTimeSeries->endSortedAccess(); ++it, ++count)
        {
	  const auto& entry = *it;
	  if (count < insampleSize)
	    insampleSeries.addEntry(entry);
	  else if (count < insampleSize + oosSize)
	    outOfSampleSeries.addEntry(entry);
	  else
	    reservedSeries.addEntry(entry);
        }

      // Create insample indicator series if in indicator mode
      NumericTimeSeries<Num> insampleIndicatorSeries(aTimeSeries->getTimeFrame());
      if (indicatorMode && selectedIndicator == "IBS") {
        insampleIndicatorSeries = IBS1Series(insampleSeries);
        std::cout << "Generated " << insampleIndicatorSeries.getNumEntries() << " IBS values for insample data." << std::endl;
      }

      // 8. Insample stop and target calculation using robust asymmetric method
      Num profitTargetValue;
      Num stopValue;
      Num medianOfRoc;
      Num robustQn;
      Num MAD;
      Num StdDev;
      Num skew;

        
      try
        {
	  // Compute asymmetric profit target and stop values
	  auto targetStopPair = ComputeRobustStopAndTargetFromSeries(insampleSeries, holdingPeriod);
	  profitTargetValue = targetStopPair.first;
	  stopValue = targetStopPair.second;

	  // Still compute traditional statistics for reporting
	  NumericTimeSeries<Num> closingPrices(insampleSeries.CloseTimeSeries());
	  NumericTimeSeries<Num> rocOfClosingPrices(RocSeries(closingPrices, holdingPeriod));
	  medianOfRoc = Median(rocOfClosingPrices);
	  robustQn = RobustQn<Num>(rocOfClosingPrices).getRobustQn();
	  MAD = MedianAbsoluteDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
	  StdDev = StandardDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
	  skew = RobustSkewMedcouple(rocOfClosingPrices);

	  if ((robustQn * DecimalConstants<Num>::DecimalTwo) < StdDev)
	    std::cout << "***** Warning Standard Devition is > (2 * Qn) *****" << std::endl;
        }
      catch (const std::domain_error& e)
        {
	  std::cerr << "ERROR: Intraday data contains duplicate timestamps preventing stop calculation." << std::endl;
	  std::cerr << "Details: " << e.what() << std::endl;
	  std::cerr << "Cause: NumericTimeSeries cannot handle multiple intraday bars with identical timestamps." << std::endl;
	  std::cerr << "Action: Clean the intraday data to ensure unique timestamps for each bar." << std::endl;
	  std::cerr << "Note: Pass the above details to your broker's data cleaning team." << std::endl;
	  return 1;
        }

      Num half(DecimalConstants<Num>::createDecimal("0.5"));
      
      // 8. Generate target/stop files and data files for each subdirectory
      for (const auto& currentPalDir : palSubDirs)
        {
          // Generate target/stop files in current subdirectory using asymmetric values
          std::ofstream tsFile1((currentPalDir / (tickerSymbol + "_0_5_.TRS")).string());
          tsFile1 << (profitTargetValue * half) << std::endl << stopValue << std::endl;
          tsFile1.close();

          std::ofstream tsFile2((currentPalDir / (tickerSymbol + "_1_0_.TRS")).string());
          tsFile2 << profitTargetValue << std::endl << stopValue << std::endl;
          tsFile2.close();

          std::ofstream tsFile3((currentPalDir / (tickerSymbol + "_2_0_.TRS")).string());
          tsFile3 << (profitTargetValue * DecimalConstants<Num>::DecimalTwo) << std::endl << stopValue << std::endl;
          tsFile3.close();

          // Write data files to current subdirectory
          if (indicatorMode) {
            // Write indicator-based PAL files
            if (timeFrameStr == "Intraday") {
              PalIndicatorIntradayCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries, insampleIndicatorSeries);
              insampleWriter.writeFile();
            } else {
              PalIndicatorEodCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries, insampleIndicatorSeries);
              insampleWriter.writeFile();
            }
          } else {
            // Write standard OHLC PAL files
            if (timeFrameStr == "Intraday") {
              // For intraday: insample uses PAL intraday format
              PalIntradayCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
              insampleWriter.writeFile();
            } else {
              // For non-intraday: all files use standard PAL EOD format
              PalTimeSeriesCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
              insampleWriter.writeFile();
            }
          }
        }

      // 9. Write validation files with risk-reward segregation
      // Write ALL.txt files to each risk-reward subdirectory
      std::vector<fs::path> riskRewardDirs = {riskReward05Dir, riskReward11Dir, riskReward21Dir};
      
      for (const auto& rrDir : riskRewardDirs)
        {
          if (timeFrameStr == "Intraday")
            {
              TradeStationIntradayCsvWriter<Num> allWriter((rrDir / (tickerSymbol + "_ALL.txt")).string(), *aTimeSeries);
              allWriter.writeFile();
            }
          else
            {
              PalTimeSeriesCsvWriter<Num> allWriter((rrDir / (tickerSymbol + "_ALL.txt")).string(), *aTimeSeries);
              allWriter.writeFile();
            }
          
          // Write config file to each risk-reward subdirectory
          writeConfigFile(rrDir.string(), tickerSymbol, insampleSeries, outOfSampleSeries, timeFrameStr);
        }

      // Write OOS and reserved files to main validation directory
      if (timeFrameStr == "Intraday")
        {
          TradeStationIntradayCsvWriter<Num> oosWriter((valDir / (tickerSymbol + "_OOS.txt")).string(), outOfSampleSeries);
          oosWriter.writeFile();
          if (reservedSize > 0)
            {
              TradeStationIntradayCsvWriter<Num> reservedWriter((valDir / (tickerSymbol + "_reserved.txt")).string(), reservedSeries);
              reservedWriter.writeFile();
            }
        }
      else
        {
          PalTimeSeriesCsvWriter<Num> oosWriter((valDir / (tickerSymbol + "_OOS.txt")).string(), outOfSampleSeries);
          oosWriter.writeFile();
          if (reservedSize > 0)
            {
              PalTimeSeriesCsvWriter<Num> reservedWriter((valDir / (tickerSymbol + "_reserved.txt")).string(), reservedSeries);
              reservedWriter.writeFile();
            }
        }

      // 10. Output statistics
      std::cout << "In-sample% = " << insamplePercent << "%\n";
      std::cout << "Out-of-sample% = " << outOfSamplePercent << "%\n";
      std::cout << "Reserved% = " << reservedPercent << "%\n";
      std::cout << "Median = " << medianOfRoc << std::endl;
      std::cout << "Qn  = " << robustQn << std::endl;
      std::cout << "MAD = " << MAD << std::endl;
      std::cout << "Std = " << StdDev << std::endl;
      std::cout << "Profit Target = " << profitTargetValue << std::endl;
      std::cout << "Stop = " << stopValue << std::endl;
      std::cout << "Skew = " << skew << std::endl;

      fs::path detailsFilePath = valDir / (tickerSymbol + "_Palsetup_Details.txt");
      std::ofstream detailsFile(detailsFilePath);
      if (!detailsFile.is_open())
	{
	  std::cerr << "Error: Unable to open details file " << detailsFilePath << std::endl;
	}
      else
	{
	  detailsFile << "In-sample% = " << insamplePercent << "%\n";
	  detailsFile << "Out-of-sample% = " << outOfSamplePercent << "%\n";
	  detailsFile << "Reserved% = " << reservedPercent << "%\n";
	  detailsFile << "Median = " << medianOfRoc << std::endl;
	  detailsFile << "Qn  = " << robustQn << std::endl;
	  detailsFile << "MAD = " << MAD << std::endl;
	  detailsFile << "Std = " << StdDev << std::endl;
	  detailsFile << "Profit Target = " << profitTargetValue << std::endl;
	  detailsFile << "Stop = " << stopValue << std::endl;
	  detailsFile << "Skew = " << skew << std::endl;
	  detailsFile.close();
	}
    }
  else
    {
      std::cout << "Usage (beta):: PalSetup [-indicator|--indicator] datafile file-type (1=CSI,2=CSI Ext,3=TradeStation,4=Pinnacle,5=PAL) [tick]" << std::endl;
      std::cout << "  -indicator|--indicator: Use indicator values (e.g., IBS) instead of close prices in PAL files" << std::endl;
    }
  return 0;
}
