UnitTest: All
	./MCPTStrategyAttributesTest
	./LogPalPatternTest
	./RobustnessTesterUnitTest
	./RobustnessTestUnitTest
	./PALMonteCarloValidationTest
	./McptConfigurationFileReaderTest
	./SecurityAttributesFactoryTest
	./SecurityAttributesTest
	./MonteCarloPermutationTest2
	./BackTesterTest
	./PalStrategyTest
	./SecurityBacktestPropertiesTest
	./PALPatternInterpreterTest
	./StrategyBrokerTest
	./StrategyTransactionManagerTest
	./StrategyTransactionTest
	./TradingOrderManagerTest
	./PortfolioTest
	./SecurityTest
	./InstrumentPositionManagerTest
	./InstrumentPositionTest
	./ProcessStopOrderTest
	./ProcessLimitOrderTest
	./BoostDateHelperTest
	./ProcessMarketOrderTest
	./StopOrderTest
	./LimitOrderTest
	./TradingPositionTest
	./MarketOrderTest
	./ClosedPositionHistoryTest
	./OpenPositionTest
	./OpenPositionHistoryTest
	./OpenPositionBarTest
	./StopLossTest
	./ProfitTargetTest
	./PercentNumberTest
	./TimeSeriesCsvReaderTest
	./TimeSeriesCsvWriterTest
	./SyntheticTimeSeriesTest
	./TimeSeriesTest
	./TimeSeriesEntryTest
	./SyntheticTimeSeriesCreatorTest
	./TimeSeriesValidatorTest

All: TimeSeriesValidatorTest SyntheticTimeSeriesCreatorTest LogPalPatternTest MCPTStrategyAttributesTest TimeSeriesTest RobustnessTesterUnitTest RobustnessTestUnitTest 

# TODO:
#All: MCPTStrategyAttributesTest LogPalPatternTest RobustnessTesterUnitTest RobustnessTestUnitTest PALMonteCarloValidationTest McptConfigurationFileReaderTest SecurityAttributesFactoryTest SecurityAttributesTest MonteCarloPermutationTest2 BackTesterTest PalStrategyTest SecurityBacktestPropertiesTest PALPatternInterpreterTest StrategyBrokerTest StrategyTransactionManagerTest StrategyTransactionTest TradingOrderManagerTest PortfolioTest SecurityTest InstrumentPositionManagerTest InstrumentPositionTest ProcessStopOrderTest ProcessLimitOrderTest BoostDateHelperTest ProcessMarketOrderTest StopOrderTest LimitOrderTest TradingPositionTest MarketOrderTest ClosedPositionHistoryTest OpenPositionTest OpenPositionHistoryTest OpenPositionBarTest StopLossTest ProfitTargetTest PercentNumberTest TimeSeriesCsvReaderTest TimeSeriesCsvWriterTest SyntheticTimeSeriesTest TimeSeriesTest TimeSeriesEntryTest TradingVolumeTest

BACKTESTERSTRATEGY_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/BacktesterStrategy.cpp.o
TIMESERIES_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/TimeSeries.cpp.o
TIMESERIES_ENTRY_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/TimeSeriesEntry.cpp.o
LOG_PAL_PATTERN_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/LogPalPattern.cpp.o
RUNNER_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/runner.cpp.o
MCPT_CONF_READER_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/McptConfigurationFileReader.cpp.o

PRICEACTION_LIB_DIR = ../../../build/libs/PALCodeGenLibrary-v2
PRICEACTION_INC_DIR = ../../PALCodeGenLibrary-v2
TIMESERIES_INC_DIR = ../

TradingVolumeTest: TradingVolumeTest.o
	g++ -o TradingVolumeTest  $< ${TIMESERIES_O} -lboost_system

MCPTStrategyAttributesTest: MCPTStrategyAttributesTest.o TestUtils.o
	g++ -o MCPTStrategyAttributesTest ${TIMESERIES_O} $< TestUtils.o -lboost_date_time -lpthread  -lboost_system

LogPalPatternTest: LogPalPatternTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o LogPalPatternTest  $< TestUtils.o ${LOG_PAL_PATTERN_O} ${TIMESERIES_ENTRY_O} -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system

RobustnessTesterUnitTest: RobustnessTesterUnitTest.o TestUtils.o ${TIMESERIES_ENTRY_O} ${BACKTESTERSTRATEGY_O}
	g++ -o RobustnessTesterUnitTest  $< ${RUNNER_O} ${TIMESERIES_O} TestUtils.o ${TIMESERIES_ENTRY_O} ${BACKTESTERSTRATEGY_O} ${LOG_PAL_PATTERN_O}  -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpthread  -lboost_thread -lpriceaction2 -lboost_system

RobustnessTestUnitTest: RobustnessTestUnitTest.o  TestUtils.o ${TIMESERIES_ENTRY_O} ${BACKTESTERSTRATEGY_O}
	g++ -o RobustnessTestUnitTest  $< ${TIMESERIES_O} ${BACKTESTERSTRATEGY_O}  TestUtils.o ${TIMESERIES_ENTRY_O} -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

PALMonteCarloValidationTest: PALMonteCarloValidationTest.o ${MCPT_CONF_READER_O}
	g++ -o PALMonteCarloValidationTest  -flto $< ${TIMESERIES_O} -L${PRICEACTION_LIB_DIR} ${MCPT_CONF_READER_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lelkhound -lsmbase -lboost_filesystem -lboost_system

McptConfigurationFileReaderTest: McptConfigurationFileReaderTest.o ${MCPT_CONF_READER_O} ../TimeFrameUtility.o
	g++ -o McptConfigurationFileReaderTest  $< ${CMPT_CONF_READER_O} ../TimeFrameUtility.o ${TIMESERIES_O} -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lelkhound -lsmbase -lboost_filesystem -lboost_system

SecurityAttributesFactoryTest: SecurityAttributesFactoryTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o SecurityAttributesFactoryTest $< ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

SecurityAttributesTest: SecurityAttributesTest.o  TestUtils.o ${TIMESERIES_ENTRY_O} ${TIMESERIES_O}
	g++ -o SecurityAttributesTest  $<  ${TIMESERIES_O} TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

MonteCarloPermutationTest2: MonteCarloPermutationTest2.o TestUtils.o
	g++ -o MonteCarloPermutationTest2  $< ${TIMESERIES_O} TestUtils.o ${BACKTESTERSTRATEGY_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_thread -lboost_system

BackTesterTest: BackTesterTest.o TestUtils.o ${TIMESERIES_ENTRY_O} ${BACKTESTERSTRATEGY_O}
	g++ -o BackTesterTest  $< ${TIMESERIES_O} TestUtils.o ${TIMESERIES_ENTRY_O} ${BACKTESTERSTRATEGY_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

PalStrategyTest: PalStrategyTest.o TestUtils.o ${TIMESERIES_ENTRY_O} ${BACKTESTERSTRATEGY_O}
	g++ -o PalStrategyTest  $< TestUtils.o ${TIMESERIES_ENTRY_O} ${TIMESERIES_O} ${BACKTESTERSTRATEGY_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

SecurityBacktestPropertiesTest: SecurityBacktestPropertiesTest.o
	g++ -o SecurityBacktestPropertiesTest ${TIMESERIES_O} $< -lboost_date_time -lpthread -lboost_system

PALPatternInterpreterTest: PALPatternInterpreterTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o PALPatternInterpreterTest ${TIMESERIES_O} $< ${TIMESERIES_ENTRY_O} TestUtils.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

StrategyBrokerTest: StrategyBrokerTest.o TestUtils.o ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O}
	g++ -o StrategyBrokerTest ${TIMESERIES_O} $<  ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lpthread  -lboost_system

StrategyTransactionManagerTest: StrategyTransactionManagerTest.o  ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o
	g++ -o StrategyTransactionManagerTest $<  ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

StrategyTransactionTest: StrategyTransactionTest.o TestUtils.o ${TIMESERIES_ENTRY_O} ${TIMESERIES_O}
	g++ -o StrategyTransactionTest  $< ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

TradingOrderManagerTest: TradingOrderManagerTest.o TestUtils.o ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O}
	g++ -o TradingOrderManagerTest  $< ${TIMESERIES_O} TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

PortfolioTest: PortfolioTest.o TestUtils.o ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O}
	g++ -o PortfolioTest $< TestUtils.o ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

SecurityTest: SecurityTest.o TestUtils.o ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O}
	g++ -o SecurityTest $< TestUtils.o ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

InstrumentPositionManagerTest: InstrumentPositionManagerTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o InstrumentPositionManagerTest $< ${TIMESERIES_O} TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lpthread  -lboost_system

InstrumentPositionTest: InstrumentPositionTest.o  TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o InstrumentPositionTest $< TestUtils.o   ${TIMESERIES_ENTRY_O} -lboost_date_time -lpthread  -lboost_system

ProcessStopOrderTest: ProcessStopOrderTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o ProcessStopOrderTest $< TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

ProcessLimitOrderTest: ProcessLimitOrderTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o ProcessLimitOrderTest $<  TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

BoostDateHelperTest: BoostDateHelperTest.o  TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o BoostDateHelperTest $<   TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

ProcessMarketOrderTest: ProcessMarketOrderTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o ProcessMarketOrderTest $<  TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

StopOrderTest: StopOrderTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o StopOrderTest $< TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

LimitOrderTest: LimitOrderTest.o TestUtils.o   ${TIMESERIES_ENTRY_O}
	g++ -o LimitOrderTest $<  ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

TradingPositionTest: TradingPositionTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o TradingPositionTest $< ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

MarketOrderTest: MarketOrderTest.o TestUtils.o  ${TIMESERIES_ENTRY_O}
	g++ -o MarketOrderTest -std=c++14 $<   ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

ClosedPositionHistoryTest: ClosedPositionHistoryTest.o TestUtils.o   ${TIMESERIES_ENTRY_O}
	g++ -o ClosedPositionHistoryTest $<   ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system -lpthread

OpenPositionTest: OpenPositionTest.o TestUtils.o  ${TIMESERIES_ENTRY_O}
	g++ -o OpenPositionTest $<  ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

OpenPositionHistoryTest: OpenPositionHistoryTest.o  TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o OpenPositionHistoryTest $<   TestUtils.o ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

OpenPositionBarTest: OpenPositionBarTest.o TestUtils.o  ${TIMESERIES_ENTRY_O}
	g++ -o OpenPositionBarTest $<  ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

StopLossTest: StopLossTest.o TestUtils.o   ${TIMESERIES_ENTRY_O}
	g++ -o StopLossTest $<   ${TIMESERIES_ENTRY_O} TestUtils.o  -lboost_date_time -lboost_system

ProfitTargetTest: ProfitTargetTest.o TestUtils.o   ${TIMESERIES_ENTRY_O}
	g++ -o ProfitTargetTest $< ${TIMESERIES_ENTRY_O} TestUtils.o  -lboost_date_time -lboost_system

PercentNumberTest: PercentNumberTest.o TestUtils.o   ${TIMESERIES_ENTRY_O}
	g++ -o PercentNumberTest $< TestUtils.o  ${TIMESERIES_ENTRY_O} -lboost_date_time -lboost_system

TimeSeriesCsvReaderTest: TimeSeriesCsvReaderTest.o ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o
	g++ -o TimeSeriesCsvReaderTest $< ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lpthread -lboost_system

TimeSeriesCsvWriterTest: TimeSeriesCsvWriterTest.o  ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o
	g++ -o TimeSeriesCsvWriterTest $<  ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lpthread -lboost_system

SyntheticTimeSeriesTest: SyntheticTimeSeriesTest.o ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o
	g++ -o SyntheticTimeSeriesTest $< ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_random -lpthread -lboost_system

TimeSeriesTest: TimeSeriesTest.o TestUtils.o  ${TIMESERIES_O} ${TIMESERIES_ENTRY_O}
	g++ -o TimeSeriesTest $< ${TIMESERIES_O}  ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lpthread -lboost_system

TimeSeriesEntryTest: TimeSeriesEntryTest.o TestUtils.o ${TIMESERIES_ENTRY_O}
	g++ -o TimeSeriesEntryTest $< ${TIMESERIES_ENTRY_O} TestUtils.o -lboost_date_time -lboost_system

DataSourceReaderTest: DataSourceReaderTest.o
	g++ -o DataSourceReaderTest $< -lcurl -ljsoncpp -lboost_date_time -lboost_system

SyntheticTimeSeriesCreatorTest: SyntheticTimeSeriesCreatorTest.o TestUtils.o
	g++ -o SyntheticTimeSeriesCreatorTest $< ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -L/usr/local/lib -lboost_date_time -lpthread -lboost_system

BarchartDataSourceReaderTest: BarchartDataSourceReaderTest.o TestUtils.o 
	g++ -o BarchartDataSourceReaderTest $< ${TIMESERIES_O} TestUtils.o -L/usr/local/lib -lboost_date_time -lcurl -lpthread -lboost_system

TimeSeriesValidatorTest: TimeSeriesValidatorTest.o TestUtils.o 
	g++ -o TimeSeriesValidatorTest $<  ${TIMESERIES_O} ${TIMESERIES_ENTRY_O} TestUtils.o -L/usr/local/lib -lboost_date_time -lpthread -lboost_system

TestUtils.o: ../PercentNumber.h ../DecimalConstants.h ../TimeFrame.h  ../TimeSeriesEntry.h ../TradingVolume.h

DataSourceReaderTest.o: ../DataSourceReader.h ../DateRange.h ../BoostDateHelper.h
TradingVolumeTest.o: ../TradingVolume.h catch.hpp
MCPTStrategyAttributesTest.o: ../MCPTStrategyAttributes.h  catch.hpp
LogPalPatternTest.o: ../LogPalPattern.h  catch.hpp
RobustnessTesterUnitTest.o: ../RobustnessTester.h ../RobustnessTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h ../decimal.h  catch.hpp
RobustnessTestUnitTest.o: ../RobustnessTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h  catch.hpp
PALMonteCarloValidationTest.o: ../PALMonteCarloValidation.h  ../McptConfigurationFileReader.h ../DateRange.h ../Security.h ../TimeSeries.h ../TimeSeriesEntry.h ../BackTester.h ../DecimalConstants.h  catch.hpp
McptConfigurationFileReaderTest.o: ../McptConfigurationFileReader.h ../DateRange.h ../BoostDateHelper.h ../Security.h ../TimeSeriesCsvReader.h ../SecurityAttributesFactory.h ../TimeFrameUtility.h  catch.hpp
SecurityAttributesFactoryTest.o: ../SecurityAttributes.h ../SecurityAttributesFactory.h ../decimal.h  catch.hpp
MonteCarloPermutationTest.o: ../MonteCarloPermutationTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h  catch.hpp
MonteCarloPermutationTest2.o: ../MonteCarloPermutationTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h  catch.hpp
BackTesterTest.o: ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h  catch.hpp
PalStrategyTest.o: ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h  catch.hpp
SecurityBacktestPropertiesTest.o: ../SecurityBacktestProperties.h  catch.hpp
PALPatternInterpreterTest.o: ../PALPatternInterpreter.h ../Security.h ../TimeSeries.h PALPatternInterpreterTest.cpp  catch.hpp
StrategyBrokerTest.o: ../StrategyBroker.h ../TradingOrder.h ../TradingPosition.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../InstrumentPositionManager.h ../TradingOrderManager.h ../Portfolio.h ../StopLoss.h ../ProfitTarget.h  catch.hpp
StrategyTransactionManagerTest.o: ../TradingOrder.h ../TradingPosition.h ../TimeSeriesEntry.h ../StrategyTransaction.h ../StrategyTransactionManager.h  catch.hpp
StrategyTransactionTest.o: ../TradingOrder.h ../TradingPosition.h ../TimeSeriesEntry.h ../StrategyTransaction.h  catch.hpp
TradingOrderManagerTest.o: ../TradingOrderManager.h ../TradingOrder.h ../TimeSeriesEntry.h  catch.hpp
PortfolioTest.o: ../Portfolio.h ../Security.h ../TimeSeries.h ../DecimalConstants.h ../TimeSeriesEntry.h  catch.hpp
SecurityAttributesTest.o: ../Security.h ../TimeSeries.h ../DecimalConstants.h ../TimeSeriesEntry.h ../SecurityAttributes.h  catch.hpp
InstrumentPositionManagerTest.o: ../TimeSeriesEntry.h ../InstrumentPosition.h ../TradingPosition.h ../InstrumentPositionManager.h ../decimal.h  catch.hpp
SecurityTest.o: ../Security.h ../TimeSeries.h ../DecimalConstants.h ../TimeSeriesEntry.h  catch.hpp
InstrumentPositionManagerTest.o: ../TimeSeriesEntry.h ../InstrumentPosition.h ../TradingPosition.h ../InstrumentPositionManager.h ../decimal.h  catch.hpp
InstrumentPositionTest.o: ../TimeSeriesEntry.h ../InstrumentPosition.h ../TradingPosition.h ../decimal.h  catch.hpp
ProcessStopOrderTest.o:../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderManager.h ../TradingOrderException.h ../decimal.h  catch.hpp
BoostDateHelperTest.o: ../BoostDateHelper.h  catch.hpp
ProcessLimitOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderManager.h ../TradingOrderException.h ../decimal.h  catch.hpp
ProcessMarketOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderManager.h ../TradingOrderException.h ../decimal.h  catch.hpp
MarketOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderException.h ../decimal.h  catch.hpp
StopOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderException.h ../decimal.h  catch.hpp
LimitOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderException.h ../decimal.h  catch.hpp
TradingPositionTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h  catch.hpp
ClosedPositionHistoryTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h ../PercentNumber.h ../ClosedPositionHistory.h  catch.hpp
ClosedPositionTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h ../PercentNumber.h  catch.hpp
OpenPositionTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h ../PercentNumber.h  catch.hpp
OpenPositionHistoryTest.o: ../TradingPosition.h ../decimal.h ../PercentNumber.h ../TimeSeriesEntry.h  catch.hpp
OpenPositionBarTest.o: ../TradingPosition.h ../decimal.h ../TimeSeriesEntry.h  catch.hpp
StopLossTest.o: ../StopLoss.h ../decimal.h ../PercentNumber.h  catch.hpp
ProfitTargetTest.o: ../ProfitTarget.h ../decimal.h ../PercentNumber.h  catch.hpp
PercentNumberTest.o: ../PercentNumber.h ../decimal.h  catch.hpp
SyntheticTimeSeriesTest.o: ../TimeSeriesEntry.h ../TimeSeries.h ../SyntheticTimeSeries.h ../decimal.h ../RandomMersenne.h  catch.hpp
TimeSeriesTest.o: ../TimeSeriesEntry.h ../TimeSeries.h ../TimeSeriesIndicators.h ../decimal.h  catch.hpp
TimeSeriesEntryTest.o: ../TimeSeriesEntry.h ../decimal.h  catch.hpp
TimeSeriesCsvReaderTest.o: ../TimeSeriesCsvReader.h ../TimeSeries.h ../TimeSeriesEntry.h ../decimal.h ../csv.h  catch.hpp
TimeSeriesCsvWriterTest.o: ../TimeSeriesCsvWriter.h ../TimeSeries.h ../TimeSeriesEntry.h ../decimal.h ../csv.h TimeSeriesCsvWriterTest.cpp  catch.hpp
SyntheticTimeSeriesCreatorTest.o: ../TimeSeriesCsvWriter.h ../TimeSeriesCsvReader.h ../TimeSeries.h ../decimal.h ../csv.h  catch.hpp
BarchartDataSourceReaderTest.o: ../decimal.h ../DateRange.h  catch.hpp
TimeSeriesValidatorTest.o: ../TimeSeriesCsvReader.h ../TimeSeries.h ../decimal.h ../csv.h  catch.hpp

clean:
	rm *.o
	rm -f ClosedPositionHistory TimeSeriesTest TimeSeriesEntryTest SyntheticTimeSeriesTest
	rm -f TimeSeriesCsvReaderTest PercentNumberTest ProfitTargetTest StopLossTest
	rm -f OpenPositionBarTest OpenPositionHistoryTest OpenPositionTest
	rm -f ClosedPositionTest SyntheticTimeSeriesCreatorTest BarchartDataSourceReaderTest
	rm -f TimeSeriesValidatorTest
	rm *~

CCFLAGS := -O2 -c -std=c++14 -I../ -I${PRICEACTION_INC_DIR}
#CCFLAGS := -g -c -std=c++14 -I../../PALCodeGenLibrary -I../../Decimal

%.o: %.cpp
	g++ $(CCFLAGS) $<
