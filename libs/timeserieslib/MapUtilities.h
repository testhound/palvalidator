// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef MAP_UTILITIES_H
#define MAP_UTILITIES_H 1

#include <map>
#include <algorithm>

namespace mkc_timeseries
{
  template< typename tPair >
  struct second_t 
  {
    typename tPair::second_type operator()( const tPair& p ) const 
    { 
      return p.second; 
    }
  };

  template<typename tMap> 
  second_t<typename tMap::value_type> second(const tMap& m) 
  { 
    return second_t<typename tMap::value_type>(); 
  }
}
#endif

