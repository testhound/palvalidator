#!/bin/bash -x

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE TestUtils.cpp

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include BoostDateHelperTest.cpp
g++ -o BoostDateHelperTest BoostDateHelperTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include InstrumentPositionManagerTest.cpp
g++ -o InstrumentPositionManagerTest InstrumentPositionManagerTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include InstrumentPositionTest.cpp
g++ -o InstrumentPositionTest InstrumentPositionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include LimitOrderTest.cpp
g++ -o LimitOrderTest LimitOrderTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include PALPatternInterpreterTest.cpp
g++ -o PALPatternInterpreterTest PALPatternInterpreterTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include PercentNumberTest.cpp
g++ -o PercentNumberTest PercentNumberTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include PortfolioTest.cpp
g++ -o PortfolioTest PortfolioTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include MarketOrderTest.cpp
g++ -o MarketOrderTest MarketOrderTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include OpenPositionBarTest.cpp
g++ -o OpenPositionBarTest OpenPositionBarTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include OpenPositionHistoryTest.cpp
g++ -o OpenPositionHistoryTest OpenPositionHistoryTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include OpenPositionTest.cpp
g++ -o OpenPositionTest OpenPositionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include TradingPositionTest.cpp
g++ -o TradingPositionTest TradingPositionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include ProcessLimitOrderTest.cpp
g++ -o ProcessLimitOrderTest ProcessLimitOrderTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include ProcessMarketOrderTest.cpp
g++ -o ProcessMarketOrderTest ProcessMarketOrderTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include ProcessStopOrderTest.cpp
g++ -o ProcessStopOrderTest ProcessStopOrderTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include SecurityAttributesTest.cpp
g++ -o SecurityAttributesTest SecurityAttributesTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include SecurityAttributesFactoryTest.cpp
g++ -o SecurityAttributesFactoryTest SecurityAttributesFactoryTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include SecurityTest.cpp
g++ -o SecurityTest SecurityAttributesTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StopLossTest.cpp
g++ -o StopLossTest StopLossTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StopOrderTest.cpp
g++ -o StopOrderTest StopOrderTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StrategyBrokerTest.cpp
g++ -o StrategyBrokerTest StrategyBrokerTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StrategyTransactionTest.cpp
g++ -o StrategyTransactionTest StrategyTransactionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StrategyTransactionManagerTest.cpp
g++ -o StrategyTransactionManagerTest StrategyTransactionManagerTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include SyntheticTimeSeriesCreatorTest.cpp
g++ -o SyntheticTimeSeriesCreatorTest SyntheticTimeSeriesCreatorTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE TimeSeriesTest.cpp

g++ -o TimeSeriesTest TimeSeriesTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -g -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE PalStrategyTest.cpp

g++ -o PalStrategyTest PalStrategyTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -g -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE BackTesterTest.cpp
g++ -o BackTesterTest BackTesterTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -lcpptrace -ldwarf -lzstd -ldwarf -lz -lboost_filesystem -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab ClosedPositionHistoryTest.cpp

g++ -o ClosedPositionHistoryTest ClosedPositionHistoryTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -lboost_filesystem -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab MultipleTestCorrectionTest.cpp

g++ -o MultipleTestCorrectionTest MultipleTestCorrectionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_filesystem -lCatch2 -pthread

g++ -O2 -g -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab MastersComputationPolicyTest.cpp

g++ -o MastersComputationPolicyTest MastersComputationPolicyTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_thread -lboost_system -lboost_filesystem -lCatch2

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StrategyDataPreparerTest.cpp

g++ -o StrategyDataPreparerTest StrategyDataPreparerTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2  -lboost_thread -lboost_chrono -lboost_filesystem -lcpptrace -ldwarf -lzstd -ldwarf -lz -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include SyntheticTimeSeriesTest2.cpp

g++ -o SyntheticTimeSeriesTest2 SyntheticTimeSeriesTest2.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include MastersRomanoWolfTest.cpp

g++ -o MastersRomanoWolfTest MastersRomanoWolfTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include MastersRomanoWolfImprovedTest.cpp

g++ -o MastersRomanoWolfImprovedTest MastersRomanoWolfImprovedTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include ValidatorConfigurationTest.cpp

g++ -o ValidatorConfigurationTest ValidatorConfigurationTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../ValidatorConfiguration.o ../TimeFrameUtility.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread
