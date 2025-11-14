#pragma once
#include "randutils.hpp"
#include "TradingBootstrapFactory.h"

namespace palvalidator::bootstrap_cfg
{
  using BootstrapEngine  = randutils::mt19937_rng;
  using BootstrapFactory = TradingBootstrapFactory<BootstrapEngine>;
}
