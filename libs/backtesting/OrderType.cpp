// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, February 2026
//

#include "OrderType.h"
#include <stdexcept>

namespace mkc_timeseries
{
  std::string orderTypeToString(OrderType orderType)
  {
    switch (orderType) {
      case OrderType::MARKET_ON_OPEN_LONG:
        return "MARKET_ON_OPEN_LONG";
      case OrderType::MARKET_ON_OPEN_SHORT:
        return "MARKET_ON_OPEN_SHORT";
      case OrderType::MARKET_ON_OPEN_SELL:
        return "MARKET_ON_OPEN_SELL";
      case OrderType::MARKET_ON_OPEN_COVER:
        return "MARKET_ON_OPEN_COVER";
      case OrderType::SELL_AT_LIMIT:
        return "SELL_AT_LIMIT";
      case OrderType::COVER_AT_LIMIT:
        return "COVER_AT_LIMIT";
      case OrderType::SELL_AT_STOP:
        return "SELL_AT_STOP";
      case OrderType::COVER_AT_STOP:
        return "COVER_AT_STOP";
      case OrderType::UNKNOWN:
        return "UNKNOWN";
      default:
        return "UNKNOWN";
    }
  }

  bool isEntryOrderType(OrderType orderType)
  {
    switch (orderType) {
      case OrderType::MARKET_ON_OPEN_LONG:
      case OrderType::MARKET_ON_OPEN_SHORT:
        return true;
      case OrderType::MARKET_ON_OPEN_SELL:
      case OrderType::MARKET_ON_OPEN_COVER:
      case OrderType::SELL_AT_LIMIT:
      case OrderType::COVER_AT_LIMIT:
      case OrderType::SELL_AT_STOP:
      case OrderType::COVER_AT_STOP:
      case OrderType::UNKNOWN:
        return false;
      default:
        return false;
    }
  }

  bool isExitOrderType(OrderType orderType)
  {
    switch (orderType) {
      case OrderType::MARKET_ON_OPEN_SELL:
      case OrderType::MARKET_ON_OPEN_COVER:
      case OrderType::SELL_AT_LIMIT:
      case OrderType::COVER_AT_LIMIT:
      case OrderType::SELL_AT_STOP:
      case OrderType::COVER_AT_STOP:
        return true;
      case OrderType::MARKET_ON_OPEN_LONG:
      case OrderType::MARKET_ON_OPEN_SHORT:
      case OrderType::UNKNOWN:
        return false;
      default:
        return false;
    }
  }

  void validateEntryOrderType(OrderType orderType)
  {
    if (!isEntryOrderType(orderType) && orderType != OrderType::UNKNOWN) {
      throw std::invalid_argument("Invalid entry order type: " + orderTypeToString(orderType) + 
                                  ". Entry orders must be MARKET_ON_OPEN_LONG or MARKET_ON_OPEN_SHORT.");
    }
  }

  void validateExitOrderType(OrderType orderType)
  {
    if (!isExitOrderType(orderType) && orderType != OrderType::UNKNOWN) {
      throw std::invalid_argument("Invalid exit order type: " + orderTypeToString(orderType) + 
                                  ". Exit orders must be one of the exit order types (SELL_, COVER_).");
    }
  }
}