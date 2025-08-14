#pragma once

#include "PatternTemplate.h"
#include <memory>

namespace pattern_universe {

class PatternHashGenerator {
public:
    static unsigned long long generatePatternHash(const PatternTemplate& pattern);
};

} // namespace pattern_universe