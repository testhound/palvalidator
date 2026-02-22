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
    bool operator==(const YearMonth& o) const {
      return year == o.year && month == o.month;
    }
    YearMonth nextMonth() const {
      if (month == 12) {
        return {year + 1, 1};
      } else {
        return {year, month + 1};
      }
    }
  };

  // Build monthly returns (compounded within each calendar month) from a ClosedPositionHistory.
  // Notes:
  // - Months with no exposure are omitted (can optionally be filled as 0% if you prefer).
  // - For each bar return r_t inside a month, we compound: M = M * (1 + r_t); month_ret = M - 1.
  // - Short positions are already handled (their per-bar returns are signed correctly).
  template <class Decimal>
  std::vector<Decimal>
  buildMonthlyReturnsFromClosedPositions(const ClosedPositionHistory<Decimal>& closedPositionHistory,
					 bool includeFlatMonths = false)
  {
    using boost::posix_time::ptime;
    using boost::gregorian::date;

    // Step 1: Aggregate per-bar portfolio P&L and exposure across all positions.
    // Map ts -> (sumPnL, sumExposure)
    std::map<ptime, std::pair<Decimal, Decimal>> barAgg;

    const Decimal zero = Decimal(0);
    const Decimal one  = Decimal(1);

    for (auto it = closedPositionHistory.beginTradingPositions();
	 it != closedPositionHistory.endTradingPositions(); ++it)
      {
	const auto& pos = it->second; // shared_ptr<TradingPosition<Decimal>>

	auto barIt  = pos->beginPositionBarHistory();
	auto barEnd = pos->endPositionBarHistory();
	if (barIt == barEnd)
	  continue;

	// Reference price for return calculations (entry convention)
	Decimal prevRef = pos->getEntryPrice();
	if (prevRef == zero)
	  continue;

	for (; barIt != barEnd; ++barIt)
	  {
	    const ptime& ts = barIt->first;

	    const bool isLastBar = (std::next(barIt) == barEnd);

	    Decimal priceNowOrExit = zero;
	    if (isLastBar) {
	      priceNowOrExit = pos->getExitPrice();
	    } else {
	      priceNowOrExit = barIt->second.getCloseValue();
	    }

	    if (prevRef == zero)
	      continue;

	    // Signed price change from prevRef -> current reference (close or exit)
	    Decimal priceChange = priceNowOrExit - prevRef;

	    // Short positions profit when price falls
	    if (pos->isShortPosition())
	      priceChange *= Decimal(-1);

	    // PnL for 1 share/contract = signed price change
	    const Decimal pnl = priceChange;

	    // Gross exposure proxy for this bar: prevRef (always positive price)
	    // (This is effectively a return on gross notional.)
	    const Decimal exposure = prevRef;

	    auto& agg = barAgg[ts];
	    agg.first  += pnl;
	    agg.second += exposure;

	    // Update reference for next bar only on intermediate bars
	    if (!isLastBar) {
	      prevRef = barIt->second.getCloseValue();
	    }
	  }
      }

    if (barAgg.empty())
      return {};

    // Step 2: Convert per-bar aggregates into portfolio bar returns and compound by month.
    std::map<YearMonth, Decimal> monthMult; // ym -> multiplier

    YearMonth firstYM{0,0}, lastYM{0,0};
    bool ymInitialized = false;

    for (const auto& kv : barAgg)
      {
	const ptime& ts = kv.first;
	const Decimal sumPnL      = kv.second.first;
	const Decimal sumExposure = kv.second.second;

	if (sumExposure == zero)
	  continue;

	const Decimal r_portfolio = sumPnL / sumExposure;  // portfolio bar return

	date d = ts.date();
	YearMonth ym{ static_cast<int>(d.year()), static_cast<int>(d.month()) };

	if (!ymInitialized) {
	  firstYM = ym;
	  lastYM  = ym;
	  ymInitialized = true;
	} else {
	  lastYM = ym;
	}

	auto itM = monthMult.find(ym);
	if (itM == monthMult.end())
	  itM = monthMult.emplace(ym, one).first;

	itM->second *= (one + r_portfolio);
      }

    // Optional: include flat months (0 return) between first and last month
    if (includeFlatMonths && ymInitialized) {
      YearMonth cur = firstYM;
      while (!(cur == lastYM)) {
	if (monthMult.find(cur) == monthMult.end())
	  monthMult.emplace(cur, one); // multiplier=1 => return 0
	cur = cur.nextMonth(); // assumes YearMonth has nextMonth()
      }
      if (monthMult.find(lastYM) == monthMult.end())
	monthMult.emplace(lastYM, one);
    }

    // Step 3: Emit monthly returns in chronological order.
    std::vector<Decimal> monthly;
    monthly.reserve(monthMult.size());
    for (const auto& kv : monthMult) {
      monthly.push_back(kv.second - one);
    }

    return monthly;
  }
} // namespace mkc_timeseries
