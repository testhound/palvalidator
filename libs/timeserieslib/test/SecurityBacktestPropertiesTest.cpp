#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "../SecurityBacktestProperties.h"

using namespace mkc_timeseries;


TEST_CASE ("SecurityBacktestProperties operations", "[SecurityBacktestProperties]")
{
  
  SecurityBacktestPropertiesManager properties;
  std::string myCornSymbol("@C");
  std::string tenYearSymbol("@TY");

  SecurityBacktestProperties cornProperties(myCornSymbol);
  REQUIRE (cornProperties.getSecuritySymbol() == myCornSymbol);
  REQUIRE (cornProperties.getBacktestBarNumber() == 0);

  SECTION ("SecurityBacktestProperties updateBacktestBarNumber") 
  {
    cornProperties.updateBacktestBarNumber();
    REQUIRE (cornProperties.getBacktestBarNumber() == 1);
  }

  SECTION ("SecurityBacktestPropertiesManager addSecurity")
  {
    properties.addSecurity(myCornSymbol);
    REQUIRE (properties.getBacktestBarNumber(myCornSymbol) == 0);
    properties.updateBacktestBarNumber(myCornSymbol);
    REQUIRE (properties.getBacktestBarNumber(myCornSymbol) == 1);
  }

  SECTION ("SecurityBacktestPropertiesManager addSecurity multiple")
  {
    properties.addSecurity(myCornSymbol);
    properties.addSecurity(tenYearSymbol);
    REQUIRE (properties.getBacktestBarNumber(myCornSymbol) == 0);
    REQUIRE (properties.getBacktestBarNumber(tenYearSymbol) == 0);

    properties.updateBacktestBarNumber(myCornSymbol);
    REQUIRE (properties.getBacktestBarNumber(myCornSymbol) == 1);
    REQUIRE (properties.getBacktestBarNumber(tenYearSymbol) == 0);

    properties.updateBacktestBarNumber(tenYearSymbol);
    REQUIRE (properties.getBacktestBarNumber(myCornSymbol) == 1);
    REQUIRE (properties.getBacktestBarNumber(tenYearSymbol) == 1);
  }

  SECTION ("SecurityBacktestPropertiesManager exceptions part 1")
  {
    REQUIRE_THROWS (properties.getBacktestBarNumber(myCornSymbol));
    REQUIRE_THROWS (properties.getBacktestBarNumber(tenYearSymbol));
  }

  SECTION ("SecurityBacktestPropertiesManager exceptions part 2")
  {
    properties.addSecurity(myCornSymbol);
    REQUIRE_THROWS (properties.addSecurity(myCornSymbol));
  }

  SECTION ("SecurityBacktestPropertiesManager exceptions part 3")
  {
    properties.addSecurity(myCornSymbol);
    REQUIRE_THROWS (properties.getBacktestBarNumber(tenYearSymbol));
  }

  SECTION ("SecurityBacktestPropertiesManager exceptions part 4")
  {
    properties.addSecurity(myCornSymbol);
    REQUIRE_THROWS (properties.updateBacktestBarNumber(tenYearSymbol));
  }
}

