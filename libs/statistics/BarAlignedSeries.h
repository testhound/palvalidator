#pragma once

#include <vector>
#include <stdexcept>
#include <string>
#include <sstream>
#include <map>      // ordered map for ptime -> index
#include <algorithm>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "TimeSeries.h"                 // mkc_timeseries::NumericTimeSeries, OHLCTimeSeries
#include "TimeSeriesEntry.h"            // mkc_timeseries::NumericTimeSeriesEntry
#include "ClosedPositionHistory.h"      // mkc_timeseries::ClosedPositionHistory
#include "TradingPosition.h"            // mkc_timeseries::TradingPosition
#include "RegimeLabeler.h"              // palvalidator::analysis::VolTercileLabeler

namespace palvalidator
{
  namespace analysis
  {
    /**
     * @brief An adapter that connects a dense market time series to a sparse trade time series.
     * @tparam Num The numeric type for price and return data (e.g., double, float).
     *
     * This class generates market volatility regime labels that are precisely aligned
     * with a strategy's trade-sequence returns. It addresses the challenge of applying
     * market context (like volatility) to the sparse record of trades, where many
     * non-trading bars are absent.
     *
     * By labeling the underlying dense bar series first and then projecting those labels
     * onto the specific bars where trades occurred, it ensures that subsequent analysis,
     * such as regime-mix stress testing, operates on accurate market conditions.
     *
     * @section workflow How it Works: An Orchestrated Process
     * Think of `BarAlignedSeries` as the project manager and `VolTercileLabeler` as a
     * specialized analyst. The manager (`BarAlignedSeries`) directs the workflow, while
     * the analyst (`VolTercileLabeler`) performs the core statistical task of labeling.
     *
     * 1.  **Analyze the Entire Market:** `BarAlignedSeries` first computes the returns for every
     * single bar in the dense market history. It then hands this complete list of returns
     * to the `VolTercileLabeler`.
     * 2.  **Label Market Regimes:** The `VolTercileLabeler` calculates a rolling volatility
     * measure and partitions all bars into three terciles: Low (0), Medium (1), and High (2).
     * It returns a dense vector of these labels, one for each market bar.
     * 3.  **Identify Trade Timestamps:** `BarAlignedSeries` inspects the `ClosedPositionHistory`
     * to get the exact timestamp of every bar where a trade was active. This creates a
     * sparse list of trade times.
     * 4.  **Project and Align:** For each timestamp in the sparse trade list, `BarAlignedSeries`
     * looks up the corresponding label from the dense market label vector. The final
     * output is a sparse list of labels that is perfectly aligned with your trade sequence.
     *
     * @section example Detailed Example
     * Let's assume a **volatility window of 3 bars** and a trade that was active from T5 to T7.
     *
     * @code
     * +-----------+-------------+------------+----------------------+--------------------+
     * | Timestamp | Close Price | Bar Return | Rolling Vol (3-bar)  | Dense Market Label |
     * +-----------+-------------+------------+----------------------+--------------------+
     * | T0        | 100         | -          | -                    | -                  |
     * | T1        | 101         | +1.0%      | -                    | 1 (Mid)  (filled)  |
     * | T2        | 100.5       | -0.5%      | -                    | 1 (Mid)  (filled)  |
     * | T3        | 102         | +1.5%      | 1.00%                | 1 (Mid)            |
     * | T4        | 102.5       | +0.5%      | 0.83%                | 0 (Low)            |
     * | T5        | 101         | -1.5%      | 1.17%                | 1 (Mid)   <-- Trade|
     * | T6        | 103         | +2.0%      | 1.33%                | 2 (High)  <-- Trade|
     * | T7        | 103.5       | +0.5%      | 1.33%                | 2 (High)  <-- Trade|
     * | T8        | 104         | +0.5%      | 1.00%                | 1 (Mid)            |
     * | T9        | 102         | -1.9%      | 0.97%                | 1 (Mid)            |
     * +-----------+-------------+------------+----------------------+--------------------+
     * @endcode
     *
     * The `collectTradeReturnTimes` method identifies the trade timestamps (T5, T6, T7). The class
     * then looks up the corresponding labels from the "Dense Market Label" column.
     *
     * - Trade at T5 -> Market Label is **1 (Mid)**
     * - Trade at T6 -> Market Label is **2 (High)**
     * - Trade at T7 -> Market Label is **2 (High)**
     *
     * The final result is the vector `[1, 2, 2]`, which is perfectly aligned with the trade returns.
     *
     * @section inputs Inputs
     * - A dense out-of-sample (OOS) time series of instrument close prices.
     * - The OOS `ClosedPositionHistory`, which identifies the exact bars that contributed
     * to the trade sequence.
     *
     * @section outputs Outputs
     * - A `std::vector<int>` of regime labels, perfectly aligned with the trade-sequence
     * returns. The labels are:
     * - 0: Low Volatility
     * - 1: Medium Volatility
     * - 2: High Volatility
     *
     * @note This class does *not* compute the trade-sequence returns themselves; it only
     * generates the corresponding labels. It is a preparatory step for resampling-based
     * stress tests.
     */
    template <class Num>
    class BarAlignedSeries
    {
    public:
      /**
       * @brief Constructs the BarAlignedSeries labeler.
       * @param volWindow The size of the rolling window used to calculate volatility
       * terciles. A minimum size of 2 is enforced. A value of 20 is
       * a reasonable default for strategies with short holding periods.
       */
      explicit BarAlignedSeries(std::size_t volWindow)
        : mVolWindow(std::max<std::size_t>(2, volWindow))
      {
      }

      /**
       * @brief Builds trade-aligned volatility labels from an OHLCTimeSeries.
       * @param oosOhlc The out-of-sample OHLC time series for the instrument.
       * @param oosClosed The out-of-sample closed position history for the strategy.
       * @return A vector of integer labels aligned with the trade sequence.
       *
       * This is a convenience overload that extracts the close series from the
       * OHLCTimeSeries and forwards it to the primary implementation.
       */
      std::vector<int> buildTradeAlignedLabels(const mkc_timeseries::OHLCTimeSeries<Num> &oosOhlc,
					       const mkc_timeseries::ClosedPositionHistory<Num> &oosClosed) const
      {
        return buildTradeAlignedLabels(oosOhlc.CloseTimeSeries(), oosClosed);
      }

      /**
       * @brief Builds trade-aligned volatility labels from a dense NumericTimeSeries of closing prices.
       *
       * This is the core method of the class. It performs a four-step process:
       * 1. Computes returns for every bar in the dense `oosCloseSeries`.
       * 2. Uses a `VolTercileLabeler` to assign a volatility regime label to each bar.
       * 3. Collects the exact timestamps of the bars where a position was held from `oosClosed`.
       * 4. Projects the dense bar labels onto the sparse trade timestamps to produce the final,
       * aligned label vector.
       *
       * @param oosCloseSeries The dense out-of-sample time series of closing prices.
       * @param oosClosed The out-of-sample closed position history, used to identify trade bars.
       * @return A `std::vector<int>` containing one volatility label for each entry in the
       * trade sequence (0=Low, 1=Mid, 2=High).
       * @throws std::invalid_argument if the close series is too short for the volatility window,
       * if no trade bars are found in `oosClosed`, or if a trade timestamp cannot be
       * found in the close series.
       * @throws std::runtime_error if an internal data inconsistency is found, such as a bar
       * index being out of range.
       */
      std::vector<int> buildTradeAlignedLabels(
					       const mkc_timeseries::NumericTimeSeries<Num> &oosCloseSeries,
					       const mkc_timeseries::ClosedPositionHistory<Num> &oosClosed) const
      {
        // 1) Dense bar returns + timestamps (ending bar timestamps).
        std::vector<Num> barRoc;
        std::vector<boost::posix_time::ptime> barTimes;
        buildDenseBarReturns(oosCloseSeries, barRoc, barTimes);

        // 2) Label bars by volatility terciles.
        if (barRoc.size() < mVolWindow + 2)
	  {
            std::ostringstream msg;
            msg << "BarAlignedSeries: OOS close series too short for vol window "
                << mVolWindow << " (bars=" << barRoc.size() << ").";
            throw std::invalid_argument(msg.str());
	  }

        VolTercileLabeler<Num> labeler(mVolWindow);
        const std::vector<int> barLabels = labeler.computeLabels(barRoc);

        // 3) Collect the timestamps for each trade-sequence return entry.
        const std::vector<boost::posix_time::ptime> tradeTimes =
	  collectTradeReturnTimes(oosClosed);

        // 4) Build ptime -> bar-index map for alignment.
        std::map<boost::posix_time::ptime, std::size_t> barIndex;
        for (std::size_t i = 0; i < barTimes.size(); ++i)
	  {
            barIndex.emplace(barTimes[i], i);
	  }

        // 5) Project bar labels onto trade-sequence entries.
        std::vector<int> labels;
        labels.reserve(tradeTimes.size());

        for (const auto &ts : tradeTimes)
	  {
            auto it = barIndex.find(ts);
            if (it == barIndex.end())
	      {
                std::ostringstream msg;
                msg << "BarAlignedSeries: trade bar timestamp " << ts
                    << " not found in OOS close series.";
                throw std::invalid_argument(msg.str());
	      }

            const std::size_t bIdx = it->second;
            if (bIdx >= barLabels.size())
	      {
                std::ostringstream msg;
                msg << "BarAlignedSeries: bar index " << bIdx
                    << " out of range for barLabels size " << barLabels.size() << ".";
                throw std::runtime_error(msg.str());
	      }

            labels.push_back(barLabels[bIdx]);
	  }

        return labels;
      }

    private:
      /**
       * @brief Computes dense, close-to-close returns from a numeric time series.
       *
       * Calculates the rate of change `r = (c[t] - c[t-1]) / c[t-1]`. Each calculated
       * return is associated with the timestamp of the *ending* bar (at time `t`).
       *
       * @param[in] closeSeries The input time series of close prices.
       * @param[out] outReturns A vector to be populated with the computed bar-over-bar returns.
       * @param[out] outTimes A vector to be populated with the corresponding ending-bar timestamps.
       * @throws std::invalid_argument if the input series has fewer than two entries.
       * @throws std::runtime_error if a close price of zero is encountered in a denominator.
       */
      static void buildDenseBarReturns(const mkc_timeseries::NumericTimeSeries<Num> &closeSeries,
				       std::vector<Num> &outReturns,
				       std::vector<boost::posix_time::ptime> &outTimes)
      {
        // Safe snapshot of entries (sorted by time).
        const auto entries = closeSeries.getEntriesCopy(); // std::vector<NumericTimeSeriesEntry<Num>>
        const std::size_t n = entries.size();

        if (n < 2)
	  {
            throw std::invalid_argument("BarAlignedSeries: close series needs at least 2 bars.");
	  }

        outReturns.clear();
        outTimes.clear();
        outReturns.reserve(n - 1);
        outTimes.reserve(n - 1);

        // Return at (t-1) covers c[t-1] -> c[t], stamped with entries[t].getDateTime()
        for (std::size_t t = 1; t < n; ++t)
	  {
            const Num c0 = entries[t - 1].getValue();
            const Num c1 = entries[t].getValue();
            if (c0 == Num(0))
	      {
                throw std::runtime_error("BarAlignedSeries: zero prior close encountered.");
	      }

            const Num r = (c1 - c0) / c0;
            outReturns.push_back(r);
            outTimes.push_back(entries[t].getDateTime());
	  }
      }

      /**
       * @brief Extracts the timestamp of every bar during which a position was active.
       *
       * This method iterates through each closed position and its associated bar history
       * to compile a single vector of all timestamps corresponding to a trade bar. This
       * represents the "sparse" series of trade events.
       *
       * @param closed The `ClosedPositionHistory` containing all trades and their bar-by-bar data.
       * @return A vector of `ptime` timestamps, one for each bar a position was held.
       * @throws std::invalid_argument if the `ClosedPositionHistory` contains no trade bars.
       */
      static std::vector<boost::posix_time::ptime>
      collectTradeReturnTimes(const mkc_timeseries::ClosedPositionHistory<Num> &closed)
      {
        std::vector<boost::posix_time::ptime> times;

        // Iterate each closed position and then each bar in its bar history.
        // TradingPosition<T>::beginPositionBarHistory() returns an iterator over
        // a map<ptime, OpenPositionBar<T>> (per TradingPosition.h).
        for (auto posIt = closed.beginTradingPositions();
             posIt != closed.endTradingPositions();
             ++posIt)
	  {
            const auto &posPtr = posIt->second; // shared_ptr<TradingPosition<Num>>
            auto barIt  = posPtr->beginPositionBarHistory();
            auto barEnd = posPtr->endPositionBarHistory();

            for (; barIt != barEnd; ++barIt)
	      {
                const auto &openBar = barIt->second; // OpenPositionBar<Num>
                times.push_back(openBar.getDateTime());
	      }
	  }

        if (times.empty())
	  {
            throw std::invalid_argument("BarAlignedSeries: no trade bar timestamps found in ClosedPositionHistory.");
	  }

        return times;
      }

    private:
      std::size_t mVolWindow;
    };

  } // namespace analysis
} // namespace palvalidator
