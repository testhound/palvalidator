#ifndef __SECURITY_H
#define __SECURITY_H 1

#include <string>
#include <list>
#include <fstream>
#include <memory>
#include "number.h"
#include "TSApiDataFile.h"
#include "TSApiConfiguration.h"

using dec::decimal4;
using dec::decimal5;

class Security
{
public:
  Security (const std::string& securitySymbol) : mSecurityName (securitySymbol)
  {}

  virtual ~Security() {}
  virtual const decimal4& getBigPointValue() const = 0;
  virtual const decimal5& getTick() const = 0;
  virtual bool isFuturesSymbol() const = 0;
  virtual bool isEquitiesSymbol() const = 0;
  const std::string& getSymbol() const { return mSecurityName; }

private:
  std::string mSecurityName;
};


class EquitySecurity : public Security
{
 public:
  EquitySecurity(const std::string& securitySymbol) 
    : Security (securitySymbol),
    mValueOfOne (1.0),
    mValueOfEquityTick (0.01)
  {}

  ~EquitySecurity() {}
  const decimal4& getBigPointValue() const { return mValueOfOne; }
  const decimal5& getTick() const { return mValueOfEquityTick; }
  bool isFuturesSymbol() const { return false; }
  bool isEquitiesSymbol() const { return true; }

 private:
  decimal4 mValueOfOne;
  decimal5 mValueOfEquityTick;
};

class FuturesSecurity : public Security
{
 public:
  FuturesSecurity(const std::string& securitySymbol, const decimal4& bigPointValue, const decimal5& tickSize) 
    : Security (securitySymbol), 
      mBigPointValue (bigPointValue), 
      mTickSize (tickSize)
  {}
  ~FuturesSecurity() {}
  const decimal4& getBigPointValue() const { return mBigPointValue; }
  const decimal5& getTick() const { return mTickSize; }
  bool isFuturesSymbol() const { return true; }
  bool isEquitiesSymbol() const { return false; }

private:
  decimal4 mBigPointValue;
  decimal5 mTickSize;
};

class PortfolioSecurity
{
 public:
  PortfolioSecurity (std::shared_ptr<Security> security, std::shared_ptr<TSApiDataFile> dataFile, 
		     std::shared_ptr<TSApiConfiguration> configuration)
    : mSecurity(security),
      mDataFile(dataFile),
      mConfiguration (configuration)
  {}

  ~PortfolioSecurity()
  {}

  const decimal4& getBigPointValue() const { return mSecurity->getBigPointValue(); }
  const decimal5& getTick() const { return mSecurity->getTick(); }
  const std::string& getSymbol() const { return mSecurity->getSymbol(); }
  const std::string& getDataFileName() const { return mDataFile->getDataFileName(); }
  const std::string& getDataPath() const { return mDataFile->getDataPath(); }
  const std::string& getDateFormat() const { return mDataFile->getDateFormat(); }
  const std::string& getTableName() const { return mConfiguration->getTableName(); }
  const std::string& getDatabaseName() const { return mConfiguration->getDatabaseName(); }
  const std::string& getFieldNamesInTable() const { return mConfiguration->getFieldNamesInTable(); }
  const std::string& getDataFieldNames() const { return mConfiguration->getDataFieldNames(); }

private:
  std::shared_ptr<Security> mSecurity;
  std::shared_ptr<TSApiDataFile> mDataFile;
  std::shared_ptr<TSApiConfiguration> mConfiguration;
};

class Portfolio
{
private:
  typedef std::list<std::shared_ptr<PortfolioSecurity>> PortfolioType;

public:
  typedef PortfolioType::iterator PortfolioIterator;
  typedef PortfolioType::const_iterator ConstPortfolioIterator;

  Portfolio() : mPortfolio()
  {}

  ~Portfolio()
  {}

  unsigned int numSecuritiesInPortfolio() const
  { return mPortfolio.size(); }

  void addSecurity (std::shared_ptr<PortfolioSecurity> aSecurity)
  { mPortfolio.push_back (aSecurity); }

  PortfolioIterator begin()
  { return mPortfolio.begin(); }

  PortfolioIterator end()
  { return mPortfolio.end(); }

  ConstPortfolioIterator begin() const
  { return mPortfolio.begin(); }

  ConstPortfolioIterator end() const
  { return mPortfolio.end(); }

private:
  PortfolioType mPortfolio;
};

class PortfolioReader
{
public:
  PortfolioReader (const std::string& fileName);
  ~PortfolioReader();

  void readPortfolio();
  std::shared_ptr<Portfolio> getPortfolio() const;

private:
  std::shared_ptr<Portfolio> mSecurityPortfolio;
  std::ifstream mFile;
};

#endif
