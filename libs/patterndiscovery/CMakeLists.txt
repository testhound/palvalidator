set(LIB_NAME patterndiscovery)

# Pattern Discovery Library source files
set(LIB_SOURCES
    PatternTemplate.cpp
    BinaryPatternTemplateDeserializer.cpp
    BinaryPatternTemplateSerializer.cpp
    PatternUniverseSerializer.cpp
    PatternUniverseDeserializer.cpp
)

# Create the library
add_library(${LIB_NAME} STATIC ${LIB_SOURCES})

# Set target properties
set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(${LIB_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against required dependencies
target_link_libraries(${LIB_NAME}
    PUBLIC
        timeseries
        priceactionlab
        backtesting
        concurrency
        ${Boost_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
)

# Compiler-specific options for consumers
target_compile_options(${LIB_NAME} PUBLIC
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Wpedantic -O3 -march=native
    >
)

# Build unit tests
file(GLOB TEST_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
)

add_executable(${LIB_NAME}_unit_tests
    ${TEST_LIST}
)

find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${LIB_NAME})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE backtesting)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE timeseries)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE priceactionlab)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE concurrency)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE testinfra)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE cpptrace::cpptrace)
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIB_NAME}_unit_tests PRIVATE ${CMAKE_THREAD_LIBS_INIT})

enable_testing()
add_test(NAME ${LIB_NAME}_unit_tests COMMAND ${LIB_NAME}_unit_tests)

file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
install(TARGETS ${LIB_NAME} DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")