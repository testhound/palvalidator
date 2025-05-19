#include <catch2/catch_test_macros.hpp>
#include "TestUtils.h"
#include "BoostDateHelper.h"
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace mkc_timeseries;
using boost::gregorian::date;

TEST_CASE("isWeekend and isWeekday", "[BoostDateHelper]") {
    date saturday(2021, 10, 2); // Saturday
    date sunday(2021, 10, 3);   // Sunday
    date monday(2021, 10, 4);   // Monday
    date wednesday(2021, 10, 6);

    REQUIRE(isWeekend(saturday));
    REQUIRE(isWeekend(sunday));
    REQUIRE(!isWeekend(monday));
    REQUIRE(!isWeekend(wednesday));

    REQUIRE(isWeekday(monday));
    REQUIRE(isWeekday(wednesday));
    REQUIRE(!isWeekday(saturday));
}

TEST_CASE("boost_previous_weekday", "[BoostDateHelper]") {
    date monday(2021, 10, 4);
    REQUIRE(boost_previous_weekday(monday) == date(2021, 10, 1)); // Friday

    date sunday(2021, 10, 3);
    REQUIRE(boost_previous_weekday(sunday) == date(2021, 10, 1)); // Friday

    date wednesday(2021, 10, 6);
    REQUIRE(boost_previous_weekday(wednesday) == date(2021, 10, 5)); // Tuesday → Wednesday -1
}

TEST_CASE("boost_next_weekday", "[BoostDateHelper]") {
    date friday(2021, 10, 1);
    REQUIRE(boost_next_weekday(friday) == date(2021, 10, 4)); // Monday

    date saturday(2021, 10, 2);
    REQUIRE(boost_next_weekday(saturday) == date(2021, 10, 4)); // Monday

    date tuesday(2021, 10, 5);
    REQUIRE(boost_next_weekday(tuesday) == date(2021, 10, 6)); // Wednesday
}

TEST_CASE("boost_next_month and boost_previous_month", "[BoostDateHelper]") {
    date jan15(2021, 1, 15);
    REQUIRE(boost_next_month(jan15) == date(2021, 2, 15));

    date dec5(2021, 12, 5);
    REQUIRE(boost_next_month(dec5) == date(2022, 1, 5));

    date mar31(2021, 3, 31);
    // February 2021 has 28 days
    REQUIRE(boost_previous_month(mar31) == date(2021, 2, 28));

    date jan15_2(2021, 1, 15);
    REQUIRE(boost_previous_month(jan15_2) == date(2020, 12, 15));
}

TEST_CASE("first_of_month and is_first_of_month", "[BoostDateHelper]") {
    date july20(2021, 7, 20);
    REQUIRE(first_of_month(july20) == date(2021, 7, 1));

    date aug1(2021, 8, 1);
    REQUIRE(first_of_month(aug1) == aug1);

    REQUIRE(is_first_of_month(date(2021, 7, 1)));
    REQUIRE(!is_first_of_month(date(2021, 7, 2)));
}

TEST_CASE("is_first_of_week and first_of_week", "[BoostDateHelper]") {
    date sunday(2021, 2, 28);
    REQUIRE(is_first_of_week(sunday));

    date monday(2021, 3, 1);
    REQUIRE(!is_first_of_week(monday));

    REQUIRE(first_of_week(sunday) == sunday);

    date wed(2021, 3, 3); // Wednesday
    REQUIRE(first_of_week(wed) == date(2021, 2, 28));
}

TEST_CASE("boost_next_week and boost_previous_week", "[BoostDateHelper]") {
    date mar7(2021, 3, 7);
    REQUIRE(boost_next_week(mar7) == date(2021, 3, 14));
    REQUIRE(boost_previous_week(mar7) == date(2021, 2, 28));
}
