// PALMonteCarloTypes.h
#ifndef __PAL_MONTE_CARLO_TYPES_H
#define __PAL_MONTE_CARLO_TYPES_H 1

#include <vector>
#include <memory>
#include "number.h"       // For Decimal type (ensure path is correct)
#include "PalStrategy.h"  // For PalStrategy definition (ensure path is correct)

namespace mkc_timeseries
{
    /**
     * @brief Shared structure for strategy context used in Monte Carlo validation.
     */
    template <class Decimal>
    struct StrategyContext
    {
        std::shared_ptr<PalStrategy<Decimal>> strategy;
        Decimal baselineStat;
        unsigned int count; // As per original definition
    };

    /**
     * @brief Shared type alias for the container holding strategy contexts.
     */
    template <class Decimal>
    using StrategyDataContainer = std::vector<StrategyContext<Decimal>>;

} // namespace mkc_timeseries

#endif // __PAL_MONTE_CARLO_TYPES_H
