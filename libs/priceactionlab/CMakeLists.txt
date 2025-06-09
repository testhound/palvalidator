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

target_link_libraries(${LIB_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(${LIB_NAME} PRIVATE timeseries)

file(GLOB TEST_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
)

add_executable(${LIB_NAME}_unit_tests
    ${TEST_LIST}
)

find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(${LIB_NAME}_unit_tests PRIVATE backtesting)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE statistics)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE cpptrace::cpptrace)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE "-lcurl")
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE concurrency)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE timeseries)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE testinfra)

enable_testing()
add_test(NAME ${LIB_NAME}_unit_tests COMMAND ${LIB_NAME}_unit_tests)

# Set the working directory for the test to the project root
set_tests_properties(${LIB_NAME}_unit_tests
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# This line for DATASET_FILES might be redundant or could be removed if
# setting the working directory solves the issue for all dataset access needs.
# If ${DATASET_FILES} is defined elsewhere and needed for other purposes, it can remain.
# Ensure QQQ_IR.txt is accessible via "dataset/QQQ_IR.txt" from CMAKE_SOURCE_DIR
file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
install(TARGETS ${LIB_NAME} DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
