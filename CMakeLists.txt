project(PalValidator)
cmake_minimum_required(VERSION 2.8)
set(BIN_NAME ${PROJECT_NAME})

set(USE_BLOOMBERG_DECIMALS false)

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

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall  -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -march=native")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

find_package( Threads )
find_package(Boost REQUIRED COMPONENTS filesystem date_time chrono  system program_options regex thread)
include_directories(${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/
  ${CMAKE_SOURCE_DIR}/libs/timeserieslib
  ${CMAKE_SOURCE_DIR}/libs/pasearchalgolib
  ${BLOOMBERG_DECIMAL_INCLUDES}
  )

add_subdirectory(libs/PALCodeGenLibrary-v2/)
add_subdirectory(libs/timeserieslib)
add_subdirectory(libs/pasearchalgolib)

set(SRC_LIST main/main2.cpp)
add_executable(${PROJECT_NAME} ${SRC_LIST})
set_target_properties(${BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${BIN_NAME}
    priceaction2
    backtest
    pasearchalgo
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${BLOOMBERG_DECIMAL_LIBRARIES}
    )

