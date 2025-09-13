# PalTradeHistory Trade Comparison Documentation

## Overview

The `paltradehistory` program now includes comprehensive trade comparison functionality that allows you to validate PAL-generated trading strategies against external backtesting results from platforms like WealthLab, TradeStation, and others. This feature helps ensure the accuracy and reliability of your PAL pattern-based trading systems.

## Key Features

- **Multi-Platform Support**: Parse CSV files from WealthLab, TradeStation, and generic formats
- **High-Precision Matching**: Uses configurable tolerances for dates, prices, and returns
- **Multiple Matching Strategies**: Strict, fuzzy, and best-match algorithms
- **Comprehensive Reporting**: Multiple output formats including console, CSV, JSON, and HTML
- **Detailed Analysis**: Match statistics, mismatch identification, and performance metrics

## Command Line Usage

### Basic Comparison
```bash
paltradehistory config.csv --compare external_trades.csv
```

### Advanced Options
```bash
paltradehistory config.csv \
    --compare external_trades.csv \
    --tolerance-strict \
    --report-format csv \
    --report-file comparison_results.csv
```

### Command Line Arguments

| Argument | Description | Default |
|----------|-------------|---------|
| `--compare <file>` | External CSV file to compare against | None (required for comparison) |
| `--tolerance-strict` | Use strict matching tolerances | Relaxed tolerances |
| `--tolerance-relaxed` | Use relaxed matching tolerances | Default |
| `--report-format <format>` | Output format: console, csv, json, html | console |
| `--report-file <file>` | Output file for comparison results | stdout |

## Supported CSV Formats

### WealthLab Format (Default)
Expected columns:
- Position: Direction (Long/Short)
- Symbol: Trading symbol
- Quantity: Position size
- Entry.Date: Entry date (YYYY-MM-DD)
- Entry.Price: Entry price
- Exit.Date: Exit date (YYYY-MM-DD)
- Exit.Price: Exit price
- ProfitPct: Profit percentage
- Bars.Held: Number of bars held

### Custom Format Configuration
You can configure custom column mappings by modifying the parser settings in the code.

## Tolerance Settings

### Strict Tolerances
- **Date Tolerance**: 0 days (exact match required)
- **Price Tolerance**: 0.01 (1 cent) or 0.1% relative
- **Return Tolerance**: 0.1% absolute or 1% relative

### Relaxed Tolerances
- **Date Tolerance**: 0 days (exact match required)
- **Price Tolerance**: 0.05 (5 cents) or 0.5% relative
- **Return Tolerance**: 0.5% absolute or 2% relative

## Matching Algorithms

### 1. Strict Matching
- Requires exact matches within specified tolerances
- Best for validating identical strategies
- Fastest performance

### 2. Fuzzy Matching
- Uses weighted scoring for partial matches
- Handles minor discrepancies in execution
- Good for real-world validation

### 3. Best Match
- Finds the closest match for each trade
- Useful for strategies with timing differences
- Most comprehensive analysis

## Output Formats

### Console Output
```
Trade Comparison Results
========================
Generated Trades: 150
External Trades: 148
Matched Trades: 145
Match Rate: 96.67%

Unmatched Generated Trades: 5
Unmatched External Trades: 3

Price Accuracy: 99.2%
Return Accuracy: 98.8%
```

### CSV Output
Detailed trade-by-trade comparison with match status, differences, and confidence scores.

### JSON Output
Structured data format suitable for further analysis or integration with other tools.

### HTML Output
Rich formatted report with charts and detailed analysis suitable for presentations.

## Example Workflow

### 1. Generate PAL Trade History
```bash
paltradehistory config.csv
```
This creates `trade_history.csv` with your PAL-generated trades.

### 2. Export External Backtesting Results
Export your WealthLab (or other platform) results to CSV format.

### 3. Run Comparison
```bash
paltradehistory config.csv --compare wealthlab_results.csv --report-format html --report-file validation_report.html
```

### 4. Analyze Results
Review the generated report to identify:
- Overall match rate and accuracy
- Specific trades with discrepancies
- Systematic differences in execution
- Areas for strategy refinement

## Troubleshooting

### Common Issues

#### "Error parsing line X: stoi"
- **Cause**: CSV format doesn't match expected structure
- **Solution**: Verify CSV has correct columns and header row

#### "Insufficient fields in CSV line"
- **Cause**: Missing columns in external CSV file
- **Solution**: Ensure all required columns are present

#### "Invalid date format"
- **Cause**: Date format not recognized
- **Solution**: Verify dates are in YYYY-MM-DD or MM/DD/YYYY format

#### Low match rates
- **Cause**: Different execution timing or strategy parameters
- **Solution**: Use relaxed tolerances or fuzzy matching

### Performance Tips

1. **Large Files**: For files with thousands of trades, consider using CSV output format for faster processing
2. **Memory Usage**: The comparison holds all trades in memory; monitor usage for very large datasets
3. **Precision**: Use strict tolerances only when you expect exact matches

## Integration Examples

### Automated Validation Pipeline
```bash
#!/bin/bash
# Generate PAL trades
paltradehistory config.csv

# Run comparison with external results
paltradehistory config.csv \
    --compare external_results.csv \
    --report-format json \
    --report-file validation.json

# Check match rate and exit with error if below threshold
python check_validation.py validation.json --min-match-rate 95
```

### Continuous Integration
Integrate trade comparison into your CI/CD pipeline to automatically validate strategy changes against known good results.

## Advanced Configuration

### Custom Tolerance Settings
Modify `ComparisonTolerance.h` to create custom tolerance profiles for specific use cases.

### Platform-Specific Parsers
Extend `ExternalTradeParser.h` to support additional backtesting platforms by implementing new column mappings.

### Custom Reporting
Implement additional report formats by extending `ComparisonReporter.h`.

## Best Practices

1. **Baseline Validation**: Establish a baseline comparison with known good results
2. **Regular Testing**: Run comparisons after any strategy modifications
3. **Documentation**: Document any expected differences between platforms
4. **Version Control**: Track both PAL configurations and external results
5. **Tolerance Tuning**: Start with strict tolerances and relax as needed

## Support and Troubleshooting

For issues with trade comparison functionality:

1. Verify CSV file format matches expected structure
2. Check date formats are consistent
3. Ensure all required columns are present
4. Review tolerance settings for your use case
5. Examine detailed mismatch reports for patterns

The comparison functionality provides a robust framework for validating PAL trading strategies against external backtesting platforms, ensuring confidence in your automated trading systems.