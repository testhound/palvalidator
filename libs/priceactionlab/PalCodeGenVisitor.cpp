/**
 * @file PalCodeGenVisitor.cpp
 * @brief Implementation of the PalCodeGenVisitor base class.
 *
 * This file currently provides the definition for the global variable
 * `firstSubExpressionVisited` and the default constructor and destructor
 * for the PalCodeGenVisitor abstract base class. The actual visitor logic
 * is implemented in derived classes.
 */
//#include "PalAst.h"
#include "PalCodeGenVisitor.h"

/**
 * @var firstSubExpressionVisited
 * @brief Global flag to track if the first sub-expression within a larger expression
 *        (e.g., an AndExpr or GreaterThanExpr) has been visited by a code generator.
 *
 * This variable is used by some concrete PalCodeGenVisitor implementations
 * (like `EasyLanguageCodeGenVisitor`) to control formatting aspects, such as
 * the placement of parentheses or indentation, when generating code for
 * complex, nested expressions. For instance, it can help in deciding whether
 * an opening parenthesis is needed before visiting a sub-expression.
 * Its global nature implies it's reset or checked at different stages of
 * expression traversal within specific visitor methods.
 */
bool firstSubExpressionVisited = false;

/**
 * @brief Default constructor for the PalCodeGenVisitor base class.
 */
PalCodeGenVisitor::PalCodeGenVisitor()
{}

/**
 * @brief Default virtual destructor for the PalCodeGenVisitor base class.
 * Ensures proper cleanup when deleting objects of derived visitor classes
 * through a base class pointer.
 */
PalCodeGenVisitor::~PalCodeGenVisitor()
{}




