PalValidator: main.o
	g++ -o PalValidator $< -L/usr/local/lib -lboost_date_time -lpriceaction2 -lboost_filesystem -lboost_system -lbacktest -lpriceaction2 -lboost_filesystem -lboost_system -lboost_thread -lpthread

install: PalValidator
	install -c PalValidator /usr/local/bin

main.o: /usr/local/include/backtester/*.h /usr/local/include/priceactionlab/*.h

CCFLAGS := -O2 -c -Wall -fpermissive -std=c++14 -I/usr/local/include/backtester -I/usr/local/include/priceactionlab

%.o: %.cpp
	g++ $(CCFLAGS) $<
