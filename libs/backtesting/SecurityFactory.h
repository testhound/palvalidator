// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SECURITY_FACTORY_H
#define __SECURITY_FACTORY_H 1

#include <string>
#include <memory>
#include "SecurityAttributes.h"
#include "SecurityAttributesFactory.h"
#include "Security.h"


using dec::decimal;
using std::string;

namespace mkc_timeseries
{
  template <class Decimal>
  class SecurityFactory
    {
    public:
      static std::shared_ptr<Security<Decimal>>
      createSecurity (const std::string& symbolName, 
		      std::shared_ptr<OHLCTimeSeries<Decimal>> aTimeSeries)
      {
	std::shared_ptr<SecurityAttributes<Decimal>> attributes = getSecurityAttributes<Decimal> (symbolName);

	if (attributes->isEquitySecurity())
	  {
	    if (attributes->isFund())
	      {
		return std::make_shared<EquitySecurity<Decimal>>(attributes->getSymbol(), 
							      attributes->getName(),
							      aTimeSeries);
	      }
	    else if (attributes->isCommonStock())
	      {
		return std::make_shared<EquitySecurity<Decimal>>(attributes->getSymbol(), 
							      attributes->getName(),
							      aTimeSeries);
	      }
	  else
	    throw SecurityException("SecurityFactory::createSecurity - Unknown security attribute");
	  }
	else
	  return std::make_shared<FuturesSecurity<Decimal>>(attributes->getSymbol(), 
							 attributes->getName(),
							 attributes->getBigPointValue(),
							 attributes->getTick(),
							 aTimeSeries);
      }
  };
}


#endif
