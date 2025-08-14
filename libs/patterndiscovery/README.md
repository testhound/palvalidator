# Pattern Discovery Library

A high-performance, header-only template library for price action pattern discovery, designed to integrate seamlessly with the Price Action Lab ecosystem.

## Overview

The Pattern Discovery Library provides a comprehensive framework for configuring and managing pattern discovery searches. It implements the foundational components needed for the High-Performance Price Action Discovery Engine, focusing on:

- **Template-based design** for decimal precision flexibility
- **Header-only implementation** for optimal performance
- **Price Action Lab compatibility** using existing AST and decimal systems
- **Immutable object design** following object-oriented principles
- **Comprehensive validation** and error handling

## Architecture

### Core Components

#### 1. SearchTypes (`SearchTypes.h`)
Defines enumerations and utility types for pattern discovery:
- `SearchType`: Pattern search complexity levels (BASIC, EXTENDED, DEEP, etc.)
- `PatternFormationMethod`: EPS (Exhaustive Pattern Search) methodology
- `OHLCComponent`: Price bar components (Open, High, Low, Close)
- `InequalityRule`: Pattern condition definitions (greater-than only, per PAL AST)

#### 2. TradeParameters (`TradeParameters.h`)
Template class encapsulating trade configuration:
```cpp
TradeParameters<Decimal> tradeParams(
    SearchType::EXTENDED,
    Decimal("6.0"),  // profit target
    Decimal("3.0"),  // stop loss
    Decimal("0.55"), // min win rate
    20,              // min trades
    4                // max consecutive losses
);
```

#### 3. PerformanceCriteria (`PerformanceCriteria.h`)
Template class defining pattern evaluation criteria based on PAL methodology:
```cpp
PerformanceCriteria<Decimal> perfCriteria(
    20,              // min trades
    4,               // max consecutive losses
    Decimal("2.0"),  // min payoff ratio
    Decimal("5.0"),  // min long profitability
    Decimal("5.0")   // min short profitability
);
```

#### 4. SearchLine (`SearchLine.h`)
Template class representing a complete search configuration:
```cpp
SearchLine<Decimal> searchLine(1, tradeParams, perfCriteria, "Extended Search");
```

## Key Features

### Decimal Type Integration
- Seamless integration with `dec::decimal<Prec>` system
- Uses `num::DefaultNumber` (`dec::decimal<7>`) by default
- Template design supports any decimal precision

### Delay Pattern Support
- Configurable delay patterns (1-5 bars, per PAL limitations)
- Automatic validation of delay ranges
- Disabled by default (immediate execution)

### PAL Methodology Compliance
- Profit factor filtering (default minimum: 1.85)
- Separate profitability criteria for long/short patterns
- PAL profitability formula: `(profit factor / (profit factor + payoff ratio)) * 100.0`
- Greater-than only comparisons (aligned with existing PAL AST)

### Performance Integration
- Designed to work with `libs/backtesting` for performance metrics
- No redundant calculation methods - relies on backtesting library
- Clean separation of concerns between pattern discovery and performance analysis

## Usage Examples

### Basic Usage
```cpp
#include "PatternDiscovery.h"
#include "number.h"

using namespace mkc_patterndiscovery;
using Decimal = num::DefaultNumber;

// Create basic search configuration
auto basicSearch = PatternDiscoveryUtils<Decimal>::createBasicSearchLine(
    1,                   // search ID
    Decimal("5.0"),      // profit target
    Decimal("2.5"),      // stop loss
    Decimal("3.0"),      // min long profitability
    Decimal("3.0"),      // min short profitability
    "Basic Pattern Search"
);

// Evaluate a pattern
bool qualifies = basicSearch.evaluatePattern(
    25,                  // total trades
    3,                   // consecutive losses
    Decimal("2.0"),      // payoff ratio
    Decimal("2.0"),      // profit factor
    Decimal("4.0"),      // profitability
    true                 // is long pattern
);
```

### Advanced Configuration
```cpp
// Create custom trade parameters with delay patterns
TradeParameters<Decimal> tradeParams(
    SearchType::DEEP,
    Decimal("8.0"),      // profit target
    Decimal("4.0"),      // stop loss
    Decimal("0.60"),     // min win rate
    30,                  // min trades
    3,                   // max consecutive losses
    true                 // enable delay patterns
);

// Create strict performance criteria
PerformanceCriteria<Decimal> perfCriteria(
    30,                  // min trades
    3,                   // max consecutive losses
    Decimal("2.0"),      // min payoff ratio
    Decimal("8.0"),      // min long profitability
    Decimal("6.0"),      // min short profitability
    Decimal("2.5")       // min profit factor
);

// Create search line
SearchLine<Decimal> deepSearch(100, tradeParams, perfCriteria, "Deep Search with Delays");
```

## Search Types and Pattern Groups

| Search Type | Pattern Groups | Bar Range | Description |
|-------------|----------------|-----------|-------------|
| BASIC       | 20             | 2-6       | Fundamental patterns |
| EXTENDED    | 120            | 2-6       | Comprehensive standard search |
| DEEP        | 545            | 2-9       | Complete deep search |
| CLOSE       | 67             | 3-9       | Close-only patterns |
| MIXED       | 172            | 2-9       | All OHLC combinations |
| HIGH_LOW    | 153            | 3-9       | High/Low focus patterns |
| OPEN_CLOSE  | 153            | 3-9       | Open/Close focus patterns |

## Integration with Existing Libraries

### Price Action Lab AST
- Uses existing `PriceBarReference::ReferenceType` enumerations
- Supports `GreaterThanExpr` only (per PAL AST limitations)
- Compatible with `AstFactory` for AST node creation

### Backtesting Library
- Relies on `ClosedPositionHistory` for profit factor calculations
- Uses `StrategyTransactionManager` for detailed performance metrics
- No duplicate calculation methods - clean separation of concerns

### Time Series Library
- Uses `dec::decimal<Prec>` for all financial calculations
- Compatible with `num::DefaultNumber` (`dec::decimal<7>`)
- Integrates with `OHLCTimeSeries` data structures

## Testing

The library includes comprehensive unit tests in `test/PatternDiscoveryTest.cpp`:
- Template functionality verification
- Decimal type integration testing
- Validation logic testing
- Delay pattern functionality testing
- Utility function testing

Run tests using the standard CMake test framework:
```bash
cd build
make test
```

## Design Principles

### Object-Oriented Design
- Immutable objects with const member variables
- No setter methods - configuration set at construction
- Validation performed during object creation

### Template Design
- Header-only implementation for optimal performance
- Support for different decimal precision types
- Compile-time type safety

### Separation of Concerns
- Pattern discovery logic separate from performance calculation
- Clean interfaces between components
- Minimal dependencies on external libraries

## Future Extensions

The library is designed for extensibility:
- Value Chart indicator patterns
- Volume-based pattern conditions
- Multi-timeframe pattern analysis
- Parallel processing integration
- Custom pattern template definitions

## Version Information

- **Version**: 1.0.0
- **Type**: Header-only template library
- **Dependencies**: `libs/timeseries` (decimal types), `libs/testinfra` (testing)
- **Compatibility**: C++11 and later

## License

This library is part of the Price Action Lab ecosystem and follows the same licensing terms as the parent project.