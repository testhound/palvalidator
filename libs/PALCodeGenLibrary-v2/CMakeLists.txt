set(LIB_NAME priceaction2)
set(SRC_LIST EasyLanguageCodeGenerator.cpp
    PalAst.cpp
    PalCodeGenerator.cpp
    PalCodeGenVisitor.cpp
    PalParseDriver.cpp
    PalParser.cpp
    PalScanner.cpp
    PriceActionLabSystem.cpp
    TradingBloxCodeGenerator.cpp
    WealthLabCodeGenerator.cpp
    PalAst.h
    PalCodeGenVisitor.h
    PalParseDriver.h
    scanner.h
    SecurityFactory.h
    Security.h

)
add_library(${LIB_NAME} STATIC ${SRC_LIST})
SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME} ${CMAKE_THREAD_LIBS_INIT})
