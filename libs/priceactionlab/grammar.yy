%skeleton "lalr1.cc"
%require "3.0"
/* Write out a header file containing the token defines */
%defines
%define api.parser.class {PalParser}

%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define api.namespace {mkc_palast}

%code requires
{
    #include <iostream>
    #include <string>
    #include <stdint.h>
    #include "PalAst.h"

    using namespace std;

    namespace mkc_palast {
        class Scanner;
        class PalParseDriver;
    }
}

// Bison calls yylex() function that must be provided by us to suck tokens
// from the scanner. This block will be placed at the beginning of IMPLEMENTATION file (cpp).
// We define this function here (function! not method).
// This function is called only inside Bison, so we make it static to limit symbol visibility for the linker
// to avoid potential linking conflicts.
%code top
{
    #include <iostream>
    #include "scanner.h"
    #include "PalParser.hpp"
    #include "PalParseDriver.h"
    #include "location.hh"
    
    // yylex() arguments are defined in parser.y
    static mkc_palast::PalParser::symbol_type yylex(mkc_palast::Scanner &scanner, mkc_palast::PalParseDriver &driver) {
        return scanner.get_next_token();
    }
    
    // you can accomplish the same thing by inlining the code using preprocessor
    // x and y are same as in above static function
    // #define yylex(x, y) scanner.get_next_token()
    
    using namespace mkc_palast;
}

%language "c++"
%start program


%{
#include <cstdlib>

#include <cstdio>
%}


%token TOK_EOF 0
%token <int> TOK_INT_NUM
%token <std::string> TOK_IDENTIFIER
%token <std::string> TOK_FLOAT_NUM;
%token TOK_PLUS
%token TOK_MINUS
%token TOK_PERCENT
%token TOK_LBRACE
%token TOK_RBRACE
%token TOK_COLON
%token TOK_GREATER_THAN
%token TOK_IF
%token TOK_THEN
%token TOK_OPEN
%token TOK_HIGH
%token TOK_LOW
%token TOK_CLOSE
%token TOK_VOLUME
%token TOK_ROC1
%token TOK_IBS1
%token TOK_IBS2
%token TOK_IBS3
%token TOK_MEANDER
%token TOK_VCHARTLOW
%token TOK_VCHARTHIGH
%token TOK_OF
%token TOK_AND
%token TOK_AGO
%token TOK_BUY
%token TOK_SELL
%token TOK_NEXT
%token TOK_ON
%token TOK_THE
%token TOK_WITH
%token TOK_PROFIT
%token TOK_TARGET
%token TOK_AT
%token TOK_ENTRY
%token TOK_PRICE
%token TOK_BARS
%token TOK_BAR
%token TOK_STOP
%token TOK_LOSS
%token TOK_FILE
%token TOK_INDEX
%token TOK_DATE
%token TOK_PL
%token TOK_PS
%token TOK_TRADES
%token TOK_CL
%token TOK_VOLATILITY
%token TOK_PORTFOLIO
%token TOK_LOW_VOL
%token TOK_HIGH_VOL
%token TOK_PORT_LONG_FILTER
%token TOK_PORT_SHORT_FILTER
%token TOK_VERY_HIGH_VOL
%token TOK_NORMAL_VOL
%token TOK_MOMERSION_FILTER
%token TOK_LEFT_PAREN
%token TOK_RIGHT_PAREN

%type <std::shared_ptr<PriceActionLabPattern>> pattern;
%type <std::shared_ptr<PatternDescription>> patterndescr;
%type <std::string> filedesc;
%type <int> indexdesc;
%type <int> indexdatedesc;
%type <std::shared_ptr<decimal7>> pldesc;
%type <std::shared_ptr<decimal7>> psdesc;
%type <int> tradesdesc;
%type <int> cldesc;
%type <std::shared_ptr<PatternExpression>> conds;
%type <std::shared_ptr<PatternExpression>> ohlc_comparison;
%type <std::shared_ptr<PriceBarReference>> ohlcref;
%type <std::shared_ptr<MarketEntryExpression>> entrystmt;
%type <std::shared_ptr<ProfitTargetInPercentExpression>> profitstmt;
%type <std::shared_ptr<StopLossInPercentExpression>> stopstmt;
%type <int> integernumber;
%type <std::shared_ptr<decimal7>> number;
%type <PriceActionLabPattern::VolatilityAttribute> pattern_volatility_attr;
%type <PriceActionLabPattern::PortfolioAttribute> pattern_portfolio_filter_attr;
%type <PriceActionLabPattern::VolatilityAttribute> volatility_attr;
%type <PriceActionLabPattern::PortfolioAttribute> portfolio_attr;


%lex-param { class mkc_palast::Scanner& scanner }
%lex-param { class PalParseDriver& driver }
%parse-param { class mkc_palast::Scanner& scanner }
%parse-param { class PalParseDriver& driver }

%locations
%define parse.trace
%define parse.error verbose


%{
//#include "PalScanner.hpp"

// Remove global factory - access through driver
%}

%%

program : patterns  
          { 
     	    //printf ("Found program\n"); 
          }
;

patterns : pattern
           {
             //  printf ("Founds patterns\n");
             driver.addPalPattern ($1);
      	   }
         | patterns pattern
      	   {
		//printf ("Founds recursive patterns\n");
         	driver.addPalPattern ($2);
      	   }
;

pattern : patterndescr TOK_IF pattern_volatility_attr pattern_portfolio_filter_attr conds TOK_THEN entrystmt TOK_WITH profitstmt TOK_AND stopstmt
      {
      	//printf ("Found pattern\n");
	// Create pattern with shared_ptr objects - proper ownership management
	auto resourceManager = driver.getResourceManagerPtr();
	auto pattern = resourceManager->createPattern(
	    $1, // Already shared_ptr<PatternDescription>
	    $5, // Already shared_ptr<PatternExpression>
	    $7, $9, $11, $3, $4  // These are already shared_ptr
	);
	$$ = pattern; // Return shared_ptr directly
	
	// NOTE: Do NOT add pattern to driver here - it will be added by the patterns rule
      }
;

patterndescr : TOK_LBRACE filedesc indexdesc indexdatedesc pldesc psdesc tradesdesc cldesc TOK_RBRACE
               {
      	       	 //printf ("Found pattern description\n");
		 // $5 and $6 are now shared_ptr<decimal7> objects directly from resource manager
		 $$ = std::make_shared<PatternDescription>((char *) $2.c_str(), $3, $4, $5, $6, $7, $8);
      	       }
;

filedesc : TOK_FILE TOK_COLON TOK_IDENTIFIER  
          { 
            $$ = $3; 
          }
;

indexdesc : TOK_INDEX TOK_COLON integernumber 
         { 
	   $$ = $3; 
         }
;

indexdatedesc : TOK_INDEX TOK_DATE TOK_COLON integernumber 
	      { 
	      	$$ =  $4; 
	      }
;

pldesc : TOK_PL TOK_COLON number TOK_PERCENT
         {
	   //printf ("Found nonterminal PL: %f\n", n->getAsDouble ());
       	   $$ = $3;
     	 }
       | TOK_PL TOK_COLON integernumber TOK_PERCENT
       	 {
    auto resourceManager = driver.getResourceManagerPtr();
    $$ = resourceManager->getDecimalNumber ($3);
  }
;

psdesc : TOK_PS TOK_COLON number TOK_PERCENT
       	 {
	   //printf ("Found nonterminal PS: %f\n", n->getAsDouble ());
       	   $$ = $3;
     	 }
       | TOK_PS TOK_COLON integernumber TOK_PERCENT
       	 {
    auto resourceManager = driver.getResourceManagerPtr();
    $$ = resourceManager->getDecimalNumber ($3);
  }
;

tradesdesc : TOK_TRADES TOK_COLON integernumber
	     { 
	       $$ = $3; 
	     }
;

cldesc :  TOK_CL TOK_COLON integernumber
       	  { 
	    $$ = $3; 
	  }
      |   TOK_CL TOK_COLON TOK_MINUS 
      	  { 
	    $$ = 1; 
	  }
;

conds : ohlc_comparison
      	{
	  //printf ("Found comparison\n");
          $$ = $1;
        }
      | conds TOK_AND ohlc_comparison
      	{
   //printf ("Found recursive comparison\n");
   auto lhsExpr = $1; // Already shared_ptr<PatternExpression>
   auto rhsExpr = $3; // Already shared_ptr<PatternExpression>
       	  $$ = std::make_shared<AndExpr>(lhsExpr, rhsExpr);
      	}
;

ohlc_comparison : ohlcref TOK_GREATER_THAN ohlcref
      		  {
		    //printf ("Found greater than ohlc comparison \n");
        	    $$ = std::make_shared<GreaterThanExpr>($1, $3);
      		  }
;

ohlcref : TOK_OPEN TOK_OF integernumber TOK_BARS TOK_AGO
      	  {
	    //printf("Found ohlc ref for open\n");
	    auto resourceManager = driver.getResourceManagerPtr();
      	    $$ = resourceManager->getPriceOpen ($3);
	  }
       | TOK_HIGH TOK_OF integernumber TOK_BARS TOK_AGO
       	 {
       	   //printf("Found ohlc ref for high\n");
    auto resourceManager = driver.getResourceManagerPtr();
       	   $$ = resourceManager->getPriceHigh ($3);
     }
       | TOK_LOW TOK_OF integernumber TOK_BARS TOK_AGO
       	 {
    //printf("Found ohlc ref for low\n");
    auto resourceManager = driver.getResourceManagerPtr();
       	   $$ = resourceManager->getPriceLow ($3);
  }
      | TOK_CLOSE TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for close\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getPriceClose ($3);
        }
      | TOK_VOLUME TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for volume\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getVolume ($3);
        }
      | TOK_ROC1 TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for roc1\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getRoc1 ($3);
        }
      | TOK_IBS1 TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for ibs1\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getIBS1 ($3);
        }
      | TOK_IBS2 TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for ibs2\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getIBS2 ($3);
        }
      | TOK_IBS3 TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for ibs3\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getIBS3 ($3);
        }
      | TOK_MEANDER TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for meander\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getMeander ($3);
        }
      | TOK_VCHARTLOW TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for vchartlow\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getVChartLow ($3);
        }
      | TOK_VCHARTHIGH TOK_OF integernumber TOK_BARS TOK_AGO
       	{
   //printf("Found ohlc ref for vcharthigh\n");
   auto resourceManager = driver.getResourceManagerPtr();
       	  $$ = resourceManager->getVChartHigh ($3);
        }

      //| TOK_MOMERSION_FILTER TOK_LEFT_PAREN integernumber TOK_RIGHT_PAREN
      //  {
      //    $$ = astFactory.getMomersionFilter (0, $3);
      //  }
	
;

entrystmt : TOK_BUY TOK_NEXT TOK_BAR TOK_ON TOK_THE TOK_OPEN
      	    {
		//printf ("Found long market entry on open\n");
		auto resourceManager = driver.getResourceManagerPtr();
      		$$ = resourceManager->getLongMarketEntryOnOpen();
	    }
          | TOK_SELL TOK_NEXT TOK_BAR TOK_ON TOK_THE TOK_OPEN
           {
  //printf ("Found short market entry on open\n");
  auto resourceManager = driver.getResourceManagerPtr();
        $$ = resourceManager->getShortMarketEntryOnOpen();
     }
;

profitstmt : TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS number TOK_PERCENT
      	     {
	       //printf ("Found long side profit target\n");
	       auto resourceManager = driver.getResourceManagerPtr();
       	       $$ = resourceManager->getLongProfitTarget($7);
      	     }
	   | TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS integernumber TOK_PERCENT
	     	     {
	       //printf ("Found long side profit target\n");
	       auto resourceManager = driver.getResourceManagerPtr();
	       auto decimalPtr = resourceManager->getDecimalNumber($7);
	      	       $$ = resourceManager->getLongProfitTarget(decimalPtr);
	     	     }
           | TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS number TOK_PERCENT
            {
        //printf ("Found short side profit target");
        auto resourceManager = driver.getResourceManagerPtr();
        $$ = resourceManager->getShortProfitTarget($7);
            }
	   | TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS integernumber TOK_PERCENT
	     	     {
	       //printf ("Found short side profit target");
	       auto resourceManager = driver.getResourceManagerPtr();
	       auto decimalPtr = resourceManager->getDecimalNumber($7);
	       $$ = resourceManager->getShortProfitTarget(decimalPtr);
	     	     }
;

stopstmt :  TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS number TOK_PERCENT
      	    {
		//printf("Found short stop loss statement\n");
		auto resourceManager = driver.getResourceManagerPtr();
       		$$ = resourceManager->getShortStopLoss($7);
            }
	 |  TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS integernumber TOK_PERCENT
	     	    {
	 //printf("Found short stop loss statement\n");
	 auto resourceManager = driver.getResourceManagerPtr();
	 auto decimalPtr = resourceManager->getDecimalNumber($7);
	      		$$ = resourceManager->getShortStopLoss(decimalPtr);
	           }
         | TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS number TOK_PERCENT
          {
  //printf("Found long stop loss statement\n");
  auto resourceManager = driver.getResourceManagerPtr();
   $$ = resourceManager->getLongStopLoss($7);
           }
	 | TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS integernumber TOK_PERCENT
	     	   {
	 //printf("Found long stop loss statement\n");
	 auto resourceManager = driver.getResourceManagerPtr();
	 auto decimalPtr = resourceManager->getDecimalNumber($7);
	 	$$ = resourceManager->getLongStopLoss(decimalPtr);
	          }
;

integernumber : TOK_INT_NUM  
      	      	{ 
		  //printf ("Found integer number %d\n", num); 
      		  $$ = $1; 
      		}
;
 
number : TOK_FLOAT_NUM
       	 {
		//printf ("Found float number %f\n", num);
		auto resourceManager = driver.getResourceManagerPtr();
      		$$ = resourceManager->getDecimalNumber ($1);
         }
;

pattern_volatility_attr : TOK_VOLATILITY TOK_COLON volatility_attr
   			  {
				$$ = $3;
   			  }
   			| %empty
			  {
				//printf ("Found empty volatility alternative\n");
     				$$ = PriceActionLabPattern::VOLATILITY_NONE;
   			  }
;

pattern_portfolio_filter_attr : TOK_PORTFOLIO TOK_COLON portfolio_attr
			      	{
					$$ = $3;;
				}
			      | %empty
			      	{
					$$ = PriceActionLabPattern::PORTFOLIO_FILTER_NONE;
				}
;

volatility_attr : TOK_LOW_VOL
		  {
			//printf ("Found low volatility token\n");
			$$ = PriceActionLabPattern::VOLATILITY_LOW;
   		  }
   		| TOK_NORMAL_VOL
		{
			//printf ("Found normal volatility token\n");
			$$ = PriceActionLabPattern::VOLATILITY_NORMAL;
		}
		| TOK_HIGH_VOL
		{
			//printf ("Found high volatility token\n");
			$$ = PriceActionLabPattern::VOLATILITY_HIGH;
		}
		| TOK_VERY_HIGH_VOL
		{
			//printf ("Found very high volatility token\n");
			$$ = PriceActionLabPattern::VOLATILITY_VERY_HIGH;
		}
;

portfolio_attr : TOK_PORT_LONG_FILTER
	       	 {
			$$ = PriceActionLabPattern::PORTFOLIO_FILTER_LONG;
		 }
   	       | TOK_PORT_SHORT_FILTER
	       	 {
			$$ = PriceActionLabPattern::PORTFOLIO_FILTER_SHORT;
		 }
;

%% /*** Additional Code ***/

void mkc_palast::PalParser::error(const mkc_palast::PalParser::location_type& l,
                                  const std::string& message)
{
    cout << "Error: " << message << endl << "Error location: " << driver.location() << endl;

}
