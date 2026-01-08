// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STOP_LOSS_H
#define __STOP_LOSS_H 1

#include <exception>
#include "number.h"
#include "DecimalConstants.h"
#include "PercentNumber.h"

namespace mkc_timeseries
{
  template <class Decimal> class StopLoss
  {
  public:
    explicit StopLoss(const Decimal& stopLoss)
      : mStopLoss (stopLoss) 
    {}

    StopLoss(const StopLoss<Decimal>& rhs)
      : mStopLoss (rhs.mStopLoss) 
    {}

    StopLoss& 
    operator=(const StopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      mStopLoss = rhs.mStopLoss;
      return *this;
    }

    virtual ~StopLoss()
    {}

    const Decimal& getStopLoss() const
    {
      return mStopLoss;
    }

    virtual bool isNullStopLoss() const = 0;
    virtual bool isLongStopLoss() const = 0;
    virtual bool isShortStopLoss() const = 0;

  protected:
    StopLoss()
      : mStopLoss(DecimalConstants<Decimal>::DecimalZero)
    {}

  private:
    Decimal mStopLoss;
  };

  template <class Decimal> 
  class LongStopLoss  : public StopLoss<Decimal>
  {
  public:
    explicit LongStopLoss(const Decimal& stopLoss)
      : StopLoss<Decimal> (stopLoss) 
    {}

    LongStopLoss(const Decimal& basePrice,
		     const PercentNumber<Decimal>& percentNum)
      : StopLoss<Decimal> (createStopFromPercent (basePrice, percentNum))
      {}

    LongStopLoss(const LongStopLoss<Decimal>& rhs)
      : StopLoss<Decimal> (rhs) 
    {}

    LongStopLoss& 
    operator=(const LongStopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      StopLoss<Decimal>::operator=(rhs);
      return *this;
    }

    ~LongStopLoss()
    {}

    bool isNullStopLoss() const
    {
      return false;
    }

    bool isLongStopLoss() const
    {
      return true;
    }

    bool isShortStopLoss() const
    {
      return false;
    }

  private:
    static Decimal createStopFromPercent (const Decimal& basePrice,
					   const PercentNumber<Decimal>& percentNum)
    {
      Decimal offset = basePrice * percentNum.getAsPercent();
      Decimal stopLoss = basePrice - offset;
      return stopLoss;
    }
  };

  template <class Decimal> 
  class ShortStopLoss  : public StopLoss<Decimal>
  {
  public:
    explicit ShortStopLoss(const Decimal& stopLoss)
      : StopLoss<Decimal> (stopLoss) 
    {}

    ShortStopLoss (const Decimal& basePrice,
		       const PercentNumber<Decimal>& percentNum)
      : StopLoss<Decimal> (createStopFromPercent (basePrice, percentNum))
      {}

    ShortStopLoss(const ShortStopLoss<Decimal>& rhs)
      : StopLoss<Decimal> (rhs) 
    {}

    ShortStopLoss& 
    operator=(const ShortStopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      StopLoss<Decimal>::operator=(rhs);
      return *this;
    }

    ~ShortStopLoss()
    {}

    bool isNullStopLoss() const
    {
      return false;
    }

    bool isLongStopLoss() const
    {
      return false;
    }

    bool isShortStopLoss() const
    {
      return true;
    }

  private:
    static Decimal createStopFromPercent (const Decimal& basePrice,
					 const PercentNumber<Decimal>& percentNum)
    {
      Decimal offset = basePrice * percentNum.getAsPercent();
      Decimal stopLoss = basePrice + offset;
      return stopLoss;
    }
  };

  template <class Decimal> 
  class NullStopLoss  : public StopLoss<Decimal>
  {
  public:
    NullStopLoss()
      : StopLoss<Decimal> () 
    {}

    NullStopLoss(const NullStopLoss<Decimal>& rhs)
      : StopLoss<Decimal> (rhs) 
    {}

    NullStopLoss& 
    operator=(const NullStopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      StopLoss<Decimal>::operator=(rhs);
      return *this;
    }

    ~NullStopLoss()
    {}

    bool isNullStopLoss() const
    {
      return true;
    }

    bool isLongStopLoss() const
    {
      return false;
    }

    bool isShortStopLoss() const
    {
      return false;
    }
  };
}
 
#endif
