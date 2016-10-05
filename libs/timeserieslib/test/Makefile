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

All: MCPTStrategyAttributesTest LogPalPatternTest RobustnessTesterUnitTest RobustnessTestUnitTest PALMonteCarloValidationTest McptConfigurationFileReaderTest SecurityAttributesFactoryTest SecurityAttributesTest MonteCarloPermutationTest2 BackTesterTest PalStrategyTest SecurityBacktestPropertiesTest PALPatternInterpreterTest StrategyBrokerTest StrategyTransactionManagerTest StrategyTransactionTest TradingOrderManagerTest PortfolioTest SecurityTest InstrumentPositionManagerTest InstrumentPositionTest ProcessStopOrderTest ProcessLimitOrderTest BoostDateHelperTest ProcessMarketOrderTest StopOrderTest LimitOrderTest TradingPositionTest MarketOrderTest ClosedPositionHistoryTest OpenPositionTest OpenPositionHistoryTest OpenPositionBarTest StopLossTest ProfitTargetTest PercentNumberTest TimeSeriesCsvReaderTest TimeSeriesCsvWriterTest SyntheticTimeSeriesTest TimeSeriesTest TimeSeriesEntryTest TradingVolumeTest

TradingVolumeTest: TradingVolumeTest.o
	g++-5 -o TradingVolumeTest ../TimeSeries.o $<

MCPTStrategyAttributesTest: MCPTStrategyAttributesTest.o
	g++-5 -o MCPTStrategyAttributesTest ../TimeSeries.o $< -lboost_date_time

LogPalPatternTest: LogPalPatternTest.o
	g++-5 -o LogPalPatternTest  $< ../LogPalPattern.o -L/usr/local/lib -lpriceaction2

RobustnessTesterUnitTest: RobustnessTesterUnitTest.o
	g++-5 -o RobustnessTesterUnitTest  $< ../TimeSeries.o ../LogPalPattern.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

RobustnessTestUnitTest: RobustnessTestUnitTest.o
	g++-5 -o RobustnessTestUnitTest  $< ../TimeSeries.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

PALMonteCarloValidationTest: PALMonteCarloValidationTest.o ../McptConfigurationFileReader.o
	g++-5 -o PALMonteCarloValidationTest  -flto $< ../TimeSeries.o ../McptConfigurationFileReader.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lelkhound -lsmbase -lboost_filesystem -lboost_system

McptConfigurationFileReaderTest: McptConfigurationFileReaderTest.o ../McptConfigurationFileReader.o ../TimeFrameUtility.o
	g++-5 -o McptConfigurationFileReaderTest  $< ../McptConfigurationFileReader.o ../TimeFrameUtility.o ../TimeSeries.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2 -lelkhound -lsmbase -lboost_filesystem -lboost_system

SecurityAttributesFactoryTest: SecurityAttributesFactoryTest.o
	g++-5 -o SecurityAttributesFactoryTest $< -lboost_date_time

SecurityAttributesTest: SecurityAttributesTest.o
	g++-5 -o SecurityAttributesTest ../TimeSeries.o $< -lboost_date_time

MonteCarloPermutationTest2: MonteCarloPermutationTest2.o
	g++-5 -o MonteCarloPermutationTest2  $< ../TimeSeries.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

BackTesterTest: BackTesterTest.o
	g++-5 -o BackTesterTest  $< ../TimeSeries.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

PalStrategyTest: PalStrategyTest.o
	g++-5 -o PalStrategyTest  $< ../TimeSeries.o -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

SecurityBacktestPropertiesTest: SecurityBacktestPropertiesTest.o
	g++-5 -o SecurityBacktestPropertiesTest ../TimeSeries.o $< -lboost_date_time -lpthread

PALPatternInterpreterTest: PALPatternInterpreterTest.o
	g++-5 -o PALPatternInterpreterTest ../TimeSeries.o $< -L/usr/local/lib -lboost_date_time -lpthread  -lpriceaction2

StrategyBrokerTest: StrategyBrokerTest.o
	g++-5 -o StrategyBrokerTest ../TimeSeries.o $< -lboost_date_time -lpthread

StrategyTransactionManagerTest: StrategyTransactionManagerTest.o
	g++-5 -o StrategyTransactionManagerTest ../TimeSeries.o $< -lboost_date_time

StrategyTransactionTest: StrategyTransactionTest.o
	g++-5 -o StrategyTransactionTest ../TimeSeries.o $< -lboost_date_time

TradingOrderManagerTest: TradingOrderManagerTest.o
	g++-5 -o TradingOrderManagerTest ../TimeSeries.o $< -lboost_date_time

PortfolioTest: PortfolioTest.o
	g++-5 -o PortfolioTest ../TimeSeries.o $< -lboost_date_time

SecurityTest: SecurityTest.o
	g++-5 -o SecurityTest ../TimeSeries.o $< -lboost_date_time

InstrumentPositionManagerTest: InstrumentPositionManagerTest.o
	g++-5 -o InstrumentPositionManagerTest ../TimeSeries.o $< -lboost_date_time

InstrumentPositionTest: InstrumentPositionTest.o
	g++-5 -o InstrumentPositionTest $< -lboost_date_time

ProcessStopOrderTest: ProcessStopOrderTest.o
	g++-5 -o ProcessStopOrderTest $<  -lboost_date_time

ProcessLimitOrderTest: ProcessLimitOrderTest.o
	g++-5 -o ProcessLimitOrderTest $<  -lboost_date_time

BoostDateHelperTest: BoostDateHelperTest.o
	g++-5 -o BoostDateHelperTest $<  -lboost_date_time

ProcessMarketOrderTest: ProcessMarketOrderTest.o
	g++-5 -o ProcessMarketOrderTest $<  -lboost_date_time

StopOrderTest: StopOrderTest.o
	g++-5 -o StopOrderTest $<  -lboost_date_time

LimitOrderTest: LimitOrderTest.o
	g++-5 -o LimitOrderTest $<  -lboost_date_time

TradingPositionTest: TradingPositionTest.o
	g++-5 -o TradingPositionTest $< -lboost_date_time

MarketOrderTest: MarketOrderTest.o
	g++-5 -o MarketOrderTest $<  -lboost_date_time

ClosedPositionHistoryTest: ClosedPositionHistoryTest.o
	g++-5 -o ClosedPositionHistoryTest $< -lboost_date_time -lpthread

OpenPositionTest: OpenPositionTest.o
	g++-5 -o OpenPositionTest $< -lboost_date_time

OpenPositionHistoryTest: OpenPositionHistoryTest.o
	g++-5 -o OpenPositionHistoryTest $< -lboost_date_time

OpenPositionBarTest: OpenPositionBarTest.o
	g++-5 -o OpenPositionBarTest $< -lboost_date_time

StopLossTest: StopLossTest.o
	g++-5 -o StopLossTest $<

ProfitTargetTest: ProfitTargetTest.o
	g++-5 -o ProfitTargetTest $<

PercentNumberTest: PercentNumberTest.o
	g++-5 -o PercentNumberTest $<

TimeSeriesCsvReaderTest: TimeSeriesCsvReaderTest.o
	g++-5 -o TimeSeriesCsvReaderTest $< ../TimeSeries.o -lboost_date_time -lpthread

TimeSeriesCsvWriterTest: TimeSeriesCsvWriterTest.o
	g++-5 -o TimeSeriesCsvWriterTest $< ../TimeSeries.o -lboost_date_time -lpthread

SyntheticTimeSeriesTest: SyntheticTimeSeriesTest.o
	g++-5 -o SyntheticTimeSeriesTest $< ../TimeSeries.o -lboost_date_time -lboost_random -lpthread

TimeSeriesTest: TimeSeriesTest.o
	g++-5 -o TimeSeriesTest $< ../TimeSeries.o -lboost_date_time -lpthread

TimeSeriesEntryTest: TimeSeriesEntryTest.o
	g++-5 -o TimeSeriesEntryTest $< -lboost_date_time

TradingVolumeTest.o: ../TradingVolume.h
MCPTStrategyAttributesTest.o: ../MCPTStrategyAttributes.h 
LogPalPatternTest.o: ../LogPalPattern.h
RobustnessTesterUnitTest.o: ../RobustnessTester.h ../RobustnessTest.h ../BackTester.h ../BacktesterStrategy.h ../PalStrategy.h ../Portfolio.h ../Security.h ../StrategyBroker.h ../TimeSeriesEntry.h ../TimeSeries.h ../InstrumentPosition.h ../TradingOrderManager.h
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

	g++-5 -g -c -std=c++14 -I../../PALCodeGenLibrary-v2 -I../../Decimal PALPatternInterpreterTest.cpp
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

clean:
	rm *.o 
	rm -f ClosedPositionHistory TimeSeriesTest TimeSeriesEntryTest SyntheticTimeSeriesTest
	rm -f TimeSeriesCsvReaderTest PercentNumberTest ProfitTargetTest StopLossTest
	rm -f OpenPositionBarTest OpenPositionHistoryTest OpenPositionTest
	rm -f ClosedPositionTest rm *~

CCFLAGS := -O2 -c -std=c++14 -I../ -I../../PALCodeGenLibrary-v2
#CCFLAGS := -g -c -std=c++14 -I../../PALCodeGenLibrary -I../../Decimal

%.o: %.cpp
	g++-5 $(CCFLAGS) $<
