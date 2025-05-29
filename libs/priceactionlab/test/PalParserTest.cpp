#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <map>
#include <vector>
#include <string>
#include <iomanip> // For logging
#include <iostream> // For std::cout, std::cerr

#include "PalParseDriver.h"
#include "PalAst.h"

using namespace std;
using namespace mkc_palast;

// Simple struct to hold one comparison from the file
struct Comparison {
    PriceBarReference::ReferenceType lhsType;
    unsigned                         lhsOffset;
    char                             op; // '>' or '<'
    PriceBarReference::ReferenceType rhsType;
    unsigned                         rhsOffset;
};

// Struct to hold all expected data for one pattern block from the file
struct ExpectedPatternBlock {
    unsigned file_index; // The "Index: XXX" value from the input file header
    std::string file_name_from_header; // The "File:XXX" value
    std::vector<Comparison> expected_comparisons;
    std::vector<std::string> source_lines_for_block; // For debugging, store all lines of this block
};

// map a string like "CLOSE" → PriceBarReference::CLOSE
// THIS IS THE FUNCTION THAT WAS MISSING
static PriceBarReference::ReferenceType stringToRefType(const string& s) {
    if (s == "OPEN")  return PriceBarReference::OPEN;
    if (s == "HIGH")  return PriceBarReference::HIGH;
    if (s == "LOW")   return PriceBarReference::LOW;
    if (s == "CLOSE") return PriceBarReference::CLOSE;
    if (s == "VOLUME")return PriceBarReference::VOLUME;
    // Add other types from your PalAst.h if they can appear in comparison lines
    if (s == "ROC1") return PriceBarReference::ROC1;
    if (s == "IBS1") return PriceBarReference::IBS1;
    if (s == "IBS2") return PriceBarReference::IBS2;
    if (s == "IBS3") return PriceBarReference::IBS3;
    if (s == "MEANDER") return PriceBarReference::MEANDER;
    if (s == "VCHARTLOW") return PriceBarReference::VCHARTLOW;
    if (s == "VCHARTHIGH") return PriceBarReference::VCHARTHIGH;
    // if (s == "MOMERSIONFILTER") return PriceBarReference::MOMERSIONFILTER; // If used
    throw runtime_error("Unknown PriceBarReference type in stringToRefType: " + s);
}

// Helper to convert PriceBarReference::ReferenceType to string for logging
static std::string refTypeToString(PriceBarReference::ReferenceType type) {
    switch (type) {
        case PriceBarReference::OPEN: return "OPEN";
        case PriceBarReference::HIGH: return "HIGH";
        case PriceBarReference::LOW: return "LOW";
        case PriceBarReference::CLOSE: return "CLOSE";
        case PriceBarReference::VOLUME: return "VOLUME";
        case PriceBarReference::ROC1: return "ROC1";
        case PriceBarReference::IBS1: return "IBS1";
        case PriceBarReference::IBS2: return "IBS2";
        case PriceBarReference::IBS3: return "IBS3";
        case PriceBarReference::MEANDER: return "MEANDER";
        case PriceBarReference::VCHARTLOW: return "VCHARTLOW";
        case PriceBarReference::VCHARTHIGH: return "VCHARTHIGH";
        // case PriceBarReference::MOMERSIONFILTER: return "MOMERSIONFILTER"; // If used
        default: return "UNKNOWN_REF_TYPE(" + std::to_string(static_cast<int>(type)) + ")";
    }
}

// recursively flatten all GreaterThanExpr nodes under an expression tree
static void flattenComparisons(PatternExpression* expr,
                               vector<GreaterThanExpr*>& out)
{
    if (!expr) return;
    if (auto a = dynamic_cast<AndExpr*>(expr)) {
        flattenComparisons(a->getLHS(), out);
        flattenComparisons(a->getRHS(), out);
    }
    else if (auto gt = dynamic_cast<GreaterThanExpr*>(expr)) {
        out.push_back(gt);
    }
    else {
        // This case might be hit if a pattern has no AND expressions, just a single comparison.
        if (auto gt_single = dynamic_cast<GreaterThanExpr*>(expr)) {
             out.push_back(gt_single);
        } else {
             // Potentially log or handle other types if they are valid but not GreaterThanExpr
             // For this test, this is fine as we are focusing on GreaterThanExpr
        }
    }
}

TEST_CASE("PalParseDriver builds correct AST for each comparison in QQQ_IR.txt", "[PalParseDriver][AST]") {
    const string path = "dataset/QQQ_IR.txt";
    REQUIRE(filesystem::exists(path));

    ifstream in(path);
    REQUIRE(in.is_open());

    regex headerRe(R"(\{File:([^ ]+)\s+Index:(\d+)\s+Index Date:(\d+)\s+PL:([\d.]+)%\s+PS:([\d.]+)%\s+Trades:(\d+)\s+CL:(\d+)\})");
    // Regex for comparison lines, now accepting "BARS" or "DAYS"
    regex compRe(R"((?:(?:IF|AND)\s+)?(\w+)\s+OF\s+(\d+)\s+(?:BARS|DAYS)\s+AGO\s*([><])\s*(\w+)\s+OF\s+(\d+)\s+(?:BARS|DAYS)\s+AGO)");

    std::vector<ExpectedPatternBlock> allExpectedPatternBlocks;
    string line;
    ExpectedPatternBlock currentBlock;
    bool inBlockDefinition = false; 
    unsigned lineNum = 0;

    std::cout << "--- Populating Expected Pattern Blocks from: " << path << " ---" << std::endl;
    while (getline(in, line)) {
        lineNum++;
        smatch m; // For headerRe
        if (regex_search(line, m, headerRe)) {
            if (inBlockDefinition && !currentBlock.expected_comparisons.empty()) { // Save previous block if it had comparisons
                allExpectedPatternBlocks.push_back(currentBlock);
            } else if (inBlockDefinition && currentBlock.expected_comparisons.empty()) {
                 // If the block had a header but no comparison lines that matched compRe, 
                 // we might still want to add it if the parser is expected to produce an AST for it.
                 // For now, only adding blocks that have matched comparisons to keep alignment simple.
                 // Alternatively, always add if header is found, and expect empty comparisons in AST.
                 // For this test, let's assume patterns without IF/AND are not the primary focus or are handled by AST having no comparisons.
                 // If a pattern has a header but NO "IF/AND" lines, it might be valid.
                 // Let's refine: add block if header found, even if no comps.
                 allExpectedPatternBlocks.push_back(currentBlock);
            }
            currentBlock = ExpectedPatternBlock(); 
            currentBlock.file_name_from_header = m[1].str();
            currentBlock.file_index = static_cast<unsigned>(stoul(m[2].str()));
            inBlockDefinition = true;
            std::cout << "Found Header for Index: " << currentBlock.file_index
                      << ", File: " << currentBlock.file_name_from_header << " (Source Line: " << lineNum << ")" << std::endl;
        }

        if (inBlockDefinition) {
            currentBlock.source_lines_for_block.push_back(line); 
            smatch compMatch; 
            if (regex_search(line, compMatch, compRe)) {
                Comparison c;
                c.lhsType   = stringToRefType(compMatch[1].str()); // Fixed: stringToRefType was missing
                c.lhsOffset = static_cast<unsigned>(stoi(compMatch[2].str()));
                c.op        = compMatch[3].str()[0];
                c.rhsType   = stringToRefType(compMatch[4].str()); // Fixed: stringToRefType was missing
                c.rhsOffset = static_cast<unsigned>(stoi(compMatch[5].str()));
                currentBlock.expected_comparisons.push_back(c);
                std::cout << "  Index " << currentBlock.file_index << ": Added Expected Comp from source line " << lineNum << ": "
                          << compMatch[1].str() << " OF " << compMatch[2].str() << " " << compMatch[3].str() << " "
                          << compMatch[4].str() << " OF " << compMatch[5].str()
                          << " (LHS Offset: " << c.lhsOffset << ")" << std::endl;
            }
        }
    }
    if (inBlockDefinition) { // Add the last block being processed
        allExpectedPatternBlocks.push_back(currentBlock);
    }
    std::cout << "--- Finished Populating " << allExpectedPatternBlocks.size() << " Expected Pattern Blocks ---" << std::endl;

    // Parse the file to get ASTs
    PalParseDriver driver(path);
    REQUIRE(driver.Parse() == 0);
    PriceActionLabSystem* palSystem = driver.getPalStrategies();
    REQUIRE(palSystem);

    std::vector<PALPatternPtr> astPatternsList;
    for (auto it = palSystem->allPatternsBegin(); it != palSystem->allPatternsEnd(); ++it) {
        astPatternsList.push_back(*it);
    }

    std::cout << "\n--- Verifying AST ---" << std::endl;
    std::cout << "Number of AST patterns parsed: " << astPatternsList.size() << std::endl;
    // This assertion might be too strict if some "patterns" in the file are just headers without valid IF/THEN structures
    // that would lead to a PALPatternPtr. For now, let's assume a 1-to-1 mapping.
    REQUIRE(astPatternsList.size() == allExpectedPatternBlocks.size());

    for (size_t k = 0; k < astPatternsList.size(); ++k) {
        const auto& astPattern = astPatternsList[k];
        const auto& expectedBlock = allExpectedPatternBlocks[k];
        
        auto astDesc = astPattern->getPatternDescription();
        unsigned astPatternIndex = astDesc->getpatternIndex();
        // string astFileName = astDesc->getFileName(); // Be cautious with path comparisons

        std::cout << "\nVerifying Pattern Block " << k << ":" << std::endl;
        std::cout << "  Expected Index (from file header): " << expectedBlock.file_index
                  << ", AST Pattern Index: " << astPatternIndex << std::endl;
        REQUIRE(astPatternIndex == expectedBlock.file_index);

        const auto& expectedComps = expectedBlock.expected_comparisons;
        vector<GreaterThanExpr*> actualCompsFromAST;
        if (astPattern->getPatternExpression()) { // Check if pattern expression exists
             flattenComparisons(astPattern->getPatternExpression().get(), actualCompsFromAST);
        }


        std::cout << "  Block " << k << " (Index " << astPatternIndex << "): "
                  << "Expected " << expectedComps.size() << " comparisons (from compRe), Got " << actualCompsFromAST.size() << " from AST." << std::endl;
        
        if (expectedComps.empty() && actualCompsFromAST.empty()) {
            std::cout << "  Block " << k << " (Index " << astPatternIndex << "): No comparisons in expected or AST. Skipping detail check." << std::endl;
            continue; 
        }
        
        REQUIRE(actualCompsFromAST.size() == expectedComps.size());

        for (size_t i = 0; i < expectedComps.size(); ++i) {
            const auto& currentExpectedComp = expectedComps[i];
            const auto& currentActualCompAST = actualCompsFromAST[i];

            auto lhsRef = currentActualCompAST->getLHS();
            auto rhsRef = currentActualCompAST->getRHS();

            std::cout << "  Comparison " << i << " for block " << k << " (Index " << astPatternIndex << "):" << std::endl;
            std::cout << "    Expected: "
                      << refTypeToString(currentExpectedComp.lhsType) << " OF " << currentExpectedComp.lhsOffset
                      << " " << currentExpectedComp.op << " "
                      << refTypeToString(currentExpectedComp.rhsType) << " OF " << currentExpectedComp.rhsOffset
                      << std::endl;
            std::cout << "    Actual (AST): "
                      << refTypeToString(lhsRef->getReferenceType()) << " OF " << lhsRef->getBarOffset()
                      << " > " // Assuming GreaterThanExpr always means '>'
                      << refTypeToString(rhsRef->getReferenceType()) << " OF " << rhsRef->getBarOffset()
                      << std::endl;

            REQUIRE(lhsRef->getReferenceType() == currentExpectedComp.lhsType);
            REQUIRE(rhsRef->getReferenceType() == currentExpectedComp.rhsType);
            
            if (lhsRef->getBarOffset() != currentExpectedComp.lhsOffset) {
                std::cerr << "--> MISMATCH on Block " << k << " (AST Index " << astPatternIndex 
                          << "), Comparison " << i << " for LHS offset!" << std::endl;
                std::cerr << "    Expected LHS Offset: " << currentExpectedComp.lhsOffset 
                          << ", Actual AST LHS Offset: " << lhsRef->getBarOffset() << std::endl;
                std::cerr << "    Source lines for this expected block (" << k << "):" << std::endl;
                for(const auto& srcLine : expectedBlock.source_lines_for_block) {
                    std::cerr << "      " << srcLine << std::endl;
                }
            }
            REQUIRE(lhsRef->getBarOffset()     == currentExpectedComp.lhsOffset);
            
            REQUIRE(rhsRef->getBarOffset()     == currentExpectedComp.rhsOffset);
            REQUIRE(currentExpectedComp.op == '>'); 
        }
    }
}
