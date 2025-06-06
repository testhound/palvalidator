/**
 * @file PalParseDriver.cpp
 * @brief Implements the PalParseDriver class for orchestrating PAL file parsing.
 *
 * This file contains the constructor and method definitions for the PalParseDriver,
 * which is responsible for setting up the scanner and parser, managing the input stream,
 * and collecting the parsed Price Action Lab patterns.
 */
// $Id$

#include <fstream>
#include <sstream>

#include "PalAst.h"
#include "PalParseDriver.h"
#include "AstResourceManager.h"
#include "scanner.h" // Needs to be included for Scanner definition used by mScanner

namespace mkc_palast {

/**
 * @brief Constructs a PalParseDriver instance.
 *
 * Initializes the scanner (`mScanner`) and parser (`mParser`), associating them
 * with this driver instance. It sets the input filename (`mFileName`) for error
 * reporting and other references. A new `PriceActionLabSystem` is created to
 * store the parsed patterns, and it's initialized with a
 * `SmallestVolatilityTieBreaker` as the default strategy for resolving pattern conflicts.
 * The initial location (`m_location`) is set to 0.
 *
 * @param fileName The path to the PAL (Price Action Lab) file to be parsed.
 */
PalParseDriver::PalParseDriver(const std::string &fileName)
  : mScanner(*this), // Initialize scanner, passing reference to this driver
    mParser (mScanner, *this), // Initialize parser, passing scanner and this driver
    m_location(0),      // Initialize location counter
    mFileName(fileName), // Store the filename
    // Create a new AstResourceManager for memory management
    mResourceManager(std::make_shared<AstResourceManager>()),
    // Create a new PriceActionLabSystem with a default SmallestVolatilityTieBreaker
    mPalStrategies(std::make_shared<PriceActionLabSystem>(std::shared_ptr<PatternTieBreaker>(new SmallestVolatilityTieBreaker)))
{
  // Constructor body is empty as initialization is done in the initializer list.
}

/**
 * @brief Parses the input PAL file specified during construction.
 *
 * This method opens the file, sets up the scanner to read from this file stream,
 * and then invokes the `parse()` method of the Bison-generated parser.
 *
 * @return 0 if the parser successfully parsed the file, non-zero otherwise
 *         (following Bison convention: 0=success, 1=syntax error, 2=memory exhaustion).
 * @note The method uses `std::ifstream` to open and read the file.
 *       The scanner's input stream is switched to this file.
 *       Returns the raw result from `mParser.parse()` which follows Bison conventions.
 */
int PalParseDriver::Parse()
{
  std::ifstream in(mFileName.c_str()); // Open the input file
  if (!in.good()) {
      // Return non-zero to indicate failure if file cannot be opened
      return 1; // Use 1 to indicate file open error (similar to syntax error)
  }
  mScanner.switch_streams (&in, NULL); // Switch scanner to read from this file stream
  int res = mParser.parse(); // Invoke the Bison-generated parser
  
  // Return the raw Bison result: 0=success, 1=syntax error, 2=memory exhaustion
  return res;
}

/**
 * @brief Retrieves the collection of parsed PAL strategies.
 *
 * Provides access to the `PriceActionLabSystem` object that stores all the
 * `PriceActionLabPattern` instances created during the parsing process.
 *
 * @return Pointer to the `PriceActionLabSystem` object.
 */
std::shared_ptr<PriceActionLabSystem>
PalParseDriver::getPalStrategies() const
{
  return mPalStrategies;
}

std::shared_ptr<AstResourceManager>
PalParseDriver::getResourceManager() const
{
  return mResourceManager;
}

/**
 * @brief Adds a successfully parsed PriceActionLabPattern to the system.
 *
 * This method is typically called by the parser upon successful reduction
 * of a grammar rule that defines a complete pattern. The parsed pattern
 * is then added to the `PriceActionLabSystem` managed by this driver.
 *
 * @param pattern A shared pointer to the `PriceActionLabPattern` to be added.
 */
void
PalParseDriver::addPalPattern (std::shared_ptr<PriceActionLabPattern> pattern)
{
  mPalStrategies->addPattern (pattern);
}

/**
 * @brief Increases the current location counter within the input file.
 *
 * This method is typically called by the scanner (lexer) to update the
 * driver's record of the current line number or character position.
 * This location information is crucial for meaningful error reporting.
 *
 * @param loc The number of units (e.g., lines or characters) to advance the location by.
 */
void PalParseDriver::increaseLocation(unsigned int loc) 
{
    m_location += loc;
    // Debugging line (originally commented out):
    // cout << "increaseLocation(): " << loc << ", total = " << m_location << endl;
}

/**
 * @brief Gets the current location (e.g., line number) in the input file.
 *
 * This method provides access to the current position being processed by the
 * scanner/parser, primarily for use in generating informative error messages.
 *
 * @return The current location as an unsigned integer.
 */
unsigned int PalParseDriver::location() const 
{
    return m_location;
}

} // namespace mkc_palast

