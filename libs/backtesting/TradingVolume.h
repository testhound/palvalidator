// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef TRADING_VOLUME_H
#define TRADING_VOLUME_H 1

#include <memory>
#include <exception>
#include <utility>

namespace mkc_timeseries
{
  typedef unsigned long long volume_t;

  class TradingVolume
  {
  public:
    enum VolumeUnit {SHARES, CONTRACTS};
    static std::shared_ptr<TradingVolume> ZeroShares;
    static std::shared_ptr<TradingVolume> ZeroContracts;

    TradingVolume (volume_t volume, TradingVolume::VolumeUnit units) :
      mVolume(volume), mVolumeUnits(units)
    {}

    // Copy constructor
    TradingVolume (const TradingVolume& rhs)
      : mVolume(rhs.mVolume),
	mVolumeUnits(rhs.mVolumeUnits)
    {}

    // Move constructor
    TradingVolume (TradingVolume&& rhs) noexcept
      : mVolume(rhs.mVolume),
	mVolumeUnits(rhs.mVolumeUnits)
    {}

    // Copy assignment operator
    TradingVolume&
    operator=(const TradingVolume &rhs)
    {
      if (this == &rhs)
	return *this;

      mVolume = rhs.mVolume;
      mVolumeUnits = rhs.mVolumeUnits;
      return *this;
    }

    // Move assignment operator
    TradingVolume&
    operator=(TradingVolume&& rhs) noexcept
    {
      if (this == &rhs)
	return *this;

      mVolume = rhs.mVolume;
      mVolumeUnits = rhs.mVolumeUnits;
      
      return *this;
    }

    // Destructor
    ~TradingVolume() = default;

    volume_t getTradingVolume() const
    {
      return mVolume;
    }

    TradingVolume::VolumeUnit getVolumeUnits() const
    {
      return mVolumeUnits;
    }

  private:
    volume_t mVolume;
    TradingVolume::VolumeUnit mVolumeUnits;
  };

  inline bool operator< (const TradingVolume& lhs, const TradingVolume& rhs)
  {
    if (lhs.getVolumeUnits() == rhs.getVolumeUnits())
      return (lhs.getTradingVolume() < rhs.getTradingVolume());
    else
      throw std::domain_error ("Volume units do not match for comparison");
  }

  inline bool operator> (const TradingVolume& lhs, const TradingVolume& rhs){ return rhs < lhs; }
  inline bool operator<=(const TradingVolume& lhs, const TradingVolume& rhs){ return !(lhs > rhs); }
  inline bool operator>=(const TradingVolume& lhs, const TradingVolume& rhs){ return !(lhs < rhs); }

  inline bool operator==(const TradingVolume& lhs, const TradingVolume& rhs)
  {
    if (lhs.getVolumeUnits() == rhs.getVolumeUnits())
      return (lhs.getTradingVolume() == rhs.getTradingVolume());
    else
      return false;
  }
  inline bool operator!=(const TradingVolume& lhs, const TradingVolume& rhs){ return !(lhs == rhs); }
}
#endif
