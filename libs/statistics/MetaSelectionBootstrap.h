#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <utility>

#include "BiasCorrectedBootstrap.h"   // for Stat types; we won’t call its resampler now
#include "StatUtils.h"                // GeoMeanStat, num::to_double
#include "randutils.hpp"

namespace palvalidator
{
  namespace analysis
  {
    template <class Rng>
    static inline std::vector<uint8_t> make_restart_mask(std::size_t m, double L, Rng &rng)
    {
      // Geometric with mean L => restart probability p = 1/L.
      // We encode a mask where mask[t] == 1 means "restart at t".
      const double p = (L <= 1.0) ? 1.0 : (1.0 / L);

      std::bernoulli_distribution restart_dist(p);

      std::vector<uint8_t> mask(m, 0);
      mask[0] = 1; // Always restart at t=0.

      for (std::size_t t = 1; t < m; ++t)
	{
	  mask[t] = restart_dist(rng) ? 1u : 0u;
	}
      return mask;
    }
    
    /**
     * MetaSelectionBootstrap (synchronised blocks)
     *
     * Selection-aware outer bootstrap for a single meta-strategy.
     * For each replicate:
     *   1) Build a SHARED stationary-bootstrap index path of length m = min_i n_i
     *      using mean block length L (probability p = 1/L of restart).
     *   2) For each component series i, map the shared index path modulo n_i to
     *      produce a resampled component series of length m.
     *   3) Rebuild the meta using `metaBuilder(resampledComponents)`.
     *   4) Record the per-period GeoMean (log-aware) statistic.
     * Return a percentile CI (lower bound per-period + annualized).
     *
     * Notes
     * - Synchronized restarts/extensions preserve cross-strategy timing co-movement,
     *   reducing optimism relative to independent per-strategy resampling.
     * - We use percentile on the outer layer (simple, robust); inner layers in your
     *   pipeline already use BCa where appropriate.
     */
    template <class Num,
	      class Rng = randutils::mt19937_rng>
    class MetaSelectionBootstrap {
    public:
      using Series  = std::vector<Num>;
      using Matrix  = std::vector<Series>;
      using Builder = std::function<Series(const Matrix&)>;

      struct Result {
	Num      lbPerPeriod;
	Num      lbAnnualized;
	double   cl;
	std::size_t B;
      };

      MetaSelectionBootstrap(std::size_t B,
			     double confidenceLevel,
			     std::size_t meanBlockLength,
			     double periodsPerYear)
	: mB(B),
	  mCL(confidenceLevel),
	  mL(meanBlockLength),
	  mPPY(periodsPerYear)
      {
	if (mB < 400)
	  throw std::invalid_argument("MetaSelectionBootstrap: B should be >= 400");

	if (!(mCL > 0.5 && mCL < 1.0))
	  throw std::invalid_argument("MetaSelectionBootstrap: CL must be in (0.5,1)");

	if (mL < 1)
	  throw std::invalid_argument("MetaSelectionBootstrap: mean block length must be >= 1");

	if (!(mPPY > 0.0))
	  throw std::invalid_argument("MetaSelectionBootstrap: periodsPerYear must be > 0");
      }

      Result run(const Matrix& componentReturns,
		 const Builder& metaBuilder,
		 Rng& rng) const
      {
	if (componentReturns.empty())
	  {
	    throw std::invalid_argument("MetaSelectionBootstrap.run: no components");
	  }

	// Require at least 2 obs per component; find common length m
	std::size_t m = std::numeric_limits<std::size_t>::max();
	for (const auto& s : componentReturns)
	  {
	    if (s.size() < 2)
	      {
		throw std::invalid_argument("MetaSelectionBootstrap.run: component too short");
	      }
	    m = std::min(m, s.size());
	  }
	if (m < 2)
	  {
	    throw std::invalid_argument("MetaSelectionBootstrap.run: insufficient common length");
	  }

	// Statistic (log-aware geometric mean) with your ruin/winsor guards
	mkc_timeseries::GeoMeanStat<Num> geoStat(
						 /*clip_ruin=*/true,
						 /*winsor_small_n=*/true,
						 /*winsor_alpha=*/0.02,
						 /*ruin_eps=*/1e-8   // must be double
						 );

	std::vector<Num> stats;
	stats.reserve(mB);

	const std::size_t k = componentReturns.size();
	Matrix resampled(k, Series(m));

	for (std::size_t b = 0; b < mB; ++b)
	  {
	    // (1) Shared restart mask (synchronized block timing across strategies)
	    const std::vector<uint8_t> restart_mask =
	      make_restart_mask(m, static_cast<double>(mL), rng.engine());

	    // (2) Per-component resampling with uniform starts at each restart
	    bool degenerate = false;

	    for (std::size_t i = 0; i < k; ++i)
	      {
		const auto& src = componentReturns[i];
		const std::size_t n_i = src.size();
		if (n_i == 0)
		  {
		    degenerate = true;
		    break;
		  }

		auto& dst = resampled[i];
		dst.resize(m);

		std::uniform_int_distribution<std::size_t> ustart(0, n_i - 1);

		std::size_t pos = 0;
		bool have_pos = false;

		for (std::size_t t = 0; t < m; ++t)
		  {
		    if (restart_mask[t] || !have_pos)
		      {
			pos = ustart(rng.engine());
			have_pos = true;
		      }
		    else
		      {
			pos = (pos + 1) % n_i;
		      }

		    dst[t] = src[pos];
		  }
	      }

	    if (degenerate)
	      {
		continue;
	      }

	    // (3) Rebuild the meta using the production rule
	    Series meta = metaBuilder(resampled);
	    if (meta.size() < 2)
	      {
		continue;
	      }

	    // (4) Statistic: per-period GeoMean (log-aware)
	    const Num gm = geoStat(meta);
	    stats.emplace_back(gm);
	  }

	if (stats.size() < mB / 2)
	  {
	    throw std::runtime_error("MetaSelectionBootstrap: too many degenerate replicates");
	  }

	// (5) Hyndman–Fan type-7 quantile for the lower bound
	std::sort(stats.begin(), stats.end());
	const double alpha = 1.0 - mCL;
	const double n = static_cast<double>(stats.size());
	const double p = alpha;
	const double h = (n - 1) * p + 1.0;   // type-7 definition
	const std::size_t i = static_cast<std::size_t>(std::floor(h));
	const double frac = h - static_cast<double>(i);

	Num lbPer;
	if (i <= 1)
	  {
	    lbPer = stats.front();
	  }
	else if (i >= n)
	  {
	    lbPer = stats.back();
	  }
	else
	  {
	    const Num x0 = stats[i - 1];
	    const Num x1 = stats[i];
	    lbPer = x0 + (x1 - x0) * Num(frac);
	  }

	// (6) Annualize via exp(K*log1p(g)) - 1 for stability
	const long double g  = static_cast<long double>(num::to_double(lbPer));
	const long double K  = static_cast<long double>(mPPY);
	const long double ann = std::exp(K * std::log1p(g)) - 1.0L;
	const Num lbAnn = Num(static_cast<double>(ann));

	return Result{lbPer, lbAnn, mCL, mB};
      }
      
    private:
      std::size_t mB;
      double      mCL;
      std::size_t mL;
      double      mPPY;
    };

  }
} // namespace palvalidator::analysis
