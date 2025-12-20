#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <memory>
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>

#include "PalParseDriver.h"
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include "WealthLab8CodeGenerator.h"
#include "number.h"

using namespace std;
using namespace mkc_palast;

// Return codes
constexpr int SUCCESS = 0;
constexpr int FILE_SYSTEM_ERROR = 1;
constexpr int PARSING_ERROR = 2;
constexpr int CODE_GENERATION_ERROR = 3;
constexpr int INVALID_INPUT_ERROR = 4;

/**
 * @brief Trims whitespace from both ends of a string
 * @param str The string to trim
 * @return Trimmed string
 */
string trim(const string& str)
{
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == string::npos)
    {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

/**
 * @brief Prompts user for input with a default value
 * @param prompt The prompt message to display
 * @param defaultValue The default value if user presses enter
 * @return User input or default value
 */
string getUserInput(const string& prompt, const string& defaultValue = "")
{
    cout << prompt;
    if (!defaultValue.empty())
    {
        cout << " [" << defaultValue << "]";
    }
    cout << ": ";

    string input;
    getline(cin, input);

    // Trim whitespace from input
    input = trim(input);

    if (input.empty() && !defaultValue.empty())
    {
        return defaultValue;
    }

    return input;
}

/**
 * @brief Gets the current year as a string
 * @return String representing the current year (e.g., "2025")
 */
string getCurrentYear()
{
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm local_tm = *localtime(&now_time);
    
    stringstream ss;
    ss << (local_tm.tm_year + 1900);
    return ss.str();
}

/**
 * @brief Gets the current date formatted as Month_Day_Year
 * @return String like "December_19_2025"
 */
string getFormattedDate()
{
    const vector<string> months = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm local_tm = *localtime(&now_time);

    stringstream ss;
    
    // Ensure month index is valid (0-11)
    if (local_tm.tm_mon >= 0 && local_tm.tm_mon < 12)
    {
        ss << months[local_tm.tm_mon];
    }
    else
    {
        ss << "Unknown";
    }

    ss << "_" << local_tm.tm_mday << "_" << (local_tm.tm_year + 1900);
    return ss.str();
}

/**
 * @brief Extracts stop loss value from long patterns
 * @param system The PriceActionLabSystem containing patterns
 * @param stopValue Output parameter for the extracted stop loss value
 * @return true if a long pattern with stop loss was found, false otherwise
 */
bool extractLongStopLoss(const std::shared_ptr<PriceActionLabSystem>& system,
                         num::DefaultNumber& stopValue)
{
    auto longIt = system->patternLongsBegin();
    if (longIt != system->patternLongsEnd())
    {
        auto pattern = longIt->second;
        auto stopLoss = pattern->getStopLoss();
        if (stopLoss)
        {
            stopValue = *(stopLoss->getStopLoss());
            return true;
        }
    }
    return false;
}

/**
 * @brief Extracts stop loss value from short patterns
 * @param system The PriceActionLabSystem containing patterns
 * @param stopValue Output parameter for the extracted stop loss value
 * @return true if a short pattern with stop loss was found, false otherwise
 */
bool extractShortStopLoss(const std::shared_ptr<PriceActionLabSystem>& system,
                          num::DefaultNumber& stopValue)
{
    auto shortIt = system->patternShortsBegin();
    if (shortIt != system->patternShortsEnd())
    {
        auto pattern = shortIt->second;
        auto stopLoss = pattern->getStopLoss();
        if (stopLoss)
        {
            stopValue = *(stopLoss->getStopLoss());
            return true;
        }
    }
    return false;
}

/**
 * @brief Validates that a file exists and is readable
 * @param filePath Path to the file to validate
 * @return true if file exists and is readable, false otherwise
 */
bool validateInputFile(const filesystem::path& filePath)
{
    if (!filesystem::exists(filePath))
    {
        cerr << "Error: Input file does not exist: " << filePath << endl;
        return false;
    }

    if (!filesystem::is_regular_file(filePath))
    {
        cerr << "Error: Input path is not a regular file: " << filePath << endl;
        return false;
    }

    // Test if file is readable
    ifstream testFile(filePath);
    if (!testFile.is_open())
    {
        cerr << "Error: Cannot read input file: " << filePath << endl;
        return false;
    }

    return true;
}

/**
 * @brief Validates that the output file path is writable
 * @param filePath Path to the output file
 * @return true if path is writable, false otherwise
 */
bool validateOutputFile(const filesystem::path& filePath)
{
    // Check if parent directory exists and is writable
    filesystem::path parentDir = filePath.parent_path();
    if (parentDir.empty())
    {
        parentDir = filesystem::current_path();
    }

    if (!filesystem::exists(parentDir))
    {
        cerr << "Error: Output directory does not exist: " << parentDir << endl;
        return false;
    }

    if (!filesystem::is_directory(parentDir))
    {
        cerr << "Error: Output parent path is not a directory: " << parentDir << endl;
        return false;
    }

    // Test if we can write to the output file
    ofstream testFile(filePath);
    if (!testFile.is_open())
    {
        cerr << "Error: Cannot write to output file: " << filePath << endl;
        return false;
    }
    testFile.close();

    // Remove the test file if it was created
    if (filesystem::exists(filePath))
    {
        filesystem::remove(filePath);
    }

    return true;
}

/**
 * @brief Processes generation for TradeStation
 */
void processTradeStation(const std::shared_ptr<PriceActionLabSystem>& system, 
                        const string& outputFileName)
{
    cout << "Generating TradeStation code..." << endl;
    EasyLanguageRADCodeGenVisitor codeGen(system, outputFileName);
    codeGen.generateCode();
}

/**
 * @brief Processes generation for WealthLab8
 */
void processWealthLab(const std::shared_ptr<PriceActionLabSystem>& system, 
                      const string& outputFileName,
                      const string& ticker)
{
    // Class name strategy: Diversity_<Ticker>_<Year>
    string currentYear = getCurrentYear();
    string className = "Diversity_" + ticker + "_" + currentYear;
    
    cout << "WealthLab8 Class Name: " << className << endl;

    num::DefaultNumber longStopPercent = num::fromString<num::DefaultNumber>("2.0");
    num::DefaultNumber shortStopPercent = num::fromString<num::DefaultNumber>("2.0");

    // Extract stop loss values from patterns using helper functions
    bool longStopFound = extractLongStopLoss(system, longStopPercent);
    bool shortStopFound = extractShortStopLoss(system, shortStopPercent);

    if (longStopFound)
    {
        cout << "Extracted long side stop: " << num::toString(longStopPercent) << "%" << endl;
    }

    if (shortStopFound)
    {
        cout << "Extracted short side stop: " << num::toString(shortStopPercent) << "%" << endl;
    }

    // If one side is missing, use the value from the other side
    if (!longStopFound && shortStopFound)
    {
        longStopPercent = shortStopPercent;
        cout << "No long patterns found, using short side stop for long side: "
             << num::toString(longStopPercent) << "%" << endl;
    }
    else if (longStopFound && !shortStopFound)
    {
        shortStopPercent = longStopPercent;
        cout << "No short patterns found, using long side stop for short side: "
             << num::toString(shortStopPercent) << "%" << endl;
    }
    else if (!longStopFound && !shortStopFound)
    {
        cerr << "Warning: No patterns found with stop loss values, using default 2.0%" << endl;
    }

    cout << "Generating WealthLab8 code..." << endl;
    
    WealthLab8CodeGenVisitor codeGen(system, outputFileName, className,
                                     num::to_double(longStopPercent),
                                     num::to_double(shortStopPercent));
    codeGen.generateCode();
}

/**
 * @brief Displays the program header
 */
void displayHeader()
{
    cout << "PAL Code Generator" << endl;
    cout << "==================" << endl;
    cout << endl;
}

/**
 * @brief Displays usage information
 */
void displayUsage(const string& programName)
{
    cout << "Usage: " << programName << " [OPTIONS] [INPUT_FILE] [TICKER_SYMBOL]" << endl;
    cout << endl;
    cout << "Arguments:" << endl;
    cout << "  INPUT_FILE     Path to the PAL IR file to process" << endl;
    cout << "  TICKER_SYMBOL  The ticker symbol for naming conventions" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "  -p, --platform PLATFORM  Trading platform: TradeStation, WealthLab8, WL8 (default: WealthLab8)" << endl;
    cout << "  -h, --help               Show this help message" << endl;
    cout << endl;
    cout << "If INPUT_FILE or TICKER_SYMBOL are not provided, the program will" << endl;
    cout << "prompt for them interactively." << endl;
    cout << endl;
}

/**
 * @brief Main program entry point
 */
int main(int argc, char* argv[])
{
    displayHeader();

    string irFilePathStr;
    string tickerSymbol;
    string platform = "WealthLab8";
    bool showHelp = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            showHelp = true;
            break;
        }
        else if (arg == "-p" || arg == "--platform")
        {
            if (i + 1 < argc)
            {
                platform = argv[++i];
            }
            else
            {
                cerr << "Error: --platform option requires a value" << endl;
                return INVALID_INPUT_ERROR;
            }
        }
        else if (arg.length() > 0 && arg[0] == '-')
        {
            cerr << "Error: Unknown option: " << arg << endl;
            displayUsage(argv[0]);
            return INVALID_INPUT_ERROR;
        }
        else
        {
            // Positional arguments: input file, then ticker
            if (irFilePathStr.empty())
            {
                irFilePathStr = arg;
            }
            else if (tickerSymbol.empty())
            {
                tickerSymbol = arg;
            }
            else
            {
                cerr << "Error: Too many arguments provided" << endl;
                displayUsage(argv[0]);
                return INVALID_INPUT_ERROR;
            }
        }
    }

    if (showHelp)
    {
        displayUsage(argv[0]);
        return SUCCESS;
    }

    try
    {
        // Get PAL IR file path (from command line or user input)
        if (irFilePathStr.empty())
        {
            irFilePathStr = getUserInput("Enter PAL IR file path");
            if (irFilePathStr.empty())
            {
                cerr << "Error: PAL IR file path cannot be empty" << endl;
                return INVALID_INPUT_ERROR;
            }
        }

        filesystem::path irFilePath(irFilePathStr);

        // Validate input file
        if (!validateInputFile(irFilePath))
        {
            return FILE_SYSTEM_ERROR;
        }

        // Get trading platform (from command line, user input, or default)
        if (platform.empty())
        {
            platform = getUserInput("Select trading platform (TradeStation, WealthLab8, WL8)", "WealthLab8");
        }

        // Normalize platform names
        if (platform == "WL8")
        {
            platform = "WealthLab8";
        }

        // Validate supported platforms
        if (platform != "TradeStation" && platform != "WealthLab8")
        {
            cerr << "Error: Supported platforms are: TradeStation, WealthLab8, WL8" << endl;
            return INVALID_INPUT_ERROR;
        }

        // Get ticker symbol
        if (tickerSymbol.empty())
        {
            tickerSymbol = getUserInput("Enter Ticker Symbol");
            if (tickerSymbol.empty())
            {
                cerr << "Error: Ticker symbol cannot be empty" << endl;
                return INVALID_INPUT_ERROR;
            }
        }

        // Generate Output Filename
        // Format: Diversity_<Ticker>_<WL/TS>_<Date>.txt
        // Example: Diversity_MSFT_WL_December_19_2025.txt
        string formattedDate = getFormattedDate();
        string platformSuffix = (platform == "TradeStation") ? "TS" : "WL";
        string outputFileName = "Diversity_" + tickerSymbol + "_" + platformSuffix + "_" + formattedDate + ".txt";

        filesystem::path outputFilePath(outputFileName);

        // Validate output file path
        if (!validateOutputFile(outputFilePath))
        {
            return FILE_SYSTEM_ERROR;
        }

        cout << endl;
        cout << "Parsing PAL IR file..." << endl;

        // Parse the PAL IR file
        PalParseDriver driver(irFilePath.string());
        int parseResult = driver.Parse();

        if (parseResult != 0)
        {
            cerr << "Error: Failed to parse PAL IR file. Parse result: " << parseResult << endl;
            return PARSING_ERROR;
        }

        // Get the parsed strategies
        auto system = driver.getPalStrategies();
        if (!system)
        {
            cerr << "Error: No strategies found in PAL IR file" << endl;
            return PARSING_ERROR;
        }

        // Dispatch to appropriate platform handler
        if (platform == "TradeStation")
        {
            processTradeStation(system, outputFileName);
        }
        else if (platform == "WealthLab8")
        {
            processWealthLab(system, outputFileName, tickerSymbol);
        }

        cout << "Code generation completed successfully!" << endl;
        cout << "Output file: " << outputFileName << endl;

    }
    catch (const filesystem::filesystem_error& e)
    {
        cerr << "Error: File system error: " << e.what() << endl;
        return FILE_SYSTEM_ERROR;
    }
    catch (const exception& e)
    {
        cerr << "Error: Unexpected error: " << e.what() << endl;
        return CODE_GENERATION_ERROR;
    }

    return SUCCESS;
}
