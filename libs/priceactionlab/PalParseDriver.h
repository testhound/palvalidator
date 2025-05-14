/** \file driver.h Declaration of the example::Driver class. */

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

/** The PalParseDriver class brings together all components. It creates an instance of
 * the Parser and Scanner classes and connects them. Then the input stream is
 * fed into the scanner object and the parser gets it's token
 * sequence. Furthermore the driver object is available in the grammar rules as
 * a parameter. Therefore the driver class contains a reference to the
 * structure into which the parsed data is saved. */
class PalParseDriver
{
public:
  /// construct a new parser driver context
  PalParseDriver(const std::string &filename);


  void addPalPattern (std::shared_ptr<PriceActionLabPattern> pattern);
  
  PriceActionLabSystem* getPalStrategies();
  
  bool Parse();

 

private:
  // Used internally by Scanner YY_USER_ACTION to update location indicator
    void increaseLocation(unsigned int loc);
    
  // Used to get last Scanner location. Used in error messages.
  unsigned int location() const;
    /**
     * This is needed so that Scanner and Parser can call some
     * methods that we want to keep hidden from the end user.
     */
    friend class PalParser;
    friend class Scanner;

private:
  Scanner mScanner;
  PalParser mParser;
  unsigned int m_location;


  /// stream name (file or input stream) used for error messages.
  std::string mFileName;
  
  PriceActionLabSystem* mPalStrategies;
};
}

#endif // PAL_PARSE_DRIVER_H
