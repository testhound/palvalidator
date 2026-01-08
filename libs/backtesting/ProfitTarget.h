// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PROFIT_TARGET_H
#define __PROFIT_TARGET_H 1

#include <exception>
#include "number.h"
#include "DecimalConstants.h"
#include "PercentNumber.h"

namespace mkc_timeseries
{
  template <class Decimal> class ProfitTarget
  {
  public:
    explicit ProfitTarget(const Decimal& target)
      : mProfitTarget (target) 
    {}

    ProfitTarget(const ProfitTarget<Decimal>& rhs)
      : mProfitTarget (rhs.mProfitTarget) 
    {}

    ProfitTarget& 
    operator=(const ProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      mProfitTarget = rhs.mProfitTarget;
      return *this;
    }

    virtual ~ProfitTarget()
    {}

    const Decimal& getProfitTarget() const
    {
      return mProfitTarget;
    }

    virtual bool isNullProfitTarget() const = 0;
    virtual bool isLongProfitTarget() const = 0;
    virtual bool isShortProfitTarget() const = 0;

  protected:
   ProfitTarget()
      : mProfitTarget(DecimalConstants<Decimal>::DecimalZero)
    {}

  private:
    Decimal mProfitTarget;
  };

  template <class Decimal> 
  class LongProfitTarget  : public ProfitTarget<Decimal>
  {
  public:
    explicit LongProfitTarget(const Decimal& target)
      : ProfitTarget<Decimal> (target) 
    {}

    LongProfitTarget(const Decimal& basePrice,
		     const PercentNumber<Decimal>& percentNum)
      : ProfitTarget<Decimal> (createTargetFromPercent (basePrice, percentNum))
      {}

    LongProfitTarget(const LongProfitTarget<Decimal>& rhs)
      : ProfitTarget<Decimal> (rhs) 
    {}

    LongProfitTarget& 
    operator=(const LongProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      ProfitTarget<Decimal>::operator=(rhs);
      return *this;
    }

    ~LongProfitTarget()
    {}

    bool isNullProfitTarget() const
    {
      return false;
    }

    bool isLongProfitTarget() const
    {
      return true;
    }

    bool isShortProfitTarget() const
    {
      return false;
    }

  private:
    static Decimal createTargetFromPercent (const Decimal& basePrice,
					   const PercentNumber<Decimal>& percentNum)
    {
      Decimal offset = basePrice * percentNum.getAsPercent();
      Decimal target = basePrice + offset;
      return target;
    }
  };

  template <class Decimal> 
  class ShortProfitTarget  : public ProfitTarget<Decimal>
  {
  public:
    explicit ShortProfitTarget(const Decimal& target)
      : ProfitTarget<Decimal> (target) 
    {}

    ShortProfitTarget (const Decimal& basePrice,
		       const PercentNumber<Decimal>& percentNum)
      : ProfitTarget<Decimal> (createTargetFromPercent (basePrice, percentNum))
      {}

    ShortProfitTarget(const ShortProfitTarget<Decimal>& rhs)
      : ProfitTarget<Decimal> (rhs) 
    {}

    ShortProfitTarget& 
    operator=(const ShortProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      ProfitTarget<Decimal>::operator=(rhs);
      return *this;
    }

    ~ShortProfitTarget()
    {}

    bool isNullProfitTarget() const
    {
      return false;
    }

    bool isLongProfitTarget() const
    {
      return false;
    }

    bool isShortProfitTarget() const
    {
      return true;
    }

  private:
    static Decimal createTargetFromPercent (const Decimal& basePrice,
					    const PercentNumber<Decimal>& percentNum)
    {
      Decimal offset = basePrice * percentNum.getAsPercent();
      Decimal target = basePrice - offset;
      return target;
    }
  };

  template <class Decimal> 
  class NullProfitTarget  : public ProfitTarget<Decimal>
  {
  public:
    NullProfitTarget()
      : ProfitTarget<Decimal> () 
    {}

    NullProfitTarget(const NullProfitTarget<Decimal>& rhs)
      : ProfitTarget<Decimal> (rhs) 
    {}

    NullProfitTarget& 
    operator=(const NullProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      ProfitTarget<Decimal>::operator=(rhs);
      return *this;
    }

    ~NullProfitTarget()
    {}

    bool isNullProfitTarget() const
    {
      return true;
    }

    bool isLongProfitTarget() const
    {
      return false;
    }

    bool isShortProfitTarget() const
    {
      return false;
    }
  };
}
 
#endif
