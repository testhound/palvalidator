// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __PORTFOLIO_H
#define __PORTFOLIO_H 1

#include <string>
#include <memory>
#include <map>
#include "Security.h"

using std::string;
using std::map;
using std::shared_ptr;

namespace mkc_timeseries
{
  class PortfolioException : public std::runtime_error
  {
  public:
  PortfolioException(const std::string msg) 
    : std::runtime_error(msg)
      {}

    ~PortfolioException()
      {}

  };

  template <class Decimal>
  class Portfolio
    {
    public:
      typedef shared_ptr<Security<Decimal>> SecurityPtr;
      typedef typename map<string, 
			   SecurityPtr>::const_iterator ConstPortfolioIterator;

    public:
      explicit Portfolio (const std::string& portfolioName)
	: mPortfolioName (portfolioName),
	  mPortfolioSecurities()
      {}

      Portfolio (const Portfolio<Decimal> &rhs)
	: mPortfolioName (rhs.mPortfolioName),
	  mPortfolioSecurities(rhs.mPortfolioSecurities)
      {}

      ~Portfolio()
      {}

      Portfolio<Decimal>& 
      operator=(const Portfolio<Decimal> &rhs)
      {
	if (this == &rhs)
	  return *this;

	mPortfolioName = rhs.mPortfolioName;
	mPortfolioSecurities = rhs.mPortfolioSecurities;

	return *this;
      }

      std::shared_ptr<Portfolio<Decimal>> clone() const
      {
	return std::make_shared<Portfolio<Decimal>>(getPortfolioName());
      }

      const std::string& getPortfolioName() const
      {
	return mPortfolioName;
      }

      unsigned int getNumSecurities() const
      {
	return  mPortfolioSecurities.size();
      }

      ConstPortfolioIterator beginPortfolio() const
      {
	return mPortfolioSecurities.begin();
      }

      ConstPortfolioIterator endPortfolio() const
      {
	return mPortfolioSecurities.end();
      }

      void addSecurity (std::shared_ptr<Security<Decimal>> security)
      {
	ConstPortfolioIterator pos = 
	  mPortfolioSecurities.find(security->getSymbol()) ;

	if (pos == endPortfolio())
	  {
	    mPortfolioSecurities.insert(std::make_pair(security->getSymbol(),
						       security));
	  }
	else
	  throw PortfolioException ("addSecurity - security " +security->getSymbol() + " already exists in portfolio");
      }

      ConstPortfolioIterator
      findSecurity (const std::string& tradingSymbol) const
	{
	  return mPortfolioSecurities.find (tradingSymbol);
	}

    private:
      std::string mPortfolioName;
      std::map<std::string, SecurityPtr> mPortfolioSecurities;
    };
}

#endif
