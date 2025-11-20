// RegimeMixUtils.h
// Shared utilities for regime mix analysis used by both individual strategy
// filtering (RegimeMixStage) and meta-strategy analysis (MetaStrategyAnalyzer)

#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <ostream>
#include "RegimeMixStress.h"
#include "RegimeLabeler.h"

namespace palvalidator
{
  namespace filtering
  {
    namespace regime_mix_utils
    {
      
      /**
       * @brief Compute long-run mix weights from baseline returns using volatility terciles
       * @param baselineReturns Vector of returns (typically in-sample ROC)
       * @param volWindow Rolling window size for volatility calculation
       * @param shrinkToEqual Shrinkage factor toward equal weights (0.0 = no shrinkage, 1.0 = full shrinkage to equal)
       * @return Vector of 3 weights for Low/Mid/High volatility regimes
       */
      template<typename Num>
      std::vector<double> computeLongRunMixWeights(
        const std::vector<Num>& baselineReturns,
        std::size_t volWindow,
        double shrinkToEqual)
      {
        if (baselineReturns.size() < volWindow + 2)
        {
          // Fallback: equal weights if baseline is too short
          return {1.0/3.0, 1.0/3.0, 1.0/3.0};
        }

        palvalidator::analysis::VolTercileLabeler<Num> labeler(volWindow);
        const std::vector<int> z = labeler.computeLabels(baselineReturns);

        std::array<double, 3> cnt{0.0, 0.0, 0.0};
        for (int zi : z)
        {
          if (zi >= 0 && zi <= 2) cnt[static_cast<std::size_t>(zi)] += 1.0;
        }
        const double n = std::max(1.0, cnt[0] + cnt[1] + cnt[2]);
        std::array<double, 3> p{cnt[0]/n, cnt[1]/n, cnt[2]/n};

        // Shrink toward equal to avoid over-committing
        const double lam = std::clamp(shrinkToEqual, 0.0, 1.0);
        std::array<double, 3> w{
          (1.0 - lam) * p[0] + lam * (1.0/3.0),
          (1.0 - lam) * p[1] + lam * (1.0/3.0),
          (1.0 - lam) * p[2] + lam * (1.0/3.0)
        };

        // Clip tiny buckets and renormalize
        const double eps = 0.02; // min 2% mass per bucket
        for (double& v : w) v = std::max(v, eps);
        const double s = w[0] + w[1] + w[2];
        return {w[0]/s, w[1]/s, w[2]/s};
      }

      /**
       * @brief Adapt regime mixes to only the regimes actually present in the data
       * @param tradeLabels Vector of regime labels (0=Low, 1=Mid, 2=High) for each trade
       * @param mixesIn Input mixes with 3-element weight vectors
       * @param labelsOut Output compacted labels (0..Sobs-1 where Sobs is number of observed regimes)
       * @param mixesOut Output adapted mixes with weights renormalized to observed regimes
       * @param os Output stream for logging
       * @return true if adaptation succeeded (at least 2 regimes present), false otherwise
       */
      inline bool adaptMixesToPresentRegimes(
        const std::vector<int>& tradeLabels,
        const std::vector<palvalidator::analysis::RegimeMix>& mixesIn,
        std::vector<int>& labelsOut,
        std::vector<palvalidator::analysis::RegimeMix>& mixesOut,
        std::ostream& os)
      {
        // 1) Detect which of {0,1,2} appear and build old→new id map
        std::array<int, 3> present{0, 0, 0};
        for (int z : tradeLabels)
        {
          if (0 <= z && z <= 2)
          {
            present[static_cast<std::size_t>(z)] = 1;
          }
        }

        std::array<int, 3> old2new{-1, -1, -1};
        int next = 0;
        for (int s = 0; s < 3; ++s)
        {
          if (present[static_cast<std::size_t>(s)] == 1)
          {
            old2new[static_cast<std::size_t>(s)] = next++;
          }
        }
        const int Sobs = next;

        // If fewer than 2 regimes present, the stress is uninformative → skip (non-gating)
        if (Sobs < 2)
        {
          os << "   [RegimeMix] Skipped (only " << Sobs
             << " regime present; mix stress uninformative).\n";
          return false;
        }

        // 2) Remap labels to compact 0..Sobs-1
        labelsOut.clear();
        labelsOut.reserve(tradeLabels.size());
        for (int z : tradeLabels)
        {
          if (!(0 <= z && z <= 2))
          {
            os << "   [RegimeMix] Skipped (unexpected label " << z << ").\n";
            return false;
          }
          const int m = old2new[static_cast<std::size_t>(z)];
          if (m < 0)
          {
            os << "   [RegimeMix] Skipped (label remap failed).\n";
            return false;
          }
          labelsOut.push_back(m);
        }

        // 3) Adapt each mix's 3 weights to observed regimes and renormalize
        mixesOut.clear();
        mixesOut.reserve(mixesIn.size());

        for (const auto& mx : mixesIn)
        {
          const std::string& nm = mx.name();
          const std::vector<double>& w3 = mx.weights();

          std::vector<double> wS(static_cast<std::size_t>(Sobs), 0.0);
          double sum = 0.0;

          for (int old = 0; old < 3; ++old)
          {
            const int nw = old2new[static_cast<std::size_t>(old)];
            if (nw >= 0)
            {
              const double w = (old < static_cast<int>(w3.size())) ? w3[static_cast<std::size_t>(old)] : 0.0;
              wS[static_cast<std::size_t>(nw)] += w;
              sum += w;
            }
          }

          if (sum <= 0.0)
          {
            // Fallback to equal within observed regimes
            const double eq = 1.0 / static_cast<double>(Sobs);
            std::fill(wS.begin(), wS.end(), eq);
          }
          else
          {
            for (double& v : wS) v /= sum;
          }

          mixesOut.emplace_back(nm, wS);
        }

        return true;
      }

    } // namespace regime_mix_utils
  } // namespace filtering
} // namespace palvalidator