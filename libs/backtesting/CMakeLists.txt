set(LIB_NAME backtesting)

file(GLOB SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
)

add_library(${LIB_NAME} STATIC ${SRC_LIST})

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME} INTERFACE priceactionlab ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${LIB_NAME} PUBLIC statistics)
target_link_libraries(${LIB_NAME} PUBLIC concurrency)
target_link_libraries(${LIB_NAME} PUBLIC timeseries)

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
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${CURL_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE concurrency)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE timeseries)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE testinfra)

include(CTest)
enable_testing()
add_test(NAME ${LIB_NAME}_unit_tests COMMAND ${LIB_NAME}_unit_tests)

file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
install(TARGETS ${LIB_NAME} DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
