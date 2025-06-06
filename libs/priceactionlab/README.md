# Price Action Lab (PAL) Code Processing Library (`libs/priceactionlab`)

This document provides a roadmap to the C++ classes within the `libs/priceactionlab` directory. These classes are responsible for parsing Price Action Lab (PAL) pattern definition files, constructing an Abstract Syntax Tree (AST) representation, and then using this AST to generate trading strategy code for various platforms.

## Introduction

The `priceactionlab` library serves as the core engine for understanding and translating PAL's proprietary pattern language into executable trading strategies. It achieves this by:

1.  **Parsing**: Reading PAL files (`.txt` or similar) that define trading patterns and their associated parameters (e.g., profit targets, stop losses).
2.  **Abstract Syntax Tree (AST) Construction**: Building an in-memory tree representation (AST) of the parsed patterns. Each node in the tree corresponds to a component of a PAL pattern, such as a price comparison, logical operator, or entry/exit rule.
3.  **Code Generation**: Traversing the AST using the Visitor design pattern to generate strategy code for different trading platforms (e.g., EasyLanguage, TradingBlox, WealthLab, QuantConnect).

## Core Components

The library is structured around three main activities: Parsing, AST Representation, and Code Generation.

### 1. Parsing Engine

The parsing engine is responsible for reading PAL pattern definition files and translating them into a structured format (the AST).

* **`PalParseDriver` (`PalParseDriver.h`, `PalParseDriver.cpp`)**:
    * **Responsibilities**: Orchestrates the parsing process. It initializes and manages the scanner and parser, handles the input file stream, and serves as the entry point for parsing a PAL file. It also owns the `PriceActionLabSystem` object where parsed patterns are stored.
    * **Interaction**: Coordinates with `Scanner` to get tokens and with `PalParser` to build the AST. After successful parsing, it holds the `PriceActionLabSystem` containing all parsed patterns.

* **`Scanner` (`scanner.h`)**:
    * **Responsibilities**: This is the lexical analyzer (lexer), typically generated by Flex (`MkcPalAst_FlexLexer`). It reads the input PAL file character by character and groups characters into sequences called tokens (e.g., keywords like "IF", "THEN", operators like ">", numbers, identifiers).
    * **Interaction**: Provides a stream of tokens to the `PalParser`.

* **`PalParser` (`PalParser.hpp`, `PalParser.cpp`)**:
    * **Responsibilities**: This is the syntax analyzer, typically generated by Bison. It takes the stream of tokens from the `Scanner` and checks if they conform to the defined grammar of the PAL language. If the token sequence is valid, it constructs the Abstract Syntax Tree (AST) by invoking actions defined in the grammar rules, often using the `AstFactory` to create AST nodes.
    * **Interaction**: Receives tokens from `Scanner` and uses `PalParseDriver` (and `AstFactory`) to build the AST.

### 2. Abstract Syntax Tree (AST)

The AST is a hierarchical tree representation of the parsed PAL patterns. Each node in the tree represents a construct in the PAL language.

* **`PalAst.h`, `PalAst.cpp`**: This pair of files defines all the nodes of the AST.
    * **`PriceBarReference` (and derived classes like `PriceBarOpen`, `PriceBarClose`, `Roc1BarReference`, `IBS1BarReference`, etc.)**: Represent references to specific price bar components (Open, High, Low, Close) or indicator values at a certain bar offset.
    * **`PatternExpression` (and derived classes like `GreaterThanExpr`, `AndExpr`)**: Represent the logical conditions of a pattern (e.g., "Close > Open", "Condition1 AND Condition2").
    * **`MarketEntryExpression` (and derived classes like `LongMarketEntryOnOpen`, `ShortMarketEntryOnOpen`)**: Define the type of market entry (e.g., buy at open).
    * **`ProfitTargetInPercentExpression` (and derived classes `LongSideProfitTargetInPercent`, `ShortSideProfitTargetInPercent`)**: Define profit target levels as a percentage.
    * **`StopLossInPercentExpression` (and derived classes `LongSideStopLossInPercent`, `ShortSideStopLossInPercent`)**: Define stop-loss levels as a percentage.
    * **`PatternDescription`**: Stores metadata about a pattern, such as its source file, index, historical performance metrics (PL%, PS%, Trades, CL).
    * **`PriceActionLabPattern`**: Represents a complete trading pattern, aggregating its description, expression, entry, profit target, and stop loss. It also handles volatility and portfolio filter attributes.
    * **`PriceActionLabSystem` (`PriceActionLabSystem.cpp`, `PalAst.h`)**: Manages a collection of `PriceActionLabPattern` objects, organizing them into long and short patterns. It can use a `PatternTieBreaker` to resolve conflicts if multiple patterns with the same hash code are encountered.
    * **`AstFactory`**: A factory class responsible for creating and managing instances of AST nodes, often reusing common nodes (like price bar references for specific offsets or common decimal values) to save memory.

### 3. Code Generation (Visitor Pattern)

Code generation is achieved using the Visitor design pattern. A base visitor class defines the interface, and concrete visitor classes implement the logic to generate code for specific trading platforms.

* **`PalCodeGenVisitor` (`PalCodeGenVisitor.h`, `PalCodeGenVisitor.cpp`)**:
    * **Responsibilities**: An abstract base class defining the `visit()` interface for each type of AST node. Derived classes implement these `visit()` methods to generate platform-specific code.
    * **Key Methods**: `visit(PriceBarOpen*)`, `visit(GreaterThanExpr*)`, `visit(PriceActionLabPattern*)`, etc.

* **Concrete Code Generator Visitors**:
    * **`EasyLanguageCodeGenVisitor` (`EasyLanguageFromTemplateCodeGen.cpp`, `EasyLanguageCodeGenVisitor.cpp` - note: `EasyLanguageCodeGenVisitor.cpp` seems to be an older version or a different approach, while `EasyLanguageFromTemplateCodeGen.cpp` implements the template-based generation).**:
        * Generates EasyLanguage code, typically for TradeStation. The `EasyLanguageFromTemplateCodeGen.cpp` version processes a template file and injects pattern-specific code into marked sections.
        * Specialized versions like `EasyLanguageRADCodeGenVisitor` and `EasyLanguagePointAdjustedCodeGenVisitor` handle specific strategy types by overriding methods for setting stops and targets.
    * **`TradingBloxCodeGenVisitor` (`TradingBloxCodeGenerator.cpp`)**:
        * Generates script code for the TradingBlox™ platform.
        * Includes derived classes `TradingBloxRADCodeGenVisitor` and `TradingBloxPointAdjustedCodeGenVisitor` for RAD and Point Adjusted strategies, respectively, which customize variable declarations and stop/target logic.
    * **`WealthLabCodeGenVisitor` (`WealthLabCodeGenerator.cpp`)**:
        * Generates script code for the WealthLab platform.
        * Also has `WealthLabRADCodeGenVisitor` and `WealthLabPointAdjustedCodeGenVisitor` for specialized strategy types.
    * **`QuantConnectCodeGenVisitor` (`QuantConnectCodeGenerator.cpp`)**:
        * Generates C# code for the QuantConnect platform.
        * `QuantConnectEquityCodeGenVisitor` is a specialization, though its specific logic for equity appears to be stubbed out in the provided file.
    * **`PalCodeGenerator` (`PalCodeGenerator.cpp`)**:
        * A generic visitor that generates a textual, human-readable representation of the PAL patterns, rather than code for a specific platform. Useful for debugging or understanding the parsed patterns.

### 4. Key Data Structures & Helper Classes

* **`StopTargetDetail` (`StopTargetDetail.h`)**:
    * A class to hold stop-loss, profit-target levels, and minimum/maximum holding periods.
    * `StopTargetDetailReader` is a utility to read these details from a CSV file for different strategy "deviations". This is particularly used by some code generators (e.g., `EasyLanguageCodeGenVisitor`) to handle different stop/target regimes.
* **`location.hh`, `position.hh`**:
    * These files, typically generated by Bison or related tools, define classes (`position`, `location`) for tracking the location (line and column numbers, filename) of tokens and grammar rules within the source PAL file. This is crucial for providing meaningful error messages during parsing.
* **`decimal7` (type alias for `num::DefaultNumber` from `libs/timeseries/number.h`)**:
    * Used throughout the library for precise financial calculations to avoid floating-point inaccuracies.

## Workflow Overview

1.  A `PalParseDriver` is instantiated with the path to a PAL pattern file.
2.  The `PalParseDriver::Parse()` method is called.
    * The `Scanner` tokenizes the input file.
    * The `PalParser` consumes tokens, validates syntax against the PAL grammar, and constructs an AST using `AstFactory` and AST node classes (`PriceActionLabPattern`, `GreaterThanExpr`, etc.). Parsed patterns are stored in the `PriceActionLabSystem` object within the driver.
3.  Once parsing is complete, a concrete `PalCodeGenVisitor` (e.g., `TradingBloxCodeGenVisitor`) is instantiated with the `PriceActionLabSystem` containing the AST.
4.  The `generateCode()` method of the visitor is called.
    * The visitor traverses the AST.
    * For each AST node encountered, the corresponding `visit()` method in the concrete visitor is invoked, which writes platform-specific code to an output file stream.

This library provides a flexible framework for interpreting Price Action Lab's pattern language and converting it into various target code formats, enabling strategy deployment across multiple trading platforms.