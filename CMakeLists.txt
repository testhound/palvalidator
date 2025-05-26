project(PalValidator)

cmake_minimum_required(VERSION 2.8)

find_package(Doxygen REQUIRED)

if(DOXYGEN_FOUND)
    set(DOXYGEN_IN "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
    set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

    configure_file("${DOXYGEN_IN}" "${DOXYGEN_OUT}" @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found. Documentation cannot be generated.")
endif()

set(BIN_NAME ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)

set(USE_BLOOMBERG_DECIMALS false)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

if(USE_BLOOMBERG_DECIMALS)
  add_definitions(-DUSE_BLOOMBERG_DECIMALS)

  set(BLOOMBERG_DIR ${CMAKE_SOURCE_DIR}/../bloomberg)

  set(BLOOMBERG_DECIMAL_LIBRARIES
    ${BLOOMBERG_DIR}/bde/build/groups/bdl/bdldfp/libbdldfp.a
    ${BLOOMBERG_DIR}/bde/build/groups/bsl/bsls/libbsls.a
    ${BLOOMBERG_DIR}/bde/build/groups/bsl/libbsl.a
    ${BLOOMBERG_DIR}/bde/build/thirdparty/decnumber/libdecnumber.a
    ${BLOOMBERG_DIR}/bde/build/thirdparty/inteldfp/libinteldfp.a
    ${BLOOMBERG_DIR}/bde/build/groups/bsl/bslim/libbslim.a
    ${BLOOMBERG_DIR}/bde/build/groups/bdl/bdlscm/libbdlscm.a
    ${BLOOMBERG_DIR}/bde/build/groups/bsl/bslscm/libbslscm.a
    )

  set(BLOOMBERG_DECIMAL_INCLUDES
    ${BLOOMBERG_DIR}/bde/groups/bdl/bdldfp
    ${BLOOMBERG_DIR}/bde/groups/bdl/bdlscm
    ${BLOOMBERG_DIR}/bde/groups/bsl/bsls
    ${BLOOMBERG_DIR}/bde/groups/bsl/bslscm
    ${BLOOMBERG_DIR}/bde/groups/bsl/bslmf
    ${BLOOMBERG_DIR}/bde/groups/bsl/bslstl
    ${BLOOMBERG_DIR}/bde/groups/bsl/bslalg
    ${BLOOMBERG_DIR}/bde/groups/bsl/bslma
    ${BLOOMBERG_DIR}/bde/groups/bsl/bsl+bslhdrs
    ${BLOOMBERG_DIR}/bde/groups/bsl/bslh
    ${BLOOMBERG_DIR}/bde/thirdparty
    ${BLOOMBERG_DIR}/bde/thirdparty/inteldfp/LIBRARY/src
    )
endif()

#if(CMAKE_COMPILER_IS_GNUCXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall  -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -march=native")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
#endif()

#set(Boost_DEBUG ON)
#set(Boost_VERBOSE ON)

find_package( Threads )
find_package(Boost REQUIRED COMPONENTS filesystem date_time chrono system program_options regex thread container)
include_directories(${CMAKE_SOURCE_DIR}
  ${Boost_INCLUDE_DIR}
  ${BLOOMBERG_DECIMAL_INCLUDES}
  ${RAPIDJSON_DIR}
  ${CURL_DIR}
  ${FLEX_INCLUDE_DIR}
  )

file(GLOB TXT_FILES "${CMAKE_SOURCE_DIR}/dataset/*.txt")
set(DATASET_FILES ${TXT_FILES} CACHE INTERNAL "List of dataset files")

include(CMakePrintHelpers)

add_subdirectory(libs/timeseries)
add_subdirectory(libs/priceactionlab)
add_subdirectory(libs/testinfra)
add_subdirectory(libs/backtesting)
add_subdirectory(libs/concurrency)
add_subdirectory(libs/pasearchalgo)
add_subdirectory(libs/statistics)

add_subdirectory(src/palvalidator)
add_subdirectory(src/patterndiscovery)


file(GLOB SRC_LIST
   src/palvalidator/*.h
   src/palvalidator/*.hpp
   src/palvalidator/*.cpp
   src/palvalidator/*.txt
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

set_target_properties(${BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${BIN_NAME}
    priceaction2
    statistics
    concurrency
    timeseries
    backtesting
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${BLOOMBERG_DECIMAL_LIBRARIES}
    curl
    )

    #list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

#add_subdirectory(tests)
include(FetchContent)
FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v0.8.3 # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)

find_package(Catch2 CONFIG REQUIRED)

add_library(Catch INTERFACE)

add_custom_target(tests
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS timeseries_unit_tests backtesting_unit_tests statistics_unit_tests priceaction2_unit_tests palvalidator_unit_tests
)
