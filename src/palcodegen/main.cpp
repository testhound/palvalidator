#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <memory>

#include "PalParseDriver.h"
#include "PalAst.h"
#include "PalCodeGenVisitor.h"

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
string getUserInput(const string& prompt, const string& defaultValue = "") {
    cout << prompt;
    if (!defaultValue.empty()) {
        cout << " [" << defaultValue << "]";
    }
    cout << ": ";
    
    string input;
    getline(cin, input);
    
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
 * @brief Main program entry point
 */
int main() {
    displayHeader();
    
    try {
        // Get PAL IR file path from user
        string irFilePathStr = getUserInput("Enter PAL IR file path");
        if (irFilePathStr.empty()) {
            cerr << "Error: PAL IR file path cannot be empty" << endl;
            return INVALID_INPUT_ERROR;
        }
        
        filesystem::path irFilePath(irFilePathStr);
        
        // Validate input file
        if (!validateInputFile(irFilePath)) {
            return FILE_SYSTEM_ERROR;
        }
        
        // Get trading platform (default to TradeStation)
        string platform = getUserInput("Select trading platform", "TradeStation");
        
        // Currently only TradeStation is supported
        if (platform != "TradeStation") {
            cerr << "Error: Currently only TradeStation platform is supported" << endl;
            return INVALID_INPUT_ERROR;
        }
        
        // Get output file name from user
        string outputFileName = getUserInput("Enter output file name");
        if (outputFileName.empty()) {
            cerr << "Error: Output file name cannot be empty" << endl;
            return INVALID_INPUT_ERROR;
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
        
        cout << "Generating " << platform << " EasyLanguage code..." << endl;
        
        // Create the code generator
        try {
            EasyLanguageRADCodeGenVisitor codeGen(system.get(), outputFileName);
            
            // Generate the code
            codeGen.generateCode();
            
            cout << "Code generation completed successfully!" << endl;
            cout << "Output written to: " << outputFileName << endl;
            
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