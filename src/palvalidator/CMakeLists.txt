set(LIB_NAME palvalidator)

file(GLOB_RECURSE SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
)

# Exclude test files from the main library
list(FILTER SRC_LIST EXCLUDE REGEX ".*/test/.*")

add_library(${LIB_NAME} STATIC ${SRC_LIST})
target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/filtering
    ${CMAKE_CURRENT_SOURCE_DIR}/reporting
)
target_include_directories(${LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/filtering
    ${CMAKE_CURRENT_SOURCE_DIR}/reporting
)
target_link_libraries(${LIB_NAME} PRIVATE priceactionlab)
target_link_libraries(${LIB_NAME} PRIVATE backtesting)
target_link_libraries(${LIB_NAME} PRIVATE statistics)

add_executable(PalValidator main.cpp)
target_link_libraries(PalValidator PRIVATE
    ${LIB_NAME}
    priceactionlab
    backtesting
    statistics
    concurrency
    timeseries
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CURL_LIBRARIES}
)

file(GLOB TEST_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
)

add_executable(${LIB_NAME}_unit_tests ${TEST_LIST})

find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(${LIB_NAME}_unit_tests PRIVATE statistics)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE "-lcurl")
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE testinfra)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE palvalidator)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE backtesting)

enable_testing()

add_test(NAME ${LIB_NAME}_test COMMAND ${LIB_NAME}_unit_tests)

file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Copy config directory for tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/config)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    # Also copy to the binary root in case tests run from there
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test/config DESTINATION ${CMAKE_BINARY_DIR})
endif()
