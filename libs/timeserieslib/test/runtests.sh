#!/bin/bash -x
g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab BackTesterTest.cpp

g++ -O2 -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab MultipleTestCorrectionTest.cpp

g++ -o MultipleTestCorrectionTest MultipleTestCorrectionTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -pthread

g++ -O0 -g -c -std=c++14 -I../ -I. -I/usr/local/include/priceactionlab MastersComputationPolicyTest.cpp

g++ -o MastersComputationPolicyTest MastersComputationPolicyTest.o TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ../BacktesterStrategy.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -pthread

#g++ -o SyntheticTimeSeriesTest2 SyntheticTimeSeriesTest2.cpp TestUtils.o ../LogPalPattern.o ../TimeSeriesEntry.o ../TimeSeries.o ./catch_amalgamated.o -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_system -lCatch2 -pthread
