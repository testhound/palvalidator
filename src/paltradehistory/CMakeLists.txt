# PalTradeHistory executable
add_executable(paltradehistory
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/palvalidator/ValidatorConfiguration.cpp
)

# Include directories
target_include_directories(paltradehistory PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/timeseries
    ${CMAKE_SOURCE_DIR}/libs/backtesting
    ${CMAKE_SOURCE_DIR}/libs/priceactionlab
    ${CMAKE_SOURCE_DIR}/src/palvalidator
)

# Link required libraries
target_link_libraries(paltradehistory
    timeseries
    backtesting
    priceactionlab
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
)

# Set target properties
set_target_properties(paltradehistory PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add to tests if needed
if(BUILD_TESTING)
    # Add any specific tests for paltradehistory here if needed
endif()