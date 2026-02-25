#include <iostream>
#include <string>
#include <filesystem>
#include <memory>
#include <vector>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <iomanip>

#include "PalParseDriver.h"
#include "PalAst.h"
#include "number.h"
#include "version.h"

using namespace std;
using namespace mkc_palast;

// Return codes
constexpr int SUCCESS = 0;
constexpr int FILE_SYSTEM_ERROR = 1;
constexpr int PARSING_ERROR = 2;
constexpr int INVALID_INPUT_ERROR = 3;

/**
 * @brief Trims whitespace from both ends of a string
 * @param str The string to trim
 * @return Trimmed string
 */
string trim(const string& str)
{
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == string::npos) {
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
 * @brief Parses a PAL IR file and returns the pattern system
 * @param filePath Path to the PAL IR file
 * @return Shared pointer to PriceActionLabSystem, or nullptr on failure
 */
std::shared_ptr<PriceActionLabSystem> parsePatternFile(const std::filesystem::path& filePath)
{
    try {
        cout << "Parsing PAL IR file: " << filePath << "..." << endl;
        
        // Parse the PAL IR file
        PalParseDriver driver(filePath.string());
        int parseResult = driver.Parse();
        
        if (parseResult != 0) {
            cerr << "Error: Failed to parse PAL IR file '" << filePath << "'. Parse result: " << parseResult << endl;
            return nullptr;
        }
        
        // Get the parsed strategies
        auto system = driver.getPalStrategies();
        if (!system) {
            cerr << "Error: No strategies found in PAL IR file '" << filePath << "'" << endl;
            return nullptr;
        }
        
        cout << "Successfully parsed " << system->getNumPatterns() << " patterns ("
             << system->getNumLongPatterns() << " long, "
             << system->getNumShortPatterns() << " short)" << endl;
        
        return system;
    }
    catch (const exception& e) {
        cerr << "Error parsing file '" << filePath << "': " << e.what() << endl;
        return nullptr;
    }
}

/**
 * @brief Compares two pattern systems and generates a comparison report
 * @param system1 First pattern system
 * @param system2 Second pattern system
 * @param file1Name Name of first file for reporting
 * @param file2Name Name of second file for reporting
 */
void comparePatternSystems(const std::shared_ptr<PriceActionLabSystem>& system1,
                          const std::shared_ptr<PriceActionLabSystem>& system2,
                          const std::string& file1Name,
                          const std::string& file2Name)
{
    cout << "\n" << string(80, '=') << endl;
    cout << "PATTERN COMPARISON REPORT" << endl;
    cout << string(80, '=') << endl;
    
    // Collect all patterns from both systems
    set<PALPatternPtr> patterns1, patterns2;
    set<PALPatternPtr> commonPatterns, uniqueToFile1, uniqueToFile2;
    
    // Add all patterns from system1
    for (auto it = system1->allPatternsBegin(); it != system1->allPatternsEnd(); ++it) {
        patterns1.insert(*it);
    }
    
    // Add all patterns from system2
    for (auto it = system2->allPatternsBegin(); it != system2->allPatternsEnd(); ++it) {
        patterns2.insert(*it);
    }
    
    // Find common and unique patterns
    for (const auto& pattern1 : patterns1) {
        bool found = false;
        for (const auto& pattern2 : patterns2) {
            if (*pattern1 == *pattern2) {
                commonPatterns.insert(pattern1);
                found = true;
                break;
            }
        }
        if (!found) {
            uniqueToFile1.insert(pattern1);
        }
    }
    
    for (const auto& pattern2 : patterns2) {
        bool found = false;
        for (const auto& pattern1 : patterns1) {
            if (*pattern1 == *pattern2) {
                found = true;
                break;
            }
        }
        if (!found) {
            uniqueToFile2.insert(pattern2);
        }
    }
    
    // Generate summary report
    cout << "\nSUMMARY:" << endl;
    cout << string(40, '-') << endl;
    cout << "File 1: " << file1Name << endl;
    cout << "  Total patterns: " << patterns1.size() << endl;
    
    cout << "\nFile 2: " << file2Name << endl;
    cout << "  Total patterns: " << patterns2.size() << endl;
    
    cout << "\nComparison Results:" << endl;
    cout << "  Common patterns (exist in both files): " << commonPatterns.size() << endl;
    cout << "  Patterns unique to file 1: " << uniqueToFile1.size() << endl;
    cout << "  Patterns unique to file 2: " << uniqueToFile2.size() << endl;
    
    // Show detailed breakdown by pattern type
    size_t commonLong = 0, commonShort = 0;
    size_t unique1Long = 0, unique1Short = 0;
    size_t unique2Long = 0, unique2Short = 0;
    
    for (const auto& pattern : commonPatterns) {
        if (pattern->isLongPattern()) commonLong++;
        else commonShort++;
    }
    
    for (const auto& pattern : uniqueToFile1) {
        if (pattern->isLongPattern()) unique1Long++;
        else unique1Short++;
    }
    
    for (const auto& pattern : uniqueToFile2) {
        if (pattern->isLongPattern()) unique2Long++;
        else unique2Short++;
    }
    
    cout << "\nDetailed Breakdown:" << endl;
    cout << string(40, '-') << endl;
    cout << "Common patterns:" << endl;
    cout << "  Long patterns:  " << commonLong << endl;
    cout << "  Short patterns: " << commonShort << endl;
    
    cout << "\nPatterns unique to file 1 (" << file1Name << "):" << endl;
    cout << "  Long patterns:  " << unique1Long << endl;
    cout << "  Short patterns: " << unique1Short << endl;
    
    cout << "\nPatterns unique to file 2 (" << file2Name << "):" << endl;
    cout << "  Long patterns:  " << unique2Long << endl;
    cout << "  Short patterns: " << unique2Short << endl;
    
    // Show percentage overlap
    double totalUnique = patterns1.size() + patterns2.size();
    double overlapPercentage = (totalUnique > 0) ? (2.0 * commonPatterns.size() / totalUnique) * 100.0 : 0.0;
    
    cout << "\nOverlap Analysis:" << endl;
    cout << string(40, '-') << endl;
    cout << fixed << setprecision(2);
    cout << "Pattern overlap: " << overlapPercentage << "%" << endl;
    cout << "Jaccard similarity: " << fixed << setprecision(4);
    
    if (patterns1.size() + patterns2.size() - commonPatterns.size() > 0) {
        double jaccard = (double)commonPatterns.size() / (patterns1.size() + patterns2.size() - commonPatterns.size());
        cout << jaccard << endl;
    } else {
        cout << "1.0000" << endl;
    }
    
    cout << string(80, '=') << endl;
}

/**
 * @brief Validates that a file exists and is readable
 * @param filePath Path to the file to validate
 * @return true if file exists and is readable, false otherwise
 */
bool validateInputFile(const filesystem::path& filePath)
{
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
 * @brief Displays the program header
 */
void displayHeader()
{
    cout << "PAL File Comparator - " << palvalidator::Version::getVersionFull() << endl;
    cout << "=========================================" << endl;
    cout << "Compares two Price Action Lab IR files and reports differences" << endl;
    cout << endl;
}

/**
 * @brief Displays usage information
 */
void displayUsage(const string& programName)
{
    cout << "Usage: " << programName << " [OPTIONS] <FILE1> <FILE2>" << endl;
    cout << endl;
    cout << "Arguments:" << endl;
    cout << "  FILE1     Path to the first PAL IR file" << endl;
    cout << "  FILE2     Path to the second PAL IR file" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "  --version    Show version information and exit" << endl;
    cout << "  -h, --help   Show this help message" << endl;
    cout << endl;
    cout << "Description:" << endl;
    cout << "  Compares two PAL IR files and reports:" << endl;
    cout << "  - Number of patterns in each file" << endl;
    cout << "  - Number of patterns common to both files" << endl;
    cout << "  - Number of patterns unique to first file" << endl;
    cout << "  - Number of patterns unique to second file" << endl;
    cout << "  - Pattern overlap percentage and Jaccard similarity" << endl;
    cout << endl;
}

/**
 * @brief Main program entry point
 */
int main(int argc, char* argv[])
{
    displayHeader();
    
    string file1Path, file2Path;
    bool showHelp = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--version") {
            cout << palvalidator::Version::getAboutString() << endl;
            return SUCCESS;
        }
        else if (arg == "-h" || arg == "--help") {
            showHelp = true;
            break;
        }
        else if (arg.length() > 0 && arg[0] == '-') {
            cerr << "Error: Unknown option: " << arg << endl;
            displayUsage(argv[0]);
            return INVALID_INPUT_ERROR;
        }
        else {
            // Positional arguments: file1, then file2
            if (file1Path.empty()) {
                file1Path = arg;
            }
            else if (file2Path.empty()) {
                file2Path = arg;
            }
            else {
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
    
    // Get file paths (from command line or user input)
    if (file1Path.empty()) {
        file1Path = getUserInput("Enter path to first PAL IR file");
        if (file1Path.empty()) {
            cerr << "Error: First file path cannot be empty" << endl;
            return INVALID_INPUT_ERROR;
        }
    }
    
    if (file2Path.empty()) {
        file2Path = getUserInput("Enter path to second PAL IR file");
        if (file2Path.empty()) {
            cerr << "Error: Second file path cannot be empty" << endl;
            return INVALID_INPUT_ERROR;
        }
    }
    
    try {
        filesystem::path filePath1(file1Path);
        filesystem::path filePath2(file2Path);
        
        // Validate input files
        if (!validateInputFile(filePath1)) {
            return FILE_SYSTEM_ERROR;
        }
        
        if (!validateInputFile(filePath2)) {
            return FILE_SYSTEM_ERROR;
        }
        
        // Parse both files
        auto system1 = parsePatternFile(filePath1);
        if (!system1) {
            return PARSING_ERROR;
        }
        
        auto system2 = parsePatternFile(filePath2);
        if (!system2) {
            return PARSING_ERROR;
        }
        
        // Compare the pattern systems
        comparePatternSystems(system1, system2,
                            filePath1.filename().string(),
                            filePath2.filename().string());
        
        cout << "\nComparison completed successfully!" << endl;
    }
    catch (const filesystem::filesystem_error& e) {
        cerr << "Error: File system error: " << e.what() << endl;
        return FILE_SYSTEM_ERROR;
    }
    catch (const exception& e) {
        cerr << "Error: Unexpected error: " << e.what() << endl;
        return PARSING_ERROR;
    }
    
    return SUCCESS;
}
