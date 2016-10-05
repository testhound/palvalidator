#ifndef __SECURITY_FACTORY_H
#define __SECURITY_FACTORY_H 1

#include "csv.h"
#include "decimal.h"
#include <string>
#include <map>

using dec::decimal4;
using dec::decimal5;

class SecurityBackTestConfig
{
public:
  SecurityBackTestConfig (const std::string& symbolName,
			  decimal4& bigPointValue,
			  decimal5& tickSize,
			  const std::string& dataFileName,
			  const std::string& dataFilePath,
			  bool isFuturesSymbol) 
    : mSymbolName (symbolName),
    mBigPointValue(bigPointValue),
    mTickSize(tickSize),
    mDataFileName(dataFileName),
    mDataFilePath(dataFilePath),
    mFuturesSymbol(isFuturesSymbol)
    {}
  
  ~SecurityBackTestConfig() {}

  const std::string& getSymbol() const { return mSymbolName; }
  const decimal4& getBigPointValue() const { return mBigPointValue; }
  const decimal5& getTick() const { return mTickSize; }
  const std::string& getDataPath() { return mDataFilePath; }
  const std::string& getDataFileName() { return mDataFileName; }
  bool isFuturesSymbol() const { return (mFuturesSymbol == true); }
  bool isEquitySymbol() const  { return (mFuturesSymbol == false); }

private:
  std::string mSymbolName;
  decimal4 mBigPointValue;
  decimal5 mTickSize;
  std::string mDataFileName;
  std::string mDataFilePath;
  bool mFuturesSymbol;
};

typedef std::shared_ptr<SecurityBackTestConfig> SecurityBackTestConfigPtr;

class SecurityFactory
{
public:
  static SecurityFactory* Instance();

public:
  typedef std::map<std::string, SecurityBackTestConfigPtr>::const_iterator ConstConfigIterator;
  typedef std::map<std::string, SecurityBackTestConfigPtr>::iterator ConfigIterator;

  ConstConfigIterator findSecurityConfiguration (const std::string& symbol) const;
  ConfigIterator findSecurityConfiguration (const std::string& symbol);

  ConstConfigIterator end() const;
  ConfigIterator end();


private:
 SecurityFactory();
  ~SecurityFactory()
  {}

  void addSecurityConfiguration (std::string& key, SecurityBackTestConfigPtr value);
  void readSecurityConfigurationFile();

private:
  std::map<std::string, SecurityBackTestConfigPtr> mSecurityConfigMap;
  io::CSVReader<6> mConfigurationFile;
  static SecurityFactory* mInstance;
};

#endif
