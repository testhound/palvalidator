cmake_minimum_required(VERSION 3.1)

project(PalValidator VERSION 1.0.0 LANGUAGES CXX)

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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -g -march=native")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -march=native")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -O0 -march=native")
elseif(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O0 -g -pg")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -pg")
endif()

find_package( Threads )
find_package(Boost REQUIRED CONFIG COMPONENTS filesystem date_time chrono system program_options regex thread container)
include_directories(${CMAKE_SOURCE_DIR}
  ${Boost_INCLUDE_DIR}
  ${BLOOMBERG_DECIMAL_INCLUDES}
  ${RAPIDJSON_DIR}
  ${CURL_DIR}
  ${FLEX_INCLUDE_DIR}
  )

file(GLOB TXT_FILES "${CMAKE_SOURCE_DIR}/dataset/*.txt")
set(DATASET_FILES ${TXT_FILES} CACHE INTERNAL "List of dataset files")

# Copy policy configuration file to build directory
configure_file("${CMAKE_SOURCE_DIR}/policies.json" "${CMAKE_BINARY_DIR}/policies.json" COPYONLY)

include(CMakePrintHelpers)

add_subdirectory(libs/timeseries)
add_subdirectory(libs/testinfra)
add_subdirectory(libs/backtesting)
add_subdirectory(libs/concurrency)
add_subdirectory(libs/pasearchalgo)
add_subdirectory(libs/statistics)
add_subdirectory(libs/priceactionlab)
add_subdirectory(libs/patterndiscovery)
add_subdirectory(libraries/Alglib)
add_subdirectory(src/palvalidator)
add_subdirectory(src/palsetup)
add_subdirectory(src/palcodegen)
add_subdirectory(src/pattern_universe_generator)
add_subdirectory(src/palanalyzer)


file(GLOB SRC_LIST
   src/palvalidator/*.h
   src/palvalidator/*.hpp
   src/palvalidator/*.cpp
   src/palvalidator/*.txt
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

set_target_properties(${BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${BIN_NAME}
    priceactionlab
    statistics
    concurrency
    timeseries
    backtesting
    Alglib
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
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
    DEPENDS timeseries_unit_tests backtesting_unit_tests statistics_unit_tests priceactionlab_unit_tests patterndiscovery_unit_tests palvalidator_unit_tests pattern_generator_unit_tests palanalyzer_unit_tests
)

install(TARGETS PalValidator DESTINATION bin)
install(TARGETS palsetup DESTINATION bin)
install(TARGETS palcodegen DESTINATION bin)
install(TARGETS pattern_generator DESTINATION bin)
install(TARGETS palanalyzer DESTINATION bin)
install(FILES "${CMAKE_SOURCE_DIR}/policies.json" DESTINATION bin)
