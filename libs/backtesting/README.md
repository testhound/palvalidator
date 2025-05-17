# Trading System Backtester: A Roadmap

This document provides a roadmap to the C++ classes within the `libs/backtester` directory, outlining their responsibilities and how they interact to perform a trading system backtest.

## Introduction

The backtesting library is designed to simulate the performance of trading strategies on historical market data. It allows users to define strategies, manage portfolios of securities, process trading orders, and analyze the results of simulated trades. The system is built with a focus on extensibility, allowing for different types of strategies, order executions, and risk management techniques.

## Core Components

The backtesting system is orchestrated by a few central classes:

* **`BackTester` (`BackTester.h`)**:
    * **Responsibilities**: This is the main engine of the backtesting system. It drives the simulation loop day by day (or bar by bar for intraday strategies). It coordinates the interaction between the strategy, market data, and order processing. Key tasks include:
        * Managing the overall backtest period and stepping through time.
        * Providing the current market context (bar data) to the strategy.
        * Calling appropriate event handlers on the strategy for order generation.
        * Triggering the processing of pending orders.
        * Updating portfolio and position states.
    * **Interaction**: Works closely with `BacktesterStrategy` to get trading signals and with `StrategyBroker` to execute trades.

* **`BacktesterStrategy` (`BacktesterStrategy.h`)**:
    * **Responsibilities**: This is an abstract base class that defines the interface for all trading strategies. Users derive from this class to implement their specific trading logic. Core responsibilities include:
        * Defining methods for generating entry and exit orders based on market data and strategy rules (`eventEntryOrders`, `eventExitOrders`).
        * Accessing historical and current bar data for decision making.
        * Interacting with the `StrategyBroker` to place orders.
    * **Key Methods**:
        * `eventEntryOrders()`: Called by `BackTester` to allow the strategy to submit new entry orders.
        * `eventExitOrders()`: Called by `BackTester` to allow the strategy to submit exit orders for open positions.
        * `eventProcessPendingOrders()`: Called by `BackTester` to process pending orders through the `StrategyBroker`.
        * `eventUpdateSecurityBarNumber()`: Updates the bar count for lookback logic within strategies.
    * **Interaction**: Receives market data and timing events from `BackTester`. Sends trading orders to `StrategyBroker`.

* **`Portfolio` (`Portfolio.h`)**:
    * **Responsibilities**: Represents the collection of all securities (financial instruments) available for trading during the backtest. It holds the time series data for each security.
    * **Interaction**: Provides `Security` objects and their associated market data to the `BackTester` and `BacktesterStrategy`.

* **`Security` (`Security.h`)**:
    * **Responsibilities**: Represents a single tradable financial instrument (e.g., a stock, future). It holds the OHLC (Open, High, Low, Close) time series data for that instrument and its attributes.
    * **Associated Classes**:
        * `SecurityAttributes.h`: Defines properties of a security like tick size, point value, and trading hours.
        * `SecurityFactory.h` and `SecurityAttributesFactory.h`: Used to create and manage instances of securities and their attributes.
    * **Interaction**: Used by `Portfolio` to store market data. `BacktesterStrategy` accesses `Security` data to make trading decisions.

* **`StrategyBroker` (`StrategyBroker.h`)**:
    * **Responsibilities**: Acts as the intermediary between the trading strategy and the simulated market execution. It is responsible for:
        * Receiving order requests (buy, sell, stop-loss, profit-target) from the `BacktesterStrategy`.
        * Passing these orders to the `TradingOrderManager` for lifecycle management.
        * Updating `InstrumentPositionManager` when trades are filled.
        * Recording executed trades in `ClosedPositionHistory` and detailed transactions in `StrategyTransactionManager`.
        * Applying fills to orders based on market conditions.
    * **Interaction**: Receives orders from `BacktesterStrategy`. Uses `TradingOrderManager` to manage pending orders and `InstrumentPositionManager` to update positions. Logs trades to `ClosedPositionHistory` and `StrategyTransactionManager`.

## Order Management

The system features a comprehensive order management module:

* **`TradingOrder` and subtypes (`TradingOrder.h`)**:
    * **Responsibilities**: Defines the base class for all trading orders and its derived classes represent specific order types like:
        * `MarketOnOpenLongOrder`, `MarketOnOpenShortOrder`
        * `MarketOnOpenSellOrder`, `MarketOnOpenCoverOrder`
        * `SellAtLimitOrder`, `CoverAtLimitOrder`
        * `SellAtStopOrder`, `CoverAtStopOrder`
    * Manages the state of an order (e.g., Pending, Executed, Canceled) via the `TradingOrderState` hierarchy.
    * **Interaction**: Created by `BacktesterStrategy` (via `StrategyBroker`) and managed by `TradingOrderManager`.

* **`TradingOrderManager` (`TradingOrderManager.h`)**:
    * **Responsibilities**: Manages the lifecycle of all pending trading orders.
        * Stores active orders.
        * Processes pending orders against new market data (bars) to determine if they should be filled.
        * Notifies observers (like `StrategyBroker`) when orders are filled or canceled.
    * **Interaction**: Receives new orders from `StrategyBroker`. Uses `ProcessOrderVisitor` to evaluate fill conditions for each pending order against the current bar's OHLC data.

* **`ProcessOrderVisitor` (within `TradingOrderManager.h`)**:
    * **Responsibilities**: Implements the logic to determine if a specific order can be filled based on the current bar's data (Open, High, Low, Close prices). It encapsulates the fill rules for different order types (market, limit, stop).
    * **Interaction**: Used by `TradingOrderManager` to attempt to fill pending orders. It operates on `TradingOrder` objects.

## Position Management

Tracking and managing trading positions is crucial:

* **`InstrumentPosition` (`InstrumentPosition.h`)**:
    * **Responsibilities**: Represents the net position (e.g., long, short, or flat) for a single trading instrument. It can manage multiple units of a position (pyramiding).
    * Uses a state pattern (`InstrumentPositionState`, `FlatInstrumentPositionState`, `LongInstrumentPositionState`, `ShortInstrumentPositionState`) to handle behavior specific to the current position (e.g., adding to a long position, closing a short position).
    * Tracks details like entry prices, number of units, and potentially unrealized P&L.
    * **Interaction**: Managed by `InstrumentPositionManager`. Updated by `StrategyBroker` when trades occur.

* **`InstrumentPositionManager` (`InstrumentPositionManager.h`)**:
    * **Responsibilities**: Manages a collection of `InstrumentPosition` objects, one for each traded security. It provides a central point to query the current position status for any instrument in the portfolio.
    * **Interaction**: Used by `StrategyBroker` to update and query position states.

* **`TradingPosition` (`TradingPosition.h`)**:
    * **Responsibilities**: Represents a single unit of a trade, from entry to exit. It tracks entry/exit dates and prices, profit/loss, and other metrics for that specific trade instance. This class also uses a state pattern (`OpenLongPosition`, `OpenShortPosition`, etc.) to manage its lifecycle.
    * **Interaction**: Created when an entry order is filled. Its state transitions to closed when an exit order is filled. Closed positions are then typically moved to `ClosedPositionHistory`.

* **`ClosedPositionHistory` (`ClosedPositionHistory.h`)**:
    * **Responsibilities**: Stores a record of all closed trades. This is essential for performance analysis, calculating statistics like profit factor, win rate, etc.
    * **Interaction**: `StrategyBroker` adds `TradingPosition` objects to this history once they are closed.

* **`StrategyTransaction` (`StrategyTransaction.h`) & `StrategyTransactionManager` (`StrategyTransactionManager.h`)**:
    * **Responsibilities**: These classes are responsible for logging every individual transaction (e.g., entry fill, exit fill, commission) that occurs during the backtest, providing a detailed audit trail.
    * **Interaction**: `StrategyBroker` records transactions here.

## Strategy Implementation Example: PALStrategy

The library includes an example strategy implementation:

* **`PalStrategy` (`PalStrategy.h`)**:
    * **Responsibilities**: A concrete implementation of `BacktesterStrategy` that trades based on patterns defined by the Price Action Lab (PAL) methodology.
    * It likely uses `PALPatternInterpreter` and `PalAst` (Abstract Syntax Tree) to parse and understand trading rules defined in external files (e.g., `.txt` files defining price patterns).
    * **Associated Classes**:
        * `PALPatternInterpreter.h`: Interprets the parsed PAL patterns against market data to generate trading signals.
        * `PalAst.h` (likely from `libs/priceactionlab`): Defines the structure for the Abstract Syntax Tree representing the parsed trading rules.

## Risk Management

The system includes components for managing trade exits:

* **`ProfitTarget` (`ProfitTarget.h`)**:
    * **Responsibilities**: Defines and manages profit target exit conditions for open positions. This can be based on fixed price levels or percentage gains.
    * **Interaction**: Used by `BacktesterStrategy` or `StrategyBroker` to generate exit orders when profit targets are met.

* **`StopLoss` (`StopLoss.h`)**:
    * **Responsibilities**: Defines and manages stop-loss exit conditions for open positions to limit potential losses. This can be based on fixed price levels or percentage losses.
    * **Interaction**: Used by `BacktesterStrategy` or `StrategyBroker` to generate exit orders when stop-loss levels are breached.

## Backtest Workflow / Interaction Flow

A typical backtest proceeds as follows:

1.  **Initialization**:
    * The `BackTester` is initialized with a date range, a `Portfolio` of `Security` objects (each with its time series data), and one or more `BacktesterStrategy` instances.
    * The `StrategyBroker` is set up, which in turn initializes its `TradingOrderManager`, `InstrumentPositionManager`, `ClosedPositionHistory`, and `StrategyTransactionManager`.

2.  **Simulation Loop**: The `BackTester` iterates through each trading day (or bar) in the specified date range. For each bar:
    * **Market Update**: The current bar's OHLC data for each security is made available.
    * **Strategy Exits**: The `BackTester` calls the `eventExitOrders()` method of the `BacktesterStrategy`. The strategy evaluates its open positions and, if exit conditions are met (e.g., stop-loss hit, profit target reached, pattern-based exit), it requests exit orders via the `StrategyBroker`.
    * **Strategy Entries**: The `BackTester` calls the `eventEntryOrders()` method of the `BacktesterStrategy`. The strategy evaluates current market conditions and its rules to determine if new entry orders should be placed. If so, it requests entry orders via the `StrategyBroker`.
    * **Order Processing**: The `BackTester` (often by calling `BacktesterStrategy::eventProcessPendingOrders()`, which delegates to `StrategyBroker`) triggers the `TradingOrderManager` to process all pending orders (both new and existing).
        * The `TradingOrderManager` uses a `ProcessOrderVisitor`, configured with the current bar's data, to check each pending order.
        * If an order's fill conditions are met (e.g., market price touches limit price, stop price is breached), the `ProcessOrderVisitor` marks the order as executed, providing the fill price and date.
    * **Position Updates & Logging**:
        * The `TradingOrderManager` notifies the `StrategyBroker` of any filled orders.
        * The `StrategyBroker` updates the relevant `InstrumentPosition` in the `InstrumentPositionManager` (e.g., opening a new position, closing an existing one, adding to a position).
        * If a `TradingPosition` (a single unit of a trade) is closed, it's moved to the `ClosedPositionHistory`.
        * All transactions (fills, commissions if modeled) are logged by the `StrategyTransactionManager`.
    * **Bar Update**: The `BacktesterStrategy` may update its internal bar count for each security using `eventUpdateSecurityBarNumber()`.

3.  **Post-Simulation**: After the loop completes:
    * The `ClosedPositionHistory` and `StrategyTransactionManager` contain all the data for performance analysis.
    * The `BackTester` or associated analysis classes can then calculate various metrics (total P&L, profit factor, drawdown, etc.).

## Key Data Structures & Dependencies

* **`OHLCTimeSeries` (from `mkc_timeseries` namespace, likely in `libs/timeseries/TimeSeries.h`)**: This is a fundamental data structure used throughout the system to store and access historical open, high, low, and close prices, as well as volume and open interest for each security.
* **`Decimal` (from `mkc_timeseries` namespace, likely based on `libs/priceactionlab/decimal.h` or `libs/timeseries/decimal.h`)**: A custom decimal number type used for all financial calculations to avoid floating-point inaccuracies.
* The `libs/backtester` module has conceptual dependencies on:
    * `libs/timeseries`: For managing and accessing time series data.
    * `libs/priceactionlab` (if `PalStrategy` is used): For parsing and interpreting PAL trading patterns.

This roadmap should provide a good starting point for understanding the architecture and flow of the backtesting system. Each header file mentioned contains detailed comments that further explain the responsibilities and collaborations of the classes.
