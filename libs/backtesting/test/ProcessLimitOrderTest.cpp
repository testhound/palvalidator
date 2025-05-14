#include <catch2/catch_test_macros.hpp>
#include "BoostDateHelper.h"
#include "TimeSeriesEntry.h"
#include "TradingOrderManager.h"
#include "TestUtils.h"

using namespace mkc_timeseries;
using namespace boost::gregorian;


TEST_CASE ("ProcessOrderVisitor Market Order Operations", "[ProcessLimitOrderVisitor]")
{
  auto entry0Prev = createTimeSeriesEntry ("19851115","3683.73657226563","3683.73657226563",
					   "3645.2841796875","3660.6650390625", 0);
  auto entry0 = createTimeSeriesEntry ("19851118", "3664.51025", "3687.58178", "3656.81982","3672.20068",0);

  auto entry1 = createTimeSeriesEntry ("19851119", "3710.65307617188","3722.18872070313","3679.89135742188",
				       "3714.49829101563", 0);

  auto entry2 = createTimeSeriesEntry ("19851120", "3737.56982421875","3756.7958984375","3726.0341796875",
				       "3729.87939453125",0);

  auto entry3 = createTimeSeriesEntry ("19851121","3699.11743164063","3710.65307617188","3668.35546875",
				       "3683.73657226563",0);

  auto entry4 = createTimeSeriesEntry ("19851122","3664.43017578125","3668.23559570313","3653.0146484375",
				       "3656.81982421875", 0);

  auto entry5 = createTimeSeriesEntry ("19851125","3641.59887695313","3649.20947265625","3626.3779296875",
				       "3637.79370117188", 0);

  auto entry6 = createTimeSeriesEntry ("19851126","3656.81982421875","3675.84594726563","3653.0146484375",
				       "3660.625", 0);
  auto entry7 = createTimeSeriesEntry ("19851127", "3664.43017578125","3698.67724609375","3660.625",
				       "3691.06689453125", 0);
  auto entry8 = createTimeSeriesEntry ("19851129", "3717.70336914063","3729.119140625","3698.67724609375",
				       "3710.09301757813", 0);
  auto entry9 = createTimeSeriesEntry ("19851202", "3721.50854492188","3725.31372070313","3691.06689453125",
				       "3725.31372070313", 0);
  auto entry10 = createTimeSeriesEntry ("19851203", "3713.89819335938","3740.53466796875","3710.09301757813"
					,"3736.7294921875", 0);
  auto entry11 = createTimeSeriesEntry ("19851204","3744.33984375","3759.56079101563","3736.7294921875",
					"3740.53466796875",0);

  auto shortEntry0Prev = createTimeSeriesEntry ("19860528","3789.64575195313","3813.65625",
						"3781.64233398438","3813.65625", 0);
  auto shortEntry0 = createTimeSeriesEntry ("19860529","3789.64575195313","3801.65112304688",
					    "3769.63720703125","3785.64404296875", 0);
  auto shortEntry1 = createTimeSeriesEntry ("19860530","3785.64404296875","3793.6474609375","3769.63720703125",
					    "3793.6474609375", 0);
  auto shortEntry2 = createTimeSeriesEntry ("19860602","3789.64575195313","3833.6650390625",
					    "3773.63891601563","3825.66137695313", 0);
  auto shortEntry3 = createTimeSeriesEntry ("19860603","3837.66674804688","3837.66674804688",
					    "3761.63354492188","3769.63720703125", 0);
  auto shortEntry4 = createTimeSeriesEntry ("19860604","3773.63891601563","3801.65112304688",
					    "3757.6318359375","3793.6474609375", 0);
  auto shortEntry5 = createTimeSeriesEntry ("19860605","3793.6474609375","3801.65112304688","3777.640625",
					    "3797.6494140625", 0);
  auto shortEntry6 = createTimeSeriesEntry ("19860606","3805.65283203125","3809.6545410156",
					    "3781.64233398438","3801.65112304688", 0);
  auto shortEntry7 = createTimeSeriesEntry ("19860609","3797.6494140625","3809.65454101563",
					    "3785.64404296875","3793.6474609375", 0);
  auto shortEntry8 = createTimeSeriesEntry ("19860610","3793.6474609375","3797.6494140625",
					    "3781.64233398438","3785.64404296875", 0);
  auto shortEntry9 = createTimeSeriesEntry ("19860611","3777.640625","3781.64233398438",
					    "3733.62158203125","3749.62841796875", 0);

  auto shortEntry10 = createTimeSeriesEntry ("19861111","3100.99853515625","3119.080078125","3078.396484375",
					     "3082.91674804688", 0);
  auto shortEntry11 = createTimeSeriesEntry ("19861112","3082.91674804688","3155.24340820313","3078.396484375",
					     "3132.64135742188", 0);

  TradingVolume oneContract(1, TradingVolume::CONTRACTS);
  std::string tickerSymbol("C2");

  SellAtLimitOrder<DecimalType> longOrder1(tickerSymbol, oneContract,
				 entry0->getDateValue(), createDecimal ("3758.32172"));

  CoverAtLimitOrder<DecimalType> shortOrder1(tickerSymbol, oneContract,
				   shortEntry0->getDateValue(), createDecimal ("3738.86450"));

  ProcessOrderVisitor<DecimalType> longOrder1Processor (*entry1);
  ProcessOrderVisitor<DecimalType> shortOrder1Processor (*shortEntry1);


  SECTION ("Verify long orders are executed")
  {
    REQUIRE (longOrder1.isOrderPending() == true);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry2);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry3);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry4);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry5);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry6);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry7);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry8);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry9);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry10);
    longOrder1.accept (longOrder1Processor);
    REQUIRE (longOrder1.isOrderPending());

    longOrder1Processor.updateTradingBar (*entry11);
    longOrder1.accept (longOrder1Processor);
    REQUIRE_FALSE (longOrder1.isOrderPending());
    REQUIRE (longOrder1.isOrderExecuted());
    REQUIRE (longOrder1.getFillDate() == entry11->getDateValue());
    REQUIRE (longOrder1.getFillPrice() >= longOrder1.getLimitPrice());
    std::cout << "long order fill price = " << longOrder1.getFillPrice() << std::endl;
  }

  SECTION ("Verify exception thrown on bad processing date")
  {
    ProcessOrderVisitor<DecimalType> longOrderProcessorBad (*entry0Prev);

    REQUIRE (longOrder1.isOrderPending() == true);
    REQUIRE_THROWS (longOrder1.accept (longOrderProcessorBad));
  }

  SECTION ("Verify exception thrown on canceled order")
  {
    REQUIRE (longOrder1.isOrderPending() == true);
    longOrder1.MarkOrderCanceled();
    REQUIRE_THROWS (longOrder1.accept (longOrder1Processor));

  }

  SECTION ("Verify short orders are executed")
  {
    REQUIRE (shortOrder1.isOrderPending() == true);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry2);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry3);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry4);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry5);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry6);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry7);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry8);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE (shortOrder1.isOrderPending());

    shortOrder1Processor.updateTradingBar (*shortEntry9);
    shortOrder1.accept (shortOrder1Processor);
    REQUIRE_FALSE (shortOrder1.isOrderPending());
    REQUIRE (shortOrder1.isOrderExecuted());

    REQUIRE (shortOrder1.getFillDate() == shortEntry9->getDateValue());
    REQUIRE (shortOrder1.getFillPrice() <= shortOrder1.getLimitPrice());
    std::cout << "short order fill price = " << shortOrder1.getFillPrice() << std::endl;
  }

  SECTION ("Verify short exception thrown on bad processing date")
  {
    ProcessOrderVisitor<DecimalType> shortOrderProcessorBad (*shortEntry0Prev);

    REQUIRE (shortOrder1.isOrderPending() == true);
    REQUIRE_THROWS (shortOrder1.accept (shortOrderProcessorBad));
  }

  SECTION ("Verify short exception thrown on canceled order")
  {
    REQUIRE (shortOrder1.isOrderPending() == true);
    shortOrder1.MarkOrderCanceled();
    REQUIRE_THROWS (shortOrder1.accept (shortOrder1Processor));

  }
}
