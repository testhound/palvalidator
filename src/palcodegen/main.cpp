#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <memory>

#include "PalParseDriver.h"
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include "WealthLab8CodeGenerator.h"

using namespace std;
using namespace mkc_palast;

// Return codes
constexpr int SUCCESS = 0;
constexpr int FILE_SYSTEM_ERROR = 1;
constexpr int PARSING_ERROR = 2;
constexpr int CODE_GENERATION_ERROR = 3;
constexpr int INVALID_INPUT_ERROR = 4;

/**
 * @brief Prompts user for input with a default value
 * @param prompt The prompt message to display
 * @param defaultValue The default value if user presses enter
 * @return User input or default value
 */
/**
 * @brief Trims whitespace from both ends of a string
 * @param str The string to trim
 * @return Trimmed string
 */
string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

string getUserInput(const string& prompt, const string& defaultValue = "") {
    cout << prompt;
    if (!defaultValue.empty()) {
        cout << " [" << defaultValue << "]";
    }
    cout << ": ";
    
    string input;
    getline(cin, input);
    
    // Trim whitespace from input
    input = trim(input);
    
    if (input.empty() && !defaultValue.empty()) {
        return defaultValue;
    }
    
    return input;
}

/**
 * @brief Validates that a file exists and is readable
 * @param filePath Path to the file to validate
 * @return true if file exists and is readable, false otherwise
 */
bool validateInputFile(const filesystem::path& filePath) {
    if (!filesystem::exists(filePath)) {
        cerr << "Error: Input file does not exist: " << filePath << endl;
        return false;
    }
    
    if (!filesystem::is_regular_file(filePath)) {
        cerr << "Error: Input path is not a regular file: " << filePath << endl;
        return false;
    }
    
    // Test if file is readable
    ifstream testFile(filePath);
    if (!testFile.is_open()) {
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
bool validateOutputFile(const filesystem::path& filePath) {
    // Check if parent directory exists and is writable
    filesystem::path parentDir = filePath.parent_path();
    if (parentDir.empty()) {
        parentDir = filesystem::current_path();
    }
    
    if (!filesystem::exists(parentDir)) {
        cerr << "Error: Output directory does not exist: " << parentDir << endl;
        return false;
    }
    
    if (!filesystem::is_directory(parentDir)) {
        cerr << "Error: Output parent path is not a directory: " << parentDir << endl;
        return false;
    }
    
    // Test if we can write to the output file
    ofstream testFile(filePath);
    if (!testFile.is_open()) {
        cerr << "Error: Cannot write to output file: " << filePath << endl;
        return false;
    }
    testFile.close();
    
    // Remove the test file if it was created
    if (filesystem::exists(filePath)) {
        filesystem::remove(filePath);
    }
    
    return true;
}

/**
 * @brief Displays the program header
 */
void displayHeader() {
    cout << "PAL Code Generator" << endl;
    cout << "==================" << endl;
    cout << endl;
}

/**
 * @brief Displays usage information
 */
void displayUsage(const string& programName) {
    cout << "Usage: " << programName << " [OPTIONS] [INPUT_FILE] [OUTPUT_FILE]" << endl;
    cout << endl;
    cout << "Arguments:" << endl;
    cout << "  INPUT_FILE   Path to the PAL IR file to process" << endl;
    cout << "  OUTPUT_FILE  Path for the generated output file" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "  -p, --platform PLATFORM  Trading platform: TradeStation, WealthLab8, WL8 (default: WealthLab8)" << endl;
    cout << "  -h, --help               Show this help message" << endl;
    cout << endl;
    cout << "If INPUT_FILE and OUTPUT_FILE are not provided, the program will" << endl;
    cout << "prompt for them interactively." << endl;
    cout << endl;
}

/**
 * @brief Main program entry point
 */
int main(int argc, char* argv[]) {
    displayHeader();
    
    string irFilePathStr;
    string outputFileName;
    string platform = "WealthLab8";
    bool showHelp = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            showHelp = true;
            break;
        } else if (arg == "-p" || arg == "--platform") {
            if (i + 1 < argc) {
                platform = argv[++i];
            } else {
                cerr << "Error: --platform option requires a value" << endl;
                return INVALID_INPUT_ERROR;
            }
        } else if (arg.length() > 0 && arg[0] == '-') {
            cerr << "Error: Unknown option: " << arg << endl;
            displayUsage(argv[0]);
            return INVALID_INPUT_ERROR;
        } else {
            // Positional arguments: input file, then output file
            if (irFilePathStr.empty()) {
                irFilePathStr = arg;
            } else if (outputFileName.empty()) {
                outputFileName = arg;
            } else {
                cerr << "Error: Too many arguments provided" << endl;
                displayUsage(argv[0]);
                return INVALID_INPUT_ERROR;
            }
        }
    }
    
    if (showHelp) {
        displayUsage(argv[0]);
        return SUCCESS;
    }
    
    try {
        // Get PAL IR file path (from command line or user input)
        if (irFilePathStr.empty()) {
            irFilePathStr = getUserInput("Enter PAL IR file path");
            if (irFilePathStr.empty()) {
                cerr << "Error: PAL IR file path cannot be empty" << endl;
                return INVALID_INPUT_ERROR;
            }
        }
        
        filesystem::path irFilePath(irFilePathStr);
        
        // Validate input file
        if (!validateInputFile(irFilePath)) {
            return FILE_SYSTEM_ERROR;
        }
        
        // Get trading platform (from command line, user input, or default)
        if (platform.empty()) {
            platform = getUserInput("Select trading platform (TradeStation, WealthLab8, WL8)", "WealthLab8");
        }
        
        // Normalize platform names
        if (platform == "WL8") {
            platform = "WealthLab8";
        }
        
        // Validate supported platforms
        if (platform != "TradeStation" && platform != "WealthLab8") {
            cerr << "Error: Supported platforms are: TradeStation, WealthLab8, WL8" << endl;
            return INVALID_INPUT_ERROR;
        }
        
        // Get class name for WealthLab8
        string className;
        if (platform == "WealthLab8") {
            className = getUserInput("Enter WealthLab8 strategy class name", "GeneratedStrategy");
            if (className.empty()) {
                cerr << "Error: Class name cannot be empty for WealthLab8" << endl;
                return INVALID_INPUT_ERROR;
            }
        }
        
        // Get output file name (from command line or user input)
        if (outputFileName.empty()) {
            outputFileName = getUserInput("Enter output file name");
            if (outputFileName.empty()) {
                cerr << "Error: Output file name cannot be empty" << endl;
                return INVALID_INPUT_ERROR;
            }
        }
        
        filesystem::path outputFilePath(outputFileName);
        
        // Validate output file path
        if (!validateOutputFile(outputFilePath)) {
            return FILE_SYSTEM_ERROR;
        }
        
        cout << endl;
        cout << "Parsing PAL IR file..." << endl;
        
        // Parse the PAL IR file
        PalParseDriver driver(irFilePath.string());
        int parseResult = driver.Parse();
        
        if (parseResult != 0) {
            cerr << "Error: Failed to parse PAL IR file. Parse result: " << parseResult << endl;
            return PARSING_ERROR;
        }
        
        // Get the parsed strategies
        auto system = driver.getPalStrategies();
        if (!system) {
            cerr << "Error: No strategies found in PAL IR file" << endl;
            return PARSING_ERROR;
        }
        
        cout << "Generating " << platform << " code..." << endl;
        
        // Create the appropriate code generator based on platform
        try {
            if (platform == "TradeStation") {
                EasyLanguageRADCodeGenVisitor codeGen(system, outputFileName);
                codeGen.generateCode();
            } else if (platform == "WealthLab8") {
                WealthLab8CodeGenVisitor codeGen(system, outputFileName, className);
                codeGen.generateCode();
            }
            
            cout << "Code generation completed successfully!" << endl;
            cout << "Output file: " << outputFileName << endl;
            
        } catch (const exception& e) {
            cerr << "Error: Code generation failed: " << e.what() << endl;
            return CODE_GENERATION_ERROR;
        }
        
    } catch (const filesystem::filesystem_error& e) {
        cerr << "Error: File system error: " << e.what() << endl;
        return FILE_SYSTEM_ERROR;
    } catch (const exception& e) {
        cerr << "Error: Unexpected error: " << e.what() << endl;
        return CODE_GENERATION_ERROR;
    }
    
    return SUCCESS;
}
