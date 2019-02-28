// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//
#ifndef __MONTE_CARLO_TEST_RESULT_POLICY_H
#define __MONTE_CARLO_TEST_RESULT_POLICY_H 1

#include <exception>
#include <string>
#include "number.h"
#include "DecimalConstants.h"
#include "BackTester.h"

namespace mkc_timeseries
{
  template <class Decimal> class DefaultTestResultPolicy
    {
    public:
      DefaultTestPolicy()
      {}

      ~DefaultTestPolicy()
