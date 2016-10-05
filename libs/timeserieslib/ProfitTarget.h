// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PROFIT_TARGET_H
#define __PROFIT_TARGET_H 1

#include <exception>
#include "decimal.h"
#include "DecimalConstants.h"
#include "PercentNumber.h"

using dec::decimal;

namespace mkc_timeseries
{
  template <int Prec> class ProfitTarget
  {
  public:
    explicit ProfitTarget(const decimal<Prec>& target)
      : mProfitTarget (target) 
    {}

    ProfitTarget(const ProfitTarget<Prec>& rhs)
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

    virtual const dec::decimal<Prec>& getProfitTarget() = 0;

    virtual bool isNullProfitTarget() = 0;
    virtual bool isLongProfitTarget() = 0;
    virtual bool isShortProfitTarget() = 0;

  protected:
   ProfitTarget()
      : mProfitTarget(DecimalConstants<Prec>::DecimalZero)
    {}

  private:
    dec::decimal<Prec> mProfitTarget;
  };

  template <int Prec>
  const dec::decimal<Prec>& ProfitTarget<Prec>::getProfitTarget()
    {
      return mProfitTarget;
    }

  template <int Prec> 
  class LongProfitTarget  : public ProfitTarget<Prec>
  {
  public:
    explicit LongProfitTarget(const decimal<Prec>& target)
      : ProfitTarget<Prec> (target) 
    {}

    LongProfitTarget(const decimal<Prec>& basePrice,
		     const PercentNumber<Prec>& percentNum)
      : ProfitTarget<Prec> (createTargetFromPercent (basePrice, percentNum))
      {}

    LongProfitTarget(const LongProfitTarget<Prec>& rhs)
      : ProfitTarget<Prec> (rhs) 
    {}

    LongProfitTarget& 
    operator=(const LongProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      ProfitTarget<Prec>::operator=(rhs);
      return *this;
    }

    ~LongProfitTarget()
    {}

    const dec::decimal<Prec>& getProfitTarget()
    {
      return ProfitTarget<Prec>::getProfitTarget();
    }

    bool isNullProfitTarget()
    {
      return false;
    }

    bool isLongProfitTarget()
    {
      return true;
    }

    bool isShortProfitTarget()
    {
      return false;
    }

  private:
    static decimal<Prec> createTargetFromPercent (const decimal<Prec>& basePrice,
					   const PercentNumber<Prec>& percentNum)
    {
      decimal<Prec> offset = basePrice * percentNum.getAsPercent();
      decimal<Prec> target = basePrice + offset;
      return target;
    }
  };

  template <int Prec> 
  class ShortProfitTarget  : public ProfitTarget<Prec>
  {
  public:
    explicit ShortProfitTarget(const decimal<Prec>& target)
      : ProfitTarget<Prec> (target) 
    {}

    ShortProfitTarget (const decimal<Prec>& basePrice,
		       const PercentNumber<Prec>& percentNum)
      : ProfitTarget<Prec> (createTargetFromPercent (basePrice, percentNum))
      {}

    ShortProfitTarget(const ShortProfitTarget<Prec>& rhs)
      : ProfitTarget<Prec> (rhs) 
    {}

    ShortProfitTarget& 
    operator=(const ShortProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      ProfitTarget<Prec>::operator=(rhs);
      return *this;
    }

    ~ShortProfitTarget()
    {}

    const dec::decimal<Prec>& getProfitTarget()
    {
      return ProfitTarget<Prec>::getProfitTarget();
    }

    bool isNullProfitTarget()
    {
      return false;
    }

    bool isLongProfitTarget()
    {
      return false;
    }

    bool isShortProfitTarget()
    {
      return true;
    }

  private:
    static decimal<Prec> createTargetFromPercent (const decimal<Prec>& basePrice,
					    const PercentNumber<Prec>& percentNum)
    {
      decimal<Prec> offset = basePrice * percentNum.getAsPercent();
      decimal<Prec> target = basePrice - offset;
      return target;
    }
  };

  template <int Prec> 
  class NullProfitTarget  : public ProfitTarget<Prec>
  {
  public:
    NullProfitTarget()
      : ProfitTarget<Prec> () 
    {}

    NullProfitTarget(const NullProfitTarget<Prec>& rhs)
      : ProfitTarget<Prec> (rhs) 
    {}

    NullProfitTarget& 
    operator=(const NullProfitTarget &rhs)
    {
      if (this == &rhs)
	return *this;

      ProfitTarget<Prec>::operator=(rhs);
      return *this;
    }

    ~NullProfitTarget()
    {}

    const dec::decimal<Prec>& getProfitTarget()
    {
      throw std::domain_error(std::string("NullProfitTarget::getProfiTarget has no meaning"));
    }

    bool isNullProfitTarget()
    {
      return true;
    }

    bool isLongProfitTarget()
    {
      return false;
    }

    bool isShortProfitTarget()
    {
      return false;
    }
  };
}
 
#endif
