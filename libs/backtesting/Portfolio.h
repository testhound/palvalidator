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

  /**
   * @class Portfolio
   * @brief A named container for managing a collection of securities.
   *
   * The Portfolio holds shared pointers to Security<Decimal> instances,
   * keyed by their trading symbol, and enforces unique symbols.
   * It provides lookup and iteration capabilities for backtesting strategies.
   *
   * @tparam Decimal Numeric type used for price and quantity calculations.
   */
  template <class Decimal>
  class Portfolio
    {
    public:
      typedef shared_ptr<Security<Decimal>> SecurityPtr;
      typedef typename map<string, 
			   SecurityPtr>::const_iterator ConstPortfolioIterator;

    public:
      /**
       * @brief Construct a new, empty Portfolio.
       * @param portfolioName Name identifier for this portfolio.
       */
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

      /**
       * @brief Create a clone of this Portfolio (empty, same name).
       *
       * Used for thread-safe backtesting where each thread populates its own copy.
       * @return Shared pointer to the new, empty Portfolio.
       */
      std::shared_ptr<Portfolio<Decimal>> clone() const
      {
	return std::make_shared<Portfolio<Decimal>>(getPortfolioName());
      }

      /**
       * @brief Get the portfolio's name.
       * @return Constant reference to the portfolio name string.
       */
      const std::string& getPortfolioName() const
      {
	return mPortfolioName;
      }

      /**
       * @brief Get the number of securities in the portfolio.
       * @return Count of securities.
       */
      unsigned int getNumSecurities() const
      {
	return  mPortfolioSecurities.size();
      }

      /**
       * @brief Get iterator to the first security in the portfolio.
       * @return Const iterator pointing to the first element.
       */
      ConstPortfolioIterator beginPortfolio() const
      {
	return mPortfolioSecurities.begin();
      }

      /**
       * @brief Get iterator past the last security in the portfolio.
       * @return Const iterator pointing past the last element.
       */
      ConstPortfolioIterator endPortfolio() const
      {
	return mPortfolioSecurities.end();
      }

      /**
       * @brief Add a new security to the portfolio.
       * @param security Shared pointer to the Security to add.
       * @throws PortfolioException if a security with the same symbol already exists.
       */
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

      /**
       * @brief Find a security by its trading symbol.
       * @param tradingSymbol The ticker symbol to look up.
       * @return Iterator pointing to the found security, or endPortfolio() if not found.
       */
      ConstPortfolioIterator
      findSecurity (const std::string& tradingSymbol) const
      {
	return mPortfolioSecurities.find (tradingSymbol);
      }
      
      /**
       * @brief Remove a security by symbol if it exists. No-op if absent.
       */
      void removeSecurity(const std::string& tradingSymbol)
      {
	mPortfolioSecurities.erase(tradingSymbol);
      }

      /**
       * @brief Replace (insert-or-assign) the security stored under its symbol.
       * If a security with the same symbol exists, it is overwritten in-place.
       * If absent, it is inserted.
       */
      void replaceSecurity(std::shared_ptr<Security<Decimal>> security)
      {
	auto sym = security->getSymbol();

	mPortfolioSecurities.insert_or_assign(sym, std::move(security));
      }

      /**
       * @brief Convenience: replace by symbol explicitly.
       */
      void replaceSecurity(const std::string& tradingSymbol,
			   std::shared_ptr<Security<Decimal>> security)
      {
	mPortfolioSecurities.insert_or_assign(tradingSymbol, std::move(security));
      }
      
    private:
      std::string mPortfolioName;
      std::map<std::string, SecurityPtr> mPortfolioSecurities;
    };
}

#endif
