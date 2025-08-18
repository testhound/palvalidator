# Define the library name
set(LIB_NAME palanalyzer)

# Collect all source files except main.cpp for the library
file(GLOB LIB_SRC_LIST
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)
list(REMOVE_ITEM LIB_SRC_LIST ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

# Collect header files
file(GLOB HEADER_LIST
    ${CMAKE_CURRENT_LIST_DIR}/*.h
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
)

# Create static library from source files (excluding main.cpp)
add_library(${LIB_NAME} STATIC ${LIB_SRC_LIST} ${HEADER_LIST})

# Include directories for the library
target_include_directories(${LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}   # Add current source directory for headers
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/priceactionlab
    ${CMAKE_SOURCE_DIR}/libs/patterndiscovery  # Add pattern discovery library
    ${Boost_INCLUDE_DIR}
    ${RAPIDJSON_DIR}
)

# Link against required libraries
target_link_libraries(${LIB_NAME} PUBLIC
    patterndiscovery        # Add dependency on pattern discovery library
    priceactionlab          # PAL parser and AST infrastructure
    ${Boost_LIBRARIES}      # Filesystem, program_options, etc.
    ${CMAKE_THREAD_LIBS_INIT}
)

# Set C++17 standard for the library
set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Create the main executable
add_executable(${LIB_NAME}_exe main.cpp)

# Link the executable to the library
target_link_libraries(${LIB_NAME}_exe PRIVATE ${LIB_NAME})

# Set C++17 standard for the executable
set_target_properties(${LIB_NAME}_exe PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME ${LIB_NAME}  # Keep the original executable name
)

# Install the executable
install(TARGETS ${LIB_NAME}_exe DESTINATION bin)

# Build unit tests
file(GLOB TEST_LIST
    ${CMAKE_CURRENT_LIST_DIR}/test/*.cpp
)

if(TEST_LIST)
    add_executable(${LIB_NAME}_unit_tests ${TEST_LIST})
    
    find_package(Catch2 CONFIG REQUIRED)
    
    target_link_libraries(${LIB_NAME}_unit_tests PRIVATE
        ${LIB_NAME}  # Link to our static library
        Catch2::Catch2WithMain
        testinfra
    )
    
    set_target_properties(${LIB_NAME}_unit_tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    enable_testing()
    add_test(NAME ${LIB_NAME}_unit_tests COMMAND ${LIB_NAME}_unit_tests)
    
    # Set working directory for tests
    set_tests_properties(${LIB_NAME}_unit_tests
        PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()