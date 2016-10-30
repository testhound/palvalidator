set(LIB_NAME priceaction2)
set(SRC_LIST EasyLanguageCodeGenerator.cpp
    PalAst.cpp
    PalCodeGenerator.cpp
    PalCodeGenVisitor.cpp
    PalParseDriver.cpp
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

find_package(BISON)
bison_target(PalParser
             ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/grammar.yy
             ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/PalParser.cpp)

find_package(FLEX)
flex_target(PalScanner
            ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/lexer.ll
            ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/PalScanner.cpp)

add_flex_bison_dependency(PalScanner PalParser)

add_library(${LIB_NAME} STATIC
            ${SRC_LIST}
            ${BISON_PalParser_OUTPUTS}
            ${FLEX_PalScanner_OUTPUTS})

SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME} ${CMAKE_THREAD_LIBS_INIT})
