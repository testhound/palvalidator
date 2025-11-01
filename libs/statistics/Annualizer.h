#ifndef __ANNUALIZER_H
#define __ANNUALIZER_H

#include <cmath>
#include <limits>
#include <stdexcept>
#include "number.h"
#include "DecimalConstants.h"

namespace mkc_timeseries
{
  /**
   * Annualizer for per-period returns.
   *
   * Provides stable annualization via:  (1 + r)^K - 1
   * implemented as exp(K * log1p(r)) - 1 with guards around r <= -1.
   *
   * Use annualize_one() for a single value, or annualize_triplet() for (lower, mean, upper).
   */
  template <class Decimal>
  class Annualizer
  {
  public:
    struct Triplet
    {
      Decimal lower{};
      Decimal mean{};
      Decimal upper{};
    };

    /**
     * Annualize a single per-period return r to K periods.
     *
     * Guards:
     * - If r <= -1, clamp to (-1 + eps) to keep log1p defined.
     * - After transform, if numerical underflow produces exactly -1,
     *   bump to (-1 + bump) so the result remains > -1 in Decimal quantization.
     */
    static Decimal annualize_one(const Decimal& r, double K,
                                 double eps = 1e-12,
                                 long double bump = 1e-7L)
    {
      if (!(K > 0.0) || !std::isfinite(K))
      {
        throw std::invalid_argument("Annualizer: K must be positive and finite.");
      }

      const Decimal neg1  = DecimalConstants<Decimal>::DecimalMinusOne;
      const Decimal epsD  = Decimal(eps);
      const Decimal r_clip = (r > neg1) ? r : (neg1 + epsD);

      const long double lr = std::log1p(static_cast<long double>(num::to_double(r_clip)));
      const long double KK = static_cast<long double>(K);
      long double y = std::exp(KK * lr) - 1.0L;

      if (y <= -1.0L)
      {
        y = -1.0L + bump;
      }
      return Decimal(static_cast<double>(y));
    }

    /**
     * Annualize (lower, mean, upper) together with the same settings.
     */
    static Triplet annualize_triplet(const Decimal& lower,
                                     const Decimal& mean,
                                     const Decimal& upper,
                                     double K,
                                     double eps = 1e-12,
                                     long double bump = 1e-7L)
    {
      Triplet t;
      t.lower = annualize_one(lower, K, eps, bump);
      t.mean  = annualize_one(mean,  K, eps, bump);
      t.upper = annualize_one(upper, K, eps, bump);
      return t;
    }
  };
} // namespace mkc_timeseries

#endif // __ANNUALIZER_H
