#pragma once
#include "randutils.hpp"

template <class T> class TradingBootstrapFactory;

namespace palvalidator::bootstrap_cfg
{
  using BootstrapEngine  = randutils::mt19937_rng;
  using BootstrapFactory = TradingBootstrapFactory<BootstrapEngine>;
}
