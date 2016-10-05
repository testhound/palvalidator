%{ /*** C/C++ Declarations ***/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "scanner.h"
#include "PalAst.h"
#include "PalParseDriver.h"
#include "PalParser.hpp"



#define yyterminate() mkc_palast::PalParser::make_TOK_EOF(mkc_palast::location());

// This will track current scanner location.
// Action is called when length of the token is known.
#define YY_USER_ACTION  m_driver.increaseLocation(yyleng);

	// !!!WARNING!!!
	// Location API is used, but the location is not initialized, 'cause I'm lazy. When making
	// a token with make_{something} method you can pass detailed token location. Current location
	// is accessible with m_driver.location() method. All puzzle elements are there - just
	// pass location value in every action code block below. I'm going to waste more time writing
	// this excuse than putting this boilerplate below...
	//
	// Location class can be found in location.hh and posistion.hh files. It's just a bit too much
	// boilerplate for this small example. Bummer.

%}

%option c++
%option yyclass="PalScanner"
%option outfile="PalScanner.cpp" 
%option header-file="PalScanner.hpp"
%option nodefault
%option noyywrap
%option prefix="MkcPalAst_"


%% /*** Regular Expressions Part ***/


[ \t]+		   /* whitespace */
[\n]+              /* newline */
[\-][\-]+          /* Ignore comments */
"Code"             /* Ignore Header */
"For"             /* Ignore Header */
"Selected"             /* Ignore Header */
"Patterns"             /* Ignore Header */
"+"                {return mkc_palast::PalParser::make_TOK_PLUS(mkc_palast::location());}
"-"                {return mkc_palast::PalParser::make_TOK_MINUS(mkc_palast::location()); }

"{"                { return mkc_palast::PalParser::make_TOK_LBRACE(mkc_palast::location()); }
"}"                { return mkc_palast::PalParser::make_TOK_RBRACE(mkc_palast::location()); }

"%"                { return mkc_palast::PalParser::make_TOK_PERCENT(mkc_palast::location()); }

":"                { return mkc_palast::PalParser::make_TOK_COLON(mkc_palast::location()); }

">"                { return mkc_palast::PalParser::make_TOK_GREATER_THAN(mkc_palast::location()); }
"File"             { return mkc_palast::PalParser::make_TOK_FILE(mkc_palast::location()); }
"FILE"             { return mkc_palast::PalParser::make_TOK_FILE(mkc_palast::location()); }
"Index"            { return mkc_palast::PalParser::make_TOK_INDEX(mkc_palast::location()); }
"INDEX"            { return mkc_palast::PalParser::make_TOK_INDEX(mkc_palast::location()); }
"Date"             { return mkc_palast::PalParser::make_TOK_DATE(mkc_palast::location()); }
"DATE"             { return mkc_palast::PalParser::make_TOK_DATE(mkc_palast::location()); }
"PL"               { return mkc_palast::PalParser::make_TOK_PL(mkc_palast::location()); }
"PS"               { return mkc_palast::PalParser::make_TOK_PS(mkc_palast::location()); }
"Trades"           { return mkc_palast::PalParser::make_TOK_TRADES(mkc_palast::location()); }
"CL"               { return mkc_palast::PalParser::make_TOK_CL(mkc_palast::location()); }
"IF"               { return mkc_palast::PalParser::make_TOK_IF(mkc_palast::location()); }
"THEN"             { return mkc_palast::PalParser::make_TOK_THEN(mkc_palast::location()); }
"HIGH"             { return mkc_palast::PalParser::make_TOK_HIGH(mkc_palast::location()); }
"LOW"              { return mkc_palast::PalParser::make_TOK_LOW(mkc_palast::location()); }
"CLOSE"            { return mkc_palast::PalParser::make_TOK_CLOSE(mkc_palast::location()); }
"OPEN"             { return mkc_palast::PalParser::make_TOK_OPEN(mkc_palast::location()); }
"OF"               { return mkc_palast::PalParser::make_TOK_OF(mkc_palast::location()); }
"AND"              { return mkc_palast::PalParser::make_TOK_AND(mkc_palast::location()); }
"AGO"              { return mkc_palast::PalParser::make_TOK_AGO(mkc_palast::location()); }
"BUY"              { return mkc_palast::PalParser::make_TOK_BUY(mkc_palast::location()); }
"SELL"             { return mkc_palast::PalParser::make_TOK_SELL(mkc_palast::location()); }
"NEXT"             { return mkc_palast::PalParser::make_TOK_NEXT(mkc_palast::location()); } 
"ON"               { return mkc_palast::PalParser::make_TOK_ON(mkc_palast::location()); }
"THE"              { return mkc_palast::PalParser::make_TOK_THE(mkc_palast::location()); }
"WITH"             { return mkc_palast::PalParser::make_TOK_WITH(mkc_palast::location()); }
"PROFIT"           { return mkc_palast::PalParser::make_TOK_PROFIT(mkc_palast::location()); }
"TARGET"           { return mkc_palast::PalParser::make_TOK_TARGET(mkc_palast::location()); }
"AT"               { return mkc_palast::PalParser::make_TOK_AT(mkc_palast::location()); }
"ENTRY"            { return mkc_palast::PalParser::make_TOK_ENTRY(mkc_palast::location()); }
"PRICE"            { return mkc_palast::PalParser::make_TOK_PRICE(mkc_palast::location()); }
"BARS"             { return mkc_palast::PalParser::make_TOK_BARS(mkc_palast::location()); }
"BAR"              { return mkc_palast::PalParser::make_TOK_BAR(mkc_palast::location()); }
"DAYS"             { return mkc_palast::PalParser::make_TOK_BARS(mkc_palast::location()); }
"STOP"             { return mkc_palast::PalParser::make_TOK_STOP(mkc_palast::location()); }
"LOSS"             { return mkc_palast::PalParser::make_TOK_LOSS(mkc_palast::location()); }
"LVOL"             { return mkc_palast::PalParser::make_TOK_LOW_VOL(mkc_palast::location()); }
"NVOL"             { return mkc_palast::PalParser::make_TOK_NORMAL_VOL(mkc_palast::location()); }
"HVOL"             { return mkc_palast::PalParser::make_TOK_HIGH_VOL(mkc_palast::location()); }
"VHVOL"            { return mkc_palast::PalParser::make_TOK_VERY_HIGH_VOL(mkc_palast::location()); }
"PM_FILTER_LONG"   { return mkc_palast::PalParser::make_TOK_PORT_LONG_FILTER(mkc_palast::location()); }
"PM_FILTER_SHORT"  { return mkc_palast::PalParser::make_TOK_PORT_SHORT_FILTER(mkc_palast::location()); }
"Volatility"       { return mkc_palast::PalParser::make_TOK_VOLATILITY(mkc_palast::location()); }
"Portfolio"        { return mkc_palast::PalParser::make_TOK_PORTFOLIO(mkc_palast::location()); }
[0-9]+             {
                     return mkc_palast::PalParser::make_TOK_INT_NUM(std::atoi((const char *) yytext), 
                                                                    mkc_palast::location());
                   }
[0-9]*"."[0-9]+    {
                     return mkc_palast::PalParser::make_TOK_FLOAT_NUM(yytext, 
                                                                      mkc_palast::location());
                   }

[_a-zA-Z][_a-zA-Z0-9]*"."[a-zA-Z]*   {
                             // printf ("Found token TOK_IDENTIFIER\n");
                             return mkc_palast::PalParser::make_TOK_IDENTIFIER(yytext, 
                                                                               mkc_palast::location());
                         }

.                        {
                            printf("illegal character: %c\n", yytext[0]);
                            /* but continue anyway */
                         }

<<EOF>>            {
                     return yyterminate();
                   }

%%

