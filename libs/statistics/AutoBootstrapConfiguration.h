#pragma once

namespace AutoBootstrapConfiguration
{
  // Coverage penalty multipliers (Percentile-specific)
  constexpr double kUnderCoverageMultiplier = 10.0; ///< Under-coverage penalized 2× more than over
  constexpr double kOverCoverageMultiplier  = 1.0; ///< Base penalty for exceeding nominal coverage
  
  // Length bounds (normalized to ideal bootstrap interval length)
  constexpr double kLengthMin           = 0.8;  ///< Minimum 80% of ideal (anti-conservative cutoff)
  constexpr double kLengthMaxStandard   = 1.8;  ///< Max 1.8× ideal for BCa/Percentile-T
  constexpr double kLengthMaxMOutOfN    = 6.0;  ///< Max 6× ideal for M-out-of-N (wider allowed)
  
  // Domain enforcement for strictly-positive statistics
  constexpr double kPositiveLowerEpsilon = 1e-9;
  constexpr double kDomainViolationPenalty = 1000.0;
  
  // BCa "rejection reason" diagnostics thresholds used in select()
  
  // Hard limits -- relaxed slightly to add safety headroom (see code review)
  constexpr double kBcaZ0HardLimit = 0.6;   ///< Hard rejection at |z0| > 0.6 (Efron 1987)
  constexpr double kBcaAHardLimit  = 0.25;  // relaxed from 0.2 -> 0.25

  // Soft thresholds: beyond these values soft penalties start to apply
  constexpr double kBcaZ0SoftThreshold = 0.25;
  constexpr double kBcaASoftThreshold  = 0.10;

  // Penalty scaling defaults (can be overridden via ScoringWeights)
  constexpr double kBcaZ0PenaltyScale = 20.0;
  constexpr double kBcaAPenaltyScale  = 100.0;

  // Calculate the penalty threshold dynamically based on the hard limit and
  // the soft-threshold. Threshold = (HardLimit - SoftThreshold)^2
  constexpr double kBcaStabilityThreshold =
    (kBcaZ0HardLimit - kBcaZ0SoftThreshold) * (kBcaZ0HardLimit - kBcaZ0SoftThreshold);
  
  constexpr double kBcaLengthPenaltyThreshold  = 1.0;

  // Floating-point tie tolerance scale used in select()
  constexpr double kRelativeTieEpsilonScale = 1e-10;
  constexpr double kBcaSkewThreshold = 2.0;    // Start penalizing beyond this
  constexpr double kBcaSkewPenaltyScale = 5.0; // Quadratic scaling factor

  // PercentileT stability thresholds (from computePercentileTStability)
  constexpr double kPercentileTOuterFailThreshold = 0.10;      ///< >10% outer resample failures
  constexpr double kPercentileTInnerFailThreshold = 0.05;      ///< >5% inner SE failures
  constexpr double kPercentileTMinEffectiveFraction = 0.70;    ///< Minimum 70% effective B
  constexpr double kPercentileTOuterPenaltyScale = 100.0;      ///< Penalty scale for outer failures
  constexpr double kPercentileTInnerPenaltyScale = 200.0;      ///< Penalty scale for inner failures
  constexpr double kPercentileTEffectiveBPenaltyScale = 50.0;  ///< Penalty scale for low effective B
  constexpr double kBcaLengthOverflowScale = 2.0;
} // namespace AutoBootstrapConfiguration
