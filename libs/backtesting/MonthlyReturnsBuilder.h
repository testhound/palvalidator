#pragma once
#include <vector>
#include <map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace mkc_timeseries
{
  // Helper to turn a ptime into a (year, month) key
  struct YearMonth
  {
    int year;
    int month; // 1..12
    bool operator<(const YearMonth& o) const {
      return (year < o.year) || (year == o.year && month < o.month);
    }
  };

  // Build monthly returns (compounded within each calendar month) from a ClosedPositionHistory.
  // Notes:
  // - Months with no exposure are omitted (can optionally be filled as 0% if you prefer).
  // - For each bar return r_t inside a month, we compound: M = M * (1 + r_t); month_ret = M - 1.
  // - Short positions are already handled (their per-bar returns are signed correctly).
  template <class Decimal>
  std::vector<Decimal>
  buildMonthlyReturnsFromClosedPositions(const ClosedPositionHistory<Decimal>& closePositionHistory)
  {
    using boost::posix_time::ptime;
    using boost::gregorian::date;

    // We’ll walk each position’s bar history directly to get timestamps + returns.
    // Create a map from (year, month) -> compounded multiplier
    std::map<YearMonth, Decimal> monthMult;

    // Iterate all closed positions (in entry-time order)
    for (auto it = closePositionHistory.beginTradingPositions(); it != closePositionHistory.endTradingPositions(); ++it)
      {
	const auto& pos = it->second; // shared_ptr<TradingPosition<Decimal>>
	auto barIt  = pos->beginPositionBarHistory();
	auto barEnd = pos->endPositionBarHistory();
	if (barIt == barEnd)
	  continue;

	// First reference is actual entry price (to make the first bar accurate)
	Decimal prevRef = pos->getEntryPrice();

	for (; barIt != barEnd; ++barIt)
	  {
	    // barIt is (ptime -> BarType). We’ll use the key (ptime) for month bucketing.
	    const ptime& ts = barIt->first;

	    // Compute the bar's signed return (last bar uses exit price)
	    Decimal r_t;
	    if (std::next(barIt) == barEnd)
	      {
		const Decimal exitPx = pos->getExitPrice();
		r_t = (prevRef != Decimal(0)) ? (exitPx - prevRef) / prevRef
		  : Decimal(0);
	      }
	    else
	      {
		const Decimal closePx = barIt->second.getCloseValue();
		r_t = (prevRef != Decimal(0)) ? (closePx - prevRef) / prevRef
		  : Decimal(0);
		prevRef = closePx;
	      }

	    // Short positions: price down is a gain -> invert sign
	    if (pos->isShortPosition())
	      r_t *= Decimal(-1);

	    // Bucket by calendar month
	    date d = ts.date();
	    YearMonth ym{ static_cast<int>(d.year()), static_cast<int>(d.month()) };
	    auto& M = monthMult[ym];
	    if (M == Decimal(0))
	      M = Decimal(1);        // init multiplier

	    M *= (Decimal(1) + r_t);
	  }
      }

    // Emit month_return = (multiplier - 1) in chronological order
    std::vector<Decimal> monthly;
    monthly.reserve(monthMult.size());
    for (const auto& kv : monthMult) {
      monthly.push_back(kv.second - Decimal(1));
    }
    return monthly;
  }
} // namespace mkc_timeseries
