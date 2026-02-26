cmake_minimum_required(VERSION 3.5)

project(PalValidator VERSION 2.0.0 LANGUAGES CXX)

# Extract Git information for version header
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(GIT_COMMIT_HASH "unknown")
    set(GIT_DESCRIBE "unknown")
    set(GIT_BRANCH "unknown")
endif()

# Generate version header from template
configure_file(
    ${CMAKE_SOURCE_DIR}/include/version.h.in
    ${CMAKE_BINARY_DIR}/include/version.h
    @ONLY
)

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

# Base flags for all build types
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0 -Wall -march=native")

# Set build-type-specific flags (these replace CMake's defaults)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_COVERAGE "-O0 -g -pg")

# Set linker flags for Coverage build
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -pg")
endif()

find_package(Threads REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS filesystem date_time chrono system program_options regex thread container)

# Find CURL
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Find RapidJSON - try different methods
find_path(RAPIDJSON_INCLUDE_DIR
    NAMES rapidjson/rapidjson.h
    PATHS ${RAPIDJSON_DIR}
          /usr/include
          /usr/local/include
          /opt/homebrew/include
    PATH_SUFFIXES rapidjson
)

if(NOT RAPIDJSON_INCLUDE_DIR)
    message(FATAL_ERROR "RapidJSON not found. Please install rapidjson-dev or set RAPIDJSON_DIR")
endif()

include_directories(${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/include
  ${Boost_INCLUDE_DIR}
  ${BLOOMBERG_DECIMAL_INCLUDES}
  ${RAPIDJSON_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
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
add_subdirectory(src/palvalidator)
add_executable(bcastats src/bcastats/main.cpp)
target_include_directories(bcastats PRIVATE ${CMAKE_SOURCE_DIR}/libs/timeseries ${CMAKE_SOURCE_DIR}/libs/statistics)
target_link_libraries(bcastats PRIVATE timeseries statistics)
add_subdirectory(src/palsetup)
add_subdirectory(src/palcodegen)
add_subdirectory(src/palfilecompare)
add_subdirectory(src/pattern_universe_generator)
add_subdirectory(src/palanalyzer)
add_subdirectory(src/paltradehistory)


# Note: PalValidator executable is now created in src/palvalidator/CMakeLists.txt

    #list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

#add_subdirectory(tests)

find_package(Catch2 CONFIG REQUIRED)

add_library(Catch INTERFACE)

add_custom_target(tests
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS timeseries_unit_tests backtesting_unit_tests statistics_unit_tests priceactionlab_unit_tests patterndiscovery_unit_tests palvalidator_unit_tests pattern_generator_unit_tests palanalyzer_unit_tests concurrency_unit_tests
)

install(TARGETS PalValidator DESTINATION bin)
install(TARGETS palsetup DESTINATION bin)
install(TARGETS palcodegen DESTINATION bin)
install(TARGETS palfilecompare DESTINATION bin)
install(TARGETS pattern_generator DESTINATION bin)
install(TARGETS palanalyzer DESTINATION bin)
install(TARGETS paltradehistory DESTINATION bin)
install(TARGETS bcastats DESTINATION bin)
install(FILES "${CMAKE_SOURCE_DIR}/policies.json" DESTINATION bin)
