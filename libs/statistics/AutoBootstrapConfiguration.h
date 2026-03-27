#pragma once

#include <cstddef>

namespace AutoBootstrapConfiguration
{
  // Coverage penalty multipliers (Percentile-specific)
  constexpr double kUnderCoverageMultiplier = 10.0; ///< Under-coverage penalized 10× more than over
  constexpr double kOverCoverageMultiplier  = 1.0;  ///< Base penalty for exceeding nominal coverage

  // Length bounds (normalized to ideal bootstrap interval length)
  constexpr double kLengthMin           = 0.8;  ///< Minimum 80% of ideal (anti-conservative cutoff)
  constexpr double kLengthMaxStandard   = 1.8;  ///< Max 1.8× ideal for BCa/Percentile-T
  constexpr double kLengthMaxMOutOfN    = 6.0;  ///< Max 6× ideal for M-out-of-N (wider allowed)

  // Domain enforcement for strictly-positive statistics
  constexpr double kPositiveLowerEpsilon   = 1e-9;
  constexpr double kDomainViolationPenalty = 1000.0;

  // ===========================================================================
  // BCa HARD REJECTION LIMITS
  //
  // Methods that exceed these are disqualified from the tournament entirely,
  // before penalty scoring is applied.
  // ===========================================================================

  constexpr double kBcaZ0HardLimit   = 0.6;   ///< Hard rejection: |z0|  > 0.6  (Efron 1987)
  constexpr double kBcaAHardLimit    = 0.25;  ///< Hard rejection: |a|   > 0.25 (relaxed from 0.2)
  constexpr double kBcaSkewHardLimit = 3.0;   ///< Hard rejection: |skew_boot| > 3.0 renders the
                                              ///< Edgeworth expansion on which BCa is based
                                              ///< unreliable. Consistent with the stricter accel
                                              ///< threshold that already activates at |skew| > 3.0
                                              ///< in penalty computation.

  /// Minimum original sample size for a reliable BCa jackknife acceleration
  /// estimate. Below this, the n leave-one-out values are too sparse to produce
  /// a trustworthy acceleration parameter. The existing z0/accel hard gates
  /// handle pathological cases above this floor.
  /// Set to 8 so BCa can compete at the observed minimum of n=9 trades while
  /// still being excluded for truly degenerate sample sizes.
  constexpr std::size_t kBcaMinSampleSize = 8;

  /// Minimum absolute number of effective bootstrap replicates required for
  /// any method to pass the effective-B gate in CandidateGateKeeper::passesEffectiveBGate.
  /// Referenced by both passesEffectiveBGate (AutoBootstrapScoring.h) and the
  /// diagnostic mirror in analyzeBcaRejection (AutoBootstrapSelector.h).
  /// Both sites must agree — a single constant here guarantees that.
  constexpr std::size_t kMinEffectiveBAbsolute = 200;

  // ===========================================================================
  // PERCENTILE-T HARD REJECTION LIMITS
  // ===========================================================================

  /// Minimum original sample size for reliable inner bootstrap SE* estimation
  /// in the double bootstrap. Below this threshold, the inner bootstrap
  /// converges to a biased but numerically stable SE* estimate, evading the
  /// inner-failure-rate stability gate while producing misleading intervals.
  /// This causes PercentileT to win tournaments at small n not because its
  /// intervals are good, but because its penalties are artificially low.
  ///
  /// At n >= 20 PercentileT competes legitimately and is expected to win
  /// frequently (empirically ~59% for 50 <= n < 100, ~78% for n >= 100).
  /// At n < 20 it self-disqualifies at the gating phase before scoring begins,
  /// leaving BCa, Percentile, and MOutOfN to compete.
  constexpr std::size_t kPercentileTMinSampleSize = 20;

  // ===========================================================================
  // BCa SOFT THRESHOLDS
  //
  // Beyond these values soft penalties begin to accrue, but the method is not
  // immediately disqualified.
  // ===========================================================================

  constexpr double kBcaZ0SoftThreshold      = 0.25; ///< Soft penalty starts at |z0| > 0.25
  constexpr double kBcaASoftThreshold       = 0.10; ///< Soft penalty starts at |a|  > 0.10
  constexpr double kBcaAStrictSoftThreshold = 0.08; ///< Stricter accel soft threshold applied
                                                    ///< when |skew_boot| > 3.0

  // ===========================================================================
  // BCa PENALTY SCALING
  // ===========================================================================

  constexpr double kBcaZ0PenaltyScale   = 20.0;  ///< Default z0 penalty scale (overridable via ScoringWeights)
  constexpr double kBcaAPenaltyScale    = 100.0; ///< Default accel penalty scale (overridable via ScoringWeights)
  constexpr double kBcaSkewThreshold    = 2.0;   ///< Skewness soft penalty starts beyond this magnitude
  constexpr double kBcaSkewPenaltyScale = 5.0;   ///< Quadratic scaling factor for skewness penalty

  // ===========================================================================
  // BCa LENGTH PENALTY THRESHOLDS
  //
  // These two constants share the same numeric value (1.0) but serve distinct
  // purposes and must be kept separate so they can evolve independently:
  //
  //   kBcaLengthOverflowThreshold  — used by ScoreNormalizer::computeBcaLengthOverflow()
  //                                   to add an extra quadratic penalty to the tournament
  //                                   score when BCa's length penalty exceeds this value.
  //
  //   kBcaLengthRejectionThreshold — used by AutoBootstrapSelector::analyzeBcaRejection()
  //                                   purely as a diagnostic: records that BCa was
  //                                   "rejected for length" in SelectionDiagnostics.
  // ===========================================================================

  constexpr double kBcaLengthOverflowThreshold  = 1.0; ///< Triggers BCa length overflow penalty in scoring
  constexpr double kBcaLengthRejectionThreshold = 1.0; ///< Triggers BCa rejection diagnosis in analyzeBcaRejection
  constexpr double kBcaLengthOverflowScale      = 2.0; ///< Quadratic scale applied to length overflow

  // ===========================================================================
  // BCa TRANSFORM STABILITY PENALTY
  // ===========================================================================

  /// Soft stability penalty added to a BCa candidate whose percentile-transform
  /// mapping was non-monotone (α₁ > α₂ before clamping/swapping).
  ///
  /// A non-monotone mapping means the BCa correction reversed direction: the
  /// adjusted lower-tail quantile ended up above the upper-tail quantile.
  /// calculateBCaBounds() silently swaps the indices so the interval bounds are
  /// still valid, but the correction is degraded.  This is NOT a hard rejection
  /// gate because the resulting interval is technically correct.  The soft penalty
  /// down-weights BCa in the tournament without eliminating it as a fallback when
  /// all alternatives score worse.
  ///
  /// Magnitude rationale: at kRefStability=0.25, a raw penalty of 0.5 normalises
  /// to 2.0 — 8× the reference — making the candidate noticeably less competitive
  /// than a BCa run with a well-conditioned transform, without eliminating it when
  /// every other candidate is also penalised.
  ///
  /// Note: BcaTransformStability::isStable() (near-singular denominator) is NOT
  /// penalised here because it is geometrically impossible within the existing
  /// kBcaZ0HardLimit / kBcaAHardLimit parameter space; those hard gates already
  /// exclude every parameter combination that can trigger a near-zero denominator.
  constexpr double kBcaTransformNonMonotonePenalty = 0.5;

  // Floating-point tie tolerance scale used in ImprovedTournamentSelector
  constexpr double kRelativeTieEpsilonScale = 1e-10;

  // ===========================================================================
  // M-OUT-OF-N RELIABILITY CONSTANTS
  //
  // These constants govern the two-tier response to M-out-of-N reliability
  // flag failures identified by MOutOfNPercentileBootstrap::Result::isReliable().
  //
  // HARD GATE (distribution_degenerate || insufficient_spread):
  //   The bootstrap distribution itself is pathological — too discrete or
  //   too concentrated to support meaningful quantile estimation. The candidate
  //   is disqualified from the tournament by setting stability_penalty to
  //   infinity, which triggers the ScoreNonFinite rejection gate. This is
  //   equivalent to BCa's z0/accel hard rejection gates.
  //
  // SOFT GATE (excessive_bias || ratio_near_boundary):
  //   The distribution is valid but the interval placement is uncertain.
  //   A finite stability penalty is added to down-weight M-out-of-N relative
  //   to other valid candidates, without eliminating it as the rescue fallback.
  //
  // LOWER BOUND HAIRCUT (excessive_bias, applied post-selection):
  //   When M-out-of-N wins the tournament despite excessive_bias, the lower
  //   bound is reduced by this fraction as a conservative adjustment for the
  //   unreliable rescaling centering. Applied in select() after winner
  //   determination. A 5% haircut is a conservative starting point — tune
  //   empirically across markets.
  //
  //   Note: The haircut is intentionally applied only for excessive_bias (not
  //   ratio_near_boundary) because excessive_bias directly affects the interval
  //   center and therefore the lower bound. ratio_near_boundary affects the
  //   ratio choice but not the centering of the distribution.
  // ===========================================================================

  /// Stability penalty added to M-out-of-N when soft reliability flags fire
  /// (excessive_bias or ratio_near_boundary). Conservative starting point —
  /// should be large enough to prefer a reliable method when available, but
  /// not so large that M-out-of-N is effectively eliminated.
  /// At kRefStability = 0.25, a penalty of 2.0 is 8× the reference — enough
  /// to lose to any method with moderate stability but not to extreme outliers.
  constexpr double kMOutOfNUnreliabilityPenalty = 2.0;

  /// Fractional reduction applied to M-out-of-N lower bound when excessive_bias
  /// fires and M-out-of-N wins the tournament. Conservative default of 5%.
  /// Tune empirically: increase if strategies with excessive_bias frequently
  /// underperform their lower bound; decrease if the haircut is too aggressive.
  constexpr double kMOutOfNExcessiveBiasHaircut = 0.05;

  /// Scale factor applied to excess bias (bias_fraction - threshold) to derive
  /// the adaptive haircut component. With kMOutOfNHaircutScale=0.10, an excess
  /// bias of 0.78 produces a raw haircut of 7.8% before capping.
  /// Tune empirically alongside kMOutOfNMaxHaircutFraction.
  constexpr double kMOutOfNHaircutScale = 0.10;

  /// Maximum haircut fraction applied to the M-out-of-N lower bound regardless
  /// of how large the bias fraction is. Caps the adaptive haircut to prevent
  /// extreme cases (bias_fraction > 2.0, driven by near-breakeven theta_hat
  /// that slips through RELIABILITY_BIAS_MIN_ABS_THETA) from producing
  /// nonsensical negative lower bounds. At 20%, even the worst cases produce
  /// a conservatively adjusted but still meaningful lower bound.
  constexpr double kMOutOfNMaxHaircutFraction = 0.20;

  // ===========================================================================
  // PERCENTILE-T STABILITY THRESHOLDS
  // ===========================================================================

  constexpr double kPercentileTOuterFailThreshold     = 0.10;  ///< >10% outer resample failures
  constexpr double kPercentileTInnerFailThreshold     = 0.05;  ///< >5%  inner SE failures
  constexpr double kPercentileTMinEffectiveFraction   = 0.70;  ///< Minimum 70% effective B
  constexpr double kPercentileTOuterPenaltyScale      = 100.0; ///< Penalty scale for outer failures
  constexpr double kPercentileTInnerPenaltyScale      = 200.0; ///< Penalty scale for inner failures
  constexpr double kPercentileTEffectiveBPenaltyScale = 50.0;  ///< Penalty scale for low effective B

  /// |skew(t*)| above which the soft pivot-skewness penalty begins to accrue.
  /// Mirrors BCa's kBcaSkewThreshold (2.0). Below this value no skew penalty
  /// is applied to Percentile-T; above it the quadratic penalty grows with
  /// kPercentileTSkewPenaltyScale.
  constexpr double kPercentileTSkewSoftThreshold = 2.0;

  /// |skew(t*)| above which extreme_pivot_skewness fires in Result::isReliable().
  /// Treated as a hard reliability flag (analogous to kBcaSkewHardLimit = 3.0).
  /// Feeds algorithmIsReliable in the tournament but does NOT produce a hard
  /// rejection mask — the soft penalty path handles the tournament response.
  constexpr double kPercentileTSkewHardThreshold = 3.0;

  /// Quadratic scale applied to the excess pivot skewness when
  /// |skew(t*)| > kPercentileTSkewSoftThreshold. Mirrors kBcaSkewPenaltyScale.
  constexpr double kPercentileTSkewPenaltyScale  = 5.0;

  // ===========================================================================
  // NORMALIZATION REFERENCE VALUES
  //
  // Raw penalties are divided by these reference values to put all penalty
  // types on a comparable scale in the tournament scoring.
  // ===========================================================================

  /// Ordering penalty reference: 10% coverage error squared.
  /// Rationale: A 10% deviation from nominal coverage (e.g., 85% actual vs 95%
  /// nominal) represents a "typical" ordering violation baseline.
  constexpr double kRefOrderingErrorSq = 0.10 * 0.10;  // = 0.01

  /// Length penalty reference: ideal length error squared.
  /// Rationale: An interval exactly 1× the theoretical ideal width is optimal.
  /// Deviations from this are measured relative to 1.0.
  constexpr double kRefLengthErrorSq = 1.0 * 1.0;  // = 1.0

  /// Stability penalty reference for BCa and Percentile-T.
  /// Rationale: A stability penalty of 0.25 represents moderate instability
  /// that is noticeable but not disqualifying.
  constexpr double kRefStability = 0.25;

  /// Center shift reference: 2 standard errors squared.
  /// Rationale: A shift of 2 SE between bootstrap mean and point estimate
  /// represents "notable" bias that merits attention.
  constexpr double kRefCenterShiftSq = 2.0 * 2.0;  // = 4.0

  /// Skewness reference: |skew| = 2.0 squared.
  /// Rationale: |skew| = 2.0 is the threshold where distributions are
  /// considered "highly skewed" and may violate BCa assumptions.
  constexpr double kRefSkewSq = 2.0 * 2.0;  // = 4.0

  // ===========================================================================
  // SCORING WEIGHTS
  // ===========================================================================

  /// Fixed weight for the ordering/coverage penalty component.
  /// This is intentionally not user-configurable via ScoringWeights — ordering
  /// accuracy is a baseline correctness requirement, not a tuning parameter.
  constexpr double kOrderingPenaltyWeight = 1.0;

} // namespace AutoBootstrapConfiguration
