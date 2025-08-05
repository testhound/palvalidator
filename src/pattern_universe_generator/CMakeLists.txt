set(LIB_NAME pattern_generator)

file(GLOB SRC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.txt
)

add_executable(${LIB_NAME} ${SRC_LIST})

target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${LIB_NAME} PRIVATE
    patterndiscovery
    concurrency
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# Build unit tests
file(GLOB TEST_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

add_executable(${LIB_NAME}_unit_tests
    ${TEST_LIST}
)

find_package(Catch2 CONFIG REQUIRED)

target_include_directories(${LIB_NAME}_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE patterndiscovery)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE concurrency)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE testinfra)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE cpptrace::cpptrace)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${CMAKE_THREAD_LIBS_INIT})

enable_testing()
add_test(NAME ${LIB_NAME}_unit_tests COMMAND ${LIB_NAME}_unit_tests)

file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})