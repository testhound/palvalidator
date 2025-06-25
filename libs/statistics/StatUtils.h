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

	for (auto r : xs)
	  {
	    if (r > DecimalConstants<Decimal>::DecimalZero)
	      win += r;
	    else
	      loss += r;
	  }
	
	Decimal pf;
	
	if (loss == DecimalConstants<Decimal>::DecimalZero)
	  {
	    std::size_t n = xs.size();
	    Decimal k("0.01");
	    Decimal epsilon = k / Decimal(std::max<size_t>(1, n));
	    pf = win / epsilon;
	  }
	else
	  {
	    pf = win / num::abs(loss);
	  }
	
	return Decimal(std::log(num::to_double(DecimalConstants<Decimal>::DecimalOne + pf)));
      }

      // LPF = sum(log(1+r>1)) / abs(sum(log(1+r<1)))
      //if (ll == DecimalConstants<Decimal>::DecimalZero)
	// return DecimalConstants<Decimal>::DecimalOneHundred;
      
      static Decimal computeLogProfitFactor(const std::vector<Decimal>& xs)
      {
	Decimal lw(DecimalConstants<Decimal>::DecimalZero);
	Decimal ll(DecimalConstants<Decimal>::DecimalZero);

	for (auto r : xs)
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
	
	Decimal pf;

	// Don't allow profit factors of 100.0 when there are no
	// losses
	if (ll == DecimalConstants<Decimal>::DecimalZero)
	  {
	    std::size_t n = xs.size();
	    Decimal k("0.01"); // smoothing factor
	    Decimal epsilon = k / Decimal(std::max<size_t>(1, n));
	    pf = lw / epsilon;
	  }
	else
	  {
	    pf = lw / num::abs(ll);
	  }
	
	// Apply log compression to the final profit factor
	return Decimal(std::log(num::to_double(DecimalConstants<Decimal>::DecimalOne + pf)));
      }
    };
} 
