// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

// "%code top" blocks.
#line 33 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"

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

#line 58 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"


// First part of user prologue.
#line 56 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"

#include <cstdlib>

#include <cstdio>
#line 158 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"

//#include "PalScanner.hpp"

AstFactory astFactory;

#line 73 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"


#include "PalParser.hpp"




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


// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
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
    while (false)
# endif


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
      *yycdebug_ << '\n';                       \
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
      yy_stack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YY_USE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

#line 10 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
namespace mkc_palast {
#line 171 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"

  /// Build a parser object.
  PalParser::PalParser (class mkc_palast::Scanner& scanner_yyarg, class PalParseDriver& driver_yyarg)
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      scanner (scanner_yyarg),
      driver (driver_yyarg)
  {}

  PalParser::~PalParser ()
  {}

  PalParser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/



  // by_state.
  PalParser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  PalParser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  PalParser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  PalParser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  PalParser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  PalParser::symbol_kind_type
  PalParser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  PalParser::stack_symbol_type::stack_symbol_type ()
  {}

  PalParser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state), YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.YY_MOVE_OR_COPY< MarketEntryExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.YY_MOVE_OR_COPY< PatternDescription * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.YY_MOVE_OR_COPY< PatternExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern: // pattern
        value.YY_MOVE_OR_COPY< PriceActionLabPattern * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.YY_MOVE_OR_COPY< PriceActionLabPattern::PortfolioAttribute > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.YY_MOVE_OR_COPY< PriceActionLabPattern::VolatilityAttribute > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.YY_MOVE_OR_COPY< PriceBarReference * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.YY_MOVE_OR_COPY< ProfitTargetInPercentExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.YY_MOVE_OR_COPY< StopLossInPercentExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.YY_MOVE_OR_COPY< decimal7 * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.YY_MOVE_OR_COPY< int > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.YY_MOVE_OR_COPY< std::string > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  PalParser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s, YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.move< MarketEntryExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.move< PatternDescription * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.move< PatternExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern: // pattern
        value.move< PriceActionLabPattern * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.move< PriceActionLabPattern::PortfolioAttribute > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.move< PriceActionLabPattern::VolatilityAttribute > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.move< PriceBarReference * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.move< ProfitTargetInPercentExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.move< StopLossInPercentExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.move< decimal7 * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.move< int > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.move< std::string > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  PalParser::stack_symbol_type&
  PalParser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.copy< MarketEntryExpression * > (that.value);
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.copy< PatternDescription * > (that.value);
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.copy< PatternExpression * > (that.value);
        break;

      case symbol_kind::S_pattern: // pattern
        value.copy< PriceActionLabPattern * > (that.value);
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.copy< PriceActionLabPattern::PortfolioAttribute > (that.value);
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.copy< PriceActionLabPattern::VolatilityAttribute > (that.value);
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.copy< PriceBarReference * > (that.value);
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.copy< ProfitTargetInPercentExpression * > (that.value);
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.copy< StopLossInPercentExpression * > (that.value);
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.copy< decimal7 * > (that.value);
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.copy< int > (that.value);
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.copy< std::string > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    return *this;
  }

  PalParser::stack_symbol_type&
  PalParser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.move< MarketEntryExpression * > (that.value);
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.move< PatternDescription * > (that.value);
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.move< PatternExpression * > (that.value);
        break;

      case symbol_kind::S_pattern: // pattern
        value.move< PriceActionLabPattern * > (that.value);
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.move< PriceActionLabPattern::PortfolioAttribute > (that.value);
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.move< PriceActionLabPattern::VolatilityAttribute > (that.value);
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.move< PriceBarReference * > (that.value);
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.move< ProfitTargetInPercentExpression * > (that.value);
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.move< StopLossInPercentExpression * > (that.value);
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.move< decimal7 * > (that.value);
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.move< int > (that.value);
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.move< std::string > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  PalParser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  PalParser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YY_USE (yyoutput);
    if (yysym.empty ())
      yyo << "empty symbol";
    else
      {
        symbol_kind_type yykind = yysym.kind ();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm")
            << ' ' << yysym.name () << " ("
            << yysym.location << ": ";
        YY_USE (yykind);
        yyo << ')';
      }
  }
#endif

  void
  PalParser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  PalParser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  PalParser::yypop_ (int n) YY_NOEXCEPT
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

  PalParser::state_type
  PalParser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  PalParser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  PalParser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  PalParser::operator() ()
  {
    return parse ();
  }

  int
  PalParser::parse ()
  {
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

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int (yystack_[0].state) << '\n';
    YY_STACK_PRINT ();

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            symbol_type yylookahead (yylex (scanner, driver));
            yyla.move (yylookahead);
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    if (yyla.kind () == symbol_kind::S_YYerror)
    {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind ())
      {
        goto yydefault;
      }

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
    yypush_ ("Shifting", state_type (yyn), YY_MOVE (yyla));
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case symbol_kind::S_entrystmt: // entrystmt
        yylhs.value.emplace< MarketEntryExpression * > ();
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        yylhs.value.emplace< PatternDescription * > ();
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        yylhs.value.emplace< PatternExpression * > ();
        break;

      case symbol_kind::S_pattern: // pattern
        yylhs.value.emplace< PriceActionLabPattern * > ();
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        yylhs.value.emplace< PriceActionLabPattern::PortfolioAttribute > ();
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        yylhs.value.emplace< PriceActionLabPattern::VolatilityAttribute > ();
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        yylhs.value.emplace< PriceBarReference * > ();
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        yylhs.value.emplace< ProfitTargetInPercentExpression * > ();
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        yylhs.value.emplace< StopLossInPercentExpression * > ();
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        yylhs.value.emplace< decimal7 * > ();
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        yylhs.value.emplace< int > ();
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        yylhs.value.emplace< std::string > ();
        break;

      default:
        break;
    }


      // Default location.
      {
        stack_type::slice range (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, range, yylen);
        yyerror_range[1].location = yylhs.location;
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 2: // program: patterns
#line 167 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
          { 
     	    //printf ("Found program\n"); 
          }
#line 868 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 3: // patterns: pattern
#line 173 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
           { 
             //  printf ("Founds patterns\n");
             driver.addPalPattern (std::shared_ptr<PriceActionLabPattern> (yystack_[0].value.as < PriceActionLabPattern * > ()));
      	   }
#line 877 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 4: // patterns: patterns pattern
#line 178 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
           {
		//printf ("Founds recursive patterns\n");
         	driver.addPalPattern (std::shared_ptr<PriceActionLabPattern> (yystack_[0].value.as < PriceActionLabPattern * > ()));
      	   }
#line 886 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 5: // pattern: patterndescr TOK_IF pattern_volatility_attr pattern_portfolio_filter_attr conds TOK_THEN entrystmt TOK_WITH profitstmt TOK_AND stopstmt
#line 185 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
      { 
      	//printf ("Found pattern\n"); 
	yylhs.value.as < PriceActionLabPattern * > () = new PriceActionLabPattern (yystack_[10].value.as < PatternDescription * > (), yystack_[6].value.as < PatternExpression * > (), yystack_[4].value.as < MarketEntryExpression * > (), yystack_[2].value.as < ProfitTargetInPercentExpression * > (), yystack_[0].value.as < StopLossInPercentExpression * > (), yystack_[8].value.as < PriceActionLabPattern::VolatilityAttribute > (), yystack_[7].value.as < PriceActionLabPattern::PortfolioAttribute > ()); 
      }
#line 895 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 6: // patterndescr: TOK_LBRACE filedesc indexdesc indexdatedesc pldesc psdesc tradesdesc cldesc TOK_RBRACE
#line 192 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
               { 
      	       	 //printf ("Found pattern description\n"); 
		 yylhs.value.as < PatternDescription * > () = new PatternDescription ((char *) yystack_[7].value.as < std::string > ().c_str(), yystack_[6].value.as < int > (), yystack_[5].value.as < int > (), yystack_[4].value.as < decimal7 * > (), yystack_[3].value.as < decimal7 * > (), yystack_[2].value.as < int > (), yystack_[1].value.as < int > ()); 
      	       }
#line 904 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 7: // filedesc: TOK_FILE TOK_COLON TOK_IDENTIFIER
#line 199 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
          { 
            yylhs.value.as < std::string > () = yystack_[0].value.as < std::string > (); 
          }
#line 912 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 8: // indexdesc: TOK_INDEX TOK_COLON integernumber
#line 205 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
	   yylhs.value.as < int > () = yystack_[0].value.as < int > (); 
         }
#line 920 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 9: // indexdatedesc: TOK_INDEX TOK_DATE TOK_COLON integernumber
#line 211 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
              { 
	      	yylhs.value.as < int > () =  yystack_[0].value.as < int > (); 
	      }
#line 928 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 10: // pldesc: TOK_PL TOK_COLON number TOK_PERCENT
#line 217 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
	   //printf ("Found nonterminal PL: %f\n", n->getAsDouble ()); 
       	   yylhs.value.as < decimal7 * > () = yystack_[1].value.as < decimal7 * > (); 
     	 }
#line 937 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 11: // pldesc: TOK_PL TOK_COLON integernumber TOK_PERCENT
#line 222 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
	   yylhs.value.as < decimal7 * > () = astFactory.getDecimalNumber (yystack_[1].value.as < int > ()); 
	 }
#line 945 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 12: // psdesc: TOK_PS TOK_COLON number TOK_PERCENT
#line 228 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
	   //printf ("Found nonterminal PS: %f\n", n->getAsDouble ()); 
       	   yylhs.value.as < decimal7 * > () = yystack_[1].value.as < decimal7 * > (); 
     	 }
#line 954 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 13: // psdesc: TOK_PS TOK_COLON integernumber TOK_PERCENT
#line 233 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
	   yylhs.value.as < decimal7 * > () = astFactory.getDecimalNumber (yystack_[1].value.as < int > ()); 
	 }
#line 962 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 14: // tradesdesc: TOK_TRADES TOK_COLON integernumber
#line 239 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
             { 
	       yylhs.value.as < int > () = yystack_[0].value.as < int > (); 
	     }
#line 970 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 15: // cldesc: TOK_CL TOK_COLON integernumber
#line 245 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
          { 
	    yylhs.value.as < int > () = yystack_[0].value.as < int > (); 
	  }
#line 978 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 16: // cldesc: TOK_CL TOK_COLON TOK_MINUS
#line 249 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
          { 
	    yylhs.value.as < int > () = 1; 
	  }
#line 986 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 17: // conds: ohlc_comparison
#line 255 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf ("Found comparison\n"); 
          yylhs.value.as < PatternExpression * > () = yystack_[0].value.as < PatternExpression * > (); 
        }
#line 995 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 18: // conds: conds TOK_AND ohlc_comparison
#line 260 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf ("Found recursive comparison\n"); 
       	  yylhs.value.as < PatternExpression * > () = new AndExpr (yystack_[2].value.as < PatternExpression * > (), yystack_[0].value.as < PatternExpression * > ()); 
      	}
#line 1004 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 19: // ohlc_comparison: ohlcref TOK_GREATER_THAN ohlcref
#line 267 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                  { 
		    //printf ("Found greater than ohlc comparison \n"); 
        	    yylhs.value.as < PatternExpression * > () = new GreaterThanExpr (yystack_[2].value.as < PriceBarReference * > (), yystack_[0].value.as < PriceBarReference * > ()); 
      		  }
#line 1013 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 20: // ohlcref: TOK_OPEN TOK_OF integernumber TOK_BARS TOK_AGO
#line 274 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
          { 
	    //printf("Found ohlc ref for open\n"); 
      	    yylhs.value.as < PriceBarReference * > () = astFactory.getPriceOpen (yystack_[2].value.as < int > ()); 
	  }
#line 1022 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 21: // ohlcref: TOK_HIGH TOK_OF integernumber TOK_BARS TOK_AGO
#line 279 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
       	   //printf("Found ohlc ref for high\n"); 
      	   yylhs.value.as < PriceBarReference * > () = astFactory.getPriceHigh (yystack_[2].value.as < int > ()); 
   	 }
#line 1031 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 22: // ohlcref: TOK_LOW TOK_OF integernumber TOK_BARS TOK_AGO
#line 284 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         { 
	   //printf("Found ohlc ref for low\n"); 
       	   yylhs.value.as < PriceBarReference * > () = astFactory.getPriceLow (yystack_[2].value.as < int > ()); 
	 }
#line 1040 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 23: // ohlcref: TOK_CLOSE TOK_OF integernumber TOK_BARS TOK_AGO
#line 289 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getPriceClose (yystack_[2].value.as < int > ()); 
        }
#line 1049 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 24: // ohlcref: TOK_VOLUME TOK_OF integernumber TOK_BARS TOK_AGO
#line 294 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getVolume (yystack_[2].value.as < int > ()); 
        }
#line 1058 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 25: // ohlcref: TOK_ROC1 TOK_OF integernumber TOK_BARS TOK_AGO
#line 299 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getRoc1 (yystack_[2].value.as < int > ()); 
        }
#line 1067 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 26: // ohlcref: TOK_IBS1 TOK_OF integernumber TOK_BARS TOK_AGO
#line 304 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getIBS1 (yystack_[2].value.as < int > ()); 
        }
#line 1076 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 27: // ohlcref: TOK_IBS2 TOK_OF integernumber TOK_BARS TOK_AGO
#line 309 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getIBS2 (yystack_[2].value.as < int > ()); 
        }
#line 1085 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 28: // ohlcref: TOK_IBS3 TOK_OF integernumber TOK_BARS TOK_AGO
#line 314 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getIBS3 (yystack_[2].value.as < int > ()); 
        }
#line 1094 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 29: // ohlcref: TOK_MEANDER TOK_OF integernumber TOK_BARS TOK_AGO
#line 319 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getMeander (yystack_[2].value.as < int > ()); 
        }
#line 1103 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 30: // ohlcref: TOK_VCHARTLOW TOK_OF integernumber TOK_BARS TOK_AGO
#line 324 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getVChartLow (yystack_[2].value.as < int > ()); 
        }
#line 1112 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 31: // ohlcref: TOK_VCHARTHIGH TOK_OF integernumber TOK_BARS TOK_AGO
#line 329 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
        { 
	  //printf("Found ohlc ref for close\n"); 
       	  yylhs.value.as < PriceBarReference * > () = astFactory.getVChartHigh (yystack_[2].value.as < int > ()); 
        }
#line 1121 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 32: // entrystmt: TOK_BUY TOK_NEXT TOK_BAR TOK_ON TOK_THE TOK_OPEN
#line 342 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
            {
		//printf ("Found long market entry on open\n"); 
      		yylhs.value.as < MarketEntryExpression * > () = astFactory.getLongMarketEntryOnOpen(); 
	    }
#line 1130 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 33: // entrystmt: TOK_SELL TOK_NEXT TOK_BAR TOK_ON TOK_THE TOK_OPEN
#line 347 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
            {
		//printf ("Found short market entry on open\n"); 
      		yylhs.value.as < MarketEntryExpression * > () = astFactory.getShortMarketEntryOnOpen(); 
	    }
#line 1139 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 34: // profitstmt: TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS number TOK_PERCENT
#line 354 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
             { 
	       //printf ("Found long side profit target\n"); 
       	       yylhs.value.as < ProfitTargetInPercentExpression * > () = astFactory.getLongProfitTarget(yystack_[1].value.as < decimal7 * > ()); 
      	     }
#line 1148 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 35: // profitstmt: TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS integernumber TOK_PERCENT
#line 359 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
             { 
	       //printf ("Found long side profit target\n"); 
       	       yylhs.value.as < ProfitTargetInPercentExpression * > () = astFactory.getLongProfitTarget(astFactory.getDecimalNumber (yystack_[1].value.as < int > ())); 
      	     }
#line 1157 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 36: // profitstmt: TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS number TOK_PERCENT
#line 364 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
             { 
	       //printf ("Found short side profit target"); 
	       yylhs.value.as < ProfitTargetInPercentExpression * > () = astFactory.getShortProfitTarget(yystack_[1].value.as < decimal7 * > ()); 
      	     }
#line 1166 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 37: // profitstmt: TOK_PROFIT TOK_TARGET TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS integernumber TOK_PERCENT
#line 369 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
             { 
	       //printf ("Found short side profit target"); 
	       yylhs.value.as < ProfitTargetInPercentExpression * > () = astFactory.getShortProfitTarget(astFactory.getDecimalNumber (yystack_[1].value.as < int > ())); 
      	     }
#line 1175 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 38: // stopstmt: TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS number TOK_PERCENT
#line 376 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
            {
		//printf("Found short stop loss statement\n"); 
       		yylhs.value.as < StopLossInPercentExpression * > () = astFactory.getShortStopLoss(yystack_[1].value.as < decimal7 * > ()); 
            }
#line 1184 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 39: // stopstmt: TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_PLUS integernumber TOK_PERCENT
#line 381 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
            {
		//printf("Found short stop loss statement\n"); 
       		yylhs.value.as < StopLossInPercentExpression * > () = astFactory.getShortStopLoss(astFactory.getDecimalNumber (yystack_[1].value.as < int > ())); 
            }
#line 1193 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 40: // stopstmt: TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS number TOK_PERCENT
#line 386 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
           {
		//printf("Found long stop loss statement\n"); 
 		yylhs.value.as < StopLossInPercentExpression * > () = astFactory.getLongStopLoss(yystack_[1].value.as < decimal7 * > ()); 
           }
#line 1202 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 41: // stopstmt: TOK_STOP TOK_LOSS TOK_AT TOK_ENTRY TOK_PRICE TOK_MINUS integernumber TOK_PERCENT
#line 391 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
           {
		//printf("Found long stop loss statement\n"); 
 		yylhs.value.as < StopLossInPercentExpression * > () = astFactory.getLongStopLoss(astFactory.getDecimalNumber (yystack_[1].value.as < int > ())); 
           }
#line 1211 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 42: // integernumber: TOK_INT_NUM
#line 398 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                { 
		  //printf ("Found integer number %d\n", num); 
      		  yylhs.value.as < int > () = yystack_[0].value.as < int > (); 
      		}
#line 1220 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 43: // number: TOK_FLOAT_NUM
#line 405 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
         {
		//printf ("Found float number %f\n", num); 
      		yylhs.value.as < decimal7 * > () =  astFactory.getDecimalNumber ((char *)yystack_[0].value.as < std::string > ().c_str()); 
         }
#line 1229 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 44: // pattern_volatility_attr: TOK_VOLATILITY TOK_COLON volatility_attr
#line 412 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                          {
				yylhs.value.as < PriceActionLabPattern::VolatilityAttribute > () = yystack_[0].value.as < PriceActionLabPattern::VolatilityAttribute > ();
   			  }
#line 1237 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 45: // pattern_volatility_attr: %empty
#line 416 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                          {
				//printf ("Found empty volatility alternative\n");
     				yylhs.value.as < PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_NONE;
   			  }
#line 1246 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 46: // pattern_portfolio_filter_attr: TOK_PORTFOLIO TOK_COLON portfolio_attr
#line 423 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                                {
					yylhs.value.as < PriceActionLabPattern::PortfolioAttribute > () = yystack_[0].value.as < PriceActionLabPattern::PortfolioAttribute > ();;
				}
#line 1254 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 47: // pattern_portfolio_filter_attr: %empty
#line 427 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                                {
					yylhs.value.as < PriceActionLabPattern::PortfolioAttribute > () = PriceActionLabPattern::PORTFOLIO_FILTER_NONE;
				}
#line 1262 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 48: // volatility_attr: TOK_LOW_VOL
#line 433 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                  {
			//printf ("Found low volatility token\n");
			yylhs.value.as < PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_LOW;
   		  }
#line 1271 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 49: // volatility_attr: TOK_NORMAL_VOL
#line 438 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                {
			//printf ("Found normal volatility token\n");
			yylhs.value.as < PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_NORMAL;
		}
#line 1280 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 50: // volatility_attr: TOK_HIGH_VOL
#line 443 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                {
			//printf ("Found high volatility token\n");
			yylhs.value.as < PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_HIGH;
		}
#line 1289 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 51: // volatility_attr: TOK_VERY_HIGH_VOL
#line 448 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                {
			//printf ("Found very high volatility token\n");
			yylhs.value.as < PriceActionLabPattern::VolatilityAttribute > () = PriceActionLabPattern::VOLATILITY_VERY_HIGH;
		}
#line 1298 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 52: // portfolio_attr: TOK_PORT_LONG_FILTER
#line 455 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                 {
			yylhs.value.as < PriceActionLabPattern::PortfolioAttribute > () = PriceActionLabPattern::PORTFOLIO_FILTER_LONG;
		 }
#line 1306 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;

  case 53: // portfolio_attr: TOK_PORT_SHORT_FILTER
#line 459 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
                 {
			yylhs.value.as < PriceActionLabPattern::PortfolioAttribute > () = PriceActionLabPattern::PORTFOLIO_FILTER_SHORT;
		 }
#line 1314 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"
    break;


#line 1318 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"

            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
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
        context yyctx (*this, yyla);
        std::string msg = yysyntax_error_ (yyctx);
        error (yyla.location, YY_MOVE (msg));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.kind () == symbol_kind::S_YYEOF)
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
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;)
      {
        yyn = yypact_[+yystack_[0].state];
        if (!yy_pact_value_is_default_ (yyn))
          {
            yyn += symbol_kind::S_YYerror;
            if (0 <= yyn && yyn <= yylast_
                && yycheck_[yyn] == symbol_kind::S_YYerror)
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
    {
      stack_symbol_type error_token;

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = state_type (yyn);
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    YY_STACK_PRINT ();
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  PalParser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what ());
  }

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
        std::string yyr;
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
              else
                goto append;

            append:
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

  std::string
  PalParser::symbol_name (symbol_kind_type yysymbol)
  {
    return yytnamerr_ (yytname_[yysymbol]);
  }



  // PalParser::context.
  PalParser::context::context (const PalParser& yyparser, const symbol_type& yyla)
    : yyparser_ (yyparser)
    , yyla_ (yyla)
  {}

  int
  PalParser::context::expected_tokens (symbol_kind_type yyarg[], int yyargn) const
  {
    // Actual number of expected tokens
    int yycount = 0;

    const int yyn = yypact_[+yyparser_.yystack_[0].state];
    if (!yy_pact_value_is_default_ (yyn))
      {
        /* Start YYX at -YYN if negative to avoid negative indexes in
           YYCHECK.  In other words, skip the first -YYN actions for
           this state because they are default actions.  */
        const int yyxbegin = yyn < 0 ? -yyn : 0;
        // Stay within bounds of both yycheck and yytname.
        const int yychecklim = yylast_ - yyn + 1;
        const int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
        for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
          if (yycheck_[yyx + yyn] == yyx && yyx != symbol_kind::S_YYerror
              && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
            {
              if (!yyarg)
                ++yycount;
              else if (yycount == yyargn)
                return 0;
              else
                yyarg[yycount++] = YY_CAST (symbol_kind_type, yyx);
            }
      }

    if (yyarg && yycount == 0 && 0 < yyargn)
      yyarg[0] = symbol_kind::S_YYEMPTY;
    return yycount;
  }






  int
  PalParser::yy_syntax_error_arguments_ (const context& yyctx,
                                                 symbol_kind_type yyarg[], int yyargn) const
  {
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
         scanner and before detecting a syntax error.  Thus, state merging
         (from LALR or IELR) and default reductions corrupt the expected
         token list.  However, the list is correct for canonical LR with
         one exception: it will still contain any token that will not be
         accepted due to an error action in a later state.
    */

    if (!yyctx.lookahead ().empty ())
      {
        if (yyarg)
          yyarg[0] = yyctx.token ();
        int yyn = yyctx.expected_tokens (yyarg ? yyarg + 1 : yyarg, yyargn - 1);
        return yyn + 1;
      }
    return 0;
  }

  // Generate an error message.
  std::string
  PalParser::yysyntax_error_ (const context& yyctx) const
  {
    // Its maximum.
    enum { YYARGS_MAX = 5 };
    // Arguments of yyformat.
    symbol_kind_type yyarg[YYARGS_MAX];
    int yycount = yy_syntax_error_arguments_ (yyctx, yyarg, YYARGS_MAX);

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
      default: // Avoid compiler warnings.
        YYCASE_ (0, YY_("syntax error"));
        YYCASE_ (1, YY_("syntax error, unexpected %s"));
        YYCASE_ (2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_ (3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_ (4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_ (5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    std::ptrdiff_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += symbol_name (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char PalParser::yypact_ninf_ = -74;

  const signed char PalParser::yytable_ninf_ = -1;

  const short
  PalParser::yypact_[] =
  {
      -6,    -3,    48,    -6,   -74,    39,    42,     3,   -74,   -74,
      -1,    50,    44,    10,    46,     5,   -74,    56,    13,    14,
     -34,    52,    12,   -74,   -74,    53,    54,    17,   -74,   -74,
     -74,   -74,   -74,   -16,    34,    41,    43,    45,    47,    49,
      51,    57,    58,    59,    60,    61,    -9,   -74,    55,    56,
       1,    62,    19,   -74,   -74,   -74,    56,    56,    56,    56,
      56,    56,    56,    56,    56,    56,    56,    56,    -8,    12,
      12,   -74,   -74,    67,    69,     1,    68,    29,    63,    64,
      70,    71,    72,    73,    74,    75,    76,    79,    80,    81,
      65,    66,    88,   -74,   -74,   -74,   -74,    82,    83,    56,
      78,    86,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,    94,   107,   108,   -74,   -74,   -74,
      36,   -74,   -74,   -74,   -74,   -74,   -74,   -74,   -74,   -74,
     -74,   -74,   -74,   -74,    77,   109,   110,   111,   -74,   -74,
     112,   114,   113,   115,    84,    85,   116,    37,   -74,   -74,
     -74,   117,   118,    38,   120,     1,     1,   121,    93,   129,
     130,   132,    40,   -74,   -74,   -74,   -74,     1,     1,   133,
     135,   137,   142,   -74,   -74,   -74,   -74
  };

  const signed char
  PalParser::yydefact_[] =
  {
       0,     0,     0,     2,     3,     0,     0,     0,     1,     4,
      45,     0,     0,     0,     0,    47,     7,     0,     0,     0,
       0,     0,     0,    42,     8,     0,     0,     0,    48,    50,
      51,    49,    44,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    17,     0,     0,
       0,     0,     0,    52,    53,    46,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    43,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    18,    19,    11,    10,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    13,    12,    14,
       0,     6,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,     0,     0,     0,     0,    16,    15,
       0,     0,     0,     0,     0,     0,     0,     0,     5,    32,
      33,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    35,    34,    37,    36,     0,     0,     0,
       0,     0,     0,    39,    38,    41,    40
  };

  const signed char
  PalParser::yypgoto_[] =
  {
     -74,   -74,   -74,    89,   -74,   -74,   -74,   -74,   -74,   -74,
     -74,   -74,   -74,    24,    32,   -74,   -74,   -74,   -49,   -73,
     -74,   -74,   -74,   -74
  };

  const unsigned char
  PalParser::yydefgoto_[] =
  {
       0,     2,     3,     4,     5,     7,    13,    19,    27,    52,
      77,   101,    46,    47,    48,    92,   137,   148,    24,    74,
      15,    22,    32,    55
  };

  const unsigned char
  PalParser::yytable_[] =
  {
      71,    73,    98,     1,    23,    68,    72,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    69,
      28,    29,    90,    91,    30,    31,    97,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    23,
      53,    54,     6,   138,   155,   156,   167,   168,     8,    12,
     119,    14,    10,    11,    16,    17,    18,    20,    21,    23,
      25,    56,    26,    33,    49,    50,    51,    70,    57,    76,
      58,   139,    59,    75,    60,    95,    61,    96,    62,    99,
     100,   152,   159,   161,    63,    64,    65,    66,    67,   120,
     117,   118,     9,    93,   170,   172,   121,   114,   115,   149,
     150,   163,    94,     0,   102,   103,   158,   160,     0,     0,
     140,   104,   105,   106,   107,   108,   109,   110,   169,   171,
     111,   112,   113,   116,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   164,   165,   143,
     166,   173,   141,   174,   136,   175,   144,   142,   145,   135,
     176,   146,     0,     0,     0,   151,   154,   153,   147,   157,
       0,   162
  };

  const short
  PalParser::yycheck_[] =
  {
      49,    50,    75,     9,     3,    14,     5,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    28,
      54,    55,    30,    31,    58,    59,    75,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,     3,
      56,    57,    45,     7,     6,     7,     6,     7,     0,    46,
      99,    52,    13,    11,     4,    11,    46,    11,    53,     3,
      47,    27,    48,    11,    11,    11,    49,    12,    27,    50,
      27,   120,    27,    11,    27,     8,    27,     8,    27,    11,
      51,    44,   155,   156,    27,    27,    27,    27,    27,    11,
       8,     8,     3,    69,   167,   168,    10,    32,    32,    15,
      15,     8,    70,    -1,    41,    41,   155,   156,    -1,    -1,
      33,    41,    41,    41,    41,    41,    41,    41,   167,   168,
      41,    41,    41,    35,    29,    29,    29,    29,    29,    29,
      29,    29,    29,    29,    29,    29,    42,     8,     8,    28,
       8,     8,    33,     8,    36,     8,    34,    37,    34,    42,
       8,    38,    -1,    -1,    -1,    39,    38,    40,    43,    39,
      -1,    40
  };

  const signed char
  PalParser::yystos_[] =
  {
       0,     9,    64,    65,    66,    67,    45,    68,     0,    66,
      13,    11,    46,    69,    52,    83,     4,    11,    46,    70,
      11,    53,    84,     3,    81,    47,    48,    71,    54,    55,
      58,    59,    85,    11,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    75,    76,    77,    11,
      11,    49,    72,    56,    57,    86,    27,    27,    27,    27,
      27,    27,    27,    27,    27,    27,    27,    27,    14,    28,
      12,    81,     5,    81,    82,    11,    50,    73,    81,    81,
      81,    81,    81,    81,    81,    81,    81,    81,    81,    81,
      30,    31,    78,    76,    77,     8,     8,    81,    82,    11,
      51,    74,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41,    32,    32,    35,     8,     8,    81,
      11,    10,    29,    29,    29,    29,    29,    29,    29,    29,
      29,    29,    29,    29,    42,    42,    36,    79,     7,    81,
      33,    33,    37,    28,    34,    34,    38,    43,    80,    15,
      15,    39,    44,    40,    38,     6,     7,    39,    81,    82,
      81,    82,    40,     8,     8,     8,     8,     6,     7,    81,
      82,    81,    82,     8,     8,     8,     8
  };

  const signed char
  PalParser::yyr1_[] =
  {
       0,    63,    64,    65,    65,    66,    67,    68,    69,    70,
      71,    71,    72,    72,    73,    74,    74,    75,    75,    76,
      77,    77,    77,    77,    77,    77,    77,    77,    77,    77,
      77,    77,    78,    78,    79,    79,    79,    79,    80,    80,
      80,    80,    81,    82,    83,    83,    84,    84,    85,    85,
      85,    85,    86,    86
  };

  const signed char
  PalParser::yyr2_[] =
  {
       0,     2,     1,     1,     2,    11,     9,     3,     3,     4,
       4,     4,     4,     4,     3,     3,     3,     1,     3,     3,
       5,     5,     5,     5,     5,     5,     5,     5,     5,     5,
       5,     5,     6,     6,     8,     8,     8,     8,     8,     8,
       8,     8,     1,     1,     3,     0,     3,     0,     1,     1,
       1,     1,     1,     1
  };


#if YYDEBUG || 1
  // YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
  // First, the terminals, then, starting at \a YYNTOKENS, nonterminals.
  const char*
  const PalParser::yytname_[] =
  {
  "TOK_EOF", "error", "\"invalid token\"", "TOK_INT_NUM",
  "TOK_IDENTIFIER", "TOK_FLOAT_NUM", "TOK_PLUS", "TOK_MINUS",
  "TOK_PERCENT", "TOK_LBRACE", "TOK_RBRACE", "TOK_COLON",
  "TOK_GREATER_THAN", "TOK_IF", "TOK_THEN", "TOK_OPEN", "TOK_HIGH",
  "TOK_LOW", "TOK_CLOSE", "TOK_VOLUME", "TOK_ROC1", "TOK_IBS1", "TOK_IBS2",
  "TOK_IBS3", "TOK_MEANDER", "TOK_VCHARTLOW", "TOK_VCHARTHIGH", "TOK_OF",
  "TOK_AND", "TOK_AGO", "TOK_BUY", "TOK_SELL", "TOK_NEXT", "TOK_ON",
  "TOK_THE", "TOK_WITH", "TOK_PROFIT", "TOK_TARGET", "TOK_AT", "TOK_ENTRY",
  "TOK_PRICE", "TOK_BARS", "TOK_BAR", "TOK_STOP", "TOK_LOSS", "TOK_FILE",
  "TOK_INDEX", "TOK_DATE", "TOK_PL", "TOK_PS", "TOK_TRADES", "TOK_CL",
  "TOK_VOLATILITY", "TOK_PORTFOLIO", "TOK_LOW_VOL", "TOK_HIGH_VOL",
  "TOK_PORT_LONG_FILTER", "TOK_PORT_SHORT_FILTER", "TOK_VERY_HIGH_VOL",
  "TOK_NORMAL_VOL", "TOK_MOMERSION_FILTER", "TOK_LEFT_PAREN",
  "TOK_RIGHT_PAREN", "$accept", "program", "patterns", "pattern",
  "patterndescr", "filedesc", "indexdesc", "indexdatedesc", "pldesc",
  "psdesc", "tradesdesc", "cldesc", "conds", "ohlc_comparison", "ohlcref",
  "entrystmt", "profitstmt", "stopstmt", "integernumber", "number",
  "pattern_volatility_attr", "pattern_portfolio_filter_attr",
  "volatility_attr", "portfolio_attr", YY_NULLPTR
  };
#endif


#if YYDEBUG
  const short
  PalParser::yyrline_[] =
  {
       0,   166,   166,   172,   177,   184,   191,   198,   204,   210,
     216,   221,   227,   232,   238,   244,   248,   254,   259,   266,
     273,   278,   283,   288,   293,   298,   303,   308,   313,   318,
     323,   328,   341,   346,   353,   358,   363,   368,   375,   380,
     385,   390,   397,   404,   411,   415,   422,   426,   432,   437,
     442,   447,   454,   458
  };

  void
  PalParser::yy_stack_print_ () const
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << int (i->state);
    *yycdebug_ << '\n';
  }

  void
  PalParser::yy_reduce_print_ (int yyrule) const
  {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG


#line 10 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
} // mkc_palast
#line 1897 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.cpp"

#line 464 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
 /*** Additional Code ***/

void mkc_palast::PalParser::error(const mkc_palast::PalParser::location_type& l,
                                  const std::string& message)
{
    cout << "Error: " << message << endl << "Error location: " << driver.location() << endl;

}
