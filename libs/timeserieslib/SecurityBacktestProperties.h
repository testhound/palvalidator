// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __SECURITY_BACKTEST_PROPERTIES_H
#define __SECURITY_BACKTEST_PROPERTIES_H 1

#include <string>
#include <map>
#include <cstdint>

namespace mkc_timeseries
{
  class SecurityBacktestProperties
  {
  public:
    SecurityBacktestProperties(const std::string& symbol)
      : mSymbol(symbol),
	mDataBarNumber(0)
    {}

    ~SecurityBacktestProperties()
    {}

    SecurityBacktestProperties(const SecurityBacktestProperties& rhs)
      : mSymbol(rhs.mSymbol),
	mDataBarNumber(rhs.mDataBarNumber)
    {}

    const SecurityBacktestProperties&
    operator=(const SecurityBacktestProperties& rhs)
    {
      if (this == &rhs)
	return *this;

      mSymbol = rhs.mSymbol;
      mDataBarNumber = rhs.mDataBarNumber;

      return *this;
    }

    const std::string& getSecuritySymbol() const
    {
      return mSymbol;
    }

    // This number starts at one for the first date the security
    // has historic data and is incremented by one by the backtester
    // for each day of historic data

    uint32_t getBacktestBarNumber() const
    {
      return mDataBarNumber;
    }

    void updateBacktestBarNumber()
    {
      mDataBarNumber++;
    }

  private:
    std::string mSymbol;
    uint32_t mDataBarNumber;
  };

  class SecurityBacktestPropertiesManagerException : public std::runtime_error
  {
  public:
  SecurityBacktestPropertiesManagerException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~SecurityBacktestPropertiesManagerException()
      {}
  };

  class SecurityBacktestPropertiesManager
  {
  public:
    SecurityBacktestPropertiesManager()
      : mSecurityProperties()
    {}

    ~SecurityBacktestPropertiesManager()
    {}

    SecurityBacktestPropertiesManager(const SecurityBacktestPropertiesManager& rhs)
      : mSecurityProperties(rhs.mSecurityProperties)
    {}

    const SecurityBacktestPropertiesManager&
    operator=(const SecurityBacktestPropertiesManager& rhs)
    {
      if (this == &rhs)
	return *this;

      mSecurityProperties = rhs.mSecurityProperties;

      return *this;
    }

    void updateBacktestBarNumber(const std::string& securitySymbol)
    {
      SecurityBacktestPropertiesManager::PropertiesIterator it = 
	mSecurityProperties.find(securitySymbol);

      if (it != mSecurityProperties.end())
	{
	  it->second->updateBacktestBarNumber();
	}
      else
	{
	  throw SecurityBacktestPropertiesManagerException("SecurityBacktestPropertiesManager::updateBacktestBarNumber - symbol " +securitySymbol +" does not exist");
	}
    }
   
    uint32_t getBacktestBarNumber(const std::string& securitySymbol) const
    {
      SecurityBacktestPropertiesManager::ConstPropertiesIterator it = 
	mSecurityProperties.find(securitySymbol);

      if (it != mSecurityProperties.end())
	{
	  return it->second->getBacktestBarNumber();
	}
      else
	{
	  throw SecurityBacktestPropertiesManagerException("SecurityBacktestPropertiesManager::getBacktestBarNumber - symbol " +securitySymbol +" does not exist");
	}
    }

    void addSecurity(const std::string& securitySymbol)
    {
      SecurityBacktestPropertiesManager::ConstPropertiesIterator it = mSecurityProperties.find(securitySymbol);

      if (it == mSecurityProperties.end())
	{
	  auto sym = std::make_shared<SecurityBacktestProperties>(securitySymbol);
	  mSecurityProperties.insert(std::make_pair(securitySymbol, sym));
	}
      else
	{
	  throw SecurityBacktestPropertiesManagerException("SecurityBacktestPropertiesManager::addSecurity - symbol " +securitySymbol +" already exists"); 
	}
    }

  private:
    typedef std::map<std::string, std::shared_ptr<SecurityBacktestProperties>>::const_iterator ConstPropertiesIterator;
    typedef std::map<std::string, std::shared_ptr<SecurityBacktestProperties>>::iterator PropertiesIterator;
    std::map<std::string, std::shared_ptr<SecurityBacktestProperties>> mSecurityProperties;
  };
}
#endif
