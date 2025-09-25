# PalValidator

PalValidator Project Overview

The `PalValidator` project is a comprehensive C++ framework designed for the backtesting, statistical validation, and code generation of trading strategies, particularly those defined using the Price Action Lab (PAL) pattern language. It provides robust tools for simulating strategy performance on historical market data, performing rigorous statistical analysis, and translating validated patterns into executable code for various trading platforms.

## Key Components and Modules

The project is modularized into several core libraries and source directories, each with distinct responsibilities:

### 1. Backtesting Library (`libs/backtesting`)

This library forms the foundation of the simulation engine. It is responsible for:

- Driving the backtesting loop over historical market data.
  
- Managing financial portfolios and individual instrument positions.
  
- Processing and executing various types of trading orders (market, limit, stop).
  
- Logging detailed trade history and calculating performance metrics.
  

### 2. Concurrency Library (`libs/concurrency`)

This library provides utilities for parallel task execution, optimizing the performance of computationally intensive operations within the framework, such as Monte Carlo simulations.

### 3. Price Action Search Algorithm Library (`libs/pasearchalgo`)

This component implements algorithms for discovering and evaluating Price Action Lab patterns. Its functionalities include:

- Processing and generating comparisons between patterns.
  
- Matching patterns against historical data.
  
- Managing the parameters and execution of pattern search runs.
  

### 4. Price Action Lab (PAL) Code Processing Library (`libs/priceactionlab`)

This crucial library handles the core logic for working with PAL patterns:

- **Parsing**: Reads and interprets PAL pattern definition files.
  
- **Abstract Syntax Tree (AST) Generation**: Constructs an in-memory AST representation of the parsed patterns.
  
- **Code Generation**: Translates the AST into executable trading strategy code for multiple platforms, including EasyLanguage, QuantConnect, TradingBlox, and WealthLab.
  

### 5. Statistics Library (`libs/statistics`)

Dedicated to the statistical validation and robustness testing of trading strategies, this library offers:

- Implementations of various multiple testing correction procedures (e.g., Masters' methods, Romano-Wolf, Benjamini-Hochberg).
  
- Tools for Monte Carlo simulations to assess statistical significance.
  
- Utilities for calculating key statistical measures (e.g., Profit Factor, Profitability).
  

### 6. Time Series Library (`libs/timeseries`)

This foundational library provides robust mechanisms for handling time series data, especially OHLC (Open-High-Low-Close) financial data. It includes:

- Efficient readers and writers for various CSV data formats.
  
- Utilities for discovering and managing different time frames.
  
- Functions for calculating common technical indicators.
  

### 7. Source Applications (`src/`)

- **`src/palcodegen`**: Contains the main application for generating trading strategy code from PAL pattern definitions.
  
- **`src/palsetup`**: Provides utilities for setting up the project, potentially including data preparation or configuration file generation.
  
- **`src/palvalidator`**: Houses the primary application logic for validating strategies, managing policy configurations, and dynamically selecting validation methods based on user input or predefined settings.
  

### 8. Data and Examples

- **`data/` and `dataset/`**: These directories contain various sample financial data files (e.g., daily and hourly data for different symbols like QQQ, SSO, MSFT, AMZN, GILD) used for testing, development, and demonstrating the framework's capabilities.
  
- **`example/`**: Contains example configuration files and invocation scripts that illustrate how to use different components and set up specific backtesting or validation scenarios.

## Dependencies

PalValidator requires the following dependencies to be installed on your system:

### Required Dependencies
- **C++17 compatible compiler** (GCC 7+ or Clang 5+)
- **CMake 3.1+**
- **Boost Libraries** (filesystem, date_time, chrono, system, program_options, regex, thread, container)
- **RapidJSON** - High-performance JSON parsing library
- **libcurl** - HTTP client library for network operations
- **Doxygen** - For generating documentation
- **Bison** - Parser generator (for PAL pattern parsing)
- **Flex** - Lexical analyzer generator

### Installing Dependencies

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libboost-all-dev rapidjson-dev libcurl4-openssl-dev doxygen bison flex pkg-config catch2 graphviz
```

#### macOS (using Homebrew):
```bash
brew install cmake boost rapidjson curl doxygen bison flex
```

#### Setting Environment Variables
If dependencies are installed in non-standard locations, set these environment variables:
```bash
export RAPIDJSON_DIR=/path/to/rapidjson/include
export CURL_DIR=/path/to/curl/include
```

## How to build executables

For production, use:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
```

For debug & profiling puposes, use the following instead:
```
mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## How to build & view the doc

```
cd build
make doc
open ./html/index.html
```

`open` will use your default web browser.

## How to build & run tests

```
cd build
make tests
```

To run a specific test, first check the test name:
```
./libs/{LIB}/{LIB}_unit_tests --list-tests
```
Then simply run
```
./libs/{LIB}/{LIB}_unit_tests {TEST_NAME}
```

## How to run PalValidator

```
cd build
./PalValidator config.cfg
```
