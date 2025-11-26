#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <string>
#include <ostream>
#include <optional>
#include <random>
#include <iomanip>
#include "BiasCorrectedBootstrap.h"
#include "StationaryMaskResamplers.h"
#include "BootstrapConfig.h"
#include "Annualizer.h"
#include "StatUtils.h"

namespace palvalidator::bootstrap_helpers
{

  // -----------------------------------------------------------------------------
  // Forward declarations and helper utilities needed by template functions
  // -----------------------------------------------------------------------------

  /**
   * @brief Maps a two-sided confidence level to a Z-score (standard normal quantile).
   *
   * Used primarily to back-out an approximate standard deviation (Sigma) from
   * the width of a confidence interval for logging purposes.
   *
   * @param cl The confidence level (e.g., 0.95).
   * @return double The Z-score (e.g., 1.96). Defaults to 1.96 if CL is unrecognized.
   */
  inline double z_from_two_sided_CL(double cl)
  {
    // cl = 0.90 → z≈1.645, 0.95 → 1.960, 0.975 → 2.241 (rare), 0.99 → 2.576
    // Fallback to 1.96 if unrecognized.
    if (cl >= 0.989 && cl <= 0.991)
      return 2.576; // 99%
    
    if (cl >= 0.949 && cl <= 0.951)
      return 1.960; // 95%
    
    if (cl >= 0.899 && cl <= 0.901)
      return 1.645; // 90%
    
    if (cl >= 0.974 && cl <= 0.976)
      return 2.241; // ~97.5%
    
    return 1.960;
  }

  // tiny traits to detect availability of 'upper' member and getUpperBound()
  namespace detail
  {
    template <class T, class = void>
    struct has_member_upper : std::false_type {};
    template <class T>
    struct has_member_upper<T, std::void_t<decltype(std::declval<T>().upper)>> : std::true_type {};

    template <class T, class = void>
    struct has_getUpperBound : std::false_type {};
    template <class T>
    struct has_getUpperBound<T, std::void_t<decltype(std::declval<T>().getUpperBound())>> : std::true_type {};
  }

  // -----------------------------------------------------------------------------
  // Lightweight result carriers used by stages (kept simple on purpose)
  // -----------------------------------------------------------------------------

  /**
   * @brief Lightweight result container for a simple m-out-of-n bootstrap run.
   * @tparam Num The numeric type (e.g., double, decimal).
   */
  template<class Num>
  struct MNRunSimple
  {
    Num          lower{};         ///< Per-period lower bound (e.g., 5th percentile).
    std::size_t  m_sub{0};        ///< Subsample size used (m).
    std::size_t  L{0};            ///< Block length used.
    std::size_t  effective_B{0};  ///< Number of valid (non-degenerate) replicates generated.
  };

  /**
   * @brief Lightweight result container for a Percentile-t bootstrap run.
   * @tparam Num The numeric type.
   */
  template<class Num>
  struct PTRunSimple
  {
    Num          lower{};         ///< Per-period lower bound.
    std::size_t  m_outer{0};      ///< Outer loop subsample size.
    std::size_t  m_inner{0};      ///< Inner loop (variance estimation) subsample size.
    std::size_t  L{0};            ///< Block length used.
    std::size_t  effective_B{0};  ///< Number of valid replicates.
  };

  /**
   * @brief Encapsulates distributional characteristics of the return series for adaptive m/n decision-making.
   *
   * @details
   * This class acts as a Data Transfer Object (DTO) that carries all relevant statistical
   * properties of the input data (sample size, volatility, shape, and tail behavior) required
   * by the bootstrapping policies (e.g., `TailVolPriorPolicy`).
   *
   * By bundling these metrics, it allows policy classes to make informed decisions about:
   * - Whether the market is "Wild" (High Volatility / Heavy Tails) or "Stable".
   * - What the baseline subsampling ratio ($\rho = m/n$) should be.
   */
  class MNRatioContext
  {
  public:
    /**
     * @brief Constructs the context with calculated statistical metrics.
     *
     * @param n The sample size (number of observations).
     * @param sigmaAnn The annualized volatility (standard deviation).
     * @param skew The sample skewness.
     * @param exkurt The sample excess kurtosis.
     * @param tailIndex The estimated Pareto tail index ($\alpha$) via the Hill estimator.
     * Values $\le 0$ indicate an invalid or failed estimate.
     * @param heavyTails Boolean flag indicating if basic shape heuristics (skew/kurtosis)
     * detected heavy tails.
     */
    MNRatioContext(std::size_t n,
                   double      sigmaAnn,
                   double      skew,
                   double      exkurt,
                   double      tailIndex,
                   bool        heavyTails)
      : n_(n)
      , sigmaAnn_(sigmaAnn)
      , skew_(skew)
      , exkurt_(exkurt)
      , tailIndex_(tailIndex)
      , heavyTails_(heavyTails)
    {
    }

    /**
     * @brief Gets the sample size.
     * @return std::size_t Number of returns ($n$).
     */
    std::size_t getN() const
    {
      return n_;
    }

    /**
     * @brief Gets the annualized volatility.
     * @return double Annualized Standard Deviation ($\sigma_{ann}$).
     */
    double      getSigmaAnn() const
    {
      return sigmaAnn_;
    }

    /**
     * @brief Gets the sample skewness.
     * @return double Skewness value.
     */
    double      getSkew() const
    {
      return skew_;
    }

    /**
     * @brief Gets the sample excess kurtosis.
     * @return double Excess Kurtosis (Normal distribution $\approx 0$).
     */
    double      getExKurt() const
    {
      return exkurt_;
    }

    /**
     * @brief Gets the estimated Pareto tail index ($\alpha$).
     * @details Smaller values indicate heavier tails (e.g., $\alpha < 2$ implies infinite variance).
     * @return double The tail index, or $\le 0.0$ if estimation failed/insufficient data.
     */
    double      getTailIndex() const
    {
      return tailIndex_;
    }

    /**
     * @brief Checks if the basic heavy-tail heuristic triggered.
     * @return true If |Skew| > 0.9 or ExKurt > 1.2 (typical defaults).
     */
    bool        hasHeavyTails() const
    {
      return heavyTails_;
    }

  private:
    std::size_t n_;
    double      sigmaAnn_;
    double      skew_;
    double      exkurt_;
    double      tailIndex_;   // Pareto α estimate; <= 0 if invalid
    bool        heavyTails_;
  };

  /**
   * @brief Abstract interface for policies that determine a "prior" m-out-of-n ratio.
   *
   * @details
   * This interface defines a contract for logic that inspects the statistical context
   * of a return series (sample size, volatility, tail index) and proposes a baseline
   * subsampling ratio $\rho = m/n$.
   *
   * Implementations of this interface (e.g., `TailVolPriorPolicy`) are expected to remain
   * computationally cheap (no heavy simulations) and deterministic based on the `MNRatioContext`.
   * The returned ratio is a "prior belief" that can be further refined by other components.
   */
  class IMNRatioPolicy
  {
  public:
    virtual ~IMNRatioPolicy() = default;

    /**
     * @brief Computes a suggested m-out-of-n ratio based on the provided context.
     *
     * @param ctx The statistical context of the return series (N, vol, skew, tails).
     * @return double A ratio $\rho \in (0, 1]$ representing $m/n$.
     */
    virtual double computePriorRatio(const MNRatioContext& ctx) const = 0;
  };

  /**
   * @brief A robust prior policy that adapts the m/n ratio based on volatility and tail heaviness.
   *
   * @details
   * This policy classifies the market regime into three categories and assigns a target ratio to each:
   *
   * 1. **High Volatility / Heavy Tail Regime:**
   * - Triggered if annualized volatility $\ge$ threshold (default 40%), OR
   * - Tail index $\alpha \le$ threshold (default 2.0), OR
   * - Skew/Kurtosis heuristics indicate heavy tails.
   * - **Target Ratio:** `highVolRatio_` (default 0.80).
   * - *Rationale:* In "Wild" markets, alpha is often concentrated in rare tail events.
   * Aggressive subsampling (small m) would miss these events, falsely failing the strategy.
   *
   * 2. **Very Light Tail Regime (Large N):**
   * - Triggered if N is large ($\ge 50$), Volatility is low, and tails are thin ($\alpha \ge 4.0$).
   * - **Target Ratio:** `lightTailRatio_` (default 0.35).
   * - *Rationale:* In well-behaved, large datasets, we can afford to follow asymptotic theory
   * ($m/n \to 0$) more strictly to maximize the independence of subsamples.
   *
   * 3. **Normal Regime:**
   * - Everything else.
   * - **Target Ratio:** `normalRatio_` (default 0.50).
   * - *Rationale:* A conservative middle ground for typical market strategies.
   *
   * **Safety:**
   * - For ultra-small samples ($N < 5$), it falls back to a simple 50% rule to ensure stability.
   * - The final ratio is always clamped to valid bounds $[2/N, (N-1)/N]$.
   */
  class TailVolPriorPolicy : public IMNRatioPolicy
  {
  public:
    /**
     * @brief Constructs the policy with configurable thresholds and target ratios.
     *
     * @param highVolAnnThreshold Annualized volatility threshold for "High Vol" regime (default 0.40).
     * @param highVolRatio Target m/n ratio for high volatility regimes (default 0.80).
     * @param normalRatio Target m/n ratio for normal regimes (default 0.50).
     * @param lightTailRatio Target m/n ratio for light-tail/large-N regimes (default 0.35).
     * @param heavyTailAlphaThreshold Pareto tail index $\alpha$ below which tails are considered "Heavy" (default 2.0).
     * @param lightTailAlphaThreshold Pareto tail index $\alpha$ above which tails are considered "Light" (default 4.0).
     * @param nLargeThreshold Minimum sample size N required to trigger "Light Tail" logic (default 50).
     */
    TailVolPriorPolicy(double highVolAnnThreshold       = 0.40,
		       double highVolRatio              = 0.80,
		       double normalRatio               = 0.50,
		       double lightTailRatio            = 0.35,
		       double heavyTailAlphaThreshold   = 2.0,
		       double lightTailAlphaThreshold   = 4.0,
		       std::size_t nLargeThreshold      = 50)
      : highVolAnnThreshold_(highVolAnnThreshold)
      , highVolRatio_(highVolRatio)
      , normalRatio_(normalRatio)
      , lightTailRatio_(lightTailRatio)
      , heavyTailAlphaThreshold_(heavyTailAlphaThreshold)
      , lightTailAlphaThreshold_(lightTailAlphaThreshold)
      , nLargeThreshold_(nLargeThreshold)
    {
    }

    double getHighVolAnnThreshold() const     { return highVolAnnThreshold_; }
    double getHighVolRatio() const            { return highVolRatio_; }
    double getNormalRatio() const             { return normalRatio_; }
    double getLightTailRatio() const          { return lightTailRatio_; }
    double getHeavyTailAlphaThreshold() const { return heavyTailAlphaThreshold_; }
    double getLightTailAlphaThreshold() const { return lightTailAlphaThreshold_; }
    std::size_t getNLargeThreshold() const    { return nLargeThreshold_; }

    /**
     * @brief Computes the prior m/n ratio based on the statistical context.
     *
     * @param ctx The statistical context (N, vol, skew, tails).
     * @return double The target ratio $\rho$, clamped to valid bounds.
     */
    double computePriorRatio(const MNRatioContext& ctx) const override
    {
      const std::size_t n = ctx.getN();

      // Degenerate: let caller handle n < 3 specially if needed
      if (n < 3)
	return 1.0;

      // Clamping bounds: 2 <= m <= n-1
      const double minRho = 2.0 / static_cast<double>(n);
      const double maxRho = (n > 2)
        ? static_cast<double>(n - 1) / static_cast<double>(n)
        : 0.5;

      // For ultra-tiny n, use ~50% rule regardless of regime
      if (n < 5)
	{
	  const double m = std::max(2.0,
				    std::min(std::ceil(0.50 * n),
					     static_cast<double>(n - 1)));
	  double rho = m / static_cast<double>(n);
	  rho = std::max(minRho, std::min(rho, maxRho));
	  return rho;
	}

      const double sigmaAnn = ctx.getSigmaAnn();
      const double tailIdx  = ctx.getTailIndex();
      const bool   heavyFlg = ctx.hasHeavyTails();

      const bool tailIdxValid = (tailIdx > 0.0);

      // Very heavy tails (α small) – classical “infinite-variance-ish” region
      const bool extremeHeavyTail =
        tailIdxValid && (tailIdx <= heavyTailAlphaThreshold_);

      // High-vol regime: heavy tails OR high σ_ann
      const bool isHighVol =
        extremeHeavyTail ||
        heavyFlg ||
        (sigmaAnn >= highVolAnnThreshold_);

      // Very light tails, only considered when n is large and not high-vol
      const bool isVeryLightTail =
        tailIdxValid &&
        (tailIdx >= lightTailAlphaThreshold_) &&
        !heavyFlg &&
        (sigmaAnn < highVolAnnThreshold_) &&
        (n >= nLargeThreshold_);

      double target;
      if (isHighVol)
	{
	  // Heavy-tail / high-vol regime: keep m close to n
	  target = highVolRatio_;
	}
      else if (isVeryLightTail)
	{
	  // Very light tail & large n: smaller m/n is acceptable
	  target = lightTailRatio_;
	}
      else
	{
	  // Everything else: "normal" medium subsample
	  target = normalRatio_;
	}

      // Clamp to [2/n, (n-1)/n]
      double rho = std::max(minRho, std::min(target, maxRho));
      return rho;
    }

  private:
    double      highVolAnnThreshold_;
    double      highVolRatio_;
    double      normalRatio_;
    double      lightTailRatio_;
    double      heavyTailAlphaThreshold_;
    double      lightTailAlphaThreshold_;
    std::size_t nLargeThreshold_;
  };
  
  /**
   * @brief Estimates the Pareto tail index (alpha) of the left tail (losses) using the Hill estimator.
   *
   * @details
   * **Purpose:**
   * To quantify "Tail Risk". This function isolates negative returns (losses), converts them to
   * positive magnitudes, and estimates the decay rate of the tail distribution.
   *
   * **Logic:**
   * 1. Extract all negative returns and take their absolute value.
   * 2. Sort them descending (largest loss first).
   * 3. Take the top **k** extreme losses.
   * 4. Compute the average logarithmic distance between the extreme losses and the k-th loss (the threshold).
   * 5. Alpha = 1.0 / (Average Log Distance).
   *
   * **Interpretation:**
   * - **Alpha < 2.0**: Very Heavy Tails (infinite variance region). High risk.
   * - **Alpha > 4.0**: Light Tails (Gaussian-like). Low risk.
   *
   * @tparam Num Numeric type.
   * @param returns The vector of raw returns.
   * @param k The number of tail observations to use (default 5).
   * Will be automatically clamped if there are fewer losses available.
   * @return double The estimated tail index alpha. Returns -1.0 if there isn't enough data
   * (e.g., fewer than ~8 losses) to form a valid estimate.
   */
  template <class Num>
  double estimate_left_tail_index_hill(const std::vector<Num>& returns,
				       std::size_t             k = 5)
  {
    std::vector<double> losses;
    losses.reserve(returns.size());

    for (const auto& r : returns)
      {
        const double v = num::to_double(r);
        if (v < 0.0)
	  losses.push_back(-v);
      }

    constexpr std::size_t minLossesForHill = 8; // or 10

    if (losses.size() < std::max<std::size_t>(k + 1, minLossesForHill))
      return -1.0; // treat tail index as "unknown" for small samples

    std::sort(losses.begin(), losses.end(), std::greater<double>());

    k = std::min<std::size_t>(k, losses.size() - 1);
    const double xk = losses[k];

    if (xk <= 0.0)
      return -1.0;

    double sumLog = 0.0;
    for (std::size_t i = 0; i < k; ++i)
      sumLog += std::log(losses[i] / xk);

    const double hill = sumLog / static_cast<double>(k);
    if (hill <= 0.0)
      return -1.0;

    return 1.0 / hill;  // α
  }

  /**
   * @brief Abstract interface for policies that refine or optimize the m-out-of-n ratio.
   *
   * @details
   * The adaptive m-out-of-n decision process is split into two stages:
   *
   * 1. **The Prior (Fast):**
   * A heuristic policy (like `TailVolPriorPolicy`) looks at simple stats
   * (volatility, skew, tail index) and suggests a starting ratio.
   * (e.g., "This looks like a wild market, start at 0.80").
   *
   * 2. **The Refinement (Slow/Precise):**
   * This policy takes that starting ratio and performs a data-driven search
   * to find the optimal ratio. It often involves running multiple small
   * bootstrap simulations ("probes") to find a region of stability.
   *
   * Implementations of this interface (like `LBStabilityRefinementPolicy`) handle
   * the computationally intensive second step.
   *
   * @tparam Num Numeric type.
   * @tparam GeoStat Statistic functor.
   * @tparam StrategyT Strategy type.
   * @tparam ResamplerT Resampler type.
   * @tparam BootstrapFactoryT Factory type.
   */
  template <typename Num,
	    typename GeoStat,
	    typename StrategyT,
	    typename ResamplerT,
	    typename BootstrapFactoryT>
  class IRatioRefinementPolicy
  {
  public:
    virtual ~IRatioRefinementPolicy() = default;

    /**
     * @brief Calculates the final, refined m-out-of-n ratio.
     *
     * @param returns The vector of returns.
     * @param ctx Statistical context (N, volatility, tail index, etc).
     * @param L_small The block length to be used for resampling.
     * @param confLevel The target confidence level.
     * @param B_full The total bootstrap replicates budget for the final run
     * (refinement usually uses a smaller B for probing).
     * @param baseRatio The starting ratio suggested by the Prior policy.
     * @param strategy The strategy object (for CRN generation).
     * @param bootstrapFactory The factory used to create engines for probing.
     * @param resampler The resampler instance.
     * @param os Optional output stream for diagnostic logging.
     * @param stageTag CRN stage tag.
     * @param fold CRN fold index.
     * @return double The final subsampling ratio rho = m/n (clamped to 0..1).
     */
    virtual double refineRatio(const std::vector<Num>& returns,
                               const MNRatioContext&    ctx,
                               std::size_t              L_small,
                               double                   confLevel,
                               std::size_t              B_full,
                               double                   baseRatio,
                               StrategyT&               strategy,
                               BootstrapFactoryT&       bootstrapFactory,
                               ResamplerT&              resampler,
                               std::ostream*            os,
                               int                      stageTag,
                               int                      fold) const = 0;
  };

  /**
   * @brief A refinement policy that seeks a stable "plateau" for the m-out-of-n ratio.
   *
   * @details
   * This policy implements a data-driven search to fine-tune the subsampling ratio ($\rho = m/n$).
   * It is inspired by the method proposed by Bickel & Sakov (2008) for choosing $m$.
   *
   * **The Logic:**
   * 1. **Candidate Generation:** Takes a `baseRatio` (provided by a Prior policy) and generates
   * a set of nearby candidates (e.g., base-0.1, base, base+0.1).
   * 2. **Probing:** Runs a small bootstrap simulation for each candidate ratio to estimate the
   * Lower Bound (LB) and the implied volatility ($\sigma$) at that specific $m$.
   * 3. **Stability Analysis:** Calculates an "Instability Score" for each candidate. The score
   * represents the local slope of the LB curve (how much the result changes relative to its
   * neighbors).
   * 4. **Selection:** Selects the candidate with the **minimum instability** (the "flat" region
   * of the curve). In case of ties, it conservatively prefers the smaller $\rho$.
   *
   * @tparam Num The numeric type (e.g., double, decimal).
   * @tparam GeoStat The statistic functor type.
   * @tparam StrategyT The strategy type.
   * @tparam ResamplerT The resampler type (IID or Block).
   * @tparam BootstrapFactoryT The factory used to create bootstrap engines.
   */
  template <typename Num,
            typename GeoStat,
            typename StrategyT,
            typename ResamplerT,
            typename BootstrapFactoryT>
  class LBStabilityRefinementPolicy
    : public IRatioRefinementPolicy<Num, GeoStat, StrategyT, ResamplerT, BootstrapFactoryT>
  {
  public:
    /**
     * @brief Constructs the stability refinement policy.
     *
     * @param deltas A vector of offsets from the base ratio to probe (e.g., {-0.1, 0.0, 0.1}).
     * @param minB Minimum bootstrap replicates for the probing phase (default 400).
     * @param maxB Maximum bootstrap replicates for the probing phase (default 1000).
     * @param minNForRefine Minimum sample size required to trigger refinement (default 15).
     * @param maxNForRefine Maximum sample size to allow refinement (default 60).
     */
    LBStabilityRefinementPolicy(std::vector<double> deltas,
                                std::size_t         minB = 400,
                                std::size_t         maxB = 1000,
                                std::size_t         minNForRefine = 15,
                                std::size_t         maxNForRefine = 60)
      : deltas_(std::move(deltas))
      , minB_(minB)
      , maxB_(maxB)
      , minNForRefine_(minNForRefine)
      , maxNForRefine_(maxNForRefine)
    {
      if (deltas_.empty())
      {
        deltas_.push_back(-0.10);
        deltas_.push_back( 0.00);
        deltas_.push_back(+0.10);
      }
    }

    const std::vector<double>& getDeltas() const { return deltas_; }
    std::size_t getMinB() const                  { return minB_; }
    std::size_t getMaxB() const                  { return maxB_; }
    std::size_t getMinNForRefine() const         { return minNForRefine_; }
    std::size_t getMaxNForRefine() const         { return maxNForRefine_; }

    /**
     * @brief Executes the refinement process to find the most stable m/n ratio.
     *
     * @details
     * 1. Checks if sample size $n$ allows for refinement. If not, returns `baseRatio`.
     * 2. Generates candidate ratios via `generateCandidates`.
     * 3. Calls `probeCandidate` for each ratio to get LBs and variances.
     * 4. Calls `selectBestCandidate` to find the optimal ratio based on local stability.
     *
     * @param returns The vector of returns.
     * @param ctx Distributional context (N, skew, etc.).
     * @param L_small Block length used for small-N.
     * @param confLevel Confidence level.
     * @param B_full The 'B' used for the main run (used to scale the probing 'B').
     * @param baseRatio The starting ratio suggested by the prior policy.
     * @param strategy The strategy object.
     * @param bootstrapFactory The factory for creating engines.
     * @param resampler The resampler instance.
     * @param os Logging stream.
     * @param stageTag CRN tag.
     * @param fold CRN fold.
     * @return double The refined m/n ratio.
     */
    double refineRatio(const std::vector<Num>& returns,
                       const MNRatioContext&    ctx,
                       std::size_t              L_small,
                       double                   confLevel,
                       std::size_t              B_full,
                       double                   baseRatio,
                       StrategyT&               strategy,
                       BootstrapFactoryT&       bootstrapFactory,
                       ResamplerT&              resampler,
                       std::ostream*            os,
                       int                      stageTag,
                       int                      fold) const override
    {
      const std::size_t n = ctx.getN();

      // 1. Check Constraints
      if (n < minNForRefine_ || n > maxNForRefine_)
        return baseRatio;

      // 2. Generate Candidates (base + deltas, clamped)
      std::vector<double> candidates = generateCandidates(baseRatio, n);
      if (candidates.empty())
        return baseRatio;

      // 3. Run Simulations (Probe each ratio)
      std::size_t B_small = std::max(minB_, std::min(B_full, maxB_));
      double z = z_from_two_sided_CL(confLevel);

      std::vector<CandidateScore> scores;
      scores.reserve(candidates.size());

      for (double rho : candidates)
      {
        scores.push_back(probeCandidate(rho, returns, L_small, confLevel, B_small, z,
                                        strategy, bootstrapFactory, resampler,
                                        stageTag, fold, os));
      }

      // 4. Select Best (Stability Optimization)
      return selectBestCandidate(scores, baseRatio, os);
    }

  private:
    /**
     * @brief Internal struct to hold the results of a bootstrap probe.
     */
    struct CandidateScore
    {
      double rho;    // candidate m/n
      double lb;     // per-period lower bound
      double width;  // CI width (upper - lower)
      double sigma;  // implied sigma
    };

    /**
     * @brief Generates a list of valid candidate ratios based on the base ratio and deltas.
     *
     * @details
     * Applies strict clamping logic to ensure the resulting $m$ is valid:
     * - $m \ge 2$ (or 7 for small N stability).
     * - $m \le n-1$ (strict subsampling).
     * - Deduplicates and sorts the resulting list.
     *
     * @param baseRatio The center point for generation.
     * @param n The sample size.
     * @return std::vector<double> Sorted unique list of valid ratios.
     */
    std::vector<double> generateCandidates(double baseRatio, std::size_t n) const
    {
      // Lambda to clamp ratio into valid m-out-of-n range
      auto clampRatio = [n](double rho) {
        const double minRho_raw = 2.0 / static_cast<double>(n);
        double       maxRho_raw = (n > 2)
            ? static_cast<double>(n - 1) / static_cast<double>(n)
            : 0.5;

        // Cap at 0.80 for tiny samples to avoid "almost n-out-of-n"
        const std::size_t nSmallCap = 25;
        if (n <= nSmallCap)
          maxRho_raw = std::min(maxRho_raw, 0.80);

        return std::max(minRho_raw, std::min(rho, maxRho_raw));
      };

      std::vector<double> candidates;
      candidates.reserve(deltas_.size() + 1);
      candidates.push_back(baseRatio);

      for (double d : deltas_)
        candidates.push_back(baseRatio + d);

      // Apply clamp
      for (double& r : candidates)
        r = clampRatio(r);

      // Sort and Unique
      std::sort(candidates.begin(), candidates.end());
      candidates.erase(std::unique(candidates.begin(), candidates.end(),
                                   [](double a, double b) {
                                     return std::fabs(a - b) < 1e-6;
                                   }),
                       candidates.end());
      return candidates;
    }

    /**
     * @brief Runs a single m-out-of-n bootstrap simulation for a specific ratio.
     *
     * @details
     * Creates a temporary bootstrap engine using the factory, executes it, and
     * extracts the Lower Bound. It also attempts to calculate the implied $\sigma$
     * if the engine supports upper bounds (CI width).
     *
     * @param rho The target ratio to test.
     * @param returns Return data.
     * @param L_small Block length.
     * @param confLevel Confidence level.
     * @param B Number of replicates for this probe.
     * @param z Z-score for back-calculating sigma from CI width.
     * @param strategy Strategy object.
     * @param factory Bootstrap factory.
     * @param resampler Resampler instance.
     * @param stageTag CRN tag.
     * @param fold CRN fold.
     * @param os Logger.
     * @return CandidateScore Result containing LB and Sigma.
     */
    CandidateScore probeCandidate(double                   rho,
                                  const std::vector<Num>&  returns,
                                  std::size_t              L_small,
                                  double                   confLevel,
                                  std::size_t              B,
                                  double                   z,
                                  StrategyT&               strategy,
                                  BootstrapFactoryT&       factory,
                                  ResamplerT&              resampler,
                                  int                      stageTag,
                                  int                      fold,
                                  std::ostream*            os) const
    {
      auto [mnBoot, mnCrn] = factory.template makeMOutOfN<Num, GeoStat, ResamplerT>(
          B, confLevel, rho, resampler, strategy, stageTag, static_cast<int>(L_small), fold);

      auto mnR = mnBoot.run(returns, GeoStat(), mnCrn);
      const double lbP = num::to_double(mnR.lower);

      double width = 0.0;
      double sigma = std::numeric_limits<double>::quiet_NaN();

      // Calculate Width/Sigma if upper bound available
      if constexpr (detail::has_member_upper<decltype(mnR)>::value)
      {
        width = std::max(0.0, num::to_double(mnR.upper - mnR.lower));
        if (z > 0.0)
          sigma = width / (2.0 * z);
      }

      if (os)
      {
        double sig2 = (std::isfinite(sigma) ? sigma * sigma : -1.0);
        (*os) << "   [Bootstrap/mn-ratio-stability] probe rho="
              << std::fixed << std::setprecision(3) << rho
              << "  LB(per)=" << std::setprecision(6) << lbP
              << "  sigma2≈" << sig2
              << "  B=" << B << "\n";
      }

      return {rho, lbP, width, sigma};
    }

    /**
     * @brief Analyzes probe results to select the most stable ratio.
     *
     * @details
     * Implements the "Plateau Search":
     * 1. Sorts candidates by ratio.
     * 2. For each candidate, calculates "Local Instability" by comparing its LB
     * to its neighbors (previous and next).
     * 3. Instability is defined as: `max(|LB - LB_prev|, |LB - LB_next|) / (sigma + eps)`.
     * 4. Selects the candidate with the minimum instability score.
     * 5. Tie-Breaker: If scores are equal (within tolerance), chooses the **smaller** ratio
     * to remain conservative.
     *
     * @param scores Vector of probe results.
     * @param baseRatio Fallback ratio if selection fails.
     * @param os Logger.
     * @return double The chosen ratio.
     */
    double selectBestCandidate(const std::vector<CandidateScore>& scores,
                               double baseRatio,
                               std::ostream* os) const
    {
      if (scores.empty())
        return baseRatio;

      // Valid sigma check
      bool anyFiniteSigma = std::any_of(scores.begin(), scores.end(),
          [](const CandidateScore& s) { return std::isfinite(s.sigma) && s.sigma > 0.0; });

      if (!anyFiniteSigma)
      {
        // Fallback: Max LB strategy
        auto it = std::max_element(scores.begin(), scores.end(),
            [](const CandidateScore& a, const CandidateScore& b) { return a.lb < b.lb; });
        return it->rho;
      }

      // Sort indices by rho
      std::vector<size_t> order(scores.size());
      std::iota(order.begin(), order.end(), 0);
      std::sort(order.begin(), order.end(),
          [&](size_t i, size_t j) { return scores[i].rho < scores[j].rho; });

      double bestScore = std::numeric_limits<double>::infinity();
      size_t bestIdx   = order[0];
      const double eps = 1e-12;

      // Calculate local instability
      for (size_t pos = 0; pos < order.size(); ++pos)
      {
        const size_t idx = order[pos];
        const auto&  s   = scores[idx];

        if (!std::isfinite(s.sigma) || s.sigma <= 0.0)
          continue;

        double metric = 0.0;

        // Calculate neighbor diffs (slope)
        if (order.size() > 1)
        {
          if (pos == 0)
          {
            metric = std::fabs(s.lb - scores[order[pos + 1]].lb);
          }
          else if (pos == order.size() - 1)
          {
            metric = std::fabs(s.lb - scores[order[pos - 1]].lb);
          }
          else
          {
            double d1 = std::fabs(s.lb - scores[order[pos - 1]].lb);
            double d2 = std::fabs(s.lb - scores[order[pos + 1]].lb);
            metric = std::max(d1, d2);
          }
        }

        double instability = metric / (s.sigma + eps); // Normalize

        // Minimization logic with tie-break for smaller rho
        const double tol = 1e-9;
        if (instability + tol < bestScore)
        {
          bestScore = instability;
          bestIdx = idx;
        }
        else if (std::fabs(instability - bestScore) <= tol)
        {
          if (s.rho < scores[bestIdx].rho)
            bestIdx = idx;
        }
      }

      double chosen = scores[bestIdx].rho;

      if (os)
      {
        (*os) << "   [Bootstrap/mn-ratio-stability] Selected rho="
              << std::fixed << std::setprecision(3) << chosen
              << " (Instability=" << (std::isfinite(bestScore) ? bestScore : -1.0) << ")\n";
      }
      return chosen;
    }
    
    std::vector<double> deltas_;
    std::size_t         minB_;
    std::size_t         maxB_;
    std::size_t         minNForRefine_;
    std::size_t         maxNForRefine_;
  };

  template <typename Num,
	    typename GeoStat,
	    typename StrategyT,
	    typename ResamplerT,
	    typename BootstrapFactoryT>
  class NoRefinementPolicy
    : public IRatioRefinementPolicy<Num, GeoStat, StrategyT, ResamplerT, BootstrapFactoryT>
  {
  public:
    double refineRatio(const std::vector<Num>&,
                       const MNRatioContext&,
                       std::size_t,
                       double,
                       std::size_t,
                       double baseRatio,
                       StrategyT&,
                       BootstrapFactoryT&,
                       ResamplerT&,
                       std::ostream*,
                       int,
                       int) const override
    {
      return baseRatio;
    }
  };

  /**
   * @brief Coordinates the adaptive m/n decision process by combining a **Prior** policy and a **Refinement** policy.
   *
   * @details
   * This class acts as the high-level orchestrator for the adaptive bootstrapping logic. It implements
   * a "Predict-then-Correct" pattern to determine the optimal subsampling ratio:
   *
   * 1. **Prediction (The Prior):** It uses the `TailVolPriorPolicy` to inspect cheap statistical
   * metrics (Volatility, Tail Index) and propose a "safe" baseline ratio (e.g., 0.80 for wild markets,
   * 0.50 for normal ones).
   *
   * 2. **Correction (The Refinement):** It passes that baseline to the `RefinementPolicy` (typically
   * `LBStabilityRefinementPolicy`), which runs actual bootstrap simulations ("probes") to fine-tune
   * the ratio by seeking a region of statistical stability.
   *
   * **Why separate them?**
   * This separation allows the system to be both **Context-Aware** (knowing that FXI is different from XLF)
   * and **Data-Driven** (verifying that the chosen ratio actually produces a stable confidence interval).
   *
   * @tparam Num Numeric type.
   * @tparam GeoStat Statistic functor.
   * @tparam StrategyT Strategy type.
   * @tparam ResamplerT Resampler type.
   * @tparam BootstrapFactoryT Factory type.
   * @tparam RefinementPolicyT The specific refinement implementation (e.g., `LBStabilityRefinementPolicy`).
   */
  template <typename Num,
	    typename GeoStat,
	    typename StrategyT,
	    typename ResamplerT,
	    typename BootstrapFactoryT,
	    typename RefinementPolicyT>
  class TailVolStabilityPolicy
  {
  public:
    /**
     * @brief Constructs the coordinator with specific policies.
     *
     * @param priorPolicy The policy used to determine the baseline ratio from statistical context.
     * @param refinementPolicy The policy used to optimize/fine-tune that baseline via simulation.
     */
    TailVolStabilityPolicy(const TailVolPriorPolicy& priorPolicy,
                           const RefinementPolicyT&  refinementPolicy)
      : priorPolicy_(priorPolicy)
      , refinementPolicy_(refinementPolicy)
    {
    }

    /**
     * @brief Gets the underlying prior policy configuration.
     */
    const TailVolPriorPolicy& getPriorPolicy() const
    {
      return priorPolicy_;
    }

    /**
     * @brief Gets the underlying refinement policy configuration.
     */
    const RefinementPolicyT& getRefinementPolicy() const
    {
      return refinementPolicy_;
    }

    /**
     * @brief Orchestrates the computation of the final m/n ratio.
     *
     * @details
     * **Execution Flow:**
     * 1. Calls `priorPolicy_.computePriorRatio(ctx)` to get the **baseRatio**.
     * 2. Calls `refinementPolicy_.refineRatio(...)` using that **baseRatio** as the starting point.
     * 3. Returns the final refined ratio.
     *
     * @param returns The vector of returns.
     * @param ctx Statistical context (N, volatility, tail index, etc).
     * @param L_small The block length to be used for resampling.
     * @param confLevel The target confidence level.
     * @param B_full The total bootstrap replicates budget for the final run.
     * @param strategy The strategy object.
     * @param bootstrapFactory The factory used to create engines.
     * @param resampler The resampler instance.
     * @param os Optional output stream for logging.
     * @param stageTag CRN stage tag.
     * @param fold CRN fold index.
     * @return double The final optimal subsampling ratio.
     */
    double computeRatio(const std::vector<Num>& returns,
                        const MNRatioContext&    ctx,
                        std::size_t              L_small,
                        double                   confLevel,
                        std::size_t              B_full,
                        StrategyT&               strategy,
                        BootstrapFactoryT&       bootstrapFactory,
                        ResamplerT&              resampler,
                        std::ostream*            os,
                        int                      stageTag,
                        int                      fold) const
    {
      const double baseRatio = priorPolicy_.computePriorRatio(ctx);
      return refinementPolicy_.refineRatio(returns,
					   ctx,
					   L_small,
					   confLevel,
					   B_full,
					   baseRatio,
					   strategy,
					   bootstrapFactory,
					   resampler,
					   os,
					   stageTag,
					   fold);
    }

  private:
    TailVolPriorPolicy priorPolicy_;
    RefinementPolicyT  refinementPolicy_;
  };

  // -----------------------------------------------------------------------------
  // Simple dependence proxies & small-N heuristics
  // -----------------------------------------------------------------------------

  /**
   * @brief Calculates the length of the longest contiguous sequence of returns with the same sign.
   *
   * A cheap proxy for serial dependence (clustering). It ignores zero-returns
   * when checking for sign continuity but counts the streak length.
   *
   * @param x The vector of returns.
   * @return std::size_t The length of the longest positive or negative run.
   */
  template <class Num>
  inline std::size_t longest_sign_run(const std::vector<Num>& x)
  {
    const std::size_t n = x.size();
    if (n == 0) return 0;

    auto sgn = [](double v){ return (v > 0.0) ? 1 : (v < 0.0 ? -1 : 0); };

    int last = sgn(num::to_double(x[0]));
    std::size_t cur = 1, best = 1;
    for (std::size_t i = 1; i < n; ++i) {
      const int s = sgn(num::to_double(x[i]));
      if (s == last && s != 0) { ++cur; }
      else { if (cur > best) best = cur; cur = 1; last = s; }
    }
    if (cur > best) best = cur;
    return best;
  }

  /**
   * @brief Calculates the ratio of strictly positive returns to total returns.
   *
   * Used as a proxy for trend or sign imbalance.
   *
   * @param x The vector of returns.
   * @return double The ratio (0.0 to 1.0).
   */
  template <class Num>
  inline double sign_positive_ratio(const std::vector<Num>& x)
  {
    const std::size_t n = x.size();
    if (n == 0) return 0.0;

    std::size_t num_pos = 0;
    for (const auto& r : x)
      if (num::to_double(r) > 0.0)
	++num_pos;

    return static_cast<double>(num_pos) / static_cast<double>(n);
  }

  /**
   * @brief Detects if the distribution exhibits heavy tails based on skew and kurtosis.
   *
   * Thresholds: |Skew| >= 0.90 OR Excess Kurtosis >= 1.20.
   *
   * @param skew Sample skewness.
   * @param exkurt Sample excess kurtosis.
   * @return true if heavy tails are detected.
   */
  inline bool has_heavy_tails_wide(double skew, double exkurt)
  {
    // Prior behavior might have been stricter; widen slightly:
    // - |skew| >= 0.90, or
    // - excess kurtosis >= 1.20
    return (std::fabs(skew) >= 0.90) || (exkurt >= 1.20);
  }

  /**
   * @brief Determines if the Small-N (m-out-of-n) logic path should be activated.
   *
   * Policy:
   * - Always run if N <= 40.
   * - Run if N <= 60 AND data has heavy tails.
   *
   * @param n Sample size.
   * @param heavy_tails Result of has_heavy_tails_wide().
   * @return true if Small-N bootstrap is recommended.
   */
  inline bool should_run_smallN(std::size_t n, bool heavy_tails)
  {
    return (n <= 40) || ( (n <= 60) && heavy_tails );
  }

  /**
   * @brief Heuristic to choose between Block and IID resampling for small samples.
   *
   * When N is too small for ACF to be reliable, this uses "cheap proxies" (sign ratio
   * and longest run) to detect structure.
   *
   * @param ratio_pos The ratio of positive returns.
   * @param n Sample size.
   * @param longest_run The longest run of same-sign returns.
   * @param hi_thresh Upper bound for balanced ratio (default 0.65).
   * @param lo_thresh Lower bound for balanced ratio (default 0.35).
   * @param n_thresh The sample size threshold below which this logic applies (default 40).
   * @return true if Block resampling is recommended (imbalanced or streaky).
   * @return false if IID resampling is acceptable (balanced and random-looking).
   */
  inline bool choose_block_smallN(double ratio_pos,
                                std::size_t n,
                                std::size_t longest_run,
                                double hi_thresh = 0.65,
                                double lo_thresh = 0.35,
                                std::size_t n_thresh = 40)
  {
    const bool sign_imbalance = (ratio_pos > hi_thresh) || (ratio_pos < lo_thresh);

    // Adaptive run trigger: floor at 6, grow slowly with n (≈ 0.18n capped)
    const std::size_t base = 6;
    const std::size_t scaled = static_cast<std::size_t>(std::ceil(0.18 * std::min(n, n_thresh)));
    const std::size_t run_thresh = std::max(base, scaled); // 6..7 for n in [20,40]

    const bool streaky_smallN = (n <= n_thresh) && (longest_run >= run_thresh);
    return sign_imbalance || streaky_smallN;
  }
  

  // Tiny L for individual M2M: clamp to [2,3] and ≤ requested L
  inline std::size_t clamp_smallL(std::size_t L)
  {
    return std::max<std::size_t>(2, std::min<std::size_t>(3, L));
  }
  
  /**
   * @brief Heuristic m/n rule for the m-out-of-n bootstrap in very small samples.
   *
   * This helper returns an m/n ratio used by the m-out-of-n bootstrap when n is tiny
   * (e.g., n ≈ 20–40). Conceptually, we are saying:
   *
   *   "With such a small sample, we are skeptical of the ordinary n-out-of-n bootstrap.
   *    To stress-test the statistic / strategy, we only give it a substantially smaller
   *    subsample of size m << n on each bootstrap replicate and see if it still passes."
   *
   * Rationale:
   * ----------
   * The ordinary bootstrap (resampling n points with replacement) can fail badly for
   * small n, heavy-tailed data, or non-smooth statistics (e.g., quantiles, extrema,
   * ratio-type or geometric-mean statistics). In these regimes it often produces
   * confidence intervals that are too narrow and anti-conservative. The m-out-of-n
   * bootstrap fixes this by using a subsample size m that satisfies:
   *
   *   - m → ∞ as n → ∞, but
   *   - m/n → 0 as n → ∞.
   *
   * Under these conditions, one can often recover consistency and better coverage
   * properties when the ordinary bootstrap fails; see, e.g.:
   *
   *   - Bickel & Sakov (2008), "On the choice of m in the m out of n bootstrap and
   *     confidence bounds for extrema", Statistica Sinica 18(3), 967–985.
   *       PDF: https://www.stat.berkeley.edu/~bickel/BS2008SS.pdf
   *       Journal page: https://www3.stat.sinica.edu.tw/statistica/j18n3/j18n38/j18n38.html
   *
   *   - Shao & Tu (1995), "The Jackknife and Bootstrap", Springer Series in Statistics.
   *       Springer: https://doi.org/10.1007/978-1-4612-0795-5
   *
   *   - Politis, Romano & Wolf (1999), "Subsampling", Springer Series in Statistics.
   *       Springer: https://link.springer.com/book/10.1007/978-1-4612-1554-7
   *
   *   - Hall (1992), "The Bootstrap and Edgeworth Expansion", Springer.
   *       Springer: https://link.springer.com/book/10.1007/978-1-4612-4384-7
   *
   * Choice of m:
   * ------------
   * We adopt a simple power-law rule
   *
   *      m_target = n^(2/3),
   *
   * which is a commonly recommended compromise in the small-n literature:
   *  - It grows with n (so m is not pathologically small),
   *  - but m/n shrinks as n increases (so we still get the m-out-of-n benefits).
   *
   * For the ultra-small n regime this code targets (typical n in [20, 40]):
   *
   *   - m ≈ n^(2/3) gives m/n around 0.30–0.40,
   *   - this is intentionally much smaller than the n-out-of-n bootstrap (m = n),
   *   - so each bootstrap replicate is a "harder test" of the strategy’s stability.
   *
   * Practically, a robust strategy should still show a favorable statistic when given
   * only 30–40% of the original sample on each resample; fragile, overfit, or
   * outlier-driven strategies tend to fail under this harsher subsampling regime.
   *
   * Implementation details:
   * -----------------------
   *  - For very small n, we enforce a hard floor m >= 7 so that the statistic is
   *    computed on at least a minimally meaningful subsample.
   *  - We also ensure m <= n - 1 so that we perform a genuine m-out-of-n bootstrap
   *    (never collapsing back to the ordinary n-bootstrap).
   *  - The function returns the ratio m/n as a double; the caller converts this ratio
   *    to an integer m_sub = floor( (m/n) * n ) and enforces [2, n-1] as a final guard.
   *
   * Summary:
   * --------
   * This function embodies a conservative policy: for tiny samples where we do not
   * trust the ordinary bootstrap, it deliberately shrinks m to roughly n^(2/3).
   * That makes the bootstrap a stress test of the statistic’s robustness, rather than
   * a mechanism that amplifies small-sample luck.
   */
  inline double mn_ratio_from_n(std::size_t n)
  {
    if (n == 0) return 1.0;
    if (n < 3) return 1.0; // Too small to subsample meaningfully

    // 1. Calculate Power Law Target: m = n^(2/3)
    //    For N=30 -> m=9.65 (approx 10)
    double m_target = std::pow(static_cast<double>(n), 2.0/3.0);
    
    // 2. Define Bounds
    //    Floor: We want at least ~7-8 items to calculate a meaningful statistic
    //    Ceil:  Must be strictly less than n (n-1 or n-2) to be a true subsample
    double m_floor  = 7.0; 
    double m_ceil   = static_cast<double>(n - 1); 

    // 3. Clamp
    double m = std::max(m_floor, std::min(m_target, m_ceil));
    
    // 4. Return Ratio
    return m / static_cast<double>(n);
  }
  
  /**
   * @brief Helper to dispatch a functor with the appropriate Resampler type.
   *
   * Decides between `IIDResampler` and `StationaryMaskValueResamplerAdapter`
   * based on data characteristics, then calls the provided lambda `fn`.
   *
   * @tparam Num Numeric type.
   * @tparam Fn Functor type accepting (Resampler, ratio, use_block, L_small).
   * @param data Returns vector.
   * @param L Requested block length.
   * @param fn The functor/lambda to execute.
   * @param chosen_name [out] Optional pointer to C-string for the chosen resampler name.
   * @param L_small_out [out] Optional pointer to receive the clamped block length used.
   * @return The return value of `fn`.
   */
  template <class Num, class Fn>
  auto dispatch_smallN_resampler(const std::vector<Num>& data,
  		 std::size_t L,
  		 Fn&& fn,
  		 const char** chosen_name = nullptr,
  		 std::size_t* L_small_out = nullptr)
  {
    using IIDResampler  = mkc_timeseries::IIDResampler<Num>;
    using BlockValueRes = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;

    const std::size_t n       = data.size();
    const double      ratio   = sign_positive_ratio(data);
    const std::size_t runlen  = longest_sign_run(data);
    const bool        use_blk = choose_block_smallN(ratio, n, runlen);

    const std::size_t L_small = clamp_smallL(L);
    if (L_small_out) *L_small_out = L_small;
    if (chosen_name) *chosen_name = use_blk
         ? "StationaryMaskValueResamplerAdapter(small L)"
         : "IIDResampler";

    if (use_blk) {
      BlockValueRes blockResampler(L_small);
      return fn(blockResampler, ratio, /*use_block=*/true, L_small);
    } else {
      IIDResampler iidResampler;
      return fn(iidResampler, ratio, /*use_block=*/false, L_small);
    }
  }

  // -----------------------------------------------------------------------------
  // LB combine helpers (shared by stages)
  // -----------------------------------------------------------------------------
  namespace internal
  {

    struct RunsTestConfig
    {
      double alpha_quantile = 0.95;  // one-sided quantile (e.g., 95th)
      int    num_sims       = 256;   // tiny, fast MC
    };

    /**
     * @brief Generates a single IID Bernoulli sequence and finds its longest run.
     * @param n Sequence length.
     * @param p Probability of success (positive return).
     * @param rng Random number engine.
     * @return std::size_t Longest run length in the simulated sequence.
     */
    inline std::size_t longest_run_iid_once(std::size_t n, double p, std::mt19937_64& rng)
    {
      std::bernoulli_distribution bern(p);
      if (n == 0) return 0;

      // Draw first
      bool prev = bern(rng);
      std::size_t longest = 1, cur = 1;

      for (std::size_t i = 1; i < n; ++i) {
	const bool x = bern(rng);
	if (x == prev) { ++cur; }
	else { longest = std::max(longest, cur); cur = 1; prev = x; }
      }
      longest = std::max(longest, cur);
      return longest;
    }

    /**
     * @brief Estimates the quantile of the "longest run" distribution via Monte Carlo.
     *
     * Used to determine if an observed run length is statistically significant
     * (i.e., unlikely to happen in IID data).
     *
     * @param n Sequence length.
     * @param p Probability of success.
     * @param cfg Configuration (alpha level, number of simulations).
     * @param seed Random seed.
     * @return std::size_t The estimated quantile (e.g., 95th percentile) of run length.
     */
    inline std::size_t runs_longest_quantile_MC(std::size_t n, double p,
						RunsTestConfig cfg = {},
						std::uint64_t seed = 0xC0FFEEULL)
    {
      if (n == 0) return 0;
      std::mt19937_64 rng(seed);

      std::vector<std::size_t> samples;
      samples.reserve(std::max(1, cfg.num_sims));
      for (int s = 0; s < cfg.num_sims; ++s) {
	samples.push_back(longest_run_iid_once(n, p, rng));
      }
      std::sort(samples.begin(), samples.end());
      const std::size_t k = static_cast<std::size_t>(
						     std::min<std::size_t>(samples.size()-1,
									   std::max<std::size_t>(0, static_cast<std::size_t>(std::ceil(cfg.alpha_quantile * samples.size()) - 1))));
      return samples[k];
    }

    /**
     * @brief Checks if the observed longest run exceeds the MC-simulated 95th percentile.
     * @return true if the observed run is suspiciously long (suggesting dependence).
     */
    inline bool borderline_run_exceeds_MC95(std::size_t n,
					    double ratio_pos,
					    std::size_t observed_longest_run,
					    RunsTestConfig cfg = {},
					    std::uint64_t seed = 0xC0FFEEULL)
    {
      // Compute the 95th percentile under IID(p=ratio_pos)
      const std::size_t q95 = runs_longest_quantile_MC(n, ratio_pos, cfg, seed);
      return observed_longest_run >= q95;
    }

    /**
     * @brief Combines Lower Bounds (LBs) from multiple engines, adjusting strategy based on hurdle proximity.
     *
     * Logic:
     * 1. If the median of the annualized LBs is "close" (within `proximity_bps`) to the hurdle,
     * it acts conservatively and returns the MINIMUM of the LBs.
     * 2. Otherwise, it returns the Median-of-Present (or closer neighbor to the mean for N=2).
     *
     * @param parts_per Vector of per-period LBs.
     * @param annualizationFactor Factor to convert per-period to annualized.
     * @param hurdle_annual The target annualized return.
     * @param proximity_bps The window around the hurdle (in basis points) to trigger conservative mode.
     * @return Num The selected per-period Lower Bound.
     */
    template <class Num>
    inline Num combine_LBs_with_near_hurdle(const std::vector<Num>& parts_per,
					    double annualizationFactor,
					    Num hurdle_annual,
					    double proximity_bps = 75.0) // 0.75% default window
    {
      if (parts_per.empty())
	return Num(0);

      // If only one, nothing to combine.
      if (parts_per.size() == 1)
	return parts_per.front();

      // Build annualized candidates
      std::vector<Num> annualized; annualized.reserve(parts_per.size());
      for (const auto& p : parts_per)
	annualized.push_back(mkc_timeseries::Annualizer<Num>::annualize_one(p, annualizationFactor));

      // Find the "central" candidate via median-of-present (2 or 3)
      auto median_of_2_or_3 = [](std::vector<Num> v)
      {
	std::sort(v.begin(), v.end(), [](const Num& a, const Num& b){ return a < b; });
	if (v.size() == 2)
	  return v[0] + (v[1] - v[0]) / Num(2);
      
	return v[1]; // size()==3
      };
    
      const Num med_ann = median_of_2_or_3(annualized);

      // Proximity check in basis points (annualized)
      const Num delta = (med_ann - hurdle_annual);
      const double delta_bps = 10000.0 * delta.getAsDouble(); // 10000 bps = 100%

      // If near hurdle → be conservative: use MIN across engines
      if (std::fabs(delta_bps) <= proximity_bps) {
	// pick the per-period LB corresponding to the MIN annualized
	auto it = std::min_element(annualized.begin(), annualized.end(),
				   [](const Num& a, const Num& b){ return a < b; });
	const std::size_t idx = static_cast<std::size_t>(std::distance(annualized.begin(), it));
	return parts_per[idx];
      }

      // Else use median-of-present (return the matching per-period value)
      std::vector<std::pair<Num, std::size_t>> pairs;
      pairs.reserve(annualized.size());
      for (std::size_t i = 0; i < annualized.size(); ++i)
	pairs.emplace_back(annualized[i], i);

      std::sort(pairs.begin(), pairs.end(), [](auto& A, auto& B)
      {
	return A.first < B.first;
      });

      if (pairs.size() == 2)
	{
	  const Num mid = pairs[0].first + (pairs[1].first - pairs[0].first) / Num(2);
	  // choose the closer of the two to the arithmetic mid (stable tie-break)
	  const double d0 = std::fabs((pairs[0].first - mid).getAsDouble());
	  const double d1 = std::fabs((pairs[1].first - mid).getAsDouble());
	  return parts_per[ d0 <= d1 ? pairs[0].second : pairs[1].second ];
	}
      // size()==3 → exact median index is 1
      return parts_per[pairs[1].second];
    }

    /**
     * @brief Returns the minimum value in the vector.
     */
    template <class Num>
    inline Num min_of(const std::vector<Num>& v) {
      return *std::min_element(v.begin(), v.end(),
			       [](const Num& a, const Num& b){ return a < b; });
    }

    /**
     * @brief Returns the median of a vector of size 2 or 3.
     * For size 2, returns the arithmetic mean.
     */
    template <class Num>
    inline Num median_of_2_or_3(std::vector<Num> v) {
      std::sort(v.begin(), v.end(), [](const Num& a, const Num& b){ return a < b; });
      if (v.size() == 2) return v[0] + (v[1] - v[0]) / Num(2);
      return v[1]; // size()==3
    }

    /**
     * @brief Combines bounds using either a "2-of-3" (median) vote or a strict minimum.
     * @param parts Vector of bounds.
     * @param vote2 If true, uses median logic. If false, uses min_of().
     * @return Num The combined bound.
     */
    template <class Num>
    inline Num combine_LBs_2of3_or_min(const std::vector<Num>& parts, bool vote2) {
      if (!vote2 || parts.empty()) return min_of(parts);
      if (parts.size() == 1)      return parts.front();
      if (parts.size() == 2)      return median_of_2_or_3(parts);
      return median_of_2_or_3(parts);
    }

    /**
     * @brief Logs the decision policy used for LB construction to the stream.
     */
    inline void log_policy_line(std::ostream& os,
				const char* policyLabel,
				std::size_t n, std::size_t L,
				double skew, double exkurt,
				bool heavy_tails,
				const char* resamplerName,
				std::size_t L_small)
    {
      os << "   [Bootstrap] Conservative LB construction policy = policy: "
	 << policyLabel
	 << "  | n=" << n << " L=" << L
	 << "  skew=" << skew << " exkurt=" << exkurt
	 << "  heavy_tails=" << (heavy_tails ? "yes" : "no")
	 << "  SmallNResampler=" << resamplerName
	 << "  L_small=" << L_small << "\n";
    }
  } // namespace internal

  /**
   * @brief Aggregated result from the "Conservative Small-N" logic.
   * * Contains the combined lower bound (min of m-out-of-n and BCa) and
   * diagnostic information about which resampler and parameters were used.
   */
  template <typename Num, typename GeoStat, typename StrategyT>
  struct SmallNConservativeResult {
    Num         per_lower{};       ///< Combined per-period LB (min of engines).
    Num         ann_lower{};       ///< Annualized LB.
    std::size_t m_sub{0};          ///< The subsample size used for m-out-of-n.
    std::size_t L_used{0};         ///< The block length actually used (clamped).
    std::size_t effB_mn{0};        ///< Effective B (non-degenerate) for m-out-of-n.
    std::size_t effB_bca{0};       ///< Effective B for BCa.
    const char* resampler_name{""};///< Name of the chosen resampler (IID or Block).
  };

  /**
   * @brief Determines the optimal m-out-of-n subsampling ratio (rho).
   *
   * @details
   * This function resolves the policy for the subsampling ratio:
   * 1. Explicit Override: If the user provided a fixed `requested_rho` > 0,
   * it clamps that value to valid bounds and returns it.
   * 2. Adaptive Policy: If `requested_rho` <= 0, it instantiates the
   * TailVolStabilityPolicy (Prior + Refinement) to calculate the optimal
   * ratio based on data characteristics.
   *
   * @tparam Num Numeric type.
   * @tparam GeoStat Statistic functor type.
   * @tparam StrategyT Strategy type.
   * @tparam ResamplerT The resampler type (IID or Block).
   * @tparam BootstrapFactoryT Factory type.
   *
   * @param requested_rho The user-requested ratio (pass <= 0 for adaptive).
   * @param returns The vector of returns.
   * @param ctx The statistical context (volatility, skew, tail index).
   * @param L_small The block length to use.
   * @param confLevel The confidence level.
   * @param B The bootstrap replicates budget.
   * @param strategy The strategy object.
   * @param factory The bootstrap factory.
   * @param resampler The resampler instance.
   * @param os Optional output stream for logging.
   * @param stageTag CRN tag.
   * @param fold CRN fold.
   * @return double The final subsampling ratio to use.
   */
  template <typename Num, typename GeoStat, typename StrategyT, typename ResamplerT, typename BootstrapFactoryT>
  double resolve_adaptive_subsample_ratio(
					  double requested_rho,
					  const std::vector<Num>& returns,
					  const MNRatioContext& ctx,
					  std::size_t L_small,
					  double confLevel,
					  std::size_t B,
					  StrategyT& strategy,
					  BootstrapFactoryT& factory,
					  ResamplerT& resampler,
					  std::ostream* os,
					  int stageTag,
					  int fold)
  {
    const std::size_t n = ctx.getN();

    // Case 1: Explicit User Request
    if (requested_rho > 0.0)
      {
	if (n < 3) return 1.0;

	// Clamping logic (from original implementation)
	const double minRho = 2.0 / static_cast<double>(n);
	const double maxRho = (n > 2)
	  ? static_cast<double>(n - 1) / static_cast<double>(n)
	  : 0.5;

	return std::max(minRho, std::min(requested_rho, maxRho));
      }

    // Case 2: Adaptive TailVolStability Policy
    // Using the heavy-tail/volatility prior and the LB-stability refinement
    using RefineT = LBStabilityRefinementPolicy<Num, GeoStat, StrategyT, ResamplerT, BootstrapFactoryT>;
  
    TailVolPriorPolicy priorPolicy;
    RefineT refinePolicy({ -0.10, 0.0, +0.10 }); // Search +/- 10% around prior

    TailVolStabilityPolicy<Num, GeoStat, StrategyT, ResamplerT, BootstrapFactoryT, RefineT>
      policy(priorPolicy, refinePolicy);

    double calculated_rho = policy.computeRatio(returns, ctx, L_small, confLevel, B,
						strategy, factory, resampler, os, stageTag, fold);

    // Logging specific to the adaptive decision
    if (os)
      {
	const double m_cont = calculated_rho * static_cast<double>(n);
	(*os) << "   [Bootstrap] Adaptive m/n (TailVolStabilityPolicy): n=" << n
	      << "  sigmaAnn=" << std::fixed << std::setprecision(2) << (ctx.getSigmaAnn() * 100.0) << "%"
	      << "  skew=" << std::setprecision(3) << ctx.getSkew()
	      << "  exkurt=" << ctx.getExKurt()
	      << "  tailIndex=" << std::setprecision(3) << ctx.getTailIndex()
	      << "  heavy_tails=" << (ctx.hasHeavyTails() ? "yes" : "no")
	      << "  m≈" << std::setprecision(2) << m_cont
	      << "  ratio=" << std::setprecision(3) << calculated_rho
	      << "\n";
      }

    return calculated_rho;
  }

  /**
   * @brief Executes the "Duel" between m-out-of-n and BCa bootstraps.
   *
   * @details
   * This kernel function performs the heavy lifting:
   * 1. Runs the m-out-of-n bootstrap using the provided resampler and ratio.
   * 2. Runs the BCa bootstrap using the same resampler.
   * 3. Logs detailed diagnostics (shrinkage rates, implied sigma).
   * 4. Returns the result containing the MINIMUM of the two Lower Bounds.
   *
   * @tparam ResamplerT The specific resampler type (IID or Block).
   * @tparam Num Numeric type.
   * @tparam GeoStat Statistic functor type.
   * @tparam StrategyT Strategy type.
   * @tparam BootstrapFactoryT Factory type.
   *
   * @param returns The vector of returns.
   * @param resampler The instantiated resampler object.
   * @param rho The subsampling ratio to use for m-out-of-n.
   * @param L_small The block length.
   * @param annualizationFactor Factor to annualize results.
   * @param confLevel Confidence level.
   * @param B Number of bootstrap replicates.
   * @param z Z-score corresponding to confidence level (for sigma calc).
   * @param strategy The strategy object.
   * @param factory The bootstrap factory.
   * @param stageTag CRN tag.
   * @param fold CRN fold.
   * @param os Optional output stream.
   * @param resamplerName String name of the resampler for reporting.
   * @return SmallNConservativeResult Combined result struct.
   */
  template <typename ResamplerT, typename Num, typename GeoStat, typename StrategyT, typename BootstrapFactoryT>
  SmallNConservativeResult<Num, GeoStat, StrategyT>
  execute_bootstrap_duel(
			 const std::vector<Num>& returns,
			 ResamplerT& resampler,
			 double rho,
			 std::size_t L_small,
			 double annualizationFactor,
			 double confLevel,
			 std::size_t B,
			 double z,
			 StrategyT& strategy,
			 BootstrapFactoryT& factory,
			 int stageTag,
			 int fold,
			 std::ostream* os,
			 const char* resamplerName)
  {
    SmallNConservativeResult<Num, GeoStat, StrategyT> r{};
    r.L_used = L_small;
    r.resampler_name = resamplerName;

    const std::size_t n = returns.size();

    // ---------------------------------------------------------
    // 1. Run m-out-of-n Bootstrap
    // ---------------------------------------------------------
    auto [mnBoot, mnCrn] = factory.template makeMOutOfN<Num, GeoStat, ResamplerT>(B,
										  confLevel,
										  rho,
										  resampler,
										  strategy,
										  stageTag,
										  static_cast<int>(L_small),
										  fold);

    auto mnR = mnBoot.run(returns, GeoStat(), mnCrn);
    const Num lbP_mn = mnR.lower;

    r.m_sub = mnR.m_sub;
    r.effB_mn = mnR.effective_B;

    // --- Diagnostics for m-out-of-n ---
    if (os)
      {
	const double mn_ratio = (n > 0)
	  ? (static_cast<double>(mnR.m_sub) / static_cast<double>(n))
	  : 0.0;
	const double shrinkRate = 1.0 - mn_ratio;

	(*os) << "   [Bootstrap] m_sub=" << mnR.m_sub
	      << "  n=" << n
	      << "  m/n=" << std::fixed << std::setprecision(3) << mn_ratio
	      << "  shrink=" << std::fixed << std::setprecision(3) << shrinkRate
	      << "\n";

	// Attempt to calculate implied sigma if upper bound is available
	if constexpr (detail::has_member_upper<decltype(mnR)>::value)
	  {
	    const double width = std::max(0.0, num::to_double(mnR.upper - mnR.lower));
	    const double sigma_mn = (z > 0.0)
	      ? (width / (2.0 * z))
	      : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma_mn * sigma_mn) * 100.0;

	    (*os) << "   [Diag] m/n σ(per-period)≈ " << sigma_mn
		  << "  var≈ " << var
		  << "  effB=" << mnR.effective_B
		  << "  L=" << mnR.L
		  << "\n";
	  }
	else
	  {
	    (*os) << "   [Diag] m/n σ: skipped (no two-sided CI available)\n";
	  }
      }

    // ---------------------------------------------------------
    // 2. Run BCa Bootstrap
    // ---------------------------------------------------------
    auto bca = factory.template makeBCa<Num>(
					     returns, B, confLevel, GeoStat(), resampler,
					     strategy, stageTag, static_cast<int>(L_small), fold);

    const Num lbP_bca = bca.getLowerBound();
    r.effB_bca = B;

    // --- Diagnostics for BCa ---
    if (os)
      {
	if constexpr (detail::has_getUpperBound<decltype(bca)>::value)
	  {
	    const Num ubP_bca  = bca.getUpperBound();
	    const double width = std::max(0.0, num::to_double(ubP_bca - lbP_bca));
	    const double sigma_bca = (z > 0.0)
	      ? (width / (2.0 * z))
	      : std::numeric_limits<double>::quiet_NaN();
	    const double var = (sigma_bca * sigma_bca) * 100.0;

	    (*os) << "   [Diag] BCa σ(per-period)≈ " << sigma_bca
		  << "  var≈ " << var
		  << "  effB=" << B
		  << "  L=" << L_small
		  << "\n";
	  }
	else
	  {
	    (*os) << "   [Diag] BCa σ: skipped (upper bound API not available)\n";
	  }
      }

    // ---------------------------------------------------------
    // 3. Combine (Conservative Minimum)
    // ---------------------------------------------------------
    r.per_lower = (lbP_mn < lbP_bca) ? lbP_mn : lbP_bca;
    r.ann_lower = mkc_timeseries::Annualizer<Num>::annualize_one(
								 r.per_lower, annualizationFactor);

    if (os)
      {
	(*os) << "   [Bootstrap] SmallNResampler=" << r.resampler_name
	      << "  (L_small=" << r.L_used << ")\n";
      }

    return r;
  }

  // Forward declaration so the 11-arg legacy overload can delegate to it
  template <typename Num, typename GeoStat, typename StrategyT,
            typename BootstrapFactoryT = palvalidator::bootstrap_cfg::BootstrapFactory>
  SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
                                  std::size_t              L,
                                  double                   annualizationFactor,
                                  double                   confLevel,
                                  std::size_t              B,
                                  double                   rho_m,
                                  StrategyT&               strategy,
                                  BootstrapFactoryT&       bootstrapFactory,
                                  std::ostream*            os,
                                  int                      stageTag,
                                  int                      fold,
                                  std::optional<bool>      heavy_tails_override);


  /**
   * @brief runs the conservative small-N lower bound analysis with automatic heavy-tail detection.
   *
   * This function acts as a convenience wrapper. It automatically computes the skewness
   * and excess kurtosis of the input returns to determine if a "heavy tail" override
   * is necessary, then delegates to the core implementation.
   *
   * @details
   * **Objective:** To provide a robust Lower Bound (LB) estimate for strategies with small sample
   * sizes (typically N=20 to 60) where the standard bootstrap might be too optimistic.
   *
   * **Logic:**
   * 1. Computes sample skewness and excess kurtosis.
   * 2. Determines if the distribution has "heavy tails" (via `has_heavy_tails_wide`).
   * 3. Calls the main implementation, passing the detected heavy-tail status as an override.
   *
   * @tparam Num Numeric type (e.g., double, decimal).
   * @tparam GeoStat Statistic functor type (e.g., GeoMeanStat).
   * @tparam StrategyT The strategy type (used for CRN hashing).
   * @tparam BootstrapFactoryT The factory type for creating bootstrap engines.
   *
   * @param returns The vector of high-resolution returns (e.g., per-trade or daily).
   * @param L The block length suggestion (will be clamped to [2,3] for small N internally).
   * @param annualizationFactor The factor to convert per-period metrics to annualized (e.g., 252.0).
   * @param confLevel The confidence level for the lower bound (e.g., 0.95).
   * @param B The number of bootstrap replicates (e.g., 1000+).
   * @param rho_m The m-out-of-n subsampling ratio. Pass <= 0.0 to trigger the adaptive
   *              TailVolStabilityPolicy (tail/vol prior + LB-stability refinement).
   * @param strategy Reference to the strategy object (for identification/hashing).
   * @param bootstrapFactory Reference to the factory generating RNG engines.
   * @param os Optional output stream for logging diagnostics (can be nullptr).
   * @param stageTag CRN tag for the pipeline stage (default 3).
   * @param fold CRN fold index (default 0).
   *
   * @return SmallNConservativeResult containing the annualized/per-period LB and diagnostics.
   */
  template <typename Num, typename GeoStat, typename StrategyT,
            typename BootstrapFactoryT = palvalidator::bootstrap_cfg::BootstrapFactory>
  inline SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(const std::vector<Num>& returns,
                                  std::size_t              L,
                                  double                   annualizationFactor,
                                  double                   confLevel,
                                  std::size_t              B,
                                  double                   rho_m,  // if <=0 → use TailVolStabilityPolicy
                                  StrategyT&               strategy,
                                  BootstrapFactoryT&       bootstrapFactory,
                                  std::ostream*            os = nullptr,
                                  int                      stageTag = 3,
                                  int                      fold = 0)
  {
    const auto [skew, exkurt] =
      mkc_timeseries::StatUtils<Num>::computeSkewAndExcessKurtosis(returns);
    const bool heavy = has_heavy_tails_wide(skew, exkurt);
    const std::optional<bool> heavy_override = heavy ? std::optional<bool>(true)
      : std::nullopt;

    return conservative_smallN_lower_bound<Num, GeoStat, StrategyT, BootstrapFactoryT>(
										       returns,
										       L,
										       annualizationFactor,
										       confLevel,
										       B,
										       rho_m,
										       strategy,
										       bootstrapFactory,
										       os,
										       stageTag,
										       fold,
										       heavy_override);
  }

  /**
   * @brief Core implementation of the conservative small-N lower bound logic.
   *
   * @details
   * This function orchestrates the "Small-N" bootstrap process:
   * 1. Analyzes distribution (Vol, Skew, Tail Index).
   * 2. Selects the appropriate Resampler (IID or StationaryBlock) based on
   * data characteristics or overrides.
   * 3. Calculates the optimal m-out-of-n ratio (using `resolve_adaptive_subsample_ratio`).
   * 4. Executes the "Duel" (m/n vs BCa) via `execute_bootstrap_duel`.
   *
   * @tparam Num Numeric type.
   * @tparam GeoStat Statistic functor.
   * @tparam StrategyT Strategy type.
   * @tparam BootstrapFactoryT Factory type.
   */
  template <typename Num, typename GeoStat, typename StrategyT,
	    typename BootstrapFactoryT>
  inline SmallNConservativeResult<Num, GeoStat, StrategyT>
  conservative_smallN_lower_bound(
				  const std::vector<Num>& returns,
				  std::size_t              L,
				  double                   annualizationFactor,
				  double                   confLevel,
				  std::size_t              B,
				  double                   rho_m,   // if <=0 → use TailVolStabilityPolicy
				  StrategyT&               strategy,
				  BootstrapFactoryT&       bootstrapFactory,
				  std::ostream* os,       // optional stage logger
				  int                      stageTag,
				  int                      fold,
				  std::optional<bool>      heavy_tails_override)
  {
    // ---------------------------------------------------------
    // 1. Setup & Context Analysis
    // ---------------------------------------------------------
    const std::size_t n = returns.size();

    const auto [mean, variance] = mkc_timeseries::StatUtils<Num>::computeMeanAndVarianceFast(returns);
    const double sigma = std::sqrt(num::to_double(variance));
    double sigmaAnn = sigma;
    if (annualizationFactor > 0.0) sigmaAnn *= std::sqrt(annualizationFactor);

    const auto [skew, exkurt] = mkc_timeseries::StatUtils<Num>::computeSkewAndExcessKurtosis(returns);
    const double tailIndex    = estimate_left_tail_index_hill(returns);
   
    // Detect Heavy Tails
    const bool heavy_from_shape = has_heavy_tails_wide(skew, exkurt);
    const bool heavy_flag = heavy_tails_override.has_value()
      ? *heavy_tails_override
      : heavy_from_shape;

    MNRatioContext ctx(n, sigmaAnn, skew, exkurt, tailIndex, heavy_flag);

    // ---------------------------------------------------------
    // 2. Resampler Selection Logic
    // ---------------------------------------------------------
    bool use_block = false;
    constexpr std::size_t N_BLOCK_ALWAYS = 60;

    if (heavy_tails_override.has_value())
      {
	use_block = *heavy_tails_override;
      }
    else if (n <= N_BLOCK_ALWAYS)
      {
	use_block = true; // Force block for very small samples
      }
    else
      {
	// Use dependence proxies for larger N
	const double ratio_pos   = sign_positive_ratio(returns);
	const std::size_t runlen = longest_sign_run(returns);
	use_block = choose_block_smallN(ratio_pos, n, runlen);
      }

    const std::size_t L_small = clamp_smallL(L);
    const double z = z_from_two_sided_CL(confLevel);

    // ---------------------------------------------------------
    // 3. Execution Dispatch via Generic Lambda
    // ---------------------------------------------------------
    // This lambda encapsulates the policy resolution and execution steps.
    auto run_variant = [&](auto& resampler, const char* name)
    {
      // Use std::decay_t to strip the reference (T&) from decltype(resampler).
      // This ensures ResamplerT is passed as the pure Value Type (T) to the template,
      // matching the expectation of std::is_same_v in the tests.
      using ResamplerType = std::decay_t<decltype(resampler)>;

      // A. Resolve Ratio (Policy)
      double final_rho = resolve_adaptive_subsample_ratio<Num, GeoStat>(rho_m,
									returns,
									ctx,
									L_small,
									confLevel,
									B,
									strategy,
									bootstrapFactory,
									resampler,
									os,
									stageTag,
									fold);

      // B. Execute Duel (Kernel)
      return execute_bootstrap_duel<ResamplerType, Num, GeoStat>(returns,
								       resampler,
								       final_rho,
								       L_small,
								       annualizationFactor,
								       confLevel,
								       B,
								       z,
								       strategy,
								       bootstrapFactory,
								       stageTag,
								       fold,
								       os,
								       name);
    };

    // ---------------------------------------------------------
    // 4. Branch & Execute
    // ---------------------------------------------------------
    if (use_block)
      {
	using BlockValueRes = palvalidator::resampling::StationaryMaskValueResamplerAdapter<Num>;
	BlockValueRes resampler(L_small);
	return run_variant(resampler, "StationaryMaskValueResamplerAdapter");
      }
    else
      {
	using IIDResampler = mkc_timeseries::IIDResampler<Num>;
	IIDResampler resampler;
	return run_variant(resampler, "IIDResampler");
      }
  }
} // namespace palvalidator::bootstrap_helpers
