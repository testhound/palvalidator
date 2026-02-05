#pragma once

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief Specifies whether a bootstrap confidence interval is one-sided or two-sided.
     * 
     * TWO_SIDED: Traditional symmetric interval [L, U] with α/2 in each tail
     * ONE_SIDED_LOWER: Only lower bound L is meaningful, with α in lower tail only
     * ONE_SIDED_UPPER: Only upper bound U is meaningful, with α in upper tail only
     */
    enum class IntervalType
    {
      TWO_SIDED,        // Default for backward compatibility
      ONE_SIDED_LOWER,  // For strategy filtering (only care about minimum performance)
      ONE_SIDED_UPPER   // For risk management (only care about maximum loss)
    };
  }
}
