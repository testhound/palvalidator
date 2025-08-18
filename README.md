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

## Prerequisites

Before building the project, you need to initialize and update the git submodules:

```bash
git submodule update --init --recursive
```

This will checkout the Alglib library located in `libraries/Alglib` which is required for compilation.

## For Maintainers: Adding/Updating Submodules

When adding a new submodule (like the Alglib library), you need to commit both the `.gitmodules` file and the submodule directory:

```bash
git add .gitmodules
git add libraries/Alglib
git commit -m "Add Alglib submodule"
git push
```

### Troubleshooting Submodule Issues

If you see "untracked content" or "modified" status for a submodule after adding it, this usually means the submodule directory has untracked files or isn't properly initialized. To fix this:

1. **Check the submodule status:**
   ```bash
   git submodule status
   ```

2. **If the submodule shows as uninitialized (no commit hash), initialize it:**
   ```bash
   git submodule update --init libraries/Alglib
   ```

3. **If there are untracked files in the submodule, investigate and clean them:**
   ```bash
   cd libraries/Alglib
   git status  # Check what's untracked
   ls -la      # List all files including hidden ones
   ```
   
   Common causes of untracked content:
   - Build artifacts (`.o`, `.so`, `.dll`, `.exe` files)
   - IDE files (`.vscode/`, `.idea/`, etc.)
   - Temporary files
   
   To clean untracked files:
   ```bash
   git clean -fd  # Remove untracked files and directories (be careful!)
   # Or more safely, remove specific files:
   # rm -rf build/  # if there's a build directory
   # rm *.o         # if there are object files
   cd ../..
   ```
   
   **Specific case for Alglib:** If you see `libalglib/alglibversion.h` as untracked, this is a generated build artifact (created from `alglibversion.template`) that's needed for builds but shouldn't be tracked. The best approach is to ignore it:
   ```bash
   git config submodule.libraries/Alglib.ignore untracked
   ```
   
   This tells git to ignore untracked files in the Alglib submodule, allowing the generated `alglibversion.h` file to remain for builds while not causing git status issues.
   
   Note: The `alglibversion.h` file will be regenerated during the build process from the `alglibversion.template` file that's properly tracked in the repository.

4. **Alternative: Ignore untracked content (if the files should remain):**
   If the untracked files are meant to be there (like build outputs), you can configure git to ignore them:
   ```bash
   git config submodule.libraries/Alglib.ignore untracked
   ```

5. **Then add and commit the submodule reference:**
   ```bash
   git add libraries/Alglib
   git commit -m "Add Alglib submodule"
   ```

### Updating an existing submodule to a newer commit:
```bash
cd libraries/Alglib
git pull origin main  # or the appropriate branch
cd ../..
git add libraries/Alglib
git commit -m "Update Alglib submodule"
git push
```

### Branch Switching with Submodules

When switching between branches that have different submodule configurations, you may see warnings like:
```
warning: unable to rmdir 'libraries/Alglib': Directory not empty
```

This happens when switching to a branch without the submodule while untracked files exist in the submodule directory. To handle this:

1. **After switching branches, clean up leftover submodule directories:**
   ```bash
   git checkout master  # or any branch without the submodule
   # If you see the warning, manually clean up:
   rm -rf libraries/Alglib  # Remove the leftover directory
   ```

2. **When switching back to a branch with submodules:**
   ```bash
   git checkout your-feature-branch
   git submodule update --init --recursive  # Reinitialize submodules
   ```

3. **To avoid this issue in the future, use git's submodule-aware checkout:**
   ```bash
   git checkout --recurse-submodules your-branch-name
   ```

### Working with Upstream Changes

**Switching to master branch (without submodules):**
If your local master branch doesn't have submodules yet, you can switch normally:
```bash
git checkout master
# If you see the "Directory not empty" warning, clean up:
rm -rf libraries/Alglib
```

**Updating master branch with upstream submodule changes:**
If the upstream master branch now has submodule changes merged in:
```bash
git checkout master
git fetch origin
git rebase origin/master  # or git merge origin/master
git submodule update --init --recursive  # Initialize new submodules
```

**Alternative using pull:**
```bash
git checkout master
git pull origin master
git submodule update --init --recursive
```

## How to build executables

For production, use:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

For debug & profiling puposes, use the following instead:
```
mkdir build-debug
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
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
