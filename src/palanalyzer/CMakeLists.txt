set(EXECUTABLE_NAME palanalyzer)

# Collect all source files
file(GLOB SRC_LIST
    ${CMAKE_CURRENT_LIST_DIR}/*.h
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

# Create the executable
add_executable(${EXECUTABLE_NAME} ${SRC_LIST})

# Include directories
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/priceactionlab
    ${Boost_INCLUDE_DIR}
    ${RAPIDJSON_DIR}
)

# Link against required libraries
target_link_libraries(${EXECUTABLE_NAME}
    priceactionlab          # PAL parser and AST infrastructure
    ${Boost_LIBRARIES}      # Filesystem, program_options, etc.
    ${CMAKE_THREAD_LIBS_INIT}
)

# Set C++17 standard
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Install the executable
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)

# Optional: Add unit tests for the analyzer components
if(BUILD_TESTING)
    file(GLOB TEST_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    )
    
    if(TEST_LIST)
        add_executable(${EXECUTABLE_NAME}_unit_tests ${TEST_LIST})
        
        target_link_libraries(${EXECUTABLE_NAME}_unit_tests PRIVATE
            priceactionlab
            ${Boost_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT}
            Catch2::Catch2WithMain
        )
        
        target_include_directories(${EXECUTABLE_NAME}_unit_tests PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/libs
            ${CMAKE_SOURCE_DIR}/libs/priceactionlab
            ${CMAKE_CURRENT_SOURCE_DIR}
        )
        
        set_target_properties(${EXECUTABLE_NAME}_unit_tests PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )
        
        enable_testing()
        add_test(NAME ${EXECUTABLE_NAME}_unit_tests COMMAND ${EXECUTABLE_NAME}_unit_tests)
        
        # Set working directory for tests
        set_tests_properties(${EXECUTABLE_NAME}_unit_tests
            PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()
endif()