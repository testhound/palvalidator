#include "PatternHashGenerator.h"
#include <functional>

namespace {
    // A simple hash_combine function
    template <class T>
    inline void hash_combine(std::size_t& seed, const T& v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }
}

namespace pattern_universe {

unsigned long long PatternHashGenerator::generatePatternHash(const PatternTemplate& pattern) {
    std::size_t seed = 0;
    for (const auto& condition : pattern.getConditions()) {
        hash_combine(seed, static_cast<int>(condition.getLhs().getComponentType()));
        hash_combine(seed, condition.getLhs().getBarOffset());
        hash_combine(seed, static_cast<int>(condition.getOperator()));
        hash_combine(seed, static_cast<int>(condition.getRhs().getComponentType()));
        hash_combine(seed, condition.getRhs().getBarOffset());
    }
    return seed;
}

} // namespace pattern_universe