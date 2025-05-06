#pragma once
#include <vector>
#include <cmath>
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  template<class Decimal>
    struct StatUtils
    {
      // PF = sum(positive returns) / sum(abs(negative returns))

      static Decimal computeProfitFactor(const std::vector<Decimal>& xs)
      {
	Decimal win=0, loss=0;

	for (auto r: xs)
	  {
	    if (r > 0)
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
	Decimal lw=0, ll=0;

	for (auto r: xs)
	  {
	    double m = 1 + num::to_double(r);

	    if (m <= 0)
	      continue;

	    Decimal lr = num::to_decimal(std::log(m));
	    if (r>0)
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
