set(LIB_NAME priceaction2)

file(GLOB SRC_LIST
    ${CMAKE_CURRENT_LIST_DIR}/*.h
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/*.txt
)

################################################################################
# Bison
################################################################################
set(BisonDir  ${CMAKE_CURRENT_LIST_DIR})
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
set(FlexInput ${CMAKE_CURRENT_LIST_DIR}/lexer.ll)
if(FLEX_FOUND)
    add_custom_command(
      OUTPUT ${FlexOutput}
      COMMAND ${FLEX_EXECUTABLE} ${FlexInput}
      DEPENDS ${FlexInput}
      COMMENT "Generating Lexer files ${FlexInput}"
    )
endif()

################################################################################
add_library(${LIB_NAME} STATIC
            ${SRC_LIST}
            ${BisonOutput}
            ${FlexOutput})

target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${LIB_NAME} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(priceaction2 PRIVATE timeseries)
