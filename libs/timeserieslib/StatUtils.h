#pragma once
#include <vector>
#include <cmath>
#include "DecimalConstants.h"
#include "number.h"

namespace mkc_timeseries
{
  template<class Decimal>
    struct StatUtils
    {
      // PF = sum(positive returns) / sum(abs(negative returns))

      static Decimal computeProfitFactor(const std::vector<Decimal>& xs)
      {
	Decimal win(DecimalConstants<Decimal>::DecimalZero);
	Decimal loss(DecimalConstants<Decimal>::DecimalZero);

	for (auto r: xs)
	  {
	    if (r > DecimalConstants<Decimal>::DecimalZero)
	      win += r;
	    else
	      loss += r;
	  }
    
	if (loss == DecimalConstants<Decimal>::DecimalZero) 
	  return DecimalConstants<Decimal>::DecimalOneHundred;

	return win / num::abs(loss);
      }

      // LPF = sum(log(1+r>1)) / abs(sum(log(1+r<1)))
      static Decimal computeLogProfitFactor(const std::vector<Decimal>& xs)
      {
	Decimal lw(DecimalConstants<Decimal>::DecimalZero);
	Decimal ll(DecimalConstants<Decimal>::DecimalZero);

	for (auto r: xs)
	  {
	    double m = 1 + num::to_double(r);

	    if (m <= 0)
	      continue;

	    Decimal lr(std::log(m));
	    if (r > DecimalConstants<Decimal>::DecimalZero)
	      lw += lr;
	    else
	      ll += lr;
	  }

	if (ll == DecimalConstants<Decimal>::DecimalZero)
	  return DecimalConstants<Decimal>::DecimalOneHundred;

	return lw / num::abs(ll);
      }
    };
} 
