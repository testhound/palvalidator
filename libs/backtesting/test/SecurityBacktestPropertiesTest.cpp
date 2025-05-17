#include <catch2/catch_test_macros.hpp>
#include "SecurityBacktestProperties.h"

using namespace mkc_timeseries;

TEST_CASE ("SecurityBacktestProperties operations", "[SecurityBacktestProperties]")
{
    std::string symbol = "TEST";
    SecurityBacktestProperties props(symbol);

    SECTION ("Constructor sets symbol and initial bar number")
    {
        REQUIRE (props.getSecuritySymbol() == symbol);
        REQUIRE (props.getBacktestBarNumber() == 0);
    }

    SECTION ("updateBacktestBarNumber increments bar number")
    {
        props.updateBacktestBarNumber();
        REQUIRE (props.getBacktestBarNumber() == 1);
        props.updateBacktestBarNumber();
        REQUIRE (props.getBacktestBarNumber() == 2);
    }

    SECTION ("Copy constructor copies symbol and bar number")
    {
        props.updateBacktestBarNumber();
        SecurityBacktestProperties copyProps(props);
        REQUIRE (copyProps.getSecuritySymbol() == props.getSecuritySymbol());
        REQUIRE (copyProps.getBacktestBarNumber() == props.getBacktestBarNumber());
    }

    SECTION ("Assignment operator copies symbol and bar number")
    {
        props.updateBacktestBarNumber();
        props.updateBacktestBarNumber();
        SecurityBacktestProperties assignProps("OTHER");
        assignProps = props;
        REQUIRE (assignProps.getSecuritySymbol() == props.getSecuritySymbol());
        REQUIRE (assignProps.getBacktestBarNumber() == props.getBacktestBarNumber());
    }
}

TEST_CASE ("SecurityBacktestPropertiesManager operations", "[SecurityBacktestPropertiesManager]")
{
    SecurityBacktestPropertiesManager mgr;

    SECTION ("getBacktestBarNumber on unknown symbol throws")
    {
        REQUIRE_THROWS_AS (mgr.getBacktestBarNumber("UNKNOWN"), SecurityBacktestPropertiesManagerException);
    }

    SECTION ("updateBacktestBarNumber on unknown symbol throws")
    {
        REQUIRE_THROWS_AS (mgr.updateBacktestBarNumber("UNKNOWN"), SecurityBacktestPropertiesManagerException);
    }

    SECTION ("addSecurity and basic operations")
    {
        mgr.addSecurity("ABC");
        REQUIRE (mgr.getBacktestBarNumber("ABC") == 0);
        mgr.updateBacktestBarNumber("ABC");
        REQUIRE (mgr.getBacktestBarNumber("ABC") == 1);
        mgr.updateBacktestBarNumber("ABC");
        REQUIRE (mgr.getBacktestBarNumber("ABC") == 2);
    }

    SECTION ("addSecurity duplicate throws")
    {
        mgr.addSecurity("XYZ");
        REQUIRE_THROWS_AS (mgr.addSecurity("XYZ"), SecurityBacktestPropertiesManagerException);
    }

    SECTION ("copy constructor shares state")
    {
        mgr.addSecurity("SHARE");
        mgr.updateBacktestBarNumber("SHARE");
        SecurityBacktestPropertiesManager mgrCopy(mgr);
        mgrCopy.updateBacktestBarNumber("SHARE");
        REQUIRE (mgr.getBacktestBarNumber("SHARE") == 2);
        REQUIRE (mgrCopy.getBacktestBarNumber("SHARE") == 2);
    }

    SECTION ("assignment operator shares state")
    {
        mgr.addSecurity("ASSIGN");
        SecurityBacktestPropertiesManager mgrAssign;
        mgrAssign = mgr;
        mgrAssign.updateBacktestBarNumber("ASSIGN");
        REQUIRE (mgr.getBacktestBarNumber("ASSIGN") == 1);
    }
}
