RANLIB := ranlib
CC := g++-5
object-set := PalCodeGenVisitor.o PalAst.o TradingBloxCodeGenerator.o PalParser.o PalScanner.o PalParseDriver.o PriceActionLabSystem.o WealthLabCodeGenerator.o EasyLanguageCodeGenerator.o PalCodeGenerator.o

libpriceaction2.a: objects
	rm -f $@
	ar -r $@ $(object-set)
	-$(RANLIB) $@

install: libpriceaction2.a
	install -c -m 644 libpriceaction2.a /usr/local/lib
	install -c -m 644 PalCodeGenVisitor.h /usr/local/include/priceactionlab
	install -c -m 644 PalParseDriver.h /usr/local/include/priceactionlab
	install -c -m 644 PalParser.hpp /usr/local/include/priceactionlab
	install -c -m 644 scanner.h /usr/local/include/priceactionlab
	install -c -m 644 stack.hh /usr/local/include/priceactionlab
	install -c -m 644 location.hh /usr/local/include/priceactionlab
	install -c -m 644 position.hh /usr/local/include/priceactionlab
	install -c -m 644 PalAst.h /usr/local/include/priceactionlab

objects: PalCodeGenVisitor.o PalAst.o TradingBloxCodeGenerator.o PalParser.o PalScanner.o PalParseDriver.o PriceActionLabSystem.o WealthLabCodeGenerator.o EasyLanguageCodeGenerator.o PalCodeGenerator.o

PalParsedriver.o: PalParseDriver.cpp PalParseDriver.h scanner.h PalParser.hpp PalAst.h
PalCodeGenVisitor.o: PalCodeGenVisitor.cpp PalAst.h PalCodeGenVisitor.h
PalCodeGenerator.o: PalCodeGenerator.cpp PalAst.h PalCodeGenVisitor.h
PalAst.o: PalAst.cpp PalAst.h
TradingBloxCodeGenerator.o: TradingBloxCodeGenerator.cpp PalAst.h PalCodeGenVisitor.h
WealthLabCodeGenerator.o: WealthLabCodeGenerator.cpp PalAst.h PalCodeGenVisitor.h
EasyLanguageCodeGenerator.o: EasyLanguageCodeGenerator.cpp PalAst.h PalCodeGenVisitor.h
PalParser.o: PalParser.cpp PalAst.h scanner.h PalScanner.hpp
PriceActionLabSystem.o: PriceActionLabSystem.cpp PalAst.h
PalScanner.o: PalScanner.cpp PalParser.hpp PalAst.h scanner.h
	$(CC) $(CCFLAGS) PalScanner.cpp

PalParser.cpp: grammar.yy 
	bison -o PalParser.cpp grammar.yy

PalScanner.hpp PalScanner.cpp: lexer.ll
	flex lexer.ll

fresh:
	rm -f stack.hh location.hh position.hh
	rm -f PalParser.hpp PalParser.cpp PalScanner.hpp PalScanner.cpp
	rm -f *.o libpriceaction2.a

CCFLAGS := -O2 -c -std=c++14 -I/usr/local/include/backtester


%.o: %.cpp
	$(CC) $(CCFLAGS) $<


