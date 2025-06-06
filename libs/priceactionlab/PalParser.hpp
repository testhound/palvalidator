// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton interface for Bison LALR(1) parsers in C++

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


/**
 ** \file /workspace/codementor/palvalidator/libs/priceactionlab/PalParser.hpp
 ** Define the mkc_palast::parser class.
 */

// C++ LALR(1) parser skeleton written by Akim Demaille.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.

#ifndef YY_YY_WORKSPACE_CODEMENTOR_PALVALIDATOR_LIBS_PRICEACTIONLAB_PALPARSER_HPP_INCLUDED
# define YY_YY_WORKSPACE_CODEMENTOR_PALVALIDATOR_LIBS_PRICEACTIONLAB_PALPARSER_HPP_INCLUDED
// "%code requires" blocks.
#line 13 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"

    #include <iostream>
    #include <string>
    #include <stdint.h>
    #include "PalAst.h"

    using namespace std;

    namespace mkc_palast {
        class Scanner;
        class PalParseDriver;
    }

#line 63 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.hpp"

# include <cassert>
# include <cstdlib> // std::abort
# include <iostream>
# include <stdexcept>
# include <string>
# include <vector>

#if defined __cplusplus
# define YY_CPLUSPLUS __cplusplus
#else
# define YY_CPLUSPLUS 199711L
#endif

// Support move semantics when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_MOVE           std::move
# define YY_MOVE_OR_COPY   move
# define YY_MOVE_REF(Type) Type&&
# define YY_RVREF(Type)    Type&&
# define YY_COPY(Type)     Type
#else
# define YY_MOVE
# define YY_MOVE_OR_COPY   copy
# define YY_MOVE_REF(Type) Type&
# define YY_RVREF(Type)    const Type&
# define YY_COPY(Type)     const Type&
#endif

// Support noexcept when possible.
#if 201103L <= YY_CPLUSPLUS
# define YY_NOEXCEPT noexcept
# define YY_NOTHROW
#else
# define YY_NOEXCEPT
# define YY_NOTHROW throw ()
#endif

// Support constexpr when possible.
#if 201703 <= YY_CPLUSPLUS
# define YY_CONSTEXPR constexpr
#else
# define YY_CONSTEXPR
#endif
# include "location.hh"
#include <typeinfo>
#ifndef YY_ASSERT
# include <cassert>
# define YY_ASSERT assert
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

#line 10 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
namespace mkc_palast {
#line 204 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.hpp"




  /// A Bison parser.
  class PalParser
  {
  public:
#ifdef YYSTYPE
# ifdef __GNUC__
#  pragma GCC message "bison: do not #define YYSTYPE in C++, use %define api.value.type"
# endif
    typedef YYSTYPE value_type;
#else
  /// A buffer to store and retrieve objects.
  ///
  /// Sort of a variant, but does not keep track of the nature
  /// of the stored data, since that knowledge is available
  /// via the current parser state.
  class value_type
  {
  public:
    /// Type of *this.
    typedef value_type self_type;

    /// Empty construction.
    value_type () YY_NOEXCEPT
      : yyraw_ ()
      , yytypeid_ (YY_NULLPTR)
    {}

    /// Construct and fill.
    template <typename T>
    value_type (YY_RVREF (T) t)
      : yytypeid_ (&typeid (T))
    {
      YY_ASSERT (sizeof (T) <= size);
      new (yyas_<T> ()) T (YY_MOVE (t));
    }

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    value_type (const self_type&) = delete;
    /// Non copyable.
    self_type& operator= (const self_type&) = delete;
#endif

    /// Destruction, allowed only if empty.
    ~value_type () YY_NOEXCEPT
    {
      YY_ASSERT (!yytypeid_);
    }

# if 201103L <= YY_CPLUSPLUS
    /// Instantiate a \a T in here from \a t.
    template <typename T, typename... U>
    T&
    emplace (U&&... u)
    {
      YY_ASSERT (!yytypeid_);
      YY_ASSERT (sizeof (T) <= size);
      yytypeid_ = & typeid (T);
      return *new (yyas_<T> ()) T (std::forward <U>(u)...);
    }
# else
    /// Instantiate an empty \a T in here.
    template <typename T>
    T&
    emplace ()
    {
      YY_ASSERT (!yytypeid_);
      YY_ASSERT (sizeof (T) <= size);
      yytypeid_ = & typeid (T);
      return *new (yyas_<T> ()) T ();
    }

    /// Instantiate a \a T in here from \a t.
    template <typename T>
    T&
    emplace (const T& t)
    {
      YY_ASSERT (!yytypeid_);
      YY_ASSERT (sizeof (T) <= size);
      yytypeid_ = & typeid (T);
      return *new (yyas_<T> ()) T (t);
    }
# endif

    /// Instantiate an empty \a T in here.
    /// Obsolete, use emplace.
    template <typename T>
    T&
    build ()
    {
      return emplace<T> ();
    }

    /// Instantiate a \a T in here from \a t.
    /// Obsolete, use emplace.
    template <typename T>
    T&
    build (const T& t)
    {
      return emplace<T> (t);
    }

    /// Accessor to a built \a T.
    template <typename T>
    T&
    as () YY_NOEXCEPT
    {
      YY_ASSERT (yytypeid_);
      YY_ASSERT (*yytypeid_ == typeid (T));
      YY_ASSERT (sizeof (T) <= size);
      return *yyas_<T> ();
    }

    /// Const accessor to a built \a T (for %printer).
    template <typename T>
    const T&
    as () const YY_NOEXCEPT
    {
      YY_ASSERT (yytypeid_);
      YY_ASSERT (*yytypeid_ == typeid (T));
      YY_ASSERT (sizeof (T) <= size);
      return *yyas_<T> ();
    }

    /// Swap the content with \a that, of same type.
    ///
    /// Both variants must be built beforehand, because swapping the actual
    /// data requires reading it (with as()), and this is not possible on
    /// unconstructed variants: it would require some dynamic testing, which
    /// should not be the variant's responsibility.
    /// Swapping between built and (possibly) non-built is done with
    /// self_type::move ().
    template <typename T>
    void
    swap (self_type& that) YY_NOEXCEPT
    {
      YY_ASSERT (yytypeid_);
      YY_ASSERT (*yytypeid_ == *that.yytypeid_);
      std::swap (as<T> (), that.as<T> ());
    }

    /// Move the content of \a that to this.
    ///
    /// Destroys \a that.
    template <typename T>
    void
    move (self_type& that)
    {
# if 201103L <= YY_CPLUSPLUS
      emplace<T> (std::move (that.as<T> ()));
# else
      emplace<T> ();
      swap<T> (that);
# endif
      that.destroy<T> ();
    }

# if 201103L <= YY_CPLUSPLUS
    /// Move the content of \a that to this.
    template <typename T>
    void
    move (self_type&& that)
    {
      emplace<T> (std::move (that.as<T> ()));
      that.destroy<T> ();
    }
#endif

    /// Copy the content of \a that to this.
    template <typename T>
    void
    copy (const self_type& that)
    {
      emplace<T> (that.as<T> ());
    }

    /// Destroy the stored \a T.
    template <typename T>
    void
    destroy ()
    {
      as<T> ().~T ();
      yytypeid_ = YY_NULLPTR;
    }

  private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    value_type (const self_type&);
    /// Non copyable.
    self_type& operator= (const self_type&);
#endif

    /// Accessor to raw memory as \a T.
    template <typename T>
    T*
    yyas_ () YY_NOEXCEPT
    {
      void *yyp = yyraw_;
      return static_cast<T*> (yyp);
     }

    /// Const accessor to raw memory as \a T.
    template <typename T>
    const T*
    yyas_ () const YY_NOEXCEPT
    {
      const void *yyp = yyraw_;
      return static_cast<const T*> (yyp);
     }

    /// An auxiliary type to compute the largest semantic type.
    union union_type
    {
      // entrystmt
      char dummy1[sizeof (MarketEntryExpression *)];

      // patterndescr
      char dummy2[sizeof (PatternDescription *)];

      // conds
      // ohlc_comparison
      char dummy3[sizeof (PatternExpression *)];

      // pattern
      char dummy4[sizeof (PriceActionLabPattern *)];

      // pattern_portfolio_filter_attr
      // portfolio_attr
      char dummy5[sizeof (PriceActionLabPattern::PortfolioAttribute)];

      // pattern_volatility_attr
      // volatility_attr
      char dummy6[sizeof (PriceActionLabPattern::VolatilityAttribute)];

      // ohlcref
      char dummy7[sizeof (PriceBarReference *)];

      // profitstmt
      char dummy8[sizeof (ProfitTargetInPercentExpression *)];

      // stopstmt
      char dummy9[sizeof (StopLossInPercentExpression *)];

      // pldesc
      // psdesc
      // number
      char dummy10[sizeof (decimal7 *)];

      // TOK_INT_NUM
      // indexdesc
      // indexdatedesc
      // tradesdesc
      // cldesc
      // integernumber
      char dummy11[sizeof (int)];

      // TOK_IDENTIFIER
      // TOK_FLOAT_NUM
      // filedesc
      char dummy12[sizeof (std::string)];
    };

    /// The size of the largest semantic type.
    enum { size = sizeof (union_type) };

    /// A buffer to store semantic values.
    union
    {
      /// Strongest alignment constraints.
      long double yyalign_me_;
      /// A buffer large enough to store any of the semantic values.
      char yyraw_[size];
    };

    /// Whether the content is built: if defined, the name of the stored type.
    const std::type_info *yytypeid_;
  };

#endif
    /// Backward compatibility (Bison 3.8).
    typedef value_type semantic_type;

    /// Symbol locations.
    typedef location location_type;

    /// Syntax errors thrown from user actions.
    struct syntax_error : std::runtime_error
    {
      syntax_error (const location_type& l, const std::string& m)
        : std::runtime_error (m)
        , location (l)
      {}

      syntax_error (const syntax_error& s)
        : std::runtime_error (s.what ())
        , location (s.location)
      {}

      ~syntax_error () YY_NOEXCEPT YY_NOTHROW;

      location_type location;
    };

    /// Token kinds.
    struct token
    {
      enum token_kind_type
      {
        YYEMPTY = -2,
    TOK_EOF = 0,                   // TOK_EOF
    YYerror = 256,                 // error
    YYUNDEF = 257,                 // "invalid token"
    TOK_INT_NUM = 258,             // TOK_INT_NUM
    TOK_IDENTIFIER = 259,          // TOK_IDENTIFIER
    TOK_FLOAT_NUM = 260,           // TOK_FLOAT_NUM
    TOK_PLUS = 261,                // TOK_PLUS
    TOK_MINUS = 262,               // TOK_MINUS
    TOK_PERCENT = 263,             // TOK_PERCENT
    TOK_LBRACE = 264,              // TOK_LBRACE
    TOK_RBRACE = 265,              // TOK_RBRACE
    TOK_COLON = 266,               // TOK_COLON
    TOK_GREATER_THAN = 267,        // TOK_GREATER_THAN
    TOK_IF = 268,                  // TOK_IF
    TOK_THEN = 269,                // TOK_THEN
    TOK_OPEN = 270,                // TOK_OPEN
    TOK_HIGH = 271,                // TOK_HIGH
    TOK_LOW = 272,                 // TOK_LOW
    TOK_CLOSE = 273,               // TOK_CLOSE
    TOK_VOLUME = 274,              // TOK_VOLUME
    TOK_ROC1 = 275,                // TOK_ROC1
    TOK_IBS1 = 276,                // TOK_IBS1
    TOK_IBS2 = 277,                // TOK_IBS2
    TOK_IBS3 = 278,                // TOK_IBS3
    TOK_MEANDER = 279,             // TOK_MEANDER
    TOK_VCHARTLOW = 280,           // TOK_VCHARTLOW
    TOK_VCHARTHIGH = 281,          // TOK_VCHARTHIGH
    TOK_OF = 282,                  // TOK_OF
    TOK_AND = 283,                 // TOK_AND
    TOK_AGO = 284,                 // TOK_AGO
    TOK_BUY = 285,                 // TOK_BUY
    TOK_SELL = 286,                // TOK_SELL
    TOK_NEXT = 287,                // TOK_NEXT
    TOK_ON = 288,                  // TOK_ON
    TOK_THE = 289,                 // TOK_THE
    TOK_WITH = 290,                // TOK_WITH
    TOK_PROFIT = 291,              // TOK_PROFIT
    TOK_TARGET = 292,              // TOK_TARGET
    TOK_AT = 293,                  // TOK_AT
    TOK_ENTRY = 294,               // TOK_ENTRY
    TOK_PRICE = 295,               // TOK_PRICE
    TOK_BARS = 296,                // TOK_BARS
    TOK_BAR = 297,                 // TOK_BAR
    TOK_STOP = 298,                // TOK_STOP
    TOK_LOSS = 299,                // TOK_LOSS
    TOK_FILE = 300,                // TOK_FILE
    TOK_INDEX = 301,               // TOK_INDEX
    TOK_DATE = 302,                // TOK_DATE
    TOK_PL = 303,                  // TOK_PL
    TOK_PS = 304,                  // TOK_PS
    TOK_TRADES = 305,              // TOK_TRADES
    TOK_CL = 306,                  // TOK_CL
    TOK_VOLATILITY = 307,          // TOK_VOLATILITY
    TOK_PORTFOLIO = 308,           // TOK_PORTFOLIO
    TOK_LOW_VOL = 309,             // TOK_LOW_VOL
    TOK_HIGH_VOL = 310,            // TOK_HIGH_VOL
    TOK_PORT_LONG_FILTER = 311,    // TOK_PORT_LONG_FILTER
    TOK_PORT_SHORT_FILTER = 312,   // TOK_PORT_SHORT_FILTER
    TOK_VERY_HIGH_VOL = 313,       // TOK_VERY_HIGH_VOL
    TOK_NORMAL_VOL = 314,          // TOK_NORMAL_VOL
    TOK_MOMERSION_FILTER = 315,    // TOK_MOMERSION_FILTER
    TOK_LEFT_PAREN = 316,          // TOK_LEFT_PAREN
    TOK_RIGHT_PAREN = 317          // TOK_RIGHT_PAREN
      };
      /// Backward compatibility alias (Bison 3.6).
      typedef token_kind_type yytokentype;
    };

    /// Token kind, as returned by yylex.
    typedef token::token_kind_type token_kind_type;

    /// Backward compatibility alias (Bison 3.6).
    typedef token_kind_type token_type;

    /// Symbol kinds.
    struct symbol_kind
    {
      enum symbol_kind_type
      {
        YYNTOKENS = 63, ///< Number of tokens.
        S_YYEMPTY = -2,
        S_YYEOF = 0,                             // TOK_EOF
        S_YYerror = 1,                           // error
        S_YYUNDEF = 2,                           // "invalid token"
        S_TOK_INT_NUM = 3,                       // TOK_INT_NUM
        S_TOK_IDENTIFIER = 4,                    // TOK_IDENTIFIER
        S_TOK_FLOAT_NUM = 5,                     // TOK_FLOAT_NUM
        S_TOK_PLUS = 6,                          // TOK_PLUS
        S_TOK_MINUS = 7,                         // TOK_MINUS
        S_TOK_PERCENT = 8,                       // TOK_PERCENT
        S_TOK_LBRACE = 9,                        // TOK_LBRACE
        S_TOK_RBRACE = 10,                       // TOK_RBRACE
        S_TOK_COLON = 11,                        // TOK_COLON
        S_TOK_GREATER_THAN = 12,                 // TOK_GREATER_THAN
        S_TOK_IF = 13,                           // TOK_IF
        S_TOK_THEN = 14,                         // TOK_THEN
        S_TOK_OPEN = 15,                         // TOK_OPEN
        S_TOK_HIGH = 16,                         // TOK_HIGH
        S_TOK_LOW = 17,                          // TOK_LOW
        S_TOK_CLOSE = 18,                        // TOK_CLOSE
        S_TOK_VOLUME = 19,                       // TOK_VOLUME
        S_TOK_ROC1 = 20,                         // TOK_ROC1
        S_TOK_IBS1 = 21,                         // TOK_IBS1
        S_TOK_IBS2 = 22,                         // TOK_IBS2
        S_TOK_IBS3 = 23,                         // TOK_IBS3
        S_TOK_MEANDER = 24,                      // TOK_MEANDER
        S_TOK_VCHARTLOW = 25,                    // TOK_VCHARTLOW
        S_TOK_VCHARTHIGH = 26,                   // TOK_VCHARTHIGH
        S_TOK_OF = 27,                           // TOK_OF
        S_TOK_AND = 28,                          // TOK_AND
        S_TOK_AGO = 29,                          // TOK_AGO
        S_TOK_BUY = 30,                          // TOK_BUY
        S_TOK_SELL = 31,                         // TOK_SELL
        S_TOK_NEXT = 32,                         // TOK_NEXT
        S_TOK_ON = 33,                           // TOK_ON
        S_TOK_THE = 34,                          // TOK_THE
        S_TOK_WITH = 35,                         // TOK_WITH
        S_TOK_PROFIT = 36,                       // TOK_PROFIT
        S_TOK_TARGET = 37,                       // TOK_TARGET
        S_TOK_AT = 38,                           // TOK_AT
        S_TOK_ENTRY = 39,                        // TOK_ENTRY
        S_TOK_PRICE = 40,                        // TOK_PRICE
        S_TOK_BARS = 41,                         // TOK_BARS
        S_TOK_BAR = 42,                          // TOK_BAR
        S_TOK_STOP = 43,                         // TOK_STOP
        S_TOK_LOSS = 44,                         // TOK_LOSS
        S_TOK_FILE = 45,                         // TOK_FILE
        S_TOK_INDEX = 46,                        // TOK_INDEX
        S_TOK_DATE = 47,                         // TOK_DATE
        S_TOK_PL = 48,                           // TOK_PL
        S_TOK_PS = 49,                           // TOK_PS
        S_TOK_TRADES = 50,                       // TOK_TRADES
        S_TOK_CL = 51,                           // TOK_CL
        S_TOK_VOLATILITY = 52,                   // TOK_VOLATILITY
        S_TOK_PORTFOLIO = 53,                    // TOK_PORTFOLIO
        S_TOK_LOW_VOL = 54,                      // TOK_LOW_VOL
        S_TOK_HIGH_VOL = 55,                     // TOK_HIGH_VOL
        S_TOK_PORT_LONG_FILTER = 56,             // TOK_PORT_LONG_FILTER
        S_TOK_PORT_SHORT_FILTER = 57,            // TOK_PORT_SHORT_FILTER
        S_TOK_VERY_HIGH_VOL = 58,                // TOK_VERY_HIGH_VOL
        S_TOK_NORMAL_VOL = 59,                   // TOK_NORMAL_VOL
        S_TOK_MOMERSION_FILTER = 60,             // TOK_MOMERSION_FILTER
        S_TOK_LEFT_PAREN = 61,                   // TOK_LEFT_PAREN
        S_TOK_RIGHT_PAREN = 62,                  // TOK_RIGHT_PAREN
        S_YYACCEPT = 63,                         // $accept
        S_program = 64,                          // program
        S_patterns = 65,                         // patterns
        S_pattern = 66,                          // pattern
        S_patterndescr = 67,                     // patterndescr
        S_filedesc = 68,                         // filedesc
        S_indexdesc = 69,                        // indexdesc
        S_indexdatedesc = 70,                    // indexdatedesc
        S_pldesc = 71,                           // pldesc
        S_psdesc = 72,                           // psdesc
        S_tradesdesc = 73,                       // tradesdesc
        S_cldesc = 74,                           // cldesc
        S_conds = 75,                            // conds
        S_ohlc_comparison = 76,                  // ohlc_comparison
        S_ohlcref = 77,                          // ohlcref
        S_entrystmt = 78,                        // entrystmt
        S_profitstmt = 79,                       // profitstmt
        S_stopstmt = 80,                         // stopstmt
        S_integernumber = 81,                    // integernumber
        S_number = 82,                           // number
        S_pattern_volatility_attr = 83,          // pattern_volatility_attr
        S_pattern_portfolio_filter_attr = 84,    // pattern_portfolio_filter_attr
        S_volatility_attr = 85,                  // volatility_attr
        S_portfolio_attr = 86                    // portfolio_attr
      };
    };

    /// (Internal) symbol kind.
    typedef symbol_kind::symbol_kind_type symbol_kind_type;

    /// The number of tokens.
    static const symbol_kind_type YYNTOKENS = symbol_kind::YYNTOKENS;

    /// A complete symbol.
    ///
    /// Expects its Base type to provide access to the symbol kind
    /// via kind ().
    ///
    /// Provide access to semantic value and location.
    template <typename Base>
    struct basic_symbol : Base
    {
      /// Alias to Base.
      typedef Base super_type;

      /// Default constructor.
      basic_symbol () YY_NOEXCEPT
        : value ()
        , location ()
      {}

#if 201103L <= YY_CPLUSPLUS
      /// Move constructor.
      basic_symbol (basic_symbol&& that)
        : Base (std::move (that))
        , value ()
        , location (std::move (that.location))
      {
        switch (this->kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.move< MarketEntryExpression * > (std::move (that.value));
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.move< PatternDescription * > (std::move (that.value));
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.move< PatternExpression * > (std::move (that.value));
        break;

      case symbol_kind::S_pattern: // pattern
        value.move< PriceActionLabPattern * > (std::move (that.value));
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.move< PriceActionLabPattern::PortfolioAttribute > (std::move (that.value));
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.move< PriceActionLabPattern::VolatilityAttribute > (std::move (that.value));
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.move< PriceBarReference * > (std::move (that.value));
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.move< ProfitTargetInPercentExpression * > (std::move (that.value));
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.move< StopLossInPercentExpression * > (std::move (that.value));
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.move< decimal7 * > (std::move (that.value));
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.move< int > (std::move (that.value));
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.move< std::string > (std::move (that.value));
        break;

      default:
        break;
    }

      }
#endif

      /// Copy constructor.
      basic_symbol (const basic_symbol& that);

      /// Constructors for typed symbols.
#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, location_type&& l)
        : Base (t)
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const location_type& l)
        : Base (t)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, MarketEntryExpression *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const MarketEntryExpression *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, PatternDescription *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const PatternDescription *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, PatternExpression *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const PatternExpression *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, PriceActionLabPattern *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const PriceActionLabPattern *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, PriceActionLabPattern::PortfolioAttribute&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const PriceActionLabPattern::PortfolioAttribute& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, PriceActionLabPattern::VolatilityAttribute&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const PriceActionLabPattern::VolatilityAttribute& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, PriceBarReference *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const PriceBarReference *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, ProfitTargetInPercentExpression *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const ProfitTargetInPercentExpression *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, StopLossInPercentExpression *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const StopLossInPercentExpression *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, decimal7 *&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const decimal7 *& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, int&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const int& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

#if 201103L <= YY_CPLUSPLUS
      basic_symbol (typename Base::kind_type t, std::string&& v, location_type&& l)
        : Base (t)
        , value (std::move (v))
        , location (std::move (l))
      {}
#else
      basic_symbol (typename Base::kind_type t, const std::string& v, const location_type& l)
        : Base (t)
        , value (v)
        , location (l)
      {}
#endif

      /// Destroy the symbol.
      ~basic_symbol ()
      {
        clear ();
      }



      /// Destroy contents, and record that is empty.
      void clear () YY_NOEXCEPT
      {
        // User destructor.
        symbol_kind_type yykind = this->kind ();
        basic_symbol<Base>& yysym = *this;
        (void) yysym;
        switch (yykind)
        {
       default:
          break;
        }

        // Value type destructor.
switch (yykind)
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.template destroy< MarketEntryExpression * > ();
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.template destroy< PatternDescription * > ();
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.template destroy< PatternExpression * > ();
        break;

      case symbol_kind::S_pattern: // pattern
        value.template destroy< PriceActionLabPattern * > ();
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.template destroy< PriceActionLabPattern::PortfolioAttribute > ();
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.template destroy< PriceActionLabPattern::VolatilityAttribute > ();
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.template destroy< PriceBarReference * > ();
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.template destroy< ProfitTargetInPercentExpression * > ();
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.template destroy< StopLossInPercentExpression * > ();
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.template destroy< decimal7 * > ();
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.template destroy< int > ();
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.template destroy< std::string > ();
        break;

      default:
        break;
    }

        Base::clear ();
      }

      /// The user-facing name of this symbol.
      std::string name () const YY_NOEXCEPT
      {
        return PalParser::symbol_name (this->kind ());
      }

      /// Backward compatibility (Bison 3.6).
      symbol_kind_type type_get () const YY_NOEXCEPT;

      /// Whether empty.
      bool empty () const YY_NOEXCEPT;

      /// Destructive move, \a s is emptied into this.
      void move (basic_symbol& s);

      /// The semantic value.
      value_type value;

      /// The location.
      location_type location;

    private:
#if YY_CPLUSPLUS < 201103L
      /// Assignment operator.
      basic_symbol& operator= (const basic_symbol& that);
#endif
    };

    /// Type access provider for token (enum) based symbols.
    struct by_kind
    {
      /// The symbol kind as needed by the constructor.
      typedef token_kind_type kind_type;

      /// Default constructor.
      by_kind () YY_NOEXCEPT;

#if 201103L <= YY_CPLUSPLUS
      /// Move constructor.
      by_kind (by_kind&& that) YY_NOEXCEPT;
#endif

      /// Copy constructor.
      by_kind (const by_kind& that) YY_NOEXCEPT;

      /// Constructor from (external) token numbers.
      by_kind (kind_type t) YY_NOEXCEPT;



      /// Record that this symbol is empty.
      void clear () YY_NOEXCEPT;

      /// Steal the symbol kind from \a that.
      void move (by_kind& that);

      /// The (internal) type number (corresponding to \a type).
      /// \a empty when empty.
      symbol_kind_type kind () const YY_NOEXCEPT;

      /// Backward compatibility (Bison 3.6).
      symbol_kind_type type_get () const YY_NOEXCEPT;

      /// The symbol kind.
      /// \a S_YYEMPTY when empty.
      symbol_kind_type kind_;
    };

    /// Backward compatibility for a private implementation detail (Bison 3.6).
    typedef by_kind by_type;

    /// "External" symbols: returned by the scanner.
    struct symbol_type : basic_symbol<by_kind>
    {
      /// Superclass.
      typedef basic_symbol<by_kind> super_type;

      /// Empty symbol.
      symbol_type () YY_NOEXCEPT {}

      /// Constructor for valueless symbols, and symbols from each type.
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, location_type l)
        : super_type (token_kind_type (tok), std::move (l))
#else
      symbol_type (int tok, const location_type& l)
        : super_type (token_kind_type (tok), l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT (tok == token::TOK_EOF
                   || (token::YYerror <= tok && tok <= token::YYUNDEF)
                   || (token::TOK_PLUS <= tok && tok <= token::TOK_RIGHT_PAREN));
#endif
      }
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, int v, location_type l)
        : super_type (token_kind_type (tok), std::move (v), std::move (l))
#else
      symbol_type (int tok, const int& v, const location_type& l)
        : super_type (token_kind_type (tok), v, l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT (tok == token::TOK_INT_NUM);
#endif
      }
#if 201103L <= YY_CPLUSPLUS
      symbol_type (int tok, std::string v, location_type l)
        : super_type (token_kind_type (tok), std::move (v), std::move (l))
#else
      symbol_type (int tok, const std::string& v, const location_type& l)
        : super_type (token_kind_type (tok), v, l)
#endif
      {
#if !defined _MSC_VER || defined __clang__
        YY_ASSERT ((token::TOK_IDENTIFIER <= tok && tok <= token::TOK_FLOAT_NUM));
#endif
      }
    };

    /// Build a parser object.
    PalParser (class mkc_palast::Scanner& scanner_yyarg, class PalParseDriver& driver_yyarg);
    virtual ~PalParser ();

#if 201103L <= YY_CPLUSPLUS
    /// Non copyable.
    PalParser (const PalParser&) = delete;
    /// Non copyable.
    PalParser& operator= (const PalParser&) = delete;
#endif

    /// Parse.  An alias for parse ().
    /// \returns  0 iff parsing succeeded.
    int operator() ();

    /// Parse.
    /// \returns  0 iff parsing succeeded.
    virtual int parse ();

#if YYDEBUG
    /// The current debugging stream.
    std::ostream& debug_stream () const YY_ATTRIBUTE_PURE;
    /// Set the current debugging stream.
    void set_debug_stream (std::ostream &);

    /// Type for debugging levels.
    typedef int debug_level_type;
    /// The current debugging level.
    debug_level_type debug_level () const YY_ATTRIBUTE_PURE;
    /// Set the current debugging level.
    void set_debug_level (debug_level_type l);
#endif

    /// Report a syntax error.
    /// \param loc    where the syntax error is found.
    /// \param msg    a description of the syntax error.
    virtual void error (const location_type& loc, const std::string& msg);

    /// Report a syntax error.
    void error (const syntax_error& err);

    /// The user-facing name of the symbol whose (internal) number is
    /// YYSYMBOL.  No bounds checking.
    static std::string symbol_name (symbol_kind_type yysymbol);

    // Implementation of make_symbol for each token kind.
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_EOF (location_type l)
      {
        return symbol_type (token::TOK_EOF, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_EOF (const location_type& l)
      {
        return symbol_type (token::TOK_EOF, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_YYerror (location_type l)
      {
        return symbol_type (token::YYerror, std::move (l));
      }
#else
      static
      symbol_type
      make_YYerror (const location_type& l)
      {
        return symbol_type (token::YYerror, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_YYUNDEF (location_type l)
      {
        return symbol_type (token::YYUNDEF, std::move (l));
      }
#else
      static
      symbol_type
      make_YYUNDEF (const location_type& l)
      {
        return symbol_type (token::YYUNDEF, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_INT_NUM (int v, location_type l)
      {
        return symbol_type (token::TOK_INT_NUM, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_INT_NUM (const int& v, const location_type& l)
      {
        return symbol_type (token::TOK_INT_NUM, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_IDENTIFIER (std::string v, location_type l)
      {
        return symbol_type (token::TOK_IDENTIFIER, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_IDENTIFIER (const std::string& v, const location_type& l)
      {
        return symbol_type (token::TOK_IDENTIFIER, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_FLOAT_NUM (std::string v, location_type l)
      {
        return symbol_type (token::TOK_FLOAT_NUM, std::move (v), std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_FLOAT_NUM (const std::string& v, const location_type& l)
      {
        return symbol_type (token::TOK_FLOAT_NUM, v, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PLUS (location_type l)
      {
        return symbol_type (token::TOK_PLUS, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PLUS (const location_type& l)
      {
        return symbol_type (token::TOK_PLUS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_MINUS (location_type l)
      {
        return symbol_type (token::TOK_MINUS, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_MINUS (const location_type& l)
      {
        return symbol_type (token::TOK_MINUS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PERCENT (location_type l)
      {
        return symbol_type (token::TOK_PERCENT, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PERCENT (const location_type& l)
      {
        return symbol_type (token::TOK_PERCENT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_LBRACE (location_type l)
      {
        return symbol_type (token::TOK_LBRACE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_LBRACE (const location_type& l)
      {
        return symbol_type (token::TOK_LBRACE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_RBRACE (location_type l)
      {
        return symbol_type (token::TOK_RBRACE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_RBRACE (const location_type& l)
      {
        return symbol_type (token::TOK_RBRACE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_COLON (location_type l)
      {
        return symbol_type (token::TOK_COLON, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_COLON (const location_type& l)
      {
        return symbol_type (token::TOK_COLON, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_GREATER_THAN (location_type l)
      {
        return symbol_type (token::TOK_GREATER_THAN, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_GREATER_THAN (const location_type& l)
      {
        return symbol_type (token::TOK_GREATER_THAN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_IF (location_type l)
      {
        return symbol_type (token::TOK_IF, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_IF (const location_type& l)
      {
        return symbol_type (token::TOK_IF, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_THEN (location_type l)
      {
        return symbol_type (token::TOK_THEN, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_THEN (const location_type& l)
      {
        return symbol_type (token::TOK_THEN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_OPEN (location_type l)
      {
        return symbol_type (token::TOK_OPEN, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_OPEN (const location_type& l)
      {
        return symbol_type (token::TOK_OPEN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_HIGH (location_type l)
      {
        return symbol_type (token::TOK_HIGH, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_HIGH (const location_type& l)
      {
        return symbol_type (token::TOK_HIGH, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_LOW (location_type l)
      {
        return symbol_type (token::TOK_LOW, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_LOW (const location_type& l)
      {
        return symbol_type (token::TOK_LOW, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_CLOSE (location_type l)
      {
        return symbol_type (token::TOK_CLOSE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_CLOSE (const location_type& l)
      {
        return symbol_type (token::TOK_CLOSE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_VOLUME (location_type l)
      {
        return symbol_type (token::TOK_VOLUME, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_VOLUME (const location_type& l)
      {
        return symbol_type (token::TOK_VOLUME, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_ROC1 (location_type l)
      {
        return symbol_type (token::TOK_ROC1, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_ROC1 (const location_type& l)
      {
        return symbol_type (token::TOK_ROC1, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_IBS1 (location_type l)
      {
        return symbol_type (token::TOK_IBS1, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_IBS1 (const location_type& l)
      {
        return symbol_type (token::TOK_IBS1, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_IBS2 (location_type l)
      {
        return symbol_type (token::TOK_IBS2, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_IBS2 (const location_type& l)
      {
        return symbol_type (token::TOK_IBS2, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_IBS3 (location_type l)
      {
        return symbol_type (token::TOK_IBS3, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_IBS3 (const location_type& l)
      {
        return symbol_type (token::TOK_IBS3, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_MEANDER (location_type l)
      {
        return symbol_type (token::TOK_MEANDER, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_MEANDER (const location_type& l)
      {
        return symbol_type (token::TOK_MEANDER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_VCHARTLOW (location_type l)
      {
        return symbol_type (token::TOK_VCHARTLOW, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_VCHARTLOW (const location_type& l)
      {
        return symbol_type (token::TOK_VCHARTLOW, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_VCHARTHIGH (location_type l)
      {
        return symbol_type (token::TOK_VCHARTHIGH, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_VCHARTHIGH (const location_type& l)
      {
        return symbol_type (token::TOK_VCHARTHIGH, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_OF (location_type l)
      {
        return symbol_type (token::TOK_OF, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_OF (const location_type& l)
      {
        return symbol_type (token::TOK_OF, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_AND (location_type l)
      {
        return symbol_type (token::TOK_AND, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_AND (const location_type& l)
      {
        return symbol_type (token::TOK_AND, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_AGO (location_type l)
      {
        return symbol_type (token::TOK_AGO, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_AGO (const location_type& l)
      {
        return symbol_type (token::TOK_AGO, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_BUY (location_type l)
      {
        return symbol_type (token::TOK_BUY, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_BUY (const location_type& l)
      {
        return symbol_type (token::TOK_BUY, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_SELL (location_type l)
      {
        return symbol_type (token::TOK_SELL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_SELL (const location_type& l)
      {
        return symbol_type (token::TOK_SELL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_NEXT (location_type l)
      {
        return symbol_type (token::TOK_NEXT, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_NEXT (const location_type& l)
      {
        return symbol_type (token::TOK_NEXT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_ON (location_type l)
      {
        return symbol_type (token::TOK_ON, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_ON (const location_type& l)
      {
        return symbol_type (token::TOK_ON, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_THE (location_type l)
      {
        return symbol_type (token::TOK_THE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_THE (const location_type& l)
      {
        return symbol_type (token::TOK_THE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_WITH (location_type l)
      {
        return symbol_type (token::TOK_WITH, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_WITH (const location_type& l)
      {
        return symbol_type (token::TOK_WITH, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PROFIT (location_type l)
      {
        return symbol_type (token::TOK_PROFIT, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PROFIT (const location_type& l)
      {
        return symbol_type (token::TOK_PROFIT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_TARGET (location_type l)
      {
        return symbol_type (token::TOK_TARGET, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_TARGET (const location_type& l)
      {
        return symbol_type (token::TOK_TARGET, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_AT (location_type l)
      {
        return symbol_type (token::TOK_AT, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_AT (const location_type& l)
      {
        return symbol_type (token::TOK_AT, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_ENTRY (location_type l)
      {
        return symbol_type (token::TOK_ENTRY, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_ENTRY (const location_type& l)
      {
        return symbol_type (token::TOK_ENTRY, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PRICE (location_type l)
      {
        return symbol_type (token::TOK_PRICE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PRICE (const location_type& l)
      {
        return symbol_type (token::TOK_PRICE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_BARS (location_type l)
      {
        return symbol_type (token::TOK_BARS, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_BARS (const location_type& l)
      {
        return symbol_type (token::TOK_BARS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_BAR (location_type l)
      {
        return symbol_type (token::TOK_BAR, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_BAR (const location_type& l)
      {
        return symbol_type (token::TOK_BAR, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_STOP (location_type l)
      {
        return symbol_type (token::TOK_STOP, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_STOP (const location_type& l)
      {
        return symbol_type (token::TOK_STOP, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_LOSS (location_type l)
      {
        return symbol_type (token::TOK_LOSS, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_LOSS (const location_type& l)
      {
        return symbol_type (token::TOK_LOSS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_FILE (location_type l)
      {
        return symbol_type (token::TOK_FILE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_FILE (const location_type& l)
      {
        return symbol_type (token::TOK_FILE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_INDEX (location_type l)
      {
        return symbol_type (token::TOK_INDEX, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_INDEX (const location_type& l)
      {
        return symbol_type (token::TOK_INDEX, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_DATE (location_type l)
      {
        return symbol_type (token::TOK_DATE, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_DATE (const location_type& l)
      {
        return symbol_type (token::TOK_DATE, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PL (location_type l)
      {
        return symbol_type (token::TOK_PL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PL (const location_type& l)
      {
        return symbol_type (token::TOK_PL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PS (location_type l)
      {
        return symbol_type (token::TOK_PS, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PS (const location_type& l)
      {
        return symbol_type (token::TOK_PS, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_TRADES (location_type l)
      {
        return symbol_type (token::TOK_TRADES, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_TRADES (const location_type& l)
      {
        return symbol_type (token::TOK_TRADES, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_CL (location_type l)
      {
        return symbol_type (token::TOK_CL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_CL (const location_type& l)
      {
        return symbol_type (token::TOK_CL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_VOLATILITY (location_type l)
      {
        return symbol_type (token::TOK_VOLATILITY, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_VOLATILITY (const location_type& l)
      {
        return symbol_type (token::TOK_VOLATILITY, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PORTFOLIO (location_type l)
      {
        return symbol_type (token::TOK_PORTFOLIO, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PORTFOLIO (const location_type& l)
      {
        return symbol_type (token::TOK_PORTFOLIO, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_LOW_VOL (location_type l)
      {
        return symbol_type (token::TOK_LOW_VOL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_LOW_VOL (const location_type& l)
      {
        return symbol_type (token::TOK_LOW_VOL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_HIGH_VOL (location_type l)
      {
        return symbol_type (token::TOK_HIGH_VOL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_HIGH_VOL (const location_type& l)
      {
        return symbol_type (token::TOK_HIGH_VOL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PORT_LONG_FILTER (location_type l)
      {
        return symbol_type (token::TOK_PORT_LONG_FILTER, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PORT_LONG_FILTER (const location_type& l)
      {
        return symbol_type (token::TOK_PORT_LONG_FILTER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_PORT_SHORT_FILTER (location_type l)
      {
        return symbol_type (token::TOK_PORT_SHORT_FILTER, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_PORT_SHORT_FILTER (const location_type& l)
      {
        return symbol_type (token::TOK_PORT_SHORT_FILTER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_VERY_HIGH_VOL (location_type l)
      {
        return symbol_type (token::TOK_VERY_HIGH_VOL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_VERY_HIGH_VOL (const location_type& l)
      {
        return symbol_type (token::TOK_VERY_HIGH_VOL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_NORMAL_VOL (location_type l)
      {
        return symbol_type (token::TOK_NORMAL_VOL, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_NORMAL_VOL (const location_type& l)
      {
        return symbol_type (token::TOK_NORMAL_VOL, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_MOMERSION_FILTER (location_type l)
      {
        return symbol_type (token::TOK_MOMERSION_FILTER, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_MOMERSION_FILTER (const location_type& l)
      {
        return symbol_type (token::TOK_MOMERSION_FILTER, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_LEFT_PAREN (location_type l)
      {
        return symbol_type (token::TOK_LEFT_PAREN, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_LEFT_PAREN (const location_type& l)
      {
        return symbol_type (token::TOK_LEFT_PAREN, l);
      }
#endif
#if 201103L <= YY_CPLUSPLUS
      static
      symbol_type
      make_TOK_RIGHT_PAREN (location_type l)
      {
        return symbol_type (token::TOK_RIGHT_PAREN, std::move (l));
      }
#else
      static
      symbol_type
      make_TOK_RIGHT_PAREN (const location_type& l)
      {
        return symbol_type (token::TOK_RIGHT_PAREN, l);
      }
#endif


    class context
    {
    public:
      context (const PalParser& yyparser, const symbol_type& yyla);
      const symbol_type& lookahead () const YY_NOEXCEPT { return yyla_; }
      symbol_kind_type token () const YY_NOEXCEPT { return yyla_.kind (); }
      const location_type& location () const YY_NOEXCEPT { return yyla_.location; }

      /// Put in YYARG at most YYARGN of the expected tokens, and return the
      /// number of tokens stored in YYARG.  If YYARG is null, return the
      /// number of expected tokens (guaranteed to be less than YYNTOKENS).
      int expected_tokens (symbol_kind_type yyarg[], int yyargn) const;

    private:
      const PalParser& yyparser_;
      const symbol_type& yyla_;
    };

  private:
#if YY_CPLUSPLUS < 201103L
    /// Non copyable.
    PalParser (const PalParser&);
    /// Non copyable.
    PalParser& operator= (const PalParser&);
#endif


    /// Stored state numbers (used for stacks).
    typedef unsigned char state_type;

    /// The arguments of the error message.
    int yy_syntax_error_arguments_ (const context& yyctx,
                                    symbol_kind_type yyarg[], int yyargn) const;

    /// Generate an error message.
    /// \param yyctx     the context in which the error occurred.
    virtual std::string yysyntax_error_ (const context& yyctx) const;
    /// Compute post-reduction state.
    /// \param yystate   the current state
    /// \param yysym     the nonterminal to push on the stack
    static state_type yy_lr_goto_state_ (state_type yystate, int yysym);

    /// Whether the given \c yypact_ value indicates a defaulted state.
    /// \param yyvalue   the value to check
    static bool yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT;

    /// Whether the given \c yytable_ value indicates a syntax error.
    /// \param yyvalue   the value to check
    static bool yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT;

    static const signed char yypact_ninf_;
    static const signed char yytable_ninf_;

    /// Convert a scanner token kind \a t to a symbol kind.
    /// In theory \a t should be a token_kind_type, but character literals
    /// are valid, yet not members of the token_kind_type enum.
    static symbol_kind_type yytranslate_ (int t) YY_NOEXCEPT;

    /// Convert the symbol name \a n to a form suitable for a diagnostic.
    static std::string yytnamerr_ (const char *yystr);

    /// For a symbol, its name in clear.
    static const char* const yytname_[];


    // Tables.
    // YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
    // STATE-NUM.
    static const short yypact_[];

    // YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
    // Performed when YYTABLE does not specify something else to do.  Zero
    // means the default is an error.
    static const signed char yydefact_[];

    // YYPGOTO[NTERM-NUM].
    static const signed char yypgoto_[];

    // YYDEFGOTO[NTERM-NUM].
    static const unsigned char yydefgoto_[];

    // YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
    // positive, shift that token.  If negative, reduce the rule whose
    // number is the opposite.  If YYTABLE_NINF, syntax error.
    static const unsigned char yytable_[];

    static const short yycheck_[];

    // YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
    // state STATE-NUM.
    static const signed char yystos_[];

    // YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.
    static const signed char yyr1_[];

    // YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.
    static const signed char yyr2_[];


#if YYDEBUG
    // YYRLINE[YYN] -- Source line where rule number YYN was defined.
    static const short yyrline_[];
    /// Report on the debug stream that the rule \a r is going to be reduced.
    virtual void yy_reduce_print_ (int r) const;
    /// Print the state stack on the debug stream.
    virtual void yy_stack_print_ () const;

    /// Debugging level.
    int yydebug_;
    /// Debug stream.
    std::ostream* yycdebug_;

    /// \brief Display a symbol kind, value and location.
    /// \param yyo    The output stream.
    /// \param yysym  The symbol.
    template <typename Base>
    void yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const;
#endif

    /// \brief Reclaim the memory associated to a symbol.
    /// \param yymsg     Why this token is reclaimed.
    ///                  If null, print nothing.
    /// \param yysym     The symbol.
    template <typename Base>
    void yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const;

  private:
    /// Type access provider for state based symbols.
    struct by_state
    {
      /// Default constructor.
      by_state () YY_NOEXCEPT;

      /// The symbol kind as needed by the constructor.
      typedef state_type kind_type;

      /// Constructor.
      by_state (kind_type s) YY_NOEXCEPT;

      /// Copy constructor.
      by_state (const by_state& that) YY_NOEXCEPT;

      /// Record that this symbol is empty.
      void clear () YY_NOEXCEPT;

      /// Steal the symbol kind from \a that.
      void move (by_state& that);

      /// The symbol kind (corresponding to \a state).
      /// \a symbol_kind::S_YYEMPTY when empty.
      symbol_kind_type kind () const YY_NOEXCEPT;

      /// The state number used to denote an empty symbol.
      /// We use the initial state, as it does not have a value.
      enum { empty_state = 0 };

      /// The state.
      /// \a empty when empty.
      state_type state;
    };

    /// "Internal" symbol: element of the stack.
    struct stack_symbol_type : basic_symbol<by_state>
    {
      /// Superclass.
      typedef basic_symbol<by_state> super_type;
      /// Construct an empty symbol.
      stack_symbol_type ();
      /// Move or copy construction.
      stack_symbol_type (YY_RVREF (stack_symbol_type) that);
      /// Steal the contents from \a sym to build this.
      stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) sym);
#if YY_CPLUSPLUS < 201103L
      /// Assignment, needed by push_back by some old implementations.
      /// Moves the contents of that.
      stack_symbol_type& operator= (stack_symbol_type& that);

      /// Assignment, needed by push_back by other implementations.
      /// Needed by some other old implementations.
      stack_symbol_type& operator= (const stack_symbol_type& that);
#endif
    };

    /// A stack with random access from its top.
    template <typename T, typename S = std::vector<T> >
    class stack
    {
    public:
      // Hide our reversed order.
      typedef typename S::iterator iterator;
      typedef typename S::const_iterator const_iterator;
      typedef typename S::size_type size_type;
      typedef typename std::ptrdiff_t index_type;

      stack (size_type n = 200) YY_NOEXCEPT
        : seq_ (n)
      {}

#if 201103L <= YY_CPLUSPLUS
      /// Non copyable.
      stack (const stack&) = delete;
      /// Non copyable.
      stack& operator= (const stack&) = delete;
#endif

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      const T&
      operator[] (index_type i) const
      {
        return seq_[size_type (size () - 1 - i)];
      }

      /// Random access.
      ///
      /// Index 0 returns the topmost element.
      T&
      operator[] (index_type i)
      {
        return seq_[size_type (size () - 1 - i)];
      }

      /// Steal the contents of \a t.
      ///
      /// Close to move-semantics.
      void
      push (YY_MOVE_REF (T) t)
      {
        seq_.push_back (T ());
        operator[] (0).move (t);
      }

      /// Pop elements from the stack.
      void
      pop (std::ptrdiff_t n = 1) YY_NOEXCEPT
      {
        for (; 0 < n; --n)
          seq_.pop_back ();
      }

      /// Pop all elements from the stack.
      void
      clear () YY_NOEXCEPT
      {
        seq_.clear ();
      }

      /// Number of elements on the stack.
      index_type
      size () const YY_NOEXCEPT
      {
        return index_type (seq_.size ());
      }

      /// Iterator on top of the stack (going downwards).
      const_iterator
      begin () const YY_NOEXCEPT
      {
        return seq_.begin ();
      }

      /// Bottom of the stack.
      const_iterator
      end () const YY_NOEXCEPT
      {
        return seq_.end ();
      }

      /// Present a slice of the top of a stack.
      class slice
      {
      public:
        slice (const stack& stack, index_type range) YY_NOEXCEPT
          : stack_ (stack)
          , range_ (range)
        {}

        const T&
        operator[] (index_type i) const
        {
          return stack_[range_ - i];
        }

      private:
        const stack& stack_;
        index_type range_;
      };

    private:
#if YY_CPLUSPLUS < 201103L
      /// Non copyable.
      stack (const stack&);
      /// Non copyable.
      stack& operator= (const stack&);
#endif
      /// The wrapped container.
      S seq_;
    };


    /// Stack type.
    typedef stack<stack_symbol_type> stack_type;

    /// The stack.
    stack_type yystack_;

    /// Push a new state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param sym  the symbol
    /// \warning the contents of \a s.value is stolen.
    void yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym);

    /// Push a new look ahead token on the state on the stack.
    /// \param m    a debug message to display
    ///             if null, no trace is output.
    /// \param s    the state
    /// \param sym  the symbol (for its value and location).
    /// \warning the contents of \a sym.value is stolen.
    void yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym);

    /// Pop \a n symbols from the stack.
    void yypop_ (int n = 1) YY_NOEXCEPT;

    /// Constants.
    enum
    {
      yylast_ = 161,     ///< Last index in yytable_.
      yynnts_ = 24,  ///< Number of nonterminal symbols.
      yyfinal_ = 8 ///< Termination state number.
    };


    // User arguments.
    class mkc_palast::Scanner& scanner;
    class PalParseDriver& driver;

  };

  inline
  PalParser::symbol_kind_type
  PalParser::yytranslate_ (int t) YY_NOEXCEPT
  {
    // YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to
    // TOKEN-NUM as returned by yylex.
    static
    const signed char
    translate_table[] =
    {
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62
    };
    // Last valid token kind.
    const int code_max = 317;

    if (t <= 0)
      return symbol_kind::S_YYEOF;
    else if (t <= code_max)
      return static_cast <symbol_kind_type> (translate_table[t]);
    else
      return symbol_kind::S_YYUNDEF;
  }

  // basic_symbol.
  template <typename Base>
  PalParser::basic_symbol<Base>::basic_symbol (const basic_symbol& that)
    : Base (that)
    , value ()
    , location (that.location)
  {
    switch (this->kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.copy< MarketEntryExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.copy< PatternDescription * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.copy< PatternExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern: // pattern
        value.copy< PriceActionLabPattern * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.copy< PriceActionLabPattern::PortfolioAttribute > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.copy< PriceActionLabPattern::VolatilityAttribute > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.copy< PriceBarReference * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.copy< ProfitTargetInPercentExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.copy< StopLossInPercentExpression * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.copy< decimal7 * > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.copy< int > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.copy< std::string > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

  }




  template <typename Base>
  PalParser::symbol_kind_type
  PalParser::basic_symbol<Base>::type_get () const YY_NOEXCEPT
  {
    return this->kind ();
  }


  template <typename Base>
  bool
  PalParser::basic_symbol<Base>::empty () const YY_NOEXCEPT
  {
    return this->kind () == symbol_kind::S_YYEMPTY;
  }

  template <typename Base>
  void
  PalParser::basic_symbol<Base>::move (basic_symbol& s)
  {
    super_type::move (s);
    switch (this->kind ())
    {
      case symbol_kind::S_entrystmt: // entrystmt
        value.move< MarketEntryExpression * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_patterndescr: // patterndescr
        value.move< PatternDescription * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_conds: // conds
      case symbol_kind::S_ohlc_comparison: // ohlc_comparison
        value.move< PatternExpression * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_pattern: // pattern
        value.move< PriceActionLabPattern * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_pattern_portfolio_filter_attr: // pattern_portfolio_filter_attr
      case symbol_kind::S_portfolio_attr: // portfolio_attr
        value.move< PriceActionLabPattern::PortfolioAttribute > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_pattern_volatility_attr: // pattern_volatility_attr
      case symbol_kind::S_volatility_attr: // volatility_attr
        value.move< PriceActionLabPattern::VolatilityAttribute > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_ohlcref: // ohlcref
        value.move< PriceBarReference * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_profitstmt: // profitstmt
        value.move< ProfitTargetInPercentExpression * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_stopstmt: // stopstmt
        value.move< StopLossInPercentExpression * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_pldesc: // pldesc
      case symbol_kind::S_psdesc: // psdesc
      case symbol_kind::S_number: // number
        value.move< decimal7 * > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_TOK_INT_NUM: // TOK_INT_NUM
      case symbol_kind::S_indexdesc: // indexdesc
      case symbol_kind::S_indexdatedesc: // indexdatedesc
      case symbol_kind::S_tradesdesc: // tradesdesc
      case symbol_kind::S_cldesc: // cldesc
      case symbol_kind::S_integernumber: // integernumber
        value.move< int > (YY_MOVE (s.value));
        break;

      case symbol_kind::S_TOK_IDENTIFIER: // TOK_IDENTIFIER
      case symbol_kind::S_TOK_FLOAT_NUM: // TOK_FLOAT_NUM
      case symbol_kind::S_filedesc: // filedesc
        value.move< std::string > (YY_MOVE (s.value));
        break;

      default:
        break;
    }

    location = YY_MOVE (s.location);
  }

  // by_kind.
  inline
  PalParser::by_kind::by_kind () YY_NOEXCEPT
    : kind_ (symbol_kind::S_YYEMPTY)
  {}

#if 201103L <= YY_CPLUSPLUS
  inline
  PalParser::by_kind::by_kind (by_kind&& that) YY_NOEXCEPT
    : kind_ (that.kind_)
  {
    that.clear ();
  }
#endif

  inline
  PalParser::by_kind::by_kind (const by_kind& that) YY_NOEXCEPT
    : kind_ (that.kind_)
  {}

  inline
  PalParser::by_kind::by_kind (token_kind_type t) YY_NOEXCEPT
    : kind_ (yytranslate_ (t))
  {}



  inline
  void
  PalParser::by_kind::clear () YY_NOEXCEPT
  {
    kind_ = symbol_kind::S_YYEMPTY;
  }

  inline
  void
  PalParser::by_kind::move (by_kind& that)
  {
    kind_ = that.kind_;
    that.clear ();
  }

  inline
  PalParser::symbol_kind_type
  PalParser::by_kind::kind () const YY_NOEXCEPT
  {
    return kind_;
  }


  inline
  PalParser::symbol_kind_type
  PalParser::by_kind::type_get () const YY_NOEXCEPT
  {
    return this->kind ();
  }


#line 10 "/workspace/codementor/palvalidator/libs/priceactionlab/grammar.yy"
} // mkc_palast
#line 2800 "/workspace/codementor/palvalidator/libs/priceactionlab/PalParser.hpp"




#endif // !YY_YY_WORKSPACE_CODEMENTOR_PALVALIDATOR_LIBS_PRICEACTIONLAB_PALPARSER_HPP_INCLUDED
