#include "FragileEdgeAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "DecimalConstants.h"
#include "StatUtils.h"

using namespace mkc_timeseries;

namespace
{
  // Absolute value that stays in Num
  template <typename Num>
  inline Num absNum(const Num& x) {
    return (x < Num(0)) ? (-x) : x;
  }

  // Near-hurdle check using absolute and relative bands (Num-native)
  template <typename Num>
  bool isNearHurdle(const Num& lbAnn_GM,
		    const Num& hurdleAnn,
		    double nearAbs,     // policy.nearAbs
		    double nearRel)     // policy.nearRel
  {
    const Num gap = absNum(lbAnn_GM - hurdleAnn);

    // Absolute band
    const bool withinAbs = gap <= Num(nearAbs);

    // Relative band (only meaningful if hurdle > 0)
    bool withinRel = false;
    if (hurdleAnn > Num(0)) {
      withinRel = (gap / hurdleAnn) <= Num(nearRel);
    }

    return withinAbs || withinRel;
  }

  // Tail severity vs per-period edge, using a generic tail point (Q05 or ES05)
  template <typename Num>
  bool isSevereTailAgainstEdge(const Num& tailPoint,
			       const Num& edgePer_GM,
			       double tailMultiple) // policy.tailMultiple
  {
    if (!(edgePer_GM > Num(0))) return false;      // need a positive edge
    if (!(tailPoint < Num(0))) return false;       // only downside tails
    const Num depth = absNum(tailPoint);
    return depth > (Num(tailMultiple) * edgePer_GM);
  }
} // anonymous namespace

namespace palvalidator
{
  namespace analysis
  {
    FragileDecision<FragileEdgeAnalyzer::Num> FragileEdgeAnalyzer::analyzeFragileEdge(
										      const Num& lbPer_GM,
										      const Num& lbAnn_GM,
										      const Num& hurdleAnn,
										      double     relVarL,
										      const Num& q05,
										      const Num& es05,
										      size_t     n,
										      const FragileEdgePolicy& pol)
    {
      // 1) Near-hurdle test (Num-native, no precision loss)
      const bool nearHurdle = isNearHurdle(lbAnn_GM, hurdleAnn, pol.nearAbs, pol.nearRel);

      // 2) Tail severity: consider BOTH Q05 and ES05 (OR rule)
      const bool severeTailQ  = isSevereTailAgainstEdge(q05,  lbPer_GM, pol.tailMultiple);
      const bool severeTailES = isSevereTailAgainstEdge(es05, lbPer_GM, pol.tailMultiple);
      const bool severeTail   = (severeTailQ || severeTailES);

      // 3) Heuristic advisory tree (kept from your original, but now ES-aware)

      // 3.1 Severe tails + near hurdle → DROP
      if (severeTail && nearHurdle) {
        std::string which = severeTailES
	  ? "ES05 and/or Q05"
	  : "Q05";
        return { FragileEdgeAction::Drop,
                 0.0,
                 "Severe downside tails (" + which + ") and LB near hurdle → drop" };
      }

      // 3.2 Very large L-variability and near hurdle → DROP
      if (relVarL > pol.relVarDrop && nearHurdle) {
        return { FragileEdgeAction::Drop,
                 0.0,
                 "High L-sensitivity and LB near hurdle → drop" };
      }

      // 3.3 Otherwise: downweight if any soft risk flags
      if (severeTail || relVarL > pol.relVarDown || n < pol.minNDown) {
        std::string why;
        if (severeTail)              { if (!why.empty()) why += "; "; why += (severeTailES ? "severe tails (ES05/Q05)" : "severe tails (Q05)"); }
        if (relVarL > pol.relVarDown){ if (!why.empty()) why += "; "; why += "high L-variability"; }
        if (n < pol.minNDown)        { if (!why.empty()) why += "; "; why += "small sample"; }

        return { FragileEdgeAction::Downweight,
                 0.50,
                 "Advisory downweight: " + (why.empty() ? std::string("weak signal") : why) };
      }

      // 3.4 Default keep
      return { FragileEdgeAction::Keep, 1.0, "Robust enough to keep at full weight" };
    }

    /**
     * @brief Calculates the Quantile (Value at Risk) and Expected Shortfall for a given set of returns.
     *
     * This function processes a vector of returns to find two key downside risk metrics
     * based on a specified quantile threshold (alpha).
     *
     * @tparam Num A numeric type (e.g., double, float, or a custom Decimal class) used for
     * calculations.
     *
     * @param returns A std::vector of returns. The vector does not need to be sorted.
     * An empty vector will result in {0, 0} being returned.
     * @param alpha   The quantile threshold, typically 0.05 for 5% or 0.01 for 1%.
     * This value determines the cutoff point for the risk calculations.
     *
     * @return A std::pair<Num, Num> containing:
     * - **first (Q.alpha)**: The Quantile, or **Value at Risk (VaR)**. This represents
     * the worst-case return expected at the 'alpha' probability level.
     * For example, an alpha of 0.05 yields the 5th percentile return,
     * meaning 5% of all returns are at or worse than this value.
     * It is calculated as the k-th smallest element, where k = floor(alpha * n).
     *
     * - **second (ES.alpha)**: The **Expected Shortfall (ES)**, also known as
     * Conditional Value at Risk (CVaR). This measures the *average* return
     * of all outcomes that are worse than or equal to the VaR. It provides
     * a more complete picture of the "tail risk" by answering: "If things
     * do go bad (i.e., we cross the VaR threshold), what is our
     * average expected loss?" It is calculated by averaging all returns
     * from index 0 to k in the sorted vector.
     *
     * @note This implementation uses the historical simulation method. It finds the k-th
     * element (where k = floor(alpha * n)) as the VaR and averages all elements
     * from index 0 to k (inclusive) for the ES.
     */
    std::pair<FragileEdgeAnalyzer::Num, FragileEdgeAnalyzer::Num>
    FragileEdgeAnalyzer::computeQ05_ES05(const std::vector<Num>& r, double alpha)
    {
      // Returns {Q_alpha, ES_alpha} using type-7 quantile and a consistent ES
      // with fractional inclusion of the boundary order statistic.

      using D = Num;
      const std::size_t n = r.size();
      if (n == 0)
        return { D(0), D(0) };

      // Clamp alpha to (0, 1). (Caller uses left-tail, e.g., 0.05.)
      if (alpha <= 0.0)
	alpha = 0.0;

      if (alpha >= 1.0)
	alpha = 1.0;

      // --- Quantile (type-7 via StatUtils to stay canonical everywhere) ---
      const D q = mkc_timeseries::StatUtils<D>::quantile(r, alpha);

      // --- ES with type-7–consistent fractional boundary weight -------------
      // Sort ascending once (n is small: O(n log n) is fine and simple).
      std::vector<D> v(r.begin(), r.end());
      std::sort(v.begin(), v.end());

      // Type-7 indexing baseline: idx = alpha * (n-1)
      const double idx = alpha * (static_cast<double>(n) - 1.0);
      const std::size_t lo = static_cast<std::size_t>(std::floor(idx));
      const double w = idx - std::floor(idx); // fractional part in [0,1)

      // Effective tail "count" (with fractional boundary)
      const double effCount = static_cast<double>(lo) + w;

      D es = D(0);
      if (effCount <= 0.0)
	{
	  // Extremely small alpha → ES collapses to minimum (v[0]).
	  es = v.front();
	}
      else
	{
	  // Sum the strict lower order stats [0 .. lo-1]
	  D sum = D(0);
	  for (std::size_t i = 0; i < lo; ++i)
	    sum += v[i];

        // Add fractional boundary contribution at index 'lo' (if lo<n)
        if (lo < n)
	  sum += v[lo] * D(w);

        es = sum / D(effCount);
      }

      return { q, es };
    }
  } // namespace analysis
} // namespace palvalidator
