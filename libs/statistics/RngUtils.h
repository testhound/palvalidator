#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <random>
#include <limits>
#include <array>
#include <initializer_list>

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

    // Domain-agnostic "key": just a master seed + immutable vector of 64-bit tags.
    // No knowledge of what those tags *mean*.
    class CRNKey
    {
    public:
      CRNKey(uint64_t masterSeed, std::vector<uint64_t> tags = {})
	: m_masterSeed(masterSeed), m_tags(std::move(tags))
      {}

      // Append one or more tags → return a new key (cheap: small vector)
      CRNKey with_tag(uint64_t tag) const
      {
	auto t = m_tags; t.push_back(tag);
	return CRNKey(m_masterSeed, std::move(t));
      }

      CRNKey with_tags(std::initializer_list<uint64_t> tags) const
      {
	auto t = m_tags; t.insert(t.end(), tags.begin(), tags.end());
	return CRNKey(m_masterSeed, std::move(t));
      }

      uint64_t masterSeed() const noexcept
      {
	return m_masterSeed;
      }

      const std::vector<uint64_t>& tags() const noexcept
      {
	return m_tags;
      }

      // Derive a 64-bit seed for a given replicate index (replicate is just another tag)
      uint64_t make_seed_for(std::size_t replicate) const
      {
	uint64_t h = m_masterSeed;
	for (auto v : m_tags) h = hash_combine64({h, v});
	h = hash_combine64({h, static_cast<uint64_t>(replicate)});
	return h;
      }

    private:
      uint64_t m_masterSeed;
      std::vector<uint64_t> m_tags;
    };

    inline std::seed_seq make_seed_seq(uint64_t seed64)
    {
      // Expand a 64-bit seed into eight 32-bit words using diversified SplitMix64
      const uint64_t s0 = seed64;
      const uint64_t s1 = splitmix64(s0);
      const uint64_t s2 = splitmix64(s0 ^ 0x9e3779b97f4a7c15ull);
      const uint64_t s3 = splitmix64(s0 + 0xd1342543de82ef95ull);
      const uint64_t s4 = splitmix64(s1 ^ 0x94d049bb133111ebull);
      const uint64_t s5 = splitmix64(s2 + 0xbf58476d1ce4e5b9ull);
      const uint64_t s6 = splitmix64(s3 ^ 0x6a09e667f3bcc909ull);
      const uint64_t s7 = splitmix64(s4 + 0x243f6a8885a308d3ull);

      // Combine several rounds to decorrelate
      const uint64_t mix = (s3 ^ s5 ^ s6 ^ s7);

      std::array<uint32_t, 8> words = {
	static_cast<uint32_t>(s0), static_cast<uint32_t>(s0 >> 32),
	static_cast<uint32_t>(s1), static_cast<uint32_t>(s1 >> 32),
	static_cast<uint32_t>(s2), static_cast<uint32_t>(s2 >> 32),
	static_cast<uint32_t>(mix), static_cast<uint32_t>(mix >> 32)
      };

      return std::seed_seq(words.begin(), words.end());
    }

    // Helper: construct a seeded engine regardless of API style
    template<class Eng>
    inline Eng construct_seeded_engine(std::seed_seq& sseq)
    {
      if constexpr (std::is_constructible_v<Eng, std::seed_seq&>)
	{
	  return Eng(sseq);            // e.g., std::mt19937_64(ss)
	}
      else
	{
	  Eng e;

	  e.seed(sseq);         // e.g., randutils::mt19937_rng.seed(ss)
	  return e;
      }
    }

    // Helper that constructs engines from a CRNKey. Engine defaults to std::mt19937_64,
    // but is fully generic. Completely domain-agnostic.
    template<class Eng = std::mt19937_64>
    class CRNEngineProvider
    {
    public:
      using Engine = Eng;

      explicit CRNEngineProvider(CRNKey key)
	:  m_key(std::move(key))
      {}

      // Bind more tags (returns a new provider with extended key)
      CRNEngineProvider with_tag(uint64_t tag) const
      {
	return CRNEngineProvider(m_key.with_tag(tag));
      }

      CRNEngineProvider with_tags(std::initializer_list<uint64_t> tags) const
      {
	return CRNEngineProvider(m_key.with_tags(tags));
      }

      // The only method BCa (or any client) needs:
      Engine make_engine(std::size_t replicate) const
      {
	auto seed64 = m_key.make_seed_for(replicate);
	auto sseq   = make_seed_seq(seed64);
	return construct_seeded_engine<Engine>(sseq);
      }

      const CRNKey& key() const noexcept
      {
	return m_key;
      }

    private:
      CRNKey m_key;
    };
    
    // Derive a 64-bit seed for a given CRNKey + replicate index
    inline uint64_t make_seed(const CRNKey& key, std::size_t replicate)
    {
      return key.make_seed_for(replicate);
    }


    // Domain-agnostic CRN RNG provider (wrapper over CRNEngineProvider + CRNKey)
    template<class Eng = std::mt19937_64>
    class CRNRng
    {
    public:
      using Engine = Eng;

      // Construct from a CRNKey (master seed + any opaque tag sequence)
      explicit CRNRng(CRNKey key)
	: m_provider(std::move(key))
      {}

      // Default copy and move constructors should work fine
      // (CRNKey is copyable/movable, CRNEngineProvider is copyable/movable)
      CRNRng(const CRNRng&) = default;
      CRNRng& operator=(const CRNRng&) = default;
      CRNRng(CRNRng&&) = default;
      CRNRng& operator=(CRNRng&&) = default;

      // Fluent methods to extend the tag sequence (return a new CRNRng)
      CRNRng with_tag(uint64_t tag) const
      {
	return CRNRng(m_provider.key().with_tag(tag));
      }
 
      CRNRng with_tags(std::initializer_list<uint64_t> tags) const
      {
	return CRNRng(m_provider.key().with_tags(tags));
      }

      // Produce a fresh, deterministically seeded engine for replicate k
      Engine make_engine(std::size_t replicate) const
      {
	return m_provider.make_engine(replicate);
      }

      // Access underlying key if you want to log/debug
      const CRNKey& key() const noexcept
      {
	return m_provider.key();
      }

    private:
      CRNEngineProvider<Engine> m_provider;
    };
  } // namespace rng_utils
} // namespace mkc_timeseries
