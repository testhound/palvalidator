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

find_package(BISON REQUIRED)
set(BisonOutput ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/PalParser.cpp)
if(BISON_FOUND)
    add_custom_command(
      OUTPUT ${BisonOutput}
      COMMAND ${BISON_EXECUTABLE}
              --output=${BisonOutput}
              ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/grammar.yy
      COMMENT "Generating PalParser.cpp"
    )
endif()

find_package(FLEX REQUIRED)
set(FlexOutput PalScanner.hpp PalScanner.cpp)
if(FLEX_FOUND)
    add_custom_command(
      OUTPUT ${FlexOutput}
      COMMAND ${FLEX_EXECUTABLE}
              ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/lexer.ll
      COMMENT "Generating PalScanner.{cpp,hpp}"
    )
endif()

add_library(${LIB_NAME} STATIC
            ${SRC_LIST}
            ${BisonOutput}
            ${FlexOutput})

target_link_libraries(${LIB_NAME} ${CMAKE_THREAD_LIBS_INIT})
