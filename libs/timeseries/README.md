# Time Series Library (`libs/timeseries`)

This document provides a roadmap to the C++ classes and utilities within the `libs/timeseries` directory. This library is designed for robust handling, manipulation, and generation of time series data, with a particular focus on financial Open-High-Low-Close (OHLC) data.

## Introduction

The `timeseries` library provides the foundational tools for representing and working with sequences of data points indexed by time. It is crucial for applications involving historical data analysis, such as financial backtesting, market data processing, and synthetic data generation for testing trading strategies. The library emphasizes precision in financial calculations through a custom decimal type and offers utilities for common time series operations.

## Core Components

The main components of the `timeseries` library include:

* **`OHLCTimeSeries` (`TimeSeries.h`)**:
    * **Responsibilities**: This is the primary class for storing and managing financial time series data. It holds a sequence of `OHLCTimeSeriesEntry` objects, representing individual bars (e.g., daily, hourly). It supports operations like adding entries, iterating through data in sorted or random-access order, and extracting specific data streams (e.g., close prices).
    * **Interaction**: Provides time series data to various consumers, including analytical tools (like `TimeSeriesIndicators`) and data generation utilities (`SyntheticTimeSeries`). It is a fundamental data structure used by the `backtester` library.

* **`TimeSeriesEntry` (`TimeSeriesEntry.h`)**:
    * **Responsibilities**: Represents a single data point or bar in an OHLC time series. It stores the timestamp (date and time), open, high, low, close prices, and volume for a specific period. It also includes the time frame (e.g., daily, intraday) of the bar.
    * **Interaction**: Used as the element type within `OHLCTimeSeries`.

* **`NumericTimeSeries` (`TimeSeries.h`)**:
    * **Responsibilities**: A more generic time series class for storing sequences of single decimal values indexed by time (e.g., a series of closing prices, indicator values).
    * **Interaction**: Often derived from `OHLCTimeSeries` (e.g., extracting just the close prices) or used to store the output of indicators.

* **Decimal Number Handling (`decimal.h`, `DecimalConstants.h`, `number.h`)**:
    * **Responsibilities**: Provides a custom decimal number type (`dec::decimal`) designed for high-precision financial calculations, avoiding floating-point inaccuracies. `DecimalConstants.h` defines commonly used decimal values (e.g., zero, one hundred), and `number.h` offers utility functions for decimal number conversions and rounding (e.g., `Round2Tick`).
    * **Interaction**: Used throughout the library for all price and financial value representations.

* **Date and Time Frame Management**:
    * `DateRange.h`: Defines a period between two dates. Used for filtering or specifying periods for analysis or data generation.
    * `TimeFrame.h`: Defines an enumeration for different time series granularities (e.g., Intraday, Daily, Weekly).
    * `TimeFrameUtility.h`: Provides utility functions related to time frames, like converting a string representation to a `TimeFrame::Duration`.
    * `TimeFrameDiscovery.h`: Infers the common time intervals (bars per day) from an intraday time series.

## Key Functionalities

The library offers several key functionalities:

* **Data Input/Output**:
    * `TimeSeriesCsvReader.h`: Provides classes for reading time series data from CSV files in various formats (e.g., Price Action Lab format, TradeStation format, CSI Futures format). It handles parsing dates, times, and OHLCV data into `OHLCTimeSeries` objects.
    * `TimeSeriesCsvWriter.h`: Provides classes for writing `OHLCTimeSeries` data to CSV files, suitable for consumption by other tools or for archiving.
    * `DataSourceReader.h`: Defines an interface and concrete implementations (e.g., `FinnhubIOReader`, `BarchartReader`) for fetching financial time series data from external APIs. It typically converts the fetched JSON data into a temporary CSV format that can then be read by `TimeSeriesCsvReader`.

* **Synthetic Data Generation**:
    * `SyntheticTimeSeries.h`: Implements algorithms for generating synthetic (permuted) OHLC time series data based on an original series. This is useful for Monte Carlo simulations and robustness testing of trading strategies. It supports different algorithms for End-of-Day (EOD) and Intraday data.
        * The EOD algorithm typically permutes overnight changes and intraday (relative HLC) changes separately.
        * The Intraday algorithm involves more complex permutations, including shuffling bars within days, shuffling overnight gaps, and shuffling the order of days.
    * `SyntheticTimeSeriesCreator.h`: A utility to create lower-resolution (coarser-grained) OHLC time series from a higher-resolution source (e.g., creating daily bars from hourly bars).

* **Indicator Calculation (`TimeSeriesIndicators.h`)**:
    * **Responsibilities**: Provides functions to calculate common technical indicators or perform series transformations. Examples include:
        * `DivideSeries`: Element-wise division of two `NumericTimeSeries`.
        * `RocSeries`: Calculates the Rate of Change.
        * `Median`: Calculates the median value of a series.
        * `StandardDeviation`: Calculates the standard deviation.
        * `MedianAbsoluteDeviation`: Calculates a robust measure of statistical dispersion.
        * `RobustQn`: Implements the Qn robust scale estimator.
    * **Interaction**: Takes `NumericTimeSeries` or `OHLCTimeSeries` (often after conversion to `NumericTimeSeries`) as input and produces a `NumericTimeSeries` as output.

* **Random Number Generation & Shuffling Utilities**:
    * `RandomMersenne.h`: A wrapper around the PCG (Permuted Congruential Generator) random number generator for drawing random numbers.
    * `pcg_random.hpp`, `pcg_extras.hpp`, `pcg_uint128.hpp`: Core PCG library files providing a fast and statistically robust random number generation scheme. Includes support for 128-bit integers.
    * `randutils.hpp`: Provides utilities to simplify C++11 random number generation, including improved seed sequence generation (`seed_seq_fe`) and an easy-to-use API wrapper (`random_generator`).
    * `ShuffleUtils.h`: Contains utilities for shuffling elements in a vector, like the Fisher-Yates shuffle, used by `SyntheticTimeSeries`.

## Other Components

* **`McptConfigurationFileReader.h` (`McptConfigurationFileReader.h`)**:
    * **Responsibilities**: Reads configuration files specific to a Monte Carlo Permutation Test (MCPT) setup. It likely ties together a `BackTester`, `Security`, PAL patterns, date ranges, and data file paths for running such tests. This class indicates a specific application or testing methodology built upon the `timeseries` and `backtester` libraries.

* **Utility Classes**:
    * `MapUtilities.h`: Contains template helpers for working with `std::map`.
    * `ThrowAssert.hpp`: A lightweight assertion library that throws an exception on failure.
    * `VectorDecimal.h`: Simple wrapper around `std::vector` for `Decimal` types and `boost::gregorian::date`.
    * `csv.h`: A third-party header-only library for reading CSV files, used by `TimeSeriesCsvReader`.
    * `BoostDateHelper.h`: Helper functions for working with Boost date and time objects.
    * `TimeSeriesValidator.h`: Validates time series data, for example, checking for a consistent number of bars per day in intraday data.

## Build and Metadata

* `CMakeLists.txt`: Defines how the library is built using CMake.
* `test/`: This directory contains unit tests for various components of the `timeseries` library, ensuring their correctness and reliability.

## Dependencies

* **Boost C++ Libraries**: Extensively used for date-time operations (`boost::gregorian::date`, `boost::posix_time::ptime`), flat maps, and threading utilities (mutexes).
* **Catch2**: (Indicated by test files) A testing framework used for the unit tests.
