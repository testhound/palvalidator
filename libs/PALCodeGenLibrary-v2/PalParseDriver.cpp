// $Id$
/** \file driver.cc Implementation of the example::Driver class. */

#include <fstream>
#include <sstream>

#include "PalAst.h"
#include "PalParseDriver.h"
#include "scanner.h"

using namespace mkc_palast;

PalParseDriver::PalParseDriver(const std::string &fileName)
  : mScanner(*this),
    mParser (mScanner, *this),
    m_location(0),
    mFileName(fileName),
    mPalStrategies(new PriceActionLabSystem (std::shared_ptr<PatternTieBreaker> (new SmallestVolatilityTieBreaker)))
{
  
}

  
bool PalParseDriver::Parse()
{
  bool parseResult;
  
  std::ifstream in(mFileName.c_str());
  mScanner.switch_streams (&in, NULL);
  int res = mParser.parse();
  
  return (res);
}

PriceActionLabSystem *
PalParseDriver::getPalStrategies()
{
  return mPalStrategies;
}

void
PalParseDriver::addPalPattern (std::shared_ptr<PriceActionLabPattern> pattern)
{
  mPalStrategies->addPattern (pattern);
}

void PalParseDriver::increaseLocation(unsigned int loc) 
{
    m_location += loc;
    //cout << "increaseLocation(): " << loc << ", total = " << m_location << endl;
}

unsigned int PalParseDriver::location() const 
{
    return m_location;
}


