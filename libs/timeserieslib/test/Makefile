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

All: SyntheticTimeSeriesCreatorTest LogPalPatternTest MCPTStrategyAttributesTest TimeSeriesTest RobustnessTesterUnitTest RobustnessTestUnitTest 

# TODO:
#All: MCPTStrategyAttributesTest LogPalPatternTest RobustnessTesterUnitTest RobustnessTestUnitTest PALMonteCarloValidationTest McptConfigurationFileReaderTest SecurityAttributesFactoryTest SecurityAttributesTest MonteCarloPermutationTest2 BackTesterTest PalStrategyTest SecurityBacktestPropertiesTest PALPatternInterpreterTest StrategyBrokerTest StrategyTransactionManagerTest StrategyTransactionTest TradingOrderManagerTest PortfolioTest SecurityTest InstrumentPositionManagerTest InstrumentPositionTest ProcessStopOrderTest ProcessLimitOrderTest BoostDateHelperTest ProcessMarketOrderTest StopOrderTest LimitOrderTest TradingPositionTest MarketOrderTest ClosedPositionHistoryTest OpenPositionTest OpenPositionHistoryTest OpenPositionBarTest StopLossTest ProfitTargetTest PercentNumberTest TimeSeriesCsvReaderTest TimeSeriesCsvWriterTest SyntheticTimeSeriesTest TimeSeriesTest TimeSeriesEntryTest TradingVolumeTest

BACKTESTERSTRATEGY_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/BacktesterStrategy.cpp.o
TIMESERIES_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/TimeSeries.cpp.o
LOG_PAL_PATTERN_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/LogPalPattern.cpp.o
RUNNER_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/runner.cpp.o
MCPT_CONF_READER_O = ../../../build/libs/timeserieslib/CMakeFiles/backtest.dir/McptConfigurationFileReader.cpp.o

PRICEACTION_LIB_DIR = ../../../build/libs/PALCodeGenLibrary-v2
PRICEACTION_INC_DIR = ../../PALCodeGenLibrary-v2
TIMESERIES_INC_DIR = ../

TradingVolumeTest: TradingVolumeTest.o
	g++ -o TradingVolumeTest ${TIMESERIES_O} $<

MCPTStrategyAttributesTest: MCPTStrategyAttributesTest.o TestUtils.o
	g++ -o MCPTStrategyAttributesTest ${TIMESERIES_O} $< TestUtils.o -lboost_date_time -lpthread  -lboost_system

LogPalPatternTest: LogPalPatternTest.o
	g++ -o LogPalPatternTest  $< ${LOG_PAL_PATTERN_O} -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lpriceaction2

RobustnessTesterUnitTest: RobustnessTesterUnitTest.o
	g++ -o RobustnessTesterUnitTest  $< ${TIMESERIES_O} ${LOG_PAL_PATTERN_O} ${RUNNER_O} -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpthread  -lboost_thread -lpriceaction2 -lboost_system

RobustnessTestUnitTest: RobustnessTestUnitTest.o
	g++ -o RobustnessTestUnitTest  $< ${TIMESERIES_O}  -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

PALMonteCarloValidationTest: PALMonteCarloValidationTest.o ${MCPT_CONF_READER_O}
	g++ -o PALMonteCarloValidationTest  -flto $< ${TIMESERIES_O} -L${PRICEACTION_LIB_DIR} ${MCPT_CONF_READER_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lelkhound -lsmbase -lboost_filesystem -lboost_system

McptConfigurationFileReaderTest: McptConfigurationFileReaderTest.o ${MCPT_CONF_READER_O} ../TimeFrameUtility.o
	g++ -o McptConfigurationFileReaderTest  $< ${CMPT_CONF_READER_O} ../TimeFrameUtility.o ${TIMESERIES_O} -L${PRICEACTION_LIB_DIR} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lelkhound -lsmbase -lboost_filesystem -lboost_system

SecurityAttributesFactoryTest: SecurityAttributesFactoryTest.o
	g++ -o SecurityAttributesFactoryTest $< -lboost_date_time

SecurityAttributesTest: SecurityAttributesTest.o
	g++ -o SecurityAttributesTest ${TIMESERIES_O} ${MCPT_CONF_READER_O} $< -lboost_date_time

MonteCarloPermutationTest2: MonteCarloPermutationTest2.o TestUtils.o
	g++ -o MonteCarloPermutationTest2  $< ${TIMESERIES_O} TestUtils.o ${BACKTESTERSTRATEGY_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_thread -lboost_system

BackTesterTest: BackTesterTest.o
	g++ -o BackTesterTest  $< ${TIMESERIES_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

PalStrategyTest: PalStrategyTest.o TestUtils.o
	g++ -o PalStrategyTest  $< TestUtils.o ${TIMESERIES_O} ${BACKTESTERSTRATEGY_O} -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

SecurityBacktestPropertiesTest: SecurityBacktestPropertiesTest.o
	g++ -o SecurityBacktestPropertiesTest ${TIMESERIES_O} $< -lboost_date_time -lpthread

PALPatternInterpreterTest: PALPatternInterpreterTest.o
	g++ -o PALPatternInterpreterTest ${TIMESERIES_O} $< -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lboost_system

StrategyBrokerTest: StrategyBrokerTest.o TestUtils.o
	g++ -o StrategyBrokerTest ${TIMESERIES_O} $< TestUtils.o -lboost_date_time -lpthread  -lboost_system

StrategyTransactionManagerTest: StrategyTransactionManagerTest.o
	g++ -o StrategyTransactionManagerTest ${TIMESERIES_O} $< -lboost_date_time

StrategyTransactionTest: StrategyTransactionTest.o
	g++ -o StrategyTransactionTest ${TIMESERIES_O} $< -lboost_date_time

TradingOrderManagerTest: TradingOrderManagerTest.o
	g++ -o TradingOrderManagerTest ${TIMESERIES_O} $< -lboost_date_time

PortfolioTest: PortfolioTest.o
	g++ -o PortfolioTest ${TIMESERIES_O} $< -lboost_date_time

SecurityTest: SecurityTest.o
	g++ -o SecurityTest ${TIMESERIES_O} $< -lboost_date_time

InstrumentPositionManagerTest: InstrumentPositionManagerTest.o TestUtils.o
	g++ -o InstrumentPositionManagerTest ${TIMESERIES_O} $< TestUtils.o -lboost_date_time -lpthread  -lboost_system

InstrumentPositionTest: InstrumentPositionTest.o  TestUtils.o
	g++ -o InstrumentPositionTest $< TestUtils.o -lboost_date_time -lpthread  -lboost_system

ProcessStopOrderTest: ProcessStopOrderTest.o
	g++ -o ProcessStopOrderTest $<  -lboost_date_time

ProcessLimitOrderTest: ProcessLimitOrderTest.o
	g++ -o ProcessLimitOrderTest $<  -lboost_date_time

BoostDateHelperTest: BoostDateHelperTest.o
	g++ -o BoostDateHelperTest $<  -lboost_date_time

ProcessMarketOrderTest: ProcessMarketOrderTest.o
	g++ -o ProcessMarketOrderTest $<  -lboost_date_time

StopOrderTest: StopOrderTest.o
	g++ -o StopOrderTest $<  -lboost_date_time

LimitOrderTest: LimitOrderTest.o TestUtils.o
	g++ -o LimitOrderTest $<  TestUtils.o -lboost_date_time -lboost_system

TradingPositionTest: TradingPositionTest.o TestUtils.o
	g++ -o TradingPositionTest $< TestUtils.o -lboost_date_time -lboost_system

MarketOrderTest: MarketOrderTest.o TestUtils.o
	g++ -o MarketOrderTest -std=c++14 $<  TestUtils.o -lboost_date_time -lboost_system

ClosedPositionHistoryTest: ClosedPositionHistoryTest.o TestUtils.o
	g++ -o ClosedPositionHistoryTest $< TestUtils.o -lboost_date_time -lboost_system -lpthread

OpenPositionTest: OpenPositionTest.o TestUtils.o
	g++ -o OpenPositionTest $< TestUtils.o -lboost_date_time -lboost_system

OpenPositionHistoryTest: OpenPositionHistoryTest.o
	g++ -o OpenPositionHistoryTest $< -lboost_date_time

OpenPositionBarTest: OpenPositionBarTest.o TestUtils.o
	g++ -o OpenPositionBarTest $< TestUtils.o -lboost_date_time -lboost_system

StopLossTest: StopLossTest.o
	g++ -o StopLossTest $<   -lboost_system

ProfitTargetTest: ProfitTargetTest.o
	g++ -o ProfitTargetTest $<  -lboost_system

PercentNumberTest: PercentNumberTest.o
	g++ -o PercentNumberTest $< -lboost_system

TimeSeriesCsvReaderTest: TimeSeriesCsvReaderTest.o
	g++ -o TimeSeriesCsvReaderTest $< ${TIMESERIES_O} -lboost_date_time -lpthread

TimeSeriesCsvWriterTest: TimeSeriesCsvWriterTest.o
	g++ -o TimeSeriesCsvWriterTest $< ${TIMESERIES_O} -lboost_date_time -lpthread

SyntheticTimeSeriesTest: SyntheticTimeSeriesTest.o
	g++ -o SyntheticTimeSeriesTest $< ${TIMESERIES_O} -lboost_date_time -lboost_random -lpthread

TimeSeriesTest: TimeSeriesTest.o
	g++ -o TimeSeriesTest $< ${TIMESERIES_O} -lboost_date_time -lpthread -lboost_system

TimeSeriesEntryTest: TimeSeriesEntryTest.o
	g++ -o TimeSeriesEntryTest $< -lboost_date_time

SyntheticTimeSeriesCreatorTest: SyntheticTimeSeriesCreatorTest.o TestUtils.o
	g++ -o SyntheticTimeSeriesCreatorTest $< ${TIMESERIES_O} TestUtils.o -L/usr/local/lib -lboost_date_time -lpthread -lboost_system

TestUtils.o: ../PercentNumber.h ../DecimalConstants.h ../TimeFrame.h
TradingVolumeTest.o: ../TradingVolume.h
MCPTStrategyAttributesTest.o: ../MCPTStrategyAttributes.h
LogPalPatternTest.o: ../LogPalPattern.h
RobustnessTesterUnitTest.o: ../RobustnessTester.h ../RobustnessTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h ../decimal.h
RobustnessTestUnitTest.o: ../RobustnessTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h
PALMonteCarloValidationTest.o: ../PALMonteCarloValidation.h  ../McptConfigurationFileReader.h ../DateRange.h ../Security.h ../TimeSeries.h ../TimeSeriesEntry.h ../BackTester.h ../DecimalConstants.h
McptConfigurationFileReaderTest.o: ../McptConfigurationFileReader.h ../DateRange.h ../BoostDateHelper.h ../Security.h ../TimeSeriesCsvReader.h ../SecurityAttributesFactory.h ../TimeFrameUtility.h
SecurityAttributesFactoryTest.o: ../SecurityAttributes.h ../SecurityAttributesFactory.h ../decimal.h
MonteCarloPermutationTest.o: ../MonteCarloPermutationTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h
MonteCarloPermutationTest2.o: ../MonteCarloPermutationTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h
BackTesterTest.o: ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h
PalStrategyTest.o: ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h
SecurityBacktestPropertiesTest.o: ../SecurityBacktestProperties.h
PALPatternInterpreterTest.o: ../PALPatternInterpreter.h ../Security.h ../TimeSeries.h PALPatternInterpreterTest.cpp
StrategyBrokerTest.o: ../StrategyBroker.h ../TradingOrder.h ../TradingPosition.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../InstrumentPositionManager.h ../TradingOrderManager.h ../Portfolio.h ../StopLoss.h ../ProfitTarget.h
StrategyTransactionManagerTest.o: ../TradingOrder.h ../TradingPosition.h ../TimeSeriesEntry.h ../StrategyTransaction.h ../StrategyTransactionManager.h
StrategyTransactionTest.o: ../TradingOrder.h ../TradingPosition.h ../TimeSeriesEntry.h ../StrategyTransaction.h
TradingOrderManagerTest.o: ../TradingOrderManager.h ../TradingOrder.h ../TimeSeriesEntry.h
PortfolioTest.o: ../Portfolio.h ../Security.h ../TimeSeries.h ../DecimalConstants.h ../TimeSeriesEntry.h
SecurityAttributesTest.o: ../Security.h ../TimeSeries.h ../DecimalConstants.h ../TimeSeriesEntry.h ../SecurityAttributes.h
InstrumentPositionManagerTest.o: ../TimeSeriesEntry.h ../InstrumentPosition.h ../TradingPosition.h ../InstrumentPositionManager.h ../decimal.h
SecurityTest.o: ../Security.h ../TimeSeries.h ../DecimalConstants.h ../TimeSeriesEntry.h
InstrumentPositionManagerTest.o: ../TimeSeriesEntry.h ../InstrumentPosition.h ../TradingPosition.h ../InstrumentPositionManager.h ../decimal.h
InstrumentPositionTest.o: ../TimeSeriesEntry.h ../InstrumentPosition.h ../TradingPosition.h ../decimal.h
ProcessStopOrderTest.o:../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderManager.h ../TradingOrderException.h ../decimal.h
BoostDateHelperTest.o: ../BoostDateHelper.h
ProcessLimitOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderManager.h ../TradingOrderException.h ../decimal.h
ProcessMarketOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderManager.h ../TradingOrderException.h ../decimal.h
MarketOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderException.h ../decimal.h
StopOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderException.h ../decimal.h
LimitOrderTest.o: ../TimeSeriesEntry.h ../TradingOrder.h ../TradingOrderException.h ../decimal.h
TradingPositionTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h
ClosedPositionHistoryTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h ../PercentNumber.h ../ClosedPositionHistory.h
ClosedPositionTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h ../PercentNumber.h
OpenPositionTest.o: ../TimeSeriesEntry.h ../TradingPosition.h ../decimal.h ../PercentNumber.h
OpenPositionHistoryTest.o: ../TradingPosition.h ../decimal.h ../PercentNumber.h ../TimeSeriesEntry.h
OpenPositionBarTest.o: ../TradingPosition.h ../decimal.h ../TimeSeriesEntry.h
StopLossTest.o: ../StopLoss.h ../decimal.h ../PercentNumber.h
ProfitTargetTest.o: ../ProfitTarget.h ../decimal.h ../PercentNumber.h
PercentNumberTest.o: ../PercentNumber.h ../decimal.h
SyntheticTimeSeriesTest.o: ../TimeSeriesEntry.h ../TimeSeries.h ../SyntheticTimeSeries.h ../decimal.h ../RandomMersenne.h
TimeSeriesTest.o: ../TimeSeriesEntry.h ../TimeSeries.h ../TimeSeriesIndicators.h ../decimal.h
TimeSeriesEntryTest.o: ../TimeSeriesEntry.h ../decimal.h
TimeSeriesCsvReaderTest.o: ../TimeSeriesCsvReader.h ../TimeSeries.h ../TimeSeriesEntry.h ../decimal.h ../csv.h
TimeSeriesCsvWriterTest.o: ../TimeSeriesCsvWriter.h ../TimeSeries.h ../TimeSeriesEntry.h ../decimal.h ../csv.h TimeSeriesCsvWriterTest.cpp
SyntheticTimeSeriesCreatorTest.o: ../TimeSeriesCsvWriter.h ../TimeSeriesCsvReader.h ../TimeSeries.h ../TimeSeriesEntry.h ../decimal.h ../csv.h

clean:
	rm *.o
	rm -f ClosedPositionHistory TimeSeriesTest TimeSeriesEntryTest SyntheticTimeSeriesTest
	rm -f TimeSeriesCsvReaderTest PercentNumberTest ProfitTargetTest StopLossTest
	rm -f OpenPositionBarTest OpenPositionHistoryTest OpenPositionTest
	rm -f ClosedPositionTest SyntheticTimeSeriesCreatorTest
	rm *~

CCFLAGS := -O2 -c -std=c++14 -I../ -I${PRICEACTION_INC_DIR}
#CCFLAGS := -g -c -std=c++14 -I../../PALCodeGenLibrary -I../../Decimal

%.o: %.cpp
	g++ $(CCFLAGS) $<
