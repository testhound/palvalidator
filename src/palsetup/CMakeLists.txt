set(LIB_NAME palsetup)

# Collect source files explicitly to ensure proper compilation order
set(SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PalSetupTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/QuantizationAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UserInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TimeSeriesProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DirectoryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StatisticsCalculator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FileOperations.cpp
)

# Collect header files
file(GLOB HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_executable(${LIB_NAME} ${SRC_LIST} ${HEADER_LIST})
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${LIB_NAME} PRIVATE backtesting)
target_link_libraries(${LIB_NAME} PRIVATE statistics)
target_link_libraries(${LIB_NAME} PRIVATE timeseries)
