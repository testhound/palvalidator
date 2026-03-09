#pragma once

#include <stdexcept>
#include <string>

namespace palvalidator
{
  /**
   * @file BootstrapException.h
   * @brief Exception hierarchy for bootstrap confidence interval failures.
   *
   * Three-level hierarchy mapping cleanly onto the three layers of the
   * bootstrap subsystem:
   *
   *   BootstrapException  (base — catch-all for any bootstrap failure)
   *   │
   *   ├── StrategyAutoBootstrapException
   *   │     Thrown by AutoBootstrapSelector when the tournament cannot
   *   │     produce a trustworthy result. Carries a Reason enum and a
   *   │     detail string describing exactly which candidates were attempted
   *   │     and which gates fired. Known to the bootstrap stage; not
   *   │     expected to propagate to higher-level clients.
   *   │
   *   └── BootstrapStageException
   *         Thrown by the bootstrap stage after catching a
   *         StrategyAutoBootstrapException. Hides tournament internals
   *         from upstream callers. The only bootstrap exception type that
   *         clients of the bootstrap stage need to handle. Its message
   *         is deliberately non-technical: "bootstrap stage could not
   *         produce a confidence interval — treat strategy as a failure."
   *
   * ============================================================================
   * USAGE PATTERN
   * ============================================================================
   *
   * Inside AutoBootstrapSelector::select():
   * @code
   *   throw StrategyAutoBootstrapException(
   *       StrategyAutoBootstrapException::Reason::AllGatesFailed,
   *       "n=9, BCa: |skew|=3.8 > 3.0, MOutOfN: effective_B=140 < 200");
   * @endcode
   *
   * Inside the bootstrap stage (e.g. StrategyAutoBootstrap::run()):
   * @code
   *   try {
   *     result = AutoBootstrapSelector<Decimal>::select(candidates, weights);
   *   }
   *   catch (const StrategyAutoBootstrapException& ex) {
   *     throw BootstrapStageException(ex);
   *   }
   * @endcode
   *
   * Inside a client of the bootstrap stage:
   * @code
   *   try {
   *     bootstrapStage.run();
   *     reportProfitFactor(bootstrapStage.getResult());
   *   }
   *   catch (const BootstrapStageException&) {
   *     recordStrategyAsFailure(strategyName);
   *   }
   * @endcode
   */

  // ===========================================================================
  // BASE
  // ===========================================================================

  /**
   * @brief Base class for all palvalidator bootstrap exceptions.
   *
   * Catch this type to handle any bootstrap failure regardless of origin.
   * Prefer catching the more specific derived types where the distinction
   * matters.
   */
  class BootstrapException : public std::runtime_error
  {
  public:
    explicit BootstrapException(const std::string& msg)
      : std::runtime_error(msg)
    {}
  };

  // ===========================================================================
  // TOURNAMENT LAYER
  // ===========================================================================

  /**
   * @brief Thrown by AutoBootstrapSelector when the tournament fails.
   *
   * Carries a machine-readable Reason enum so the bootstrap stage can log
   * a structured failure record without parsing the message string, and a
   * human-readable detail string describing exactly why each candidate was
   * eliminated.
   *
   * This exception is an implementation detail of the bootstrap tournament.
   * It should be caught by the bootstrap stage and translated into a
   * BootstrapStageException before propagating to higher-level clients.
   */
  class StrategyAutoBootstrapException : public BootstrapException
  {
  public:
    /**
     * @brief Machine-readable reason for tournament failure.
     *
     * AllGatesFailed      — every candidate that entered the tournament was
     *                       eliminated by at least one hard gate (non-finite
     *                       score, domain violation, low effective-B, bad
     *                       z0/accel, extreme skewness, or insufficient n).
     *                       This is a data-driven failure: the strategy's
     *                       return series is too pathological for any of the
     *                       enabled bootstrap methods to produce a reliable CI.
     *
     * NoCandidatesProvided — select() was called with an empty candidate list.
     *                        This is a programming error in the calling code,
     *                        not a data-quality issue.
     */
    enum class Reason
    {
      AllGatesFailed,
      NoCandidatesProvided
    };

    StrategyAutoBootstrapException(Reason             reason,
                                   const std::string& detail)
      : BootstrapException(makeMessage(reason, detail)),
        m_reason(reason),
        m_detail(detail)
    {}

    Reason getReason() const noexcept { return m_reason; }

    /// The raw detail string passed at construction — describes which
    /// candidates were attempted and which gates fired.
    const std::string& getDetail() const noexcept { return m_detail; }

  private:
    Reason      m_reason;
    std::string m_detail;

    static std::string makeMessage(Reason r, const std::string& detail)
    {
      switch (r)
      {
        case Reason::AllGatesFailed:
          return "Bootstrap tournament: all candidates failed hard gates "
                 "— no trustworthy CI available. " + detail;

        case Reason::NoCandidatesProvided:
          return "Bootstrap tournament: no candidates provided "
                 "(programming error). " + detail;
      }
      return detail;  // unreachable, suppresses compiler warning
    }
  };

  // ===========================================================================
  // BOOTSTRAP STAGE LAYER
  // ===========================================================================

  /**
   * @brief Thrown by the bootstrap stage when it cannot produce a CI.
   *
   * This is the only bootstrap exception type that clients of the bootstrap
   * stage (e.g. strategy evaluation pipelines) need to handle. It deliberately
   * exposes no tournament internals — the caller only needs to know that the
   * bootstrap stage could not produce a profit factor or geometric mean, and
   * should treat the strategy as a failure.
   *
   * The bootstrap stage constructs this exception by catching a
   * StrategyAutoBootstrapException and wrapping it:
   *
   * @code
   *   catch (const StrategyAutoBootstrapException& ex) {
   *     throw BootstrapStageException(ex);
   *   }
   * @endcode
   *
   * The original StrategyAutoBootstrapException detail is preserved in
   * m_tournamentDetail for diagnostic logging inside the stage if desired,
   * but is not surfaced via the public interface.
   */
  class BootstrapStageException : public BootstrapException
  {
  public:
    /**
     * @brief Construct from a StrategyAutoBootstrapException.
     *
     * The message exposed to the client is generic and non-technical.
     * The tournament detail is retained privately for stage-level logging.
     */
    explicit BootstrapStageException(
      const StrategyAutoBootstrapException& cause)
      : BootstrapException(
          "Bootstrap stage could not produce a confidence interval — "
          "treat strategy as a failure.")
      , m_tournament_detail(cause.getDetail())
    {}

    /**
     * @brief Construct with an explicit stage-level message.
     *
     * Use when the bootstrap stage wants to provide additional context
     * beyond the generic message (e.g. strategy name, parameter set).
     */
    BootstrapStageException(const StrategyAutoBootstrapException& cause,
                            const std::string& stageContext)
      : BootstrapException(
          "Bootstrap stage could not produce a confidence interval "
          "for " + stageContext + " — treat strategy as a failure.")
      , m_tournament_detail(cause.getDetail())
    {}

    /// Tournament-level detail string for stage-internal diagnostic logging.
    /// Not intended for client code.
    const std::string& getTournamentDetail() const noexcept
    {
      return m_tournament_detail;
    }

  private:
    std::string m_tournament_detail;
  };

} // namespace palvalidator
