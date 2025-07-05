#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>        // for ofstream
#include <filesystem>     // for directory operations
#include <limits>         // for numeric_limits
#include <cctype>         // for std::toupper
#include <boost/date_time/gregorian/gregorian.hpp>  // for to_iso_string
#include <boost/date_time/posix_time/posix_time.hpp>  // for ptime formatting

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
  if ((argc == 3) || (argc == 4))
    {
      // Command-line args: datafile, file-type, [securityTick]
      std::string historicDataFileName = argv[1];
      int fileType = std::stoi(argv[2]);
      Num securityTick(DecimalConstants<Num>::EquityTick);
      if (argc == 4)
	securityTick = Num(std::stof(argv[3]));

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

      // 3. Read and parse reserve percentage (default 5%)
      double reservedPercent = 5.0;
      std::cout << "Enter percent of data to reserve (0-100, default 5): ";
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

      // 4. Prepare output directories with timeframe differentiation
      fs::path baseDir = tickerSymbol + "_Validation";
      if (fs::exists(baseDir))
 fs::remove_all(baseDir);
      
      // Create timeframe-specific subdirectory
      std::string timeFrameDirName = createTimeFrameDirectoryName(timeFrameStr, intradayMinutes);
      fs::path timeFrameDir = baseDir / timeFrameDirName;
      fs::path palDir = timeFrameDir / "PAL_Files";
      fs::path valDir = timeFrameDir / "Validation_Files";
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

      // 6. Split into insample, out-of-sample, and reserved (last)
      size_t totalSize    = aTimeSeries->getNumEntries();
      size_t reservedSize = static_cast<size_t>(totalSize * (reservedPercent / 100.0));
      size_t remaining    = totalSize - reservedSize;
      size_t insampleSize = static_cast<size_t>(remaining * 0.8);
      size_t oosSize      = remaining - insampleSize;

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

      // 7. Insample stop calculation
      Num stopValue;
      Num medianOfRoc;
      Num robustQn;
      Num MAD;
      Num StdDev;
        
      try
        {
	  NumericTimeSeries<Num> closingPrices(insampleSeries.CloseTimeSeries());
	  NumericTimeSeries<Num> rocOfClosingPrices(RocSeries(closingPrices, 1));
	  medianOfRoc = Median(rocOfClosingPrices);
	  robustQn = RobustQn<Num>(rocOfClosingPrices).getRobustQn();
	  MAD = MedianAbsoluteDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
	  StdDev = StandardDeviation<Num>(rocOfClosingPrices.getTimeSeriesAsVector());
	  stopValue = medianOfRoc + robustQn;
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
          // Generate target/stop files in current subdirectory
          std::ofstream tsFile1((currentPalDir / (tickerSymbol + "_0_5_.TRS")).string());
          tsFile1 << (stopValue * half) << std::endl << stopValue << std::endl;
          tsFile1.close();

          std::ofstream tsFile2((currentPalDir / (tickerSymbol + "_1_0_.TRS")).string());
          tsFile2 << stopValue << std::endl << stopValue << std::endl;
          tsFile2.close();

          std::ofstream tsFile3((currentPalDir / (tickerSymbol + "_2_0_.TRS")).string());
          tsFile3 << (stopValue * DecimalConstants<Num>::DecimalTwo) << std::endl << stopValue << std::endl;
          tsFile3.close();

          // Write data files to current subdirectory
          if (timeFrameStr == "Intraday")
            {
              // For intraday: insample uses PAL intraday format
              PalIntradayCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
              insampleWriter.writeFile();
            }
          else
            {
              // For non-intraday: all files use standard PAL EOD format
              PalTimeSeriesCsvWriter<Num> insampleWriter((currentPalDir / (tickerSymbol + "_IS.txt")).string(), insampleSeries);
              insampleWriter.writeFile();
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
      std::cout << "Reserved% = " << reservedPercent << "%\n";
      std::cout << "Median = " << medianOfRoc << std::endl;
      std::cout << "Qn  = " << robustQn << std::endl;
      std::cout << "MAD = " << MAD << std::endl;
      std::cout << "Std = " << StdDev << std::endl;
      std::cout << "Stop = " << stopValue << std::endl;

      // Configuration files are now written to each risk-reward subdirectory above

    }
  else
    {
      std::cout << "Usage (beta):: PalSetup datafile file-type (1=CSI,2=CSI Ext,3=TradeStation,4=Pinnacle,5=PAL) [tick]" << std::endl;
    }
  return 0;
}
