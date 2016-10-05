// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __STOP_LOSS_H
#define __STOP_LOSS_H 1

#include <exception>
#include "decimal.h"
#include "DecimalConstants.h"
#include "PercentNumber.h"

using dec::decimal;

namespace mkc_timeseries
{
  template <int Prec> class StopLoss
  {
  public:
    explicit StopLoss(const decimal<Prec>& stopLoss)
      : mStopLoss (stopLoss) 
    {}

    StopLoss(const StopLoss<Prec>& rhs)
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

    virtual const dec::decimal<Prec>& getStopLoss() const = 0;

    virtual bool isNullStopLoss() const = 0;
    virtual bool isLongStopLoss() const = 0;
    virtual bool isShortStopLoss() const = 0;

  protected:
   StopLoss()
      : mStopLoss(DecimalConstants<Prec>::DecimalZero)
    {}

  private:
    dec::decimal<Prec> mStopLoss;
  };

  template <int Prec>
  const dec::decimal<Prec>& StopLoss<Prec>::getStopLoss() const
    {
      return mStopLoss;
    }

  template <int Prec> 
  class LongStopLoss  : public StopLoss<Prec>
  {
  public:
    explicit LongStopLoss(const decimal<Prec>& stopLoss)
      : StopLoss<Prec> (stopLoss) 
    {}

    LongStopLoss(const decimal<Prec>& basePrice,
		     const PercentNumber<Prec>& percentNum)
      : StopLoss<Prec> (createStopFromPercent (basePrice, percentNum))
      {}

    LongStopLoss(const LongStopLoss<Prec>& rhs)
      : StopLoss<Prec> (rhs) 
    {}

    LongStopLoss& 
    operator=(const LongStopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      StopLoss<Prec>::operator=(rhs);
      return *this;
    }

    ~LongStopLoss()
    {}

    const dec::decimal<Prec>& getStopLoss() const
    {
      return StopLoss<Prec>::getStopLoss();
    }

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
    static decimal<Prec> createStopFromPercent (const decimal<Prec>& basePrice,
					   const PercentNumber<Prec>& percentNum)
    {
      decimal<Prec> offset = basePrice * percentNum.getAsPercent();
      decimal<Prec> stopLoss = basePrice - offset;
      return stopLoss;
    }
  };

  template <int Prec> 
  class ShortStopLoss  : public StopLoss<Prec>
  {
  public:
    explicit ShortStopLoss(const decimal<Prec>& stopLoss)
      : StopLoss<Prec> (stopLoss) 
    {}

    ShortStopLoss (const decimal<Prec>& basePrice,
		       const PercentNumber<Prec>& percentNum)
      : StopLoss<Prec> (createStopFromPercent (basePrice, percentNum))
      {}

    ShortStopLoss(const ShortStopLoss<Prec>& rhs)
      : StopLoss<Prec> (rhs) 
    {}

    ShortStopLoss& 
    operator=(const ShortStopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      StopLoss<Prec>::operator=(rhs);
      return *this;
    }

    ~ShortStopLoss()
    {}

    const dec::decimal<Prec>& getStopLoss() const
    {
      return StopLoss<Prec>::getStopLoss();
    }

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
    static decimal<Prec> createStopFromPercent (const decimal<Prec>& basePrice,
					 const PercentNumber<Prec>& percentNum)
    {
      decimal<Prec> offset = basePrice * percentNum.getAsPercent();
      decimal<Prec> stopLoss = basePrice + offset;
      return stopLoss;
    }
  };

  template <int Prec> 
  class NullStopLoss  : public StopLoss<Prec>
  {
  public:
    NullStopLoss()
      : StopLoss<Prec> () 
    {}

    NullStopLoss(const NullStopLoss<Prec>& rhs)
      : StopLoss<Prec> (rhs) 
    {}

    NullStopLoss& 
    operator=(const NullStopLoss &rhs)
    {
      if (this == &rhs)
	return *this;

      StopLoss<Prec>::operator=(rhs);
      return *this;
    }

    ~NullStopLoss()
    {}

    const dec::decimal<Prec>& getStopLoss() const
    {
      throw std::domain_error(std::string("NullStopLoss::getStopLoss has no meaning"));
    }

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
