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

################################################################################
# Bison
################################################################################
set(BisonDir ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2)
find_package(BISON REQUIRED)
set(BisonOutput ${BisonDir}/location.hh
                ${BisonDir}/position.hh
                ${BisonDir}/stack.hh
                ${BisonDir}/PalParser.cpp
                ${BisonDir}/PalParser.hpp)

set(BisonInput ${BisonDir}/grammar.yy)

if(BISON_FOUND)
    add_custom_command(
      OUTPUT ${BisonOutput}
      COMMAND ${BISON_EXECUTABLE} --output=${BisonDir}/PalParser.cpp ${BisonInput}
      DEPENDS ${BisonInput}
      COMMENT "Generating Bison files"
    )
endif()

################################################################################
# Flex
################################################################################
find_package(FLEX REQUIRED)
set(FlexOutput PalScanner.hpp PalScanner.cpp)
set(FlexInput ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/lexer.ll)
if(FLEX_FOUND)
    add_custom_command(
      OUTPUT ${FlexOutput}
      COMMAND ${FLEX_EXECUTABLE} ${FlexInput}
      DEPENDS ${FlexInput}
      COMMENT "Generating Lexer files"
    )
endif()

################################################################################
add_library(${LIB_NAME} STATIC
            ${SRC_LIST}
            ${BisonOutput}
            ${FlexOutput})

target_link_libraries(${LIB_NAME} ${CMAKE_THREAD_LIBS_INIT})
