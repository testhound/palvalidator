/** @file PalParseDriver.h
 *  @brief Declaration of the mkc_palast::PalParseDriver class.
 */

#ifndef PAL_PARSE_DRIVER_H
#define PAL_PARSE_DRIVER_H

#include <string>
#include <vector>
#include <memory>
#include "PalAst.h"
#include "scanner.h"
#include "PalParser.hpp"

namespace mkc_palast
{

/**
 * @brief The PalParseDriver class orchestrates the parsing process.
 *
 * It integrates the Scanner (lexer) and Parser (parser) components to
 * process an input stream (typically a file) containing Price Action Lab
 * pattern definitions. The driver manages the AST (Abstract Syntax Tree)
 * being constructed and provides context to the parser rules.
 */
class PalParseDriver
{
public:
  /**
   * @brief Constructs a new PalParseDriver.
   * @param filename The name of the file to be parsed. This is used for error reporting.
   */
  PalParseDriver(const std::string &filename);

  /**
   * @brief Adds a parsed PriceActionLabPattern to the collection of strategies.
   * @param pattern A shared pointer to the PriceActionLabPattern to add.
   */
  void addPalPattern (std::shared_ptr<PriceActionLabPattern> pattern);
  
  /**
   * @brief Retrieves the collection of parsed PAL strategies.
   * @return Pointer to the PriceActionLabSystem object managing the parsed patterns.
   */
  PriceActionLabSystem* getPalStrategies();
  
  /**
   * @brief Initiates the parsing of the input file.
   * @return 0 if parsing was successful, non-zero otherwise (following Bison convention).
   */
  int Parse();

private:
  /**
   * @brief Increases the current location (line number) during scanning.
   *        Used internally by the Scanner (YY_USER_ACTION).
   * @param loc The amount to increase the location by (typically 1 for a new line).
   */
  void increaseLocation(unsigned int loc);
    
  /**
   * @brief Gets the current scanner location (line number).
   *        Used primarily for error messages.
   * @return The current line number in the input file.
   */
  unsigned int location() const;

  /**
   * @brief Grants friend access to PalParser and Scanner.
   * This allows them to call private methods of PalParseDriver,
   * such as increaseLocation and other internal helpers, while keeping
   * these methods hidden from the public API of the driver.
   */
  friend class PalParser;
  friend class Scanner;

private:
  /**
   * @brief The lexer (scanner) component.
   */
  Scanner mScanner;
  /**
   * @brief The parser component.
   */
  PalParser mParser;
  /**
   * @brief Current line number in the input file, used for error reporting.
   */
  unsigned int m_location;

  /**
   * @brief The name of the input file or stream, used for error messages.
   */
  std::string mFileName;
  
  /**
   * @brief Pointer to the PriceActionLabSystem that stores the parsed patterns.
   * The PalParseDriver takes ownership of this object.
   */
  PriceActionLabSystem* mPalStrategies;
};
} // namespace mkc_palast

#endif // PAL_PARSE_DRIVER_H
