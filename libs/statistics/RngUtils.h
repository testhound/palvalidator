#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <random>
#include <limits>
#include <array>

namespace mkc_timeseries
{
  namespace rng_utils
  {

    // --- Detection: does Rng have .engine()? (e.g., randutils::mt19937_rng) ---
    template <typename T, typename = void>
    struct has_engine_method : std::false_type {};

    template <typename T>
    struct has_engine_method<T, std::void_t<decltype(std::declval<T&>().engine())>> : std::true_type {};

    // Return a reference to the underlying engine, whether wrapped or direct.
    template <typename Rng>
    inline auto& get_engine(Rng& rng)
    {
      if constexpr (has_engine_method<Rng>::value)
	return rng.engine();     // e.g., randutils::mt19937_rng::engine()
      else
	return rng;              // e.g., std::mt19937_64
    }

    // Pull a raw 64-bit value from the engine (when you truly need an integer).
    template <typename Rng>
    inline std::uint64_t get_random_value(Rng& rng)
    {
      if constexpr (has_engine_method<Rng>::value) 
	return static_cast<std::uint64_t>(rng.engine()());
      else
	return static_cast<std::uint64_t>(rng());
    }

    /**
     * @brief Get a random index in [0, hiExclusive).
     *
     * Uses std::uniform_int_distribution on the actual engine to avoid modulo bias
     * and to behave correctly for both 32-bit and 64-bit engines.
     *
     * @pre hiExclusive > 0
     */
    template <typename Rng>
    inline std::size_t get_random_index(Rng& rng, std::size_t hiExclusive)
    {
      // Precondition guard (no-throw fallback): if 0, return 0.
      if (hiExclusive == 0)
	return 0;

      std::uniform_int_distribution<std::size_t> dist(0, hiExclusive - 1);
      return dist(get_engine(rng));
    }

    /**
     * @brief Get a random double in [0, 1).
     *
     * Uses std::uniform_real_distribution on the actual engine, ensuring correct
     * behavior regardless of engine word size and avoiding ad-hoc integer scaling.
     */
    template <typename Rng>
    inline double get_random_uniform_01(Rng& rng)
    {
      static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
      return dist(get_engine(rng));
    }

    /**
     * @brief Bernoulli(p) using the engine-backed uniform.
     *
     * @param p Probability of true in [0,1]. Values outside are clamped.
     */
    template <typename Rng>
    inline bool bernoulli(Rng& rng, double p)
    {
      if (p <= 0.0)
	return false;
      if (p >= 1.0)
	return true;
      return get_random_uniform_01(rng) < p;
    }

    // Simple 64-bit splitmix hash (deterministic, good avalanche)
    inline uint64_t splitmix64(uint64_t x) {
      x += 0x9e3779b97f4a7c15ull;
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
      return x ^ (x >> 31);
    }

    // Combine several 64-bit values into one seed
    inline uint64_t hash_combine64(std::initializer_list<uint64_t> parts) {
      uint64_t h = 0x6a09e667f3bcc909ull; // arbitrary IV
      for (auto v : parts) h = splitmix64(h ^ v);
      return h;
    }

    // A stable, immutable key describing "what this randomness is for".
    class CommonRandomNumberKey {
    public:
      // Constructor with sensible defaults for optional dimensions.
      // - Lvalue defaults to 0 when not applicable
      // - replicate defaults to 0 (caller typically sets this per replicate)
      // - fold defaults to 0 when not using CV folds or time splits
      CommonRandomNumberKey(uint64_t masterSeed,
			    uint64_t strategyId,
			    uint64_t stageTag,
			    uint64_t Lvalue    = 0,
			    uint64_t replicate = 0,
			    uint64_t fold      = 0) noexcept
	: m_masterSeed(masterSeed)
	, m_strategyId(strategyId)
	, m_stageTag(stageTag)
	, m_Lvalue(Lvalue)
	, m_replicate(replicate)
	, m_fold(fold)
      {}

      // Getters (no setters to preserve immutability)
      uint64_t masterSeed() const noexcept { return m_masterSeed; }
      uint64_t strategyId() const noexcept { return m_strategyId; }
      uint64_t stageTag()   const noexcept { return m_stageTag; }
      uint64_t Lvalue()     const noexcept { return m_Lvalue; }
      uint64_t replicate()  const noexcept { return m_replicate; }
      uint64_t fold()       const noexcept { return m_fold; }

    private:
      uint64_t m_masterSeed;
      uint64_t m_strategyId;
      uint64_t m_stageTag;
      uint64_t m_Lvalue;
      uint64_t m_replicate;
      uint64_t m_fold;
    };

    // Derive a 64-bit seed for this replicate (stable across threads/runs)
    inline uint64_t make_seed(const CommonRandomNumberKey& k) {
      return hash_combine64({
	  k.masterSeed(), k.strategyId(), k.stageTag(),
	  k.Lvalue(), k.replicate(), k.fold()
	});
    }

    // Build a seed_seq for engines that take it
    inline std::seed_seq make_seed_seq(uint64_t seed64) {
      // expand to multiple 32-bit words to better fill state
      std::array<uint32_t, 4> words = {
	static_cast<uint32_t>(seed64),
	static_cast<uint32_t>(seed64 >> 32),
	static_cast<uint32_t>(splitmix64(seed64)),
	static_cast<uint32_t>(splitmix64(seed64 ^ 0x9e3779b97f4a7c15ull))
      };
      return std::seed_seq(words.begin(), words.end());
    }

    // Helper: construct a seeded engine regardless of API style
    template<class Eng>
    inline Eng construct_seeded_engine(std::seed_seq& sseq) {
      if constexpr (std::is_constructible_v<Eng, std::seed_seq&>) {
	return Eng(sseq);            // e.g., std::mt19937_64(ss)
      } else {
	Eng e; e.seed(sseq);         // e.g., randutils::mt19937_rng.seed(ss)
	return e;
      }
    }

    // Common-Random-Numbers RNG provider (factory-style), generic over engine type.
    template<class Eng = std::mt19937_64>
    class CRNRng {
    public:
      using Engine = Eng;
      using Key    = CommonRandomNumberKey;

      // Constructor with sensible defaults for optional dimensions.
      explicit CRNRng(uint64_t masterSeed,
		      uint64_t strategyId,
		      uint64_t stageTag,
		      uint64_t Lvalue = 0,
		      uint64_t fold   = 0) noexcept
	: m_masterSeed(masterSeed)
	, m_strategyId(strategyId)
	, m_stageTag(stageTag)
	, m_Lvalue(Lvalue)
	, m_fold(fold)
      {}

      // Fluent variants for tweaking L / fold across loops
      CRNRng with_L(uint64_t Lvalue) const noexcept {
	CRNRng cpy = *this; cpy.m_Lvalue = Lvalue; return cpy;
      }
      CRNRng with_fold(uint64_t fold) const noexcept {
	CRNRng cpy = *this; cpy.m_fold = fold; return cpy;
      }

      // Make a fresh engine for replicate k (deterministic, order/parallel-safe)
      Engine make_engine(std::size_t replicate) const {
	Key key(m_masterSeed, m_strategyId, m_stageTag,
		m_Lvalue, static_cast<uint64_t>(replicate), m_fold);
	auto seed64 = make_seed(key);
	auto sseq   = make_seed_seq(seed64);
	return construct_seeded_engine<Engine>(sseq);
      }

      // Accessors (for logging/debug)
      uint64_t masterSeed() const noexcept { return m_masterSeed; }
      uint64_t strategyId() const noexcept { return m_strategyId; }
      uint64_t stageTag()   const noexcept { return m_stageTag; }
      uint64_t Lvalue()     const noexcept { return m_Lvalue; }
      uint64_t fold()       const noexcept { return m_fold; }

    private:
      uint64_t m_masterSeed, m_strategyId, m_stageTag, m_Lvalue, m_fold;
    };
  } // namespace rng_utils
} // namespace mkc_timeseries
