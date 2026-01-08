#pragma once
#include <cstdint>
#include <string>
#include <sstream>

namespace palvalidator::diagnostics
{
  /**
   * @brief Bitmask enum for rejection reasons during candidate selection.
   * 
   * Multiple reasons can be combined using bitwise OR. This allows efficient
   * storage and querying of all rejection reasons for a candidate.
   */
  enum class CandidateReject : std::uint32_t
    {
      None                    = 0u,
      ScoreNonFinite          = 1u << 0,  // Score is NaN or infinite
      ViolatesSupport         = 1u << 1,  // Interval violates StatisticSupport domain
      EffectiveBLow           = 1u << 2,  // Effective B gate failed
      BcaParamsNonFinite      = 1u << 3,  // BCa z0 or accel non-finite
      BcaZ0HardFail           = 1u << 4,  // |z0| exceeds hard limit
      BcaAccelHardFail        = 1u << 5,  // |accel| exceeds hard limit
      PercentileTInnerFails   = 1u << 6,  // Percentile-T inner fail rate too high (diagnostic)
      PercentileTLowEffB      = 1u << 7,  // Percentile-T effective B fraction too low (diagnostic)
      // bits 8..31 reserved
    };

  enum class CandidateFlag : std::uint32_t
    {
      None                 = 0u,
      UsedEnforcePositive  = 1u << 0,  // support was unbounded but weights.enforcePositive() forced a lower bound
      SkewHigh             = 1u << 1,  // |skew| above threshold (soft flag)
      BcaLengthOverflow    = 1u << 2,  // BCa length penalty exceeded soft threshold; overflow penalty applied
      // bits 3..31 reserved
    };

  inline CandidateReject operator|(CandidateReject a, CandidateReject b) noexcept
  {
    return static_cast<CandidateReject>(
					static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
  }

  inline CandidateReject operator&(CandidateReject a, CandidateReject b) noexcept
  {
    return static_cast<CandidateReject>(
					static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
  }

  inline CandidateReject& operator|=(CandidateReject& a, CandidateReject b) noexcept
  {
    a = (a | b);
    return a;
  }

  inline bool hasRejection(CandidateReject mask, CandidateReject reason) noexcept
  {
    return static_cast<std::uint32_t>(mask & reason) != 0u;
  }

  inline CandidateFlag operator|(CandidateFlag a, CandidateFlag b) noexcept
  {
    return static_cast<CandidateFlag>(
				      static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
  }

  inline CandidateFlag operator&(CandidateFlag a, CandidateFlag b) noexcept
  {
    return static_cast<CandidateFlag>(
				      static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
  }

  inline CandidateFlag& operator|=(CandidateFlag& a, CandidateFlag b) noexcept
  {
    a = (a | b);
    return a;
  }

  inline bool hasFlag(CandidateFlag mask, CandidateFlag flag) noexcept
  {
    return static_cast<std::uint32_t>(mask & flag) != 0u;
  }

  inline std::string rejectionMaskToString(CandidateReject mask)
  {
    if (mask == CandidateReject::None) {
      return "";
    }

    std::ostringstream oss;
    bool first = true;

    auto append = [&](CandidateReject flag, const char* name) {
      if (hasRejection(mask, flag)) {
	if (!first) oss << ";";
	oss << name;
	first = false;
      }
    };

    append(CandidateReject::ScoreNonFinite,        "SCORE_NON_FINITE");
    append(CandidateReject::ViolatesSupport,       "VIOLATES_SUPPORT");
    append(CandidateReject::EffectiveBLow,         "EFFECTIVE_B_LOW");
    append(CandidateReject::BcaParamsNonFinite,    "BCA_PARAMS_NON_FINITE");
    append(CandidateReject::BcaZ0HardFail,         "BCA_Z0_EXCEEDED");
    append(CandidateReject::BcaAccelHardFail,      "BCA_ACCEL_EXCEEDED");
    append(CandidateReject::PercentileTInnerFails, "PCTT_INNER_FAILURES");
    append(CandidateReject::PercentileTLowEffB,    "PCTT_LOW_EFFECTIVE_B");

    return oss.str();
  }
} // namespace palvalidator::diagnostics
