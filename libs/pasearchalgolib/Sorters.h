#ifndef SORTERS_H
#define SORTERS_H

#include "UniqueSinglePAMatrix.h"

namespace mkc_searchalgo
{
  template <class Decimal>
  struct ResultStat
  {
    ResultStat(){}
    ResultStat(Decimal pf, Decimal po, Decimal pp, Decimal wp, unsigned int trd, unsigned int mxl):
      ProfitFactor(pf),
      PayoffRatio(po),
      PALProfitability(pp),
      WinPercent(wp),
      Trades(trd),
      MaxLosers(mxl)
    {}

    Decimal ProfitFactor;
    Decimal PayoffRatio;
    Decimal PALProfitability;
    Decimal WinPercent;
    unsigned int Trades;
    unsigned int MaxLosers;
  };


  struct Sorters
  {
    ///
    /// Sorts descending on Trade-Weighted Profit Factor (TWPF)
    /// (so as to keep the more active strategies for subsequent rounds)
    /// then ascending on unique id (for no collision)
    ///
    template <class Decimal>
    struct TwpfSorter
    {
      bool static sort(const std::tuple<ResultStat<Decimal>, unsigned int, int> & lhs, const std::tuple<Decimal, unsigned int, int>& rhs)
      {
        Decimal pf1 = std::get<0>(lhs).ProfitFactor;
        Decimal pf2 = std::get<0>(rhs).ProfitFactor;

        if (pf1 > DecimalConstants<Decimal>::DecimalOne && pf2 < DecimalConstants<Decimal>::DecimalOne)
          return true;
        if (pf1 < DecimalConstants<Decimal>::DecimalOne && pf2 > DecimalConstants<Decimal>::DecimalOne)
          return false;

        Decimal factor1 = pf1 * Decimal(std::get<1>(lhs));
        Decimal factor2 = pf2 * Decimal(std::get<1>(rhs));
        if (factor1 > factor2)
          return true;
        if (factor1 < factor2)
          return false;
        //when equal
        return std::get<2>(lhs) < std::get<2>(rhs);
      }
    };

    /// Simple Profit factor sorting Desc
    template <class Decimal>
    struct PfSorter
    {
      bool static sort(const std::tuple<ResultStat<Decimal>, unsigned int, int> & lhs, const std::tuple<Decimal, unsigned int, int>& rhs)
      {
        Decimal pf1 = std::get<0>(lhs).ProfitFactor;
        Decimal pf2 = std::get<0>(rhs).ProfitFactor;
        if (pf1 > pf2)
          return true;
        if (pf1 < pf2)
          return false;
        //when profit factors equal
        unsigned int trades1 = std::get<1>(lhs);
        unsigned int trades2 = std::get<1>(rhs);
        if (trades1 > trades2)
          return true;
        if (trades1 < trades2)
          return false;
        //when trades also equal use unique id to sort with stability
        return std::get<2>(lhs) < std::get<2>(rhs);
      }
    };

    ///
    /// Sorting on a combined factor of PAL Profitability and trades, then Payoff ratio
    /// with a multiplier to weight PF more proportionally to trades
    /// this sorter needs arguments, first an average, second a multiplier
    ///
    template <class Decimal>
    class CombinationPPSorter
    {
    public:
      CombinationPPSorter(Decimal ratio, Decimal multiplier):
        mMultiplier(ratio * multiplier)
      {}

      bool operator()(const std::tuple<ResultStat<Decimal>, unsigned int, int> & lhs, const std::tuple<ResultStat<Decimal>, unsigned int, int>& rhs)
      {
        const ResultStat<Decimal>& p1 = std::get<0>(lhs);
        const ResultStat<Decimal>& p2 = std::get<0>(rhs);

        unsigned int trades1 = std::get<1>(lhs);
        unsigned int trades2 = std::get<1>(rhs);

        Decimal factor1 = p1.PALProfitability * mMultiplier + Decimal(trades1);
        Decimal factor2 = p2.PALProfitability * mMultiplier + Decimal(trades2);
        if (factor1 > factor2)
          return true;
        if (factor1 < factor2)
          return false;
        //then by payoff
        if (p1.PayoffRatio > p2.PayoffRatio)
          return true;
        if (p1.PayoffRatio < p2.PayoffRatio)
          return false;
        //when factors also equal use unique id to sort with stability
        return std::get<2>(lhs) < std::get<2>(rhs);
      }
    private:
      Decimal mMultiplier;
    };

    ///
    /// Simple sorting based on PALProfitability
    ///
    template <class Decimal>
    struct PALProfitabilitySorter
    {
      bool static sort(const std::tuple<ResultStat<Decimal>, unsigned int, int> & lhs, const std::tuple<ResultStat<Decimal>, unsigned int, int>& rhs)
      {
        const ResultStat<Decimal>& p1 = std::get<0>(lhs);
        const ResultStat<Decimal>& p2 = std::get<0>(rhs);
        return (p1.PALProfitability > p2.PALProfitability);

      }
    };




  };

}

#endif // SORTERS_H
