// A Bison parser, made by GNU Bison 3.0.4.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.
// //                    "%code top" blocks.
#line 33 "grammar.yy" // lalr1.cc:397

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

#line 53 "PalParser.cpp" // lalr1.cc:397


// First part of user declarations.
#line 56 "grammar.yy" // lalr1.cc:404

#include <cstdlib>

#include <cstdio>
#line 147 "grammar.yy" // lalr1.cc:404

//#include "PalScanner.hpp"

AstFactory astFactory;

#line 68 "PalParser.cpp" // lalr1.cc:404

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

#include "PalParser.hpp"

// User implementation prologue.

#line 82 "PalParser.cpp" // lalr1.cc:412


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (/*CONSTCOND*/ false)
# endif


// Suppress unused-variable warnings by "using" E.
#define YYUSE(E) ((void) (E))

// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << std::endl;                  \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yystack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YYUSE(Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void>(0)
# define YY_STACK_PRINT()                static_cast<void>(0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

#line 10 "grammar.yy" // lalr1.cc:479
namespace mkc_palast {
#line 168 "PalParser.cpp" // lalr1.cc:479

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  PalParser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              // Fall through.
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }


  /// Build a parser object.
  PalParser::PalParser (class mkc_palast::Scanner& scanner_yyarg, class PalParseDriver& driver_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      scanner (scanner_yyarg),
      driver (driver_yyarg)
  {}

  PalParser::~PalParser ()
  {}


  /*---------------.
  | Symbol types.  |
  `---------------*/



  // by_state.
  inline
  PalParser::by_state::by_state ()
    : state (empty_state)
  {}

  inline
  PalParser::by_state::by_state (const by_state& other)
    : state (other.state)
  {}

  inline
  void
  PalParser::by_state::clear ()
  {
    state = empty_state;
  }

  inline
  void
  PalParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  inline
  PalParser::by_state::by_state (state_type s)
    : state (s)
  {}

  inline
  PalParser::symbol_number_type
  PalParser::by_state::type_get () const
  {
    if (state == empty_state)
      return empty_symbol;
    else
      return yystos_[state];
  }

  inline
  PalParser::stack_symbol_type::stack_symbol_type ()
  {}


  inline
  PalParser::stack_symbol_type::stack_symbol_type (state_type s, symbol_type& that)
    : super_type (s, that.location)
  {
      switch (that.type_get ())
    {
      case 67: // entrystmt
        value.move< MarketEntryExpression * > (that.value);
        break;

      case 56: // patterndescr
        value.move< PatternDescription * > (that.value);
        break;

      case 64: // conds
      case 65: // ohlc_comparison
        value.move< PatternExpression * > (that.value);
        break;

      case 55: // pattern
        value.move< PriceActionLabPattern * > (that.value);
        break;

      case 73: // pattern_portfolio_filter_attr
      case 75: // portfolio_attr
        value.move< PriceActionLabPattern::PortfolioAttribute > (that.value);
        break;

      case 72: // pattern_volatility_attr
      case 74: // volatility_attr
        value.move< PriceActionLabPattern::VolatilityAttribute > (that.value);
        break;

      case 66: // ohlcref
        value.move< PriceBarReference * > (that.value);
        break;

      case 68: // profitstmt
        value.move< ProfitTargetInPercentExpression * > (that.value);
        break;

      case 69: // stopstmt
        value.move< StopLossInPercentExpression * > (that.value);
        break;

      case 60: // pldesc
      case 61: // psdesc
      case 71: // number
        value.move< decimal7 * > (that.value);
        break;

      case 3: // TOK_INT_NUM
      case 58: // indexdesc
      case 59: // indexdatedesc
      case 62: // tradesdesc
      case 63: // cldesc
      case 70: // integernumber
        value.move< int > (that.value);
        break;

      case 4: // TOK_IDENTIFIER
      case 5: // TOK_FLOAT_NUM
      case 57: // filedesc
        value.move< std::string > (that.value);
        break;

      default:
        break;
    }

    // that is emptied.
    that.type = empty_symbol;
  }

  inline
  PalParser::stack_symbol_type&
  PalParser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
      switch (that.type_get ())
    {
      case 67: // entrystmt
        value.copy< MarketEntryExpression * > (that.value);
        break;

      case 56: // patterndescr
        value.copy< PatternDescription * > (that.value);
        break;

      case 64: // conds
      case 65: // ohlc_comparison
        value.copy< PatternExpression * > (that.value);
        break;

      case 55: // pattern
        value.copy< PriceActionLabPattern * > (that.value);
        break;

      case 73: // pattern_portfolio_filter_attr
      case 75: // portfolio_attr
        value.copy< PriceActionLabPattern::PortfolioAttribute > (that.value);
        break;

      case 72: // pattern_volatility_attr
      case 74: // volatility_attr
        value.copy< PriceActionLabPattern::VolatilityAttribute > (that.value);
        break;

      case 66: // ohlcref
        value.copy< PriceBarReference * > (that.value);
        break;

      case 68: // profitstmt
        value.copy< ProfitTargetInPercentExpression * > (that.value);
        break;

      case 69: // stopstmt
        value.copy< StopLossInPercentExpression * > (that.value);
        break;

      case 60: // pldesc
      case 61: // psdesc
      case 71: // number
        value.copy< decimal7 * > (that.value);
        break;

      case 3: // TOK_INT_NUM
      case 58: // indexdesc
      case 59: // indexdatedesc
      case 62: // tradesdesc
      case 63: // cldesc
      case 70: // integernumber
        value.copy< int > (that.value);
        break;

      case 4: // TOK_IDENTIFIER
      case 5: // TOK_FLOAT_NUM
      case 57: // filedesc
        value.copy< std::string > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    return *this;
  }


  template <typename Base>
  inline
  void
  PalParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  PalParser::yy_print_ (std::ostream& yyo,
                                     const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YYUSE (yyoutput);
    symbol_number_type yytype = yysym.type_get ();
    // Avoid a (spurious) G++ 4.8 warning about "array subscript is
    // below array bounds".
    if (yysym.empty ())
      std::abort ();
    yyo << (yytype < yyntokens_ ? "token" : "nterm")
        << ' ' << yytname_[yytype] << " ("
        << yysym.location << ": ";
    YYUSE (yytype);
    yyo << ')';
  }
#endif

  inline
  void
  PalParser::yypush_ (const char* m, state_type s, symbol_type& sym)
  {
    stack_symbol_type t (s, sym);
    yypush_ (m, t);
  }

  inline
  void
  PalParser::yypush_ (const char* m, stack_symbol_type& s)
  {
    if (m)
      YY_SYMBOL_PRINT (m, s);
    yystack_.push (s);
  }

  inline
  void
  PalParser::yypop_ (unsigned int n)
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  PalParser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  PalParser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  PalParser::debug_level_type
  PalParser::debug_level () const
  {
    return yydebug_;
  }

  void
  PalParser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  inline PalParser::state_type
  PalParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - yyntokens_] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - yyntokens_];
  }

  inline bool
  PalParser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  PalParser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  PalParser::parse ()
  {
    // State.
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

    // FIXME: This shoud be completely indented.  It is not yet to
    // avoid gratuitous conflicts when merging into the master branch.
    try
      {
    YYCDEBUG << "Starting parse" << std::endl;


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, yyla);

    // A new symbol was pushed on the stack.
  yynewstate:
    YYCDEBUG << "Entering state " << yystack_[0].state << std::endl;

    // Accept?
    if (yystack_[0].state == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    // Backup.
  yybackup:

    // Try to take a decision without lookahead.
    yyn = yypact_[yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token: ";
        try
          {
            symbol_type yylookahead (yylex (scanner, driver));
            yyla.move (yylookahead);
          }
        catch (const syntax_error& yyexc)
          {
            error (yyexc);
            goto yyerrlab1;
          }
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.type_get ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.type_get ())
      goto yydefault;

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", yyn, yyla);
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_(yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
        switch (yyr1_[yyn])
    {
      case 67: // entrystmt
        yylhs.value.build< MarketEntryExpression * > ();
        break;

      case 56: // patterndescr
        yylhs.value.build< PatternDescription * > ();
        break;

      case 64: // conds
      case 65: // ohlc_comparison
        yylhs.value.build< PatternExpression * > ();
        break;

      case 55: // pattern
        yylhs.value.build< PriceActionLabPattern * > ();
        break;

      case 73: // pattern_portfolio_filter_attr
      case 75: // portfolio_attr
        yylhs.value.build< PriceActionLabPattern::PortfolioAttribute > ();
        break;

      case 72: // pattern_volatility_attr
      case 74: // volatility_attr
        yylhs.value.build< PriceActionLabPattern::VolatilityAttribute > ();
        break;

      case 66: // ohlcref
        yylhs.value.build< PriceBarReference * > ();
        break;

      case 68: // profitstmt
        yylhs.value.build< ProfitTargetInPercentExpression * > ();
        break;

      case 69: // stopstmt
        yylhs.value.build< StopLossInPercentExpression * > ();
        break;

      case 60: // pldesc
      case 61: // psdesc
      case 71: // number
        yylhs.value.build< decimal7 * > ();
        break;

      case 3: // TOK_INT_NUM
      case 58: // indexdesc
      case 59: // indexdatedesc
      case 62: // tradesdesc
      case 63: // cldesc
      case 70: // integernumber
        yylhs.value.build< int > ();
        break;

      case 4: // TOK_IDENTIFIER
      case 5: // TOK_FLOAT_NUM
      case 57: // filedesc
        yylhs.value.build< std::string > ();
        break;

      default:
        break;
    }


      // Compute the default @$.
      {
        slice<stack_symbol_type, stack_type> slice (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, slice, yylen);
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
      try
        {
          switch (yyn)
            {
  case 2:
#line 156 "grammar.yy" // lalr1.cc:859
    { 
     	    //printf ("Found program\n"); 
          }
#line 726 "PalParser.cpp" // lalr1.cc:859
    break;

  case 3:
#line 162 "grammar.yy" // lalr1.cc:859
    { 
             //  printf ("Founds patterns\n");
             driver.addPalPattern (std::shared_ptr<PriceActionLabPattern> (yystack_[0].value.as< PriceActionLabPattern * > ()));
      	   }
#line 735 "PalParser.cpp" // lalr1.cc:859
    break;

  case 4:
#line 167 "grammar.yy" // lalr1.cc:859
    {
		//printf ("Founds recursive patterns\n");
         	driver.addPalPattern (std::shared_ptr<PriceActionLabPattern> (yystack_[0].value.as< PriceActionLabPattern * > ()));
      	   }
#line 744 "PalParser.cpp" // lalr1.cc:859
    break;

  case 5:
#line 174 "grammar.yy" // lalr1.cc:859
    { 
      	//printf ("Found pattern\n"); 
	yylhs.value.as< PriceActionLabPattern * > () = new PriceActionLabPattern (yystack_[10].value.as< PatternDescription * > (), yystack_[6].value.as< PatternExpression * > (), yystack_[4].value.as< MarketEntryExpression * > (), yystack_[2].value.as< ProfitTargetInPercentExpression * > (), yystack_[0].value.as< StopLossInPercentExpression * > (), yystack_[8].value.as< PriceActionLabPattern::VolatilityAttribute > (), yystack_[7].value.as< PriceActionLabPattern::PortfolioAttribute > ()); 
      }
#line 753 "PalParser.cpp" // lalr1.cc:859
    break;

  case 6:
#line 181 "grammar.yy" // lalr1.cc:859
    { 
      	       	 //printf ("Found pattern description\n"); 
		 yylhs.value.as< PatternDescription * > () = new PatternDescription ((char *) yystack_[7].value.as< std::string > ().c_str(), yystack_[6].value.as< int > (), yystack_[5].value.as< int > (), yystack_[4].value.as< decimal7 * > (), yystack_[3].value.as< decimal7 * > (), yystack_[2].value.as< int > (), yystack_[1].value.as< int > ()); 
      	       }
#line 762 "PalParser.cpp" // lalr1.cc:859
    break;

  case 7:
#line 188 "grammar.yy" // lalr1.cc:859
    { 
            yylhs.value.as< std::string > () = yystack_[0].value.as< std::string > (); 
          }
#line 770 "PalParser.cpp" // lalr1.cc:859
    break;

  case 8:
#line 194 "grammar.yy" // lalr1.cc:859
    { 
	   yylhs.value.as< int > () = yystack_[0].value.as< int > (); 
         }
#line 778 "PalParser.cpp" // lalr1.cc:859
    break;

  case 9:
#line 200 "grammar.yy" // lalr1.cc:859
    { 
	      	yylhs.value.as< int > () =  yystack_[0].value.as< int > (); 
	      }
#line 786 "PalParser.cpp" // lalr1.cc:859
    break;

  case 10:
#line 206 "grammar.yy" // lalr1.cc:859
    { 
	   //printf ("Found nonterminal PL: %f\n", n->getAsDouble ()); 
       	   yylhs.value.as< decimal7 * > () = yystack_[1].value.as< decimal7 * > (); 
     	 }
#line 795 "PalParser.cpp" // lalr1.cc:859
    break;

  case 11:
#line 211 "grammar.yy" // lalr1.cc:859
    { 
	   yylhs.value.as< decimal7 * > () = astFactory.getDecimalNumber (yystack_[1].value.as< int > ()); 
	 }
#line 803 "PalParser.cpp" // lalr1.cc:859
    break;

  case 12:
#line 217 "grammar.yy" // lalr1.cc:859
    { 
	   //printf ("Found nonterminal PS: %f\n", n->getAsDouble ()); 
       	   yylhs.value.as< decimal7 * > () = yystack_[1].value.as< decimal7 * > (); 
     	 }
#line 812 "PalParser.cpp" // lalr1.cc:859
    break;

  case 13:
#line 222 "grammar.yy" // lalr1.cc:859
    { 
	   yylhs.value.as< decimal7 * > () = astFactory.getDecimalNumber (yystack_[1].value.as< int > ()); 
	 }
#line 820 "PalParser.cpp" // lalr1.cc:859
    break;

  case 14:
#line 228 "grammar.yy" // lalr1.cc:859
    { 
	       yylhs.value.as< int > () = yystack_[0].value.as< int > (); 
	     }
#line 828 "PalParser.cpp" // lalr1.cc:859
    break;

  case 15:
#line 234 "grammar.yy" // lalr1.cc:859
    { 
	    yylhs.value.as< int > () = yystack_[0].value.as< int > (); 
	  }
#line 836 "PalParser.cpp" // lalr1.cc:859
    break;

  case 16:
#line 238 "grammar.yy" // lalr1.cc:859
    { 
	    yylhs.value.as< int > () = 1; 
	  }
#line 844 "PalParser.cpp" // lalr1.cc:859
    break;

  case 17:
#line 244 "grammar.yy" // lalr1.cc:859
    { 
	  //printf ("Found comparison\n"); 
          yylhs.value.as< PatternExpression * > () = yystack_[0].value.as< PatternExpression * > (); 
        }
#line 853 "PalParser.cpp" // lalr1.cc:859
    break;

  case 18:
#line 249 "grammar.yy" // lalr1.cc:859
    { 
	  //printf ("Found recursive comparison\n"); 
       	  yylhs.value.as< PatternExpression * > () = new AndExpr (yystack_[2].value.as< PatternExpression * > (), yystack_[0].value.as< PatternExpression * > ()); 
      	}
#line 862 "PalParser.cpp" // lalr1.cc:859
    break;

  case 19:
#line 256 "grammar.yy" // lalr1.cc:859
    { 
		    //printf ("Found greater than ohlc comparison \n"); 
        	    yylhs.value.as< PatternExpression * > () = new GreaterThanExpr (yystack_[2].value.as< PriceBarReference * > (), yystack_[0].value.as< PriceBarReference * > ()); 
      		  }
#line 871 "PalParser.cpp" // lalr1.cc:859
    break;

  case 20:
#line 263 "grammar.yy" // lalr1.cc:859
    { 
	    //printf("Found ohlc ref for open\n"); 
      	    yylhs.value.as< PriceBarReference * > () = astFactory.getPriceOpen (yystack_[2].value.as< int > ()); 
	  }
#line 880 "PalParser.cpp" // lalr1.cc:859
    break;

  case 21:
#line 268 "grammar.yy" // lalr1.cc:859
    { 
       	   //printf("Found ohlc ref for high\n"); 
      	   yylhs.value.as< PriceBarReference * > () = astFactory.getPriceHigh (yystack_[2].value.as< int > ()); 
   	 }
#line 889 "PalParser.cpp" // lalr1.cc:859
    break;

  case 22:
#line 273 "grammar.yy" // lalr1.cc:859
    { 
	   //printf("Found ohlc ref for low\n"); 
       	   yylhs.value.as< PriceBarReference * > () = astFactory.getPriceLow (yystack_[2].value.as< int > ()); 
	 }
#line 898 "PalParser.cpp" // lalr1.cc:859
    break;

  case 23:
#line 278 "grammar.yy" // lalr1.cc:859
    { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as< PriceBarReference * > () = astFactory.getPriceClose (yystack_[2].value.as< int > ()); 
        }
#line 907 "PalParser.cpp" // lalr1.cc:859
    break;

  case 24:
#line 285 "grammar.yy" // lalr1.cc:859
    {
		//printf ("Found long market entry on open\n"); 
      		yylhs.value.as< MarketEntryExpression * > () = astFactory.getLongMarketEntryOnOpen(); 
	    }
#line 916 "PalParser.cpp" // lalr1.cc:859
    break;

  case 25:
#line 290 "grammar.yy" // lalr1.cc:859
    {
		//printf ("Found short market entry on open\n"); 
      		yylhs.value.as< MarketEntryExpression * > () = astFactory.getShortMarketEntryOnOpen(); 
	    }
#line 925 "PalParser.cpp" // lalr1.cc:859
    break;

  case 26:
#line 297 "grammar.yy" // lalr1.cc:859
    { 
	       //printf ("Found long side profit target\n"); 
       	       yylhs.value.as< ProfitTargetInPercentExpression * > () = astFactory.getLongProfitTarget(yystack_[1].value.as< decimal7 * > ()); 
      	     }
#line 934 "PalParser.cpp" // lalr1.cc:859
    break;

  case 27:
#line 302 "grammar.yy" // lalr1.cc:859
    { 
	       //printf ("Found long side profit target\n"); 
       	       yylhs.value.as< ProfitTargetInPercentExpression * > () = astFactory.getLongProfitTarget(astFactory.getDecimalNumber (yystack_[1].value.as< int > ())); 
      	     }
#line 943 "PalParser.cpp" // lalr1.cc:859
    break;

  case 28:
#line 307 "grammar.yy" // lalr1.cc:859
    { 
	       //printf ("Found short side profit target"); 
	       yylhs.value.as< ProfitTargetInPercentExpression * > () = astFactory.getShortProfitTarget(yystack_[1].value.as< decimal7 * > ()); 
      	     }
#line 952 "PalParser.cpp" // lalr1.cc:859
    break;

  case 29:
#line 312 "grammar.yy" // lalr1.cc:859
    { 
	       //printf ("Found short side profit target"); 
	       yylhs.value.as< ProfitTargetInPercentExpression * > () = astFactory.getShortProfitTarget(astFactory.getDecimalNumber (yystack_[1].value.as< int > ())); 
      	     }
#line 961 "PalParser.cpp" // lalr1.cc:859
    break;

  case 30:
#line 319 "grammar.yy" // lalr1.cc:859
    {
		//printf("Found short stop loss statement\n"); 
       		yylhs.value.as< StopLossInPercentExpression * > () = astFactory.getShortStopLoss(yystack_[1].value.as< decimal7 * > ()); 
            }
#line 970 "PalParser.cpp" // lalr1.cc:859
    break;

  case 31:
#line 324 "grammar.yy" // lalr1.cc:859
    {
		//printf("Found short stop loss statement\n"); 
       		yylhs.value.as< StopLossInPercentExpression * > () = astFactory.getShortStopLoss(astFactory.getDecimalNumber (yystack_[1].value.as< int > ())); 
            }
#line 979 "PalParser.cpp" // lalr1.cc:859
    break;

  case 32:
#line 329 "grammar.yy" // lalr1.cc:859
    {
		//printf("Found long stop loss statement\n"); 
 		yylhs.value.as< StopLossInPercentExpression * > () = astFactory.getLongStopLoss(yystack_[1].value.as< decimal7 * > ()); 
           }
#line 988 "PalParser.cpp" // lalr1.cc:859
    break;

  case 33:
#line 334 "grammar.yy" // lalr1.cc:859
    {
		//printf("Found long stop loss statement\n"); 
 		yylhs.value.as< StopLossInPercentExpression * > () = astFactory.getLongStopLoss(astFactory.getDecimalNumber (yystack_[1].value.as< int > ())); 
           }
#line 997 "PalParser.cpp" // lalr1.cc:859
    break;

  case 34:
#line 341 "grammar.yy" // lalr1.cc:859
    { 
		  //printf ("Found integer number %d\n", num); 
      		  yylhs.value.as< int > () = yystack_[0].value.as< int > (); 
      		}
#line 1006 "PalParser.cpp" // lalr1.cc:859
    break;

  case 35:
#line 348 "grammar.yy" // lalr1.cc:859
    {
		//printf ("Found float number %f\n", num); 
      		yylhs.value.as< decimal7 * > () =  astFactory.getDecimalNumber ((char *)yystack_[0].value.as< std::string > ().c_str()); 
         }
#line 1015 "PalParser.cpp" // lalr1.cc:859
    break;

  case 36:
#line 355 "grammar.yy" // lalr1.cc:859
    {
				yylhs.value.as< PriceActionLabPattern::VolatilityAttribute > () = yystack_[0].value.as< PriceActionLabPattern::VolatilityAttribute > ();
   			  }
#line 1023 "PalParser.cpp" // lalr1.cc:859
    break;

  case 37:
#line 359 "grammar.yy" // lalr1.cc:859
    {
				//printf ("Found empty volatility alternative\n");
     				yylhs.value.as< PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_NONE;
   			  }
#line 1032 "PalParser.cpp" // lalr1.cc:859
    break;

  case 38:
#line 366 "grammar.yy" // lalr1.cc:859
    {
					yylhs.value.as< PriceActionLabPattern::PortfolioAttribute > () = yystack_[0].value.as< PriceActionLabPattern::PortfolioAttribute > ();;
				}
#line 1040 "PalParser.cpp" // lalr1.cc:859
    break;

  case 39:
#line 370 "grammar.yy" // lalr1.cc:859
    {
					yylhs.value.as< PriceActionLabPattern::PortfolioAttribute > () = PriceActionLabPattern::PORTFOLIO_FILTER_NONE;
				}
#line 1048 "PalParser.cpp" // lalr1.cc:859
    break;

  case 40:
#line 376 "grammar.yy" // lalr1.cc:859
    {
			//printf ("Found low volatility token\n");
			yylhs.value.as< PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_LOW;
   		  }
#line 1057 "PalParser.cpp" // lalr1.cc:859
    break;

  case 41:
#line 381 "grammar.yy" // lalr1.cc:859
    {
			//printf ("Found normal volatility token\n");
			yylhs.value.as< PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_NORMAL;
		}
#line 1066 "PalParser.cpp" // lalr1.cc:859
    break;

  case 42:
#line 386 "grammar.yy" // lalr1.cc:859
    {
			//printf ("Found high volatility token\n");
			yylhs.value.as< PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_HIGH;
		}
#line 1075 "PalParser.cpp" // lalr1.cc:859
    break;

  case 43:
#line 391 "grammar.yy" // lalr1.cc:859
    {
			//printf ("Found very high volatility token\n");
			yylhs.value.as< PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_VERY_HIGH;
		}
#line 1084 "PalParser.cpp" // lalr1.cc:859
    break;

  case 44:
#line 398 "grammar.yy" // lalr1.cc:859
    {
			yylhs.value.as< PriceActionLabPattern::PortfolioAttribute > () = PriceActionLabPattern::PORTFOLIO_FILTER_LONG;
		 }
#line 1092 "PalParser.cpp" // lalr1.cc:859
    break;

  case 45:
#line 402 "grammar.yy" // lalr1.cc:859
    {
			yylhs.value.as< PriceActionLabPattern::PortfolioAttribute > () = PriceActionLabPattern::PORTFOLIO_FILTER_SHORT;
		 }
#line 1100 "PalParser.cpp" // lalr1.cc:859
    break;


#line 1104 "PalParser.cpp" // lalr1.cc:859
            default:
              break;
            }
        }
      catch (const syntax_error& yyexc)
        {
          error (yyexc);
          YYERROR;
        }
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;
      YY_STACK_PRINT ();

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, yylhs);
    }
    goto yynewstate;

  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        error (yyla.location, yysyntax_error_ (yystack_[0].state, yyla));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.type_get () == yyeof_)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:

    /* Pacify compilers like GCC when the user code never invokes
       YYERROR and the label yyerrorlab therefore never appears in user
       code.  */
    if (false)
      goto yyerrorlab;
    yyerror_range[1].location = yystack_[yylen - 1].location;
    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    {
      stack_symbol_type error_token;
      for (;;)
        {
          yyn = yypact_[yystack_[0].state];
          if (!yy_pact_value_is_default_ (yyn))
            {
              yyn += yyterror_;
              if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
                {
                  yyn = yytable_[yyn];
                  if (0 < yyn)
                    break;
                }
            }

          // Pop the current state because it cannot handle the error token.
          if (yystack_.size () == 1)
            YYABORT;

          yyerror_range[1].location = yystack_[0].location;
          yy_destroy_ ("Error: popping", yystack_[0]);
          yypop_ ();
          YY_STACK_PRINT ();
        }

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = yyn;
      yypush_ ("Shifting", error_token);
    }
    goto yynewstate;

    // Accept.
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    // Abort.
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack"
                 << std::endl;
        // Do not try to display the values of the reclaimed symbols,
        // as their printer might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
  }

  void
  PalParser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what());
  }

  // Generate an error message.
  std::string
  PalParser::yysyntax_error_ (state_type yystate, const symbol_type& yyla) const
  {
    // Number of reported tokens (one for the "unexpected", one per
    // "expected").
    size_t yycount = 0;
    // Its maximum.
    enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
    // Arguments of yyformat.
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state
         merging (from LALR or IELR) and default reductions corrupt the
         expected token list.  However, the list is correct for
         canonical LR with one exception: it will still contain any
         token that will not be accepted due to an error action in a
         later state.
    */
    if (!yyla.empty ())
      {
        int yytoken = yyla.type_get ();
        yyarg[yycount++] = yytname_[yytoken];
        int yyn = yypact_[yystate];
        if (!yy_pact_value_is_default_ (yyn))
          {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            // Stay within bounds of both yycheck and yytname.
            int yychecklim = yylast_ - yyn + 1;
            int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
            for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
              if (yycheck_[yyx + yyn] == yyx && yyx != yyterror_
                  && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
                {
                  if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                      yycount = 1;
                      break;
                    }
                  else
                    yyarg[yycount++] = yytname_[yyx];
                }
          }
      }

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
        YYCASE_(0, YY_("syntax error"));
        YYCASE_(1, YY_("syntax error, unexpected %s"));
        YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    size_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += yytnamerr_ (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char PalParser::yypact_ninf_ = -58;

  const signed char PalParser::yytable_ninf_ = -1;

  const signed char
  PalParser::yypact_[] =
  {
      -6,   -23,    32,    -6,   -58,    20,    25,    -3,   -58,   -58,
      -7,    34,    28,     2,    30,    -2,   -58,    39,     6,     8,
     -34,    33,     5,   -58,   -58,    35,    38,     9,   -58,   -58,
     -58,   -58,   -58,   -24,    36,    37,    41,    42,    -9,   -58,
      40,    39,     1,    43,    11,   -58,   -58,   -58,    39,    39,
      39,    39,     4,     5,     5,   -58,   -58,    49,    54,     1,
      52,    21,    18,    44,    45,    46,    48,    56,    55,   -58,
     -58,   -58,   -58,    57,    58,    39,    62,    59,    47,    60,
      63,    64,    61,    65,    66,   -58,   -58,   -58,    12,   -58,
     -58,   -58,   -58,   -58,    51,    67,    68,    69,   -58,   -58,
      70,    72,    53,    71,    73,    75,    74,    31,   -58,   -58,
     -58,    76,    77,    22,    78,     1,     1,    79,    83,    85,
      92,    93,    24,   -58,   -58,   -58,   -58,     1,     1,    94,
      95,    96,   102,   -58,   -58,   -58,   -58
  };

  const unsigned char
  PalParser::yydefact_[] =
  {
       0,     0,     0,     2,     3,     0,     0,     0,     1,     4,
      37,     0,     0,     0,     0,    39,     7,     0,     0,     0,
       0,     0,     0,    34,     8,     0,     0,     0,    40,    42,
      43,    41,    36,     0,     0,     0,     0,     0,     0,    17,
       0,     0,     0,     0,     0,    44,    45,    38,     0,     0,
       0,     0,     0,     0,     0,     9,    35,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    18,
      19,    11,    10,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    13,    12,    14,     0,     6,
      20,    21,    22,    23,     0,     0,     0,     0,    16,    15,
       0,     0,     0,     0,     0,     0,     0,     0,     5,    24,
      25,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    27,    26,    29,    28,     0,     0,     0,
       0,     0,     0,    31,    30,    33,    32
  };

  const signed char
  PalParser::yypgoto_[] =
  {
     -58,   -58,   -58,   109,   -58,   -58,   -58,   -58,   -58,   -58,
     -58,   -58,   -58,    80,    81,   -58,   -58,   -58,   -41,   -57,
     -58,   -58,   -58,   -58
  };

  const signed char
  PalParser::yydefgoto_[] =
  {
      -1,     2,     3,     4,     5,     7,    13,    19,    27,    44,
      61,    77,    38,    39,    40,    68,    97,   108,    24,    58,
      15,    22,    32,    47
  };

  const unsigned char
  PalParser::yytable_[] =
  {
      55,    57,    74,     1,    23,    52,    56,    62,    63,    64,
      65,    53,    28,    29,     6,    23,    30,    31,    73,    98,
      34,    35,    36,    37,    45,    46,    66,    67,   115,   116,
     127,   128,     8,    10,    87,    12,    11,    14,    16,    17,
      18,    20,    23,    21,    33,    25,    41,    99,    26,    42,
      43,    78,    54,    60,    59,    48,    49,    71,   119,   121,
      50,    51,    72,    75,    76,    85,    86,   112,    90,    89,
     130,   132,    82,    88,   118,   120,   100,    79,    80,    81,
      83,    91,    84,   106,    92,    93,   129,   131,   109,   103,
     110,   123,   101,   124,    96,    94,   104,   102,   105,    95,
     125,   126,   133,   134,   135,   111,   107,   114,   113,   117,
     136,   122,     9,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    69,     0,    70
  };

  const short int
  PalParser::yycheck_[] =
  {
      41,    42,    59,     9,     3,    14,     5,    48,    49,    50,
      51,    20,    46,    47,    37,     3,    50,    51,    59,     7,
      15,    16,    17,    18,    48,    49,    22,    23,     6,     7,
       6,     7,     0,    13,    75,    38,    11,    44,     4,    11,
      38,    11,     3,    45,    11,    39,    11,    88,    40,    11,
      41,    33,    12,    42,    11,    19,    19,     8,   115,   116,
      19,    19,     8,    11,    43,     8,     8,    36,    21,    10,
     127,   128,    24,    11,   115,   116,    25,    33,    33,    33,
      24,    21,    27,    30,    21,    21,   127,   128,    15,    20,
      15,     8,    25,     8,    28,    34,    26,    29,    26,    34,
       8,     8,     8,     8,     8,    31,    35,    30,    32,    31,
       8,    32,     3,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    53,    -1,    54
  };

  const unsigned char
  PalParser::yystos_[] =
  {
       0,     9,    53,    54,    55,    56,    37,    57,     0,    55,
      13,    11,    38,    58,    44,    72,     4,    11,    38,    59,
      11,    45,    73,     3,    70,    39,    40,    60,    46,    47,
      50,    51,    74,    11,    15,    16,    17,    18,    64,    65,
      66,    11,    11,    41,    61,    48,    49,    75,    19,    19,
      19,    19,    14,    20,    12,    70,     5,    70,    71,    11,
      42,    62,    70,    70,    70,    70,    22,    23,    67,    65,
      66,     8,     8,    70,    71,    11,    43,    63,    33,    33,
      33,    33,    24,    24,    27,     8,     8,    70,    11,    10,
      21,    21,    21,    21,    34,    34,    28,    68,     7,    70,
      25,    25,    29,    20,    26,    26,    30,    35,    69,    15,
      15,    31,    36,    32,    30,     6,     7,    31,    70,    71,
      70,    71,    32,     8,     8,     8,     8,     6,     7,    70,
      71,    70,    71,     8,     8,     8,     8
  };

  const unsigned char
  PalParser::yyr1_[] =
  {
       0,    52,    53,    54,    54,    55,    56,    57,    58,    59,
      60,    60,    61,    61,    62,    63,    63,    64,    64,    65,
      66,    66,    66,    66,    67,    67,    68,    68,    68,    68,
      69,    69,    69,    69,    70,    71,    72,    72,    73,    73,
      74,    74,    74,    74,    75,    75
  };

  const unsigned char
  PalParser::yyr2_[] =
  {
       0,     2,     1,     1,     2,    11,     9,     3,     3,     4,
       4,     4,     4,     4,     3,     3,     3,     1,     3,     3,
       5,     5,     5,     5,     6,     6,     8,     8,     8,     8,
       8,     8,     8,     8,     1,     1,     3,     0,     3,     0,
       1,     1,     1,     1,     1,     1
  };



  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a yyntokens_, nonterminals.
  const char*
  const PalParser::yytname_[] =
  {
  "TOK_EOF", "error", "$undefined", "TOK_INT_NUM", "TOK_IDENTIFIER",
  "TOK_FLOAT_NUM", "TOK_PLUS", "TOK_MINUS", "TOK_PERCENT", "TOK_LBRACE",
  "TOK_RBRACE", "TOK_COLON", "TOK_GREATER_THAN", "TOK_IF", "TOK_THEN",
  "TOK_OPEN", "TOK_HIGH", "TOK_LOW", "TOK_CLOSE", "TOK_OF", "TOK_AND",
  "TOK_AGO", "TOK_BUY", "TOK_SELL", "TOK_NEXT", "TOK_ON", "TOK_THE",
  "TOK_WITH", "TOK_PROFIT", "TOK_TARGET", "TOK_AT", "TOK_ENTRY",
  "TOK_PRICE", "TOK_BARS", "TOK_BAR", "TOK_STOP", "TOK_LOSS", "TOK_FILE",
  "TOK_INDEX", "TOK_DATE", "TOK_PL", "TOK_PS", "TOK_TRADES", "TOK_CL",
  "TOK_VOLATILITY", "TOK_PORTFOLIO", "TOK_LOW_VOL", "TOK_HIGH_VOL",
  "TOK_PORT_LONG_FILTER", "TOK_PORT_SHORT_FILTER", "TOK_VERY_HIGH_VOL",
  "TOK_NORMAL_VOL", "$accept", "program", "patterns", "pattern",
  "patterndescr", "filedesc", "indexdesc", "indexdatedesc", "pldesc",
  "psdesc", "tradesdesc", "cldesc", "conds", "ohlc_comparison", "ohlcref",
  "entrystmt", "profitstmt", "stopstmt", "integernumber", "number",
  "pattern_volatility_attr", "pattern_portfolio_filter_attr",
  "volatility_attr", "portfolio_attr", YY_NULLPTR
  };

#if YYDEBUG
  const unsigned short int
  PalParser::yyrline_[] =
  {
       0,   155,   155,   161,   166,   173,   180,   187,   193,   199,
     205,   210,   216,   221,   227,   233,   237,   243,   248,   255,
     262,   267,   272,   277,   284,   289,   296,   301,   306,   311,
     318,   323,   328,   333,   340,   347,   354,   358,   365,   369,
     375,   380,   385,   390,   397,   401
  };

  // Print the state stack on the debug stream.
  void
  PalParser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << i->state;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  PalParser::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):" << std::endl;
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG


#line 10 "grammar.yy" // lalr1.cc:1167
} // mkc_palast
#line 1562 "PalParser.cpp" // lalr1.cc:1167
#line 407 "grammar.yy" // lalr1.cc:1168
 /*** Additional Code ***/

void mkc_palast::PalParser::error(const mkc_palast::PalParser::location_type& l,
                                  const std::string& message)
{
    cout << "Error: " << message << endl << "Error location: " << driver.location() << endl;

}
