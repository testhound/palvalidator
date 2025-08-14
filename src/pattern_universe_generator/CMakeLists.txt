# Pattern Universe Generator Library
cmake_minimum_required(VERSION 3.1)

# Define the library
add_library(pattern_universe_generator
    OptimizedDataStructures.cpp
    PALAnalysisLoader.cpp
    CuratedGroupManager.cpp
    OptimizedPatternUniverseGenerator.cpp
    PatternHashGenerator.cpp
)

# Set target properties
set_target_properties(pattern_universe_generator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(pattern_universe_generator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/concurrency  # For parallel execution
)

# Link dependencies
target_link_libraries(pattern_universe_generator
    PUBLIC
        concurrency      # For parallel execution capabilities
        patterndiscovery # For PatternTemplate and serialization
        ${Boost_LIBRARIES}
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pattern_universe_generator PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 -march=native>
        $<$<CONFIG:Debug>:-g -O0>
    )
endif()

# Add main executable for the pattern universe generator
add_executable(pattern_generator
    main.cpp
)

target_link_libraries(pattern_generator
    PRIVATE
        pattern_universe_generator
        ${Boost_LIBRARIES}
)

# Set properties for main executable
set_target_properties(pattern_generator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Build unit tests
file(GLOB TEST_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

add_executable(pattern_generator_unit_tests
    ${TEST_LIST}
)

find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(pattern_generator_unit_tests PRIVATE pattern_universe_generator)
target_link_libraries(pattern_generator_unit_tests PRIVATE concurrency)
target_link_libraries(pattern_generator_unit_tests PRIVATE patterndiscovery)
target_link_libraries(pattern_generator_unit_tests PRIVATE testinfra)
target_link_libraries(pattern_generator_unit_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(pattern_generator_unit_tests PRIVATE cpptrace::cpptrace)
target_link_libraries(pattern_generator_unit_tests PRIVATE ${Boost_LIBRARIES})
target_link_libraries(pattern_generator_unit_tests PRIVATE ${CMAKE_THREAD_LIBS_INIT})

enable_testing()
add_test(NAME pattern_generator_unit_tests COMMAND pattern_generator_unit_tests)

# Copy dataset files and PAL analysis directory to build directory
file(COPY ${DATASET_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/dataset/pal_analysis DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dataset)

# Install targets
install(TARGETS pattern_universe_generator
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    OptimizedDataStructures.h
    PALAnalysisLoader.h
    CuratedGroupManager.h
    OptimizedPatternUniverseGenerator.h
    DESTINATION include/pattern_universe_generator
)

# Add to parent project if this is included as subdirectory
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # Export target for parent project
    set_property(GLOBAL APPEND PROPERTY PATTERN_UNIVERSE_GENERATOR_TARGETS pattern_universe_generator)
endif()