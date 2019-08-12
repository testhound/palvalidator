#ifndef PALSTRATEGYALWAYSON_H
#define PALSTRATEGYALWAYSON_H

#include <array>
#include <iostream>
#include "PalStrategy.h"

using namespace mkc_timeseries;

namespace mkc_searchalgo {


  template <class Decimal> class PalLongStrategyAlwaysOn : public PalStrategy<Decimal>
    {
    public:
    PalLongStrategyAlwaysOn(const std::string& strategyName,
                    std::shared_ptr<PriceActionLabPattern> pattern,
                    std::shared_ptr<Portfolio<Decimal>> portfolio)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio)
        {}

      PalLongStrategyAlwaysOn(const PalLongStrategyAlwaysOn<Decimal>& rhs)
        : PalStrategy<Decimal>(rhs)
      {}

      const PalLongStrategyAlwaysOn<Decimal>&
      operator=(const PalLongStrategyAlwaysOn<Decimal>& rhs)
      {
        if (this == &rhs)
          return *this;

        PalStrategy<Decimal>::operator=(rhs);

        return *this;
      }

      ~PalLongStrategyAlwaysOn()
      {}

      std::shared_ptr<BacktesterStrategy<Decimal>>
      clone (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
        return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
                                                       this->getPalPattern(),
                                                       portfolio);
      }

      std::shared_ptr<PalStrategy<Decimal>>
      clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
        return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
                                                       this->getPalPattern(),
                                                       portfolio);
      }

      std::shared_ptr<BacktesterStrategy<Decimal>>
      cloneForBackTesting () const
      {
        return std::make_shared<PalLongStrategy<Decimal>>(this->getStrategyName(),
                                                       this->getPalPattern(),
                                                       this->getPortfolio());
      }

      void eventExitOrders (std::shared_ptr<Security<Decimal>> aSecurity,
                            const InstrumentPosition<Decimal>& instrPos,
                            const date& processingDate)
      {
        if (this->isLongPosition (aSecurity->getSymbol()))
          {
            std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
            Decimal target = pattern->getProfitTargetAsDecimal();
            //std::cout << "PalLongStrategy::eventExitOrders, getProfitTargetAsDecimal(): " << pattern->getProfitTargetAsDecimal() << std::endl << std::endl;
            PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);
            //std::cout << "PalLongStrategy::eventExitOrders, createPercentNumber(): " << targetAsPercent.getAsPercent() << std::endl << std::endl;

	    Decimal stop = pattern->getStopLossAsDecimal();
	    PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);

	    Decimal fillPrice = instrPos.getFillPrice();

	    //std::cout << "PalLongStrategy::eventExitOrders, fill Price =  " << fillPrice << std::endl;
	    this->ExitLongAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitLongAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (LongStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());

	    this->addLongPositionBar (aSecurity, processingDate);
	  }
      }

      void eventEntryOrders (std::shared_ptr<Security<Decimal>> aSecurity,
                             const InstrumentPosition<Decimal>& instrPos,
                             const date& processingDate)
      {
        if (this->isFlatPosition (aSecurity->getSymbol()))
          {
            if (this->getSecurityBarNumber(aSecurity->getSymbol()) >
                this->getPalPattern()->getMaxBarsBack())
              {
//                PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
//                typename Security<Decimal>::ConstRandomAccessIterator it =
//                  aSecurity->getRandomAccessIterator (processingDate);

//The only difference
		if (true)
		  {
		    this->EnterLongOnOpen (aSecurity->getSymbol(), processingDate);
		    //std::cout << "PalLongStrategy entered LongOnOpen Order on " << processingDate << std::endl;
		  }
		this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };

  //

  template <class Decimal> class PalShortStrategyAlwaysOn : public PalStrategy<Decimal>
    {
    public:
    PalShortStrategyAlwaysOn(const std::string& strategyName,
                     std::shared_ptr<PriceActionLabPattern> pattern,
                     std::shared_ptr<Portfolio<Decimal>> portfolio)
      : PalStrategy<Decimal>(strategyName, pattern, portfolio)
        {}

      PalShortStrategyAlwaysOn(const PalShortStrategyAlwaysOn<Decimal>& rhs)
        : PalStrategy<Decimal>(rhs)
      {}

      const PalShortStrategyAlwaysOn<Decimal>&
      operator=(const PalShortStrategyAlwaysOn<Decimal>& rhs)
      {
        if (this == &rhs)
          return *this;

        PalStrategy<Decimal>::operator=(rhs);

        return *this;
      }

      ~PalShortStrategyAlwaysOn()
      {}

      std::shared_ptr<BacktesterStrategy<Decimal>>
      clone (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
        return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
                                                       this->getPalPattern(),
                                                       portfolio);
      }

      std::shared_ptr<PalStrategy<Decimal>>
      clone2 (std::shared_ptr<Portfolio<Decimal>> portfolio) const
      {
        return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
                                                       this->getPalPattern(),
                                                       portfolio);
      }

      std::shared_ptr<BacktesterStrategy<Decimal>>
      cloneForBackTesting () const
      {
        return std::make_shared<PalShortStrategy<Decimal>>(this->getStrategyName(),
                                                       this->getPalPattern(),
                                                       this->getPortfolio());
      }
      void eventExitOrders (std::shared_ptr<Security<Decimal>> aSecurity,
                            const InstrumentPosition<Decimal>& instrPos,
                            const date& processingDate)
      {
        //std::cout << "PalShortStrategy::eventExitOrders - In eventExitOrders" << std::endl;
        if (this->isShortPosition (aSecurity->getSymbol()))
          {
            //std::cout << "!!!! PalShortStrategy::eventExitOrders - isShortPosition true" << std::endl << std::endl;
            std::shared_ptr<PriceActionLabPattern> pattern = this->getPalPattern();
            Decimal target = pattern->getProfitTargetAsDecimal();

	    //std::cout << "PalShortStrategy::eventExitOrders, getProfitTargetAsDecimal(): " << pattern->getProfitTargetAsDecimal() << std::endl << std::endl;
	    PercentNumber<Decimal> targetAsPercent = PercentNumber<Decimal>::createPercentNumber (target);
	    //std::cout << "PalShortStrategy::eventExitOrders, createPercentNumber(): " << targetAsPercent.getAsPercent() << std::endl << std::endl;

	    Decimal stop = pattern->getStopLossAsDecimal();
	    //std::cout << "PalShortStrategy::eventExitOrders, getStopLossAsDecimal(): " << pattern->getStopLossAsDecimal() << std::endl << std::endl;
	    PercentNumber<Decimal> stopAsPercent = PercentNumber<Decimal>::createPercentNumber (stop);

	    Decimal fillPrice = instrPos.getFillPrice();
	    //std::cout << "PalShortStrategy::eventExitOrders, fill Price =  " << fillPrice << std::endl;

	    this->ExitShortAllUnitsAtLimit(aSecurity->getSymbol(), processingDate,
					  fillPrice, targetAsPercent);
	    this->ExitShortAllUnitsAtStop(aSecurity->getSymbol(), processingDate,
					  fillPrice, stopAsPercent);
	    instrPos.setRMultipleStop (ShortStopLoss<Decimal> (fillPrice, stopAsPercent).getStopLoss());
	    this->addShortPositionBar (aSecurity, processingDate);
	  }
      }

      void eventEntryOrders (std::shared_ptr<Security<Decimal>> aSecurity,
                             const InstrumentPosition<Decimal>& instrPos,
                             const date& processingDate)
      {
        if (this->isFlatPosition (aSecurity->getSymbol()))
          {
            if (this->getSecurityBarNumber(aSecurity->getSymbol()) >
                this->getPalPattern()->getMaxBarsBack())
              {
//                PatternExpression *expr = this->getPalPattern()->getPatternExpression().get();
//                typename Security<Decimal>::ConstRandomAccessIterator it =
//                  aSecurity->getRandomAccessIterator (processingDate);

		if (true)
		  {
		    //std::cout << "PalShortStrategy entered ShortOnOpen Order on " << processingDate << std::endl;
		    this->EnterShortOnOpen (aSecurity->getSymbol(), processingDate);
		  }
		this->addFlatPositionBar (aSecurity, processingDate);
	      }
	  }
      }
  };
}

#endif // PALSTRATEGYALWAYSON_H
