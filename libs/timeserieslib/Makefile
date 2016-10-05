CC := g++-5
RANLIB := ranlib
object-set := LogPalPattern.o TimeSeriesEntry.o TimeSeries.o McptConfigurationFileReader.o TimeFrameUtility.o runner.o

libbacktest.a: objects
	rm -f $@
	ar -r $@ $(object-set)
	-$(RANLIB) $@

install: libbacktest.a *.h
	install -c -m 644 libbacktest.a /usr/local/lib
	install -c -m 644 *.h /usr/local/include/backtester
	install -c -m 644 *.hpp /usr/local/include/backtester

objects: LogPalPattern.o TimeSeriesEntry.o TimeSeries.o McptConfigurationFileReader.o TimeFrameUtility.o runner.o

LogPalPattern.o: LogPalPattern.cpp LogPalPattern.h /usr/local/include/priceactionlab/*.h
TimeSeriesEntry.o: TimeSeriesEntry.cpp TimeSeriesEntry.h decimal.h
TimeSeries.o: TimeSeries.cpp TimeSeriesEntry.h TimeSeries.h decimal.h
McptConfigurationFileReader.o: McptConfigurationFileReader.h TimeSeriesEntry.h TimeSeriesCsvReader.h SecurityAttributes.h SecurityAttributesFactory.h /usr/local/include/priceactionlab/*.h
TimeFrameUtility.o: TimeFrameUtility.h TimeFrame.h
CCFLAGS := -O2 -c -std=c++14 -I. -I/usr/local/include/priceactionlab 


%.o: %.cpp
	$(CC) $(CCFLAGS) $<
