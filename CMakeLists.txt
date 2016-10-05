project(PalValidator)
cmake_minimum_required(VERSION 2.8)
set(BIN_NAME ${PROJECT_NAME})


if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall  -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

find_package( Threads )
find_package(Boost REQUIRED COMPONENTS filesystem date_time chrono  system program_options regex thread)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libs/PALCodeGenLibrary-v2/ ${CMAKE_SOURCE_DIR}/libs/timeserieslib)
add_subdirectory(libs/PALCodeGenLibrary-v2/)
add_subdirectory(libs/timeserieslib)

set(SRC_LIST main/main.cpp)
add_executable(${PROJECT_NAME} ${SRC_LIST})
SET_TARGET_PROPERTIES(${BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${BIN_NAME}
    priceaction2
    backtest
    priceaction2
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT})

#add_executable(tryRandGen try_randgen.cpp)
#SET_TARGET_PROPERTIES(tryRandGen PROPERTIES LINKER_LANGUAGE CXX)
#target_link_libraries(tryRandGen
#    priceaction2
#    backtest
#    priceaction2
#    ${Boost_LIBRARIES}
#    ${CMAKE_THREAD_LIBS_INIT})
