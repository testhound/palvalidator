set(LIB_NAME patterndiscovery)

file(GLOB TEST_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
)

add_executable(${LIB_NAME}_unit_tests ${TEST_LIST})

find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(${LIB_NAME}_unit_tests PRIVATE backtesting)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE statistics)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE cpptrace::cpptrace)
target_link_libraries(${LIB_NAME}_unit_tests ${Boost_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests "-lcurl")
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE testinfra)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE pasearchalgo)

enable_testing()

add_test(NAME ${LIB_NAME}_test COMMAND ${LIB_NAME}_unit_tests)

file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
