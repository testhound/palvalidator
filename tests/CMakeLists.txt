# Upgrading from Catch to Catch2 also required a command along the lines of
# sed -i -- 's@#include "../@#include "@g;s@#include "catch.hpp"@#include <catch2/catch_test_macros.hpp>@g' *.cpp

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_VERBOSE_MAKEFILE ON)

#set(Boost_DEBUG ON)
#set(Boost_VERBOSE ON)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.0-preview3
)
FetchContent_MakeAvailable(Catch2)

#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/PALCodeGenLibrary-v2/ ${CMAKE_CURRENT_BINARY_DIR}/libs/PALCodeGenLibrary-v2)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/timeserieslib/ ${CMAKE_CURRENT_BINARY_DIR}/libs/timeserieslib)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libs/pasearchalgolib/ ${CMAKE_CURRENT_BINARY_DIR}/libs/pasearchalgolib)

set(TEST_BINARY tests)

#set(TEST_SOURCES
#    BackTesterTest.cpp
#    ClosedPositionHistoryTest.cpp
#    TestUtils.h
#    TestUtils.cpp
#    )

#if (APPLE)
#    set(TEST_SOURCES ${TEST_SOURCES} TargetConditionals.h)
#endif()


#target_link_libraries(${TEST_BINARY}
#    priceaction2
#    backtest
#    pasearchalgo
#    ${Boost_LIBRARIES}
#    ${CMAKE_THREAD_LIBS_INIT}
#    ${BLOOMBERG_DECIMAL_LIBRARIES}
#    curl
#    )

include_directories(${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/
  ${CMAKE_SOURCE_DIR}/libs/timeserieslib
  ${CMAKE_SOURCE_DIR}/libs/pasearchalgolib
  ${Boost_INCLUDE_DIR}
  ${BLOOMBERG_DECIMAL_INCLUDES}
  ${RAPIDJSON_DIR}
  ${CURL_DIR}
  ${FLEX_INCLUDE_DIR}
 )

#enable_testing()

add_executable(
  ${TEST_BINARY}
  ${TEST_SOURCES}
)

target_link_libraries(
  ${TEST_BINARY} PRIVATE
  priceaction2
  backtest
  pasearchalgo
  Catch2::Catch2WithMain
  ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${BLOOMBERG_DECIMAL_LIBRARIES}
  curl
)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(CTest)
include(Catch)

catch_discover_tests(${TEST_BINARY})

add_custom_command(TARGET ${TEST_BINARY} POST_BUILD
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${TEST_BINARY}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Tests..."
)

