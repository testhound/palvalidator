#!/bin/bash -x

g++ -O2 -g -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE TestUtils.cpp

g++ -O2 -g -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE TimeSeriesTest.cpp

g++ -o TimeSeriesTest TimeSeriesTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -g -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE PalStrategyTest.cpp

g++ -o PalStrategyTest PalStrategyTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -g -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include -DCPPTRACE_STATIC_DEFINE BackTesterTest.cpp
g++ -o BackTesterTest BackTesterTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -lcpptrace -ldwarf -lzstd -ldwarf -lz -lboost_filesystem -pthread

g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab ClosedPositionHistoryTest.cpp

g++ -o ClosedPositionHistoryTest ClosedPositionHistoryTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -lboost_filesystem -pthread

g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab MultipleTestCorrectionTest.cpp

g++ -o MultipleTestCorrectionTest MultipleTestCorrectionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_filesystem -lCatch2 -pthread

g++ -O2 -g -c -std=c++17 -I../ -I. -I/usr/local/include/priceactionlab MastersComputationPolicyTest.cpp

g++ -o MastersComputationPolicyTest MastersComputationPolicyTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_thread -lboost_system -lboost_filesystem -lCatch2

g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include StrategyDataPreparerTest.cpp

g++ -o StrategyDataPreparerTest StrategyDataPreparerTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2  -lboost_thread -lboost_chrono -lboost_filesystem -lcpptrace -ldwarf -lzstd -ldwarf -lz -pthread


g++ -o SyntheticTimeSeriesTest2 SyntheticTimeSeriesTest2.cpp TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include MastersRomanoWolfTest.cpp

g++ -o MastersRomanoWolfTest MastersRomanoWolfTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread

g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab -I/usr/local/include MastersRomanoWolfImprovedTest.cpp

g++ -o MastersRomanoWolfImprovedTest MastersRomanoWolfImprovedTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ../runner.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lboost_thread -lboost_filesystem -lCatch2 -pthread
