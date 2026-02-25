#ifndef PALAST_H
#define PALAST_H

#include <mutex>
#include <memory>
#include <string>
#include <map>
#include <list>
#include <fstream>
#include <algorithm>
#include <exception>
#include <iostream>
#include <typeinfo>
#include "number.h"

using decimal7 = num::DefaultNumber;

typedef std::shared_ptr<decimal7> DecimalPtr;

using std::shared_ptr;

class PalCodeGenVisitor;
class TSApiBackTest;
class PalFileResults;

/**
 * @brief Base class for representing a reference to a price bar component (e.g., Open, High, Low, Close).
 *
 * This class provides an interface for accessing price bar data at a specific offset from the current bar.
 */
class PriceBarReference
{
public:
  /**
   * @brief Enumerates the different types of price bar components that can be referenced.
   */
  enum ReferenceType {
    OPEN,           /**< Opening price */
    HIGH,           /**< Highest price */
    LOW,            /**< Lowest price */
    CLOSE,          /**< Closing price */
    VOLUME,         /**< Trading volume */
    ROC1,           /**< Rate of Change (1-period) */
    MEANDER,        /**< Meander indicator */
    VCHARTLOW,      /**< VChart Low indicator */
    VCHARTHIGH,     /**< VChart High indicator */
    IBS1,           /**< Internal Bar Strength (IBS) indicator 1 */
    IBS2,           /**< Internal Bar Strength (IBS) indicator 2 */
    IBS3,           /**< Internal Bar Strength (IBS) indicator 3 */
    MOMERSIONFILTER /**< Momersion Filter indicator */
  };
  
  /**
   * @brief Constructs a PriceBarReference with a given bar offset.
   * @param barOffset The offset from the current bar (e.g., 0 for current bar, 1 for previous bar).
   */
  PriceBarReference (unsigned int barOffset);

  /**
   * @brief Virtual destructor.
   */
  virtual ~PriceBarReference();

  /**
   * @brief Copy constructor.
   * @param rhs The PriceBarReference object to copy.
   */
  PriceBarReference (const PriceBarReference& rhs);

  /**
   * @brief Assignment operator.
   * @param rhs The PriceBarReference object to assign.
   * @return A reference to this PriceBarReference object.
   */
  PriceBarReference& operator=(const PriceBarReference &rhs);

  /**
   * @brief Gets the bar offset.
   * @return The bar offset.
   */
  unsigned int getBarOffset () const;

  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  virtual void accept (PalCodeGenVisitor &v) = 0;

  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  virtual unsigned long long hashCode() = 0;

  /**
   * @brief Gets the reference type.
   * @return The reference type.
   */
  virtual PriceBarReference::ReferenceType getReferenceType() const = 0;

  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  virtual int extraBarsNeeded() const = 0;
  
private:
  /**
   * @brief The bar offset.
   */
  unsigned int mBarOffset;
};

/**
 * @brief Represents a reference to the opening price of a price bar.
 */
class PriceBarOpen : public PriceBarReference
{
public:
  /**
   * @brief Constructs a PriceBarOpen object.
   * @param barOffset The bar offset.
   */
  PriceBarOpen(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The PriceBarOpen object to copy.
   */
  PriceBarOpen (const PriceBarOpen& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PriceBarOpen object to assign.
   * @return A reference to this PriceBarOpen object.
   */
  PriceBarOpen& operator=(const PriceBarOpen &rhs);
  /**
   * @brief Destructor.
   */
  ~PriceBarOpen();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (OPEN).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long mComputedHash;
};

/**
 * @brief Represents a reference to the highest price of a price bar.
 */
class PriceBarHigh : public PriceBarReference
{
public:
  /**
   * @brief Constructs a PriceBarHigh object.
   * @param barOffset The bar offset.
   */
  PriceBarHigh(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The PriceBarHigh object to copy.
   */
  PriceBarHigh (const PriceBarHigh& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PriceBarHigh object to assign.
   * @return A reference to this PriceBarHigh object.
   */
  PriceBarHigh& operator=(const PriceBarHigh &rhs);
  /**
   * @brief Destructor.
   */
  ~PriceBarHigh();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (HIGH).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
    
private:
  /**
   * @brief Cached hash code.
   */
    unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the lowest price of a price bar.
 */
class PriceBarLow : public PriceBarReference
{
public:
  /**
   * @brief Constructs a PriceBarLow object.
   * @param barOffset The bar offset.
   */
  PriceBarLow(unsigned int barOffset);
  /**
   * @brief Destructor.
   */
  ~PriceBarLow();
  /**
   * @brief Copy constructor.
   * @param rhs The PriceBarLow object to copy.
   */
  PriceBarLow (const PriceBarLow& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PriceBarLow object to assign.
   * @return A reference to this PriceBarLow object.
   */
  PriceBarLow& operator=(const PriceBarLow &rhs);
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (LOW).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the closing price of a price bar.
 */
class PriceBarClose : public PriceBarReference
{
public:
  /**
   * @brief Constructs a PriceBarClose object.
   * @param barOffset The bar offset.
   */
  PriceBarClose(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The PriceBarClose object to copy.
   */
  PriceBarClose (const PriceBarClose& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PriceBarClose object to assign.
   * @return A reference to this PriceBarClose object.
   */
  PriceBarClose& operator=(const PriceBarClose &rhs);
  /**
   * @brief Destructor.
   */
  ~PriceBarClose();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (CLOSE).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the volume of a price bar.
 */
class VolumeBarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs a VolumeBarReference object.
   * @param barOffset The bar offset.
   */
  VolumeBarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The VolumeBarReference object to copy.
   */
  VolumeBarReference (const VolumeBarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The VolumeBarReference object to assign.
   * @return A reference to this VolumeBarReference object.
   */
  VolumeBarReference& operator=(const VolumeBarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~VolumeBarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (VOLUME).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
    
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the 1-period Rate of Change (ROC1) of a price bar.
 */
class Roc1BarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs a Roc1BarReference object.
   * @param barOffset The bar offset.
   */
  Roc1BarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The Roc1BarReference object to copy.
   */
  Roc1BarReference (const Roc1BarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The Roc1BarReference object to assign.
   * @return A reference to this Roc1BarReference object.
   */
  Roc1BarReference& operator=(const Roc1BarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~Roc1BarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (ROC1).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the Internal Bar Strength (IBS1) of a price bar.
 */
class IBS1BarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs an IBS1BarReference object.
   * @param barOffset The bar offset.
   */
  IBS1BarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The IBS1BarReference object to copy.
   */
  IBS1BarReference (const IBS1BarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The IBS1BarReference object to assign.
   * @return A reference to this IBS1BarReference object.
   */
  IBS1BarReference& operator=(const IBS1BarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~IBS1BarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (IBS1).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the Internal Bar Strength (IBS2) of a price bar.
 */
class IBS2BarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs an IBS2BarReference object.
   * @param barOffset The bar offset.
   */
  IBS2BarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The IBS2BarReference object to copy.
   */
  IBS2BarReference (const IBS2BarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The IBS2BarReference object to assign.
   * @return A reference to this IBS2BarReference object.
   */
  IBS2BarReference& operator=(const IBS2BarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~IBS2BarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (IBS2).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the Internal Bar Strength (IBS3) of a price bar.
 */
class IBS3BarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs an IBS3BarReference object.
   * @param barOffset The bar offset.
   */
  IBS3BarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The IBS3BarReference object to copy.
   */
  IBS3BarReference (const IBS3BarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The IBS3BarReference object to assign.
   * @return A reference to this IBS3BarReference object.
   */
  IBS3BarReference& operator=(const IBS3BarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~IBS3BarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (IBS3).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the Meander indicator of a price bar.
 */
class MeanderBarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs a MeanderBarReference object.
   * @param barOffset The bar offset.
   */
  MeanderBarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The MeanderBarReference object to copy.
   */
  MeanderBarReference (const MeanderBarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The MeanderBarReference object to assign.
   * @return A reference to this MeanderBarReference object.
   */
  MeanderBarReference& operator=(const MeanderBarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~MeanderBarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (MEANDER).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the VChart High indicator of a price bar.
 */
class VChartHighBarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs a VChartHighBarReference object.
   * @param barOffset The bar offset.
   */
  VChartHighBarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The VChartHighBarReference object to copy.
   */
  VChartHighBarReference (const VChartHighBarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The VChartHighBarReference object to assign.
   * @return A reference to this VChartHighBarReference object.
   */
  VChartHighBarReference& operator=(const VChartHighBarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~VChartHighBarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (VCHARTHIGH).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a reference to the VChart Low indicator of a price bar.
 */
class VChartLowBarReference : public PriceBarReference
{
public:
  /**
   * @brief Constructs a VChartLowBarReference object.
   * @param barOffset The bar offset.
   */
  VChartLowBarReference(unsigned int barOffset);
  /**
   * @brief Copy constructor.
   * @param rhs The VChartLowBarReference object to copy.
   */
  VChartLowBarReference (const VChartLowBarReference& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The VChartLowBarReference object to assign.
   * @return A reference to this VChartLowBarReference object.
   */
  VChartLowBarReference& operator=(const VChartLowBarReference &rhs);
  /**
   * @brief Destructor.
   */
  ~VChartLowBarReference();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Gets the reference type.
   * @return The reference type (VCHARTLOW).
   */
  PriceBarReference::ReferenceType getReferenceType() const;
  /**
   * @brief Gets the number of extra bars needed for this reference.
   * @return The number of extra bars needed.
   */
  int extraBarsNeeded() const;
  
private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/*
 * @brief Represents a reference to the Momersion Filter indicator of a price bar.
 *
 * This class is currently commented out/disabled.
 *
class MomersionFilterBarReference : public PriceBarReference
{
public:
  // Constructs a MomersionFilterBarReference object.
  // @param barOffset The bar offset.
  // @param period The period for the Momersion Filter.
  MomersionFilterBarReference(unsigned int barOffset, unsigned int period);
  
  // Copy constructor.
  // @param rhs The MomersionFilterBarReference object to copy.
  MomersionFilterBarReference (const MomersionFilterBarReference& rhs);
  
  // Assignment operator.
  // @param rhs The MomersionFilterBarReference object to assign.
  // @return A reference to this MomersionFilterBarReference object.
  MomersionFilterBarReference& operator=(const MomersionFilterBarReference &rhs);
  
  // Destructor.
  ~MomersionFilterBarReference();
  
  // Accepts a PalCodeGenVisitor.
  // @param v The visitor.
  void accept (PalCodeGenVisitor &v);
  
  // Calculates the hash code for this object.
  // @return The hash code.
  unsigned long long hashCode();
  
  // Gets the reference type.
  // @return The reference type (MOMERSIONFILTER).
  PriceBarReference::ReferenceType getReferenceType();
  
  // Gets the number of extra bars needed for this reference.
  // @return The number of extra bars needed.
  int extraBarsNeeded() const;
  
  // Gets the period for the Momersion Filter.
  // @return The period.
  unsigned int getMomersionPeriod() const;
  
private:
  // Cached hash code.
  unsigned long long mComputedHash;
  // Period for the Momersion Filter.
  unsigned int mPeriod;
};
*/

/**
 * @brief Shared pointer type for PriceBarReference.
 */
typedef std::shared_ptr<PriceBarReference> PriceBarPtr;

//////////////

/**
 * @brief Base class for pattern expressions.
 *
 * This class represents an expression in a trading pattern, such as "Close > Open".
 */
class PatternExpression : public std::enable_shared_from_this<PatternExpression> {
public:
  /**
   * @brief Default constructor.
   */
  PatternExpression();
  /**
   * @brief Copy constructor.
   * @param rhs The PatternExpression object to copy.
   */
  PatternExpression (const PatternExpression& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PatternExpression object to assign.
   * @return A reference to this PatternExpression object.
   */
  PatternExpression& operator=(const PatternExpression &rhs);
  /**
   * @brief Virtual destructor.
   */
  virtual ~PatternExpression();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  virtual void accept (PalCodeGenVisitor &v) = 0;
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  virtual unsigned long long hashCode() = 0;
};

/**
 * @brief Shared pointer type for PatternExpression.
 */
typedef std::shared_ptr<PatternExpression> PatternExpressionPtr;

/**
 * @brief Represents a "greater than" expression (e.g., Close > Open).
 */
class GreaterThanExpr : public PatternExpression
{
public:
  /**
   * @brief Constructs a GreaterThanExpr object.
   * @param lhs Shared pointer to the left-hand side PriceBarReference.
   * @param rhs Shared pointer to the right-hand side PriceBarReference.
   */
  GreaterThanExpr (std::shared_ptr<PriceBarReference> lhs, std::shared_ptr<PriceBarReference> rhs);
  
  /**
   * @brief Legacy constructor for backward compatibility.
   * @param lhs Raw pointer to the left-hand side PriceBarReference.
   * @param rhs Raw pointer to the right-hand side PriceBarReference.
   */
  GreaterThanExpr (PriceBarReference *lhs, PriceBarReference *rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The GreaterThanExpr object to copy.
   */
  GreaterThanExpr (const GreaterThanExpr& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The GreaterThanExpr object to assign.
   * @return A reference to this GreaterThanExpr object.
   */
  GreaterThanExpr& operator=(const GreaterThanExpr &rhs);
  /**
   * @brief Destructor.
   */
  ~GreaterThanExpr();

  /**
   * @brief Gets the left-hand side PriceBarReference.
   * @return Pointer to the left-hand side PriceBarReference.
   */
  PriceBarReference * getLHS() const;
  /**
   * @brief Gets the right-hand side PriceBarReference.
   * @return Pointer to the right-hand side PriceBarReference.
   */
  PriceBarReference * getRHS() const;
  
  /**
   * @brief Gets the left-hand side PriceBarReference as shared_ptr.
   * @return Shared pointer to the left-hand side PriceBarReference.
   */
  std::shared_ptr<PriceBarReference> getLHSShared() const;
  /**
   * @brief Gets the right-hand side PriceBarReference as shared_ptr.
   * @return Shared pointer to the right-hand side PriceBarReference.
   */
  std::shared_ptr<PriceBarReference> getRHSShared() const;
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();

private:
  /**
   * @brief Shared pointer to the left-hand side PriceBarReference.
   */
  std::shared_ptr<PriceBarReference> mLhs;
  /**
   * @brief Shared pointer to the right-hand side PriceBarReference.
   */
  std::shared_ptr<PriceBarReference> mRhs;
};

/**
 * @brief Represents an "AND" expression, combining two pattern expressions.
 */
class AndExpr : public PatternExpression
{
public:
  /**
   * @brief Constructs an AndExpr object with shared pointers.
   * @param lhs Shared pointer to the left-hand side PatternExpression.
   * @param rhs Shared pointer to the right-hand side PatternExpression.
   */
  AndExpr(PatternExpressionPtr lhs, PatternExpressionPtr rhs);
  /**
   * @brief Constructs an AndExpr object with raw pointers.
   * @param lhs Pointer to the left-hand side PatternExpression.
   * @param rhs Pointer to the right-hand side PatternExpression.
   */
  AndExpr (PatternExpression *lhs, PatternExpression *rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The AndExpr object to copy.
   */
  AndExpr (const AndExpr& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The AndExpr object to assign.
   * @return A reference to this AndExpr object.
   */
  AndExpr& operator=(const AndExpr &rhs);
  /**
   * @brief Destructor.
   */
  ~AndExpr();

  /**
   * @brief Gets the left-hand side PatternExpression.
   * @return Pointer to the left-hand side PatternExpression.
   */
  PatternExpression *getLHS() const;
  /**
   * @brief Gets the right-hand side PatternExpression.
   * @return Pointer to the right-hand side PatternExpression.
   */
  PatternExpression *getRHS() const;
  
  /**
   * @brief Gets the left-hand side PatternExpression as shared_ptr.
   * @return Shared pointer to the left-hand side PatternExpression.
   */
  std::shared_ptr<PatternExpression> getLHSShared() const;
  /**
   * @brief Gets the right-hand side PatternExpression as shared_ptr.
   * @return Shared pointer to the right-hand side PatternExpression.
   */
  std::shared_ptr<PatternExpression> getRHSShared() const;
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();

 private:
  /**
   * @brief Shared pointer to the left-hand side PatternExpression.
   */
  PatternExpressionPtr mLeftHandSide;
  /**
   * @brief Shared pointer to the right-hand side PatternExpression.
   */
  PatternExpressionPtr mRightHandSide;
};



//////////////////////

/**
 * @brief Base class for profit target expressions in percent.
 */
class ProfitTargetInPercentExpression
{
public:
  /**
   * @brief Constructs a ProfitTargetInPercentExpression object.
   * @param profitTarget Shared pointer to the profit target value (decimal7).
   */
  ProfitTargetInPercentExpression(std::shared_ptr<decimal7> profitTarget);
  /**
   * @brief Copy constructor.
   * @param rhs The ProfitTargetInPercentExpression object to copy.
   */
  ProfitTargetInPercentExpression (const ProfitTargetInPercentExpression& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The ProfitTargetInPercentExpression object to assign.
   * @return A reference to this ProfitTargetInPercentExpression object.
   */
  ProfitTargetInPercentExpression& operator=(const ProfitTargetInPercentExpression &rhs);
  /**
   * @brief Virtual destructor.
   */
  virtual ~ProfitTargetInPercentExpression() = 0;
  /**
   * @brief Gets the profit target value.
   * @return Pointer to the profit target value (decimal7).
   */
  decimal7 *getProfitTarget() const;
  
  /**
   * @brief Gets the profit target value as shared_ptr.
   * @return Shared pointer to the profit target value (decimal7).
   */
  std::shared_ptr<decimal7> getProfitTargetShared() const;

  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  virtual void accept (PalCodeGenVisitor &v) = 0;
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Checks if this is a long side profit target.
   * @return True if it is a long side profit target, false otherwise.
   */
  virtual bool isLongSideProfitTarget() const = 0;
  /**
   * @brief Checks if this is a short side profit target.
   * @return True if it is a short side profit target, false otherwise.
   */
  virtual bool isShortSideProfitTarget() const = 0;

 private:
  /**
   * @brief Shared pointer to the profit target value.
   */
  std::shared_ptr<decimal7> mProfitTarget;
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a long side profit target in percent.
 */
class LongSideProfitTargetInPercent : public ProfitTargetInPercentExpression
{
public:
  /**
   * @brief Constructs a LongSideProfitTargetInPercent object.
   * @param profitTarget Shared pointer to the profit target value (decimal7).
   */
  LongSideProfitTargetInPercent (std::shared_ptr<decimal7> profitTarget);
  /**
   * @brief Copy constructor.
   * @param rhs The LongSideProfitTargetInPercent object to copy.
   */
  LongSideProfitTargetInPercent (const LongSideProfitTargetInPercent& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The LongSideProfitTargetInPercent object to assign.
   * @return A reference to this LongSideProfitTargetInPercent object.
   */
  LongSideProfitTargetInPercent& operator=(const LongSideProfitTargetInPercent &rhs);
  /**
   * @brief Destructor.
   */
  ~LongSideProfitTargetInPercent();

  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);

  /**
   * @brief Checks if this is a long side profit target.
   * @return Always true for this class.
   */
  bool isLongSideProfitTarget() const
  {
    return true;
  }

  /**
   * @brief Checks if this is a short side profit target.
   * @return Always false for this class.
   */
  bool isShortSideProfitTarget() const
  {
    return false;
  }
};

/**
 * @brief Represents a short side profit target in percent.
 */
class ShortSideProfitTargetInPercent : public ProfitTargetInPercentExpression
{
public:
  /**
   * @brief Constructs a ShortSideProfitTargetInPercent object.
   * @param profitTarget Shared pointer to the profit target value (decimal7).
   */
  ShortSideProfitTargetInPercent (std::shared_ptr<decimal7> profitTarget);
  /**
   * @brief Copy constructor.
   * @param rhs The ShortSideProfitTargetInPercent object to copy.
   */
  ShortSideProfitTargetInPercent (const ShortSideProfitTargetInPercent& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The ShortSideProfitTargetInPercent object to assign.
   * @return A reference to this ShortSideProfitTargetInPercent object.
   */
  ShortSideProfitTargetInPercent& operator=(const ShortSideProfitTargetInPercent &rhs);
  /**
   * @brief Destructor.
   */
  ~ShortSideProfitTargetInPercent();

  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);

  /**
   * @brief Checks if this is a long side profit target.
   * @return Always false for this class.
   */
  bool isLongSideProfitTarget() const
  {
    return false;
  }

  /**
   * @brief Checks if this is a short side profit target.
   * @return Always true for this class.
   */
  bool isShortSideProfitTarget() const
  {
    return true;
  }
};

//typedef std::shared_ptr<ProfitTargetInPercentExpression> ProfitTargetInPercentPtr;
/**
 * @brief Pointer type for ProfitTargetInPercentExpression.
 */
typedef ProfitTargetInPercentExpression* ProfitTargetInPercentPtr;

////////////////////////////////

/**
 * @brief Base class for stop loss expressions in percent.
 */
class StopLossInPercentExpression
{
public:
  /**
   * @brief Constructs a StopLossInPercentExpression object.
   * @param stopLoss Shared pointer to the stop loss value (decimal7).
   */
  StopLossInPercentExpression(std::shared_ptr<decimal7> stopLoss);
  /**
   * @brief Copy constructor.
   * @param rhs The StopLossInPercentExpression object to copy.
   */
  StopLossInPercentExpression (const StopLossInPercentExpression& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The StopLossInPercentExpression object to assign.
   * @return A reference to this StopLossInPercentExpression object.
   */
  StopLossInPercentExpression& operator=(const StopLossInPercentExpression &rhs);
  /**
   * @brief Virtual destructor.
   */
  virtual ~StopLossInPercentExpression();
  /**
   * @brief Gets the stop loss value.
   * @return Pointer to the stop loss value (decimal7).
   */
  decimal7 *getStopLoss() const;
  
  /**
   * @brief Gets the stop loss value as shared_ptr.
   * @return Shared pointer to the stop loss value (decimal7).
   */
  std::shared_ptr<decimal7> getStopLossShared() const;
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  virtual void accept (PalCodeGenVisitor &v) = 0;
  /**
   * @brief Checks if this is a long side stop loss.
   * @return True if it is a long side stop loss, false otherwise.
   */
  virtual bool isLongSideStopLoss() const = 0;
  /**
   * @brief Checks if this is a short side stop loss.
   * @return True if it is a short side stop loss, false otherwise.
   */
  virtual bool isShortSideStopLoss() const = 0;

 private:
  /**
   * @brief Shared pointer to the stop loss value.
   */
  std::shared_ptr<decimal7> mStopLoss;
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a long side stop loss in percent.
 */
class LongSideStopLossInPercent : public StopLossInPercentExpression
{
public:
  /**
   * @brief Constructs a LongSideStopLossInPercent object.
   * @param stopLoss Shared pointer to the stop loss value (decimal7).
   */
  LongSideStopLossInPercent (std::shared_ptr<decimal7> stopLoss);
  /**
   * @brief Destructor.
   */
  ~LongSideStopLossInPercent();
  /**
   * @brief Assignment operator.
   * @param rhs The LongSideStopLossInPercent object to assign.
   * @return A reference to this LongSideStopLossInPercent object.
   */
  LongSideStopLossInPercent& operator=(const LongSideStopLossInPercent &rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The LongSideStopLossInPercent object to copy.
   */
  LongSideStopLossInPercent (const LongSideStopLossInPercent& rhs);
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);

  /**
   * @brief Checks if this is a long side stop loss.
   * @return Always true for this class.
   */
  bool isLongSideStopLoss() const
  {
    return true;
  }

  /**
   * @brief Checks if this is a short side stop loss.
   * @return Always false for this class.
   */
  bool isShortSideStopLoss() const
  {
    return false;
  }
};

/**
 * @brief Represents a short side stop loss in percent.
 */
class ShortSideStopLossInPercent : public StopLossInPercentExpression
{
public:
  /**
   * @brief Constructs a ShortSideStopLossInPercent object.
   * @param stopLoss Shared pointer to the stop loss value (decimal7).
   */
  ShortSideStopLossInPercent (std::shared_ptr<decimal7> stopLoss);
  /**
   * @brief Destructor.
   */
  ~ShortSideStopLossInPercent();
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);

  /**
   * @brief Assignment operator.
   * @param rhs The ShortSideStopLossInPercent object to assign.
   * @return A reference to this ShortSideStopLossInPercent object.
   */
  ShortSideStopLossInPercent& operator=(const ShortSideStopLossInPercent &rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The ShortSideStopLossInPercent object to copy.
   */
  ShortSideStopLossInPercent (const ShortSideStopLossInPercent& rhs);

  /**
   * @brief Checks if this is a long side stop loss.
   * @return Always false for this class.
   */
  bool isLongSideStopLoss() const
  {
    return false;
  }

  /**
   * @brief Checks if this is a short side stop loss.
   * @return Always true for this class.
   */
  bool isShortSideStopLoss() const
  {
    return true;
  }
};

//typedef std::shared_ptr<StopLossInPercentExpression> StopLossInPercentPtr;
/**
 * @brief Pointer type for StopLossInPercentExpression.
 */
typedef StopLossInPercentExpression* StopLossInPercentPtr;

/////////////////////

/**
 * @brief Base class for market entry expressions.
 */
class MarketEntryExpression
{
public:
  /**
   * @brief Default constructor.
   */
  MarketEntryExpression();
  /**
   * @brief Virtual destructor.
   */
  virtual ~MarketEntryExpression();
  /**
   * @brief Assignment operator.
   * @param rhs The MarketEntryExpression object to assign.
   * @return A reference to this MarketEntryExpression object.
   */
  MarketEntryExpression& operator=(const MarketEntryExpression &rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The MarketEntryExpression object to copy.
   */
  MarketEntryExpression (const MarketEntryExpression& rhs);
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  virtual void accept (PalCodeGenVisitor &v) = 0;
  /**
   * @brief Checks if this is a long pattern.
   * @return True if it is a long pattern, false otherwise.
   */
  virtual bool isLongPattern() const = 0;
  /**
   * @brief Checks if this is a short pattern.
   * @return True if it is a short pattern, false otherwise.
   */
  virtual bool isShortPattern() const = 0;
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  virtual unsigned long long hashCode() = 0;
};

/**
 * @brief Represents a market entry on open.
 */
class MarketEntryOnOpen : public MarketEntryExpression
{
public:
  /**
   * @brief Default constructor.
   */
  MarketEntryOnOpen();
  /**
   * @brief Virtual destructor.
   */
  virtual ~MarketEntryOnOpen();
  /**
   * @brief Assignment operator.
   * @param rhs The MarketEntryOnOpen object to assign.
   * @return A reference to this MarketEntryOnOpen object.
   */
  MarketEntryOnOpen& operator=(const MarketEntryOnOpen &rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The MarketEntryOnOpen object to copy.
   */
  MarketEntryOnOpen (const MarketEntryOnOpen& rhs);
};

/**
 * @brief Represents a long market entry on open.
 */
class LongMarketEntryOnOpen : public MarketEntryOnOpen
{
public:
  /**
   * @brief Default constructor.
   */
  LongMarketEntryOnOpen();
  /**
   * @brief Destructor.
   */
  ~LongMarketEntryOnOpen();
  /**
   * @brief Assignment operator.
   * @param rhs The LongMarketEntryOnOpen object to assign.
   * @return A reference to this LongMarketEntryOnOpen object.
   */
  LongMarketEntryOnOpen& operator=(const LongMarketEntryOnOpen &rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The LongMarketEntryOnOpen object to copy.
   */
  LongMarketEntryOnOpen (const LongMarketEntryOnOpen& rhs);
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Checks if this is a long pattern.
   * @return Always true for this class.
   */
  bool isLongPattern() const
  { return true; }
  /**
   * @brief Checks if this is a short pattern.
   * @return Always false for this class.
   */
  bool isShortPattern() const
  { return false; }
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();

private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Represents a short market entry on open.
 */
class ShortMarketEntryOnOpen : public MarketEntryOnOpen
{
public:
  /**
   * @brief Default constructor.
   */
  ShortMarketEntryOnOpen();
  /**
   * @brief Destructor.
   */
  ~ShortMarketEntryOnOpen();

  /**
   * @brief Assignment operator.
   * @param rhs The ShortMarketEntryOnOpen object to assign.
   * @return A reference to this ShortMarketEntryOnOpen object.
   */
  ShortMarketEntryOnOpen& operator=(const ShortMarketEntryOnOpen &rhs);
  /**
   * @brief Copy constructor.
   * @param rhs The ShortMarketEntryOnOpen object to copy.
   */
  ShortMarketEntryOnOpen (const ShortMarketEntryOnOpen& rhs);
  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);

  /**
   * @brief Checks if this is a long pattern.
   * @return Always false for this class.
   */
  bool isLongPattern() const
  { return false; }
  /**
   * @brief Checks if this is a short pattern.
   * @return Always true for this class.
   */
  bool isShortPattern() const
  { return true; }
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();

private:
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Shared pointer type for MarketEntryExpression.
 */
typedef std::shared_ptr<MarketEntryExpression> MarketEntryPtr;

//////////////////////////////

/**
 * @brief Describes a trading pattern including its metadata.
 */
class PatternDescription : public std::enable_shared_from_this<PatternDescription>
{
public:
  /**
   * @brief Constructs a PatternDescription object.
   * @param fileName The name of the file containing the pattern.
   * @param patternIndex The index of the pattern in the file.
   * @param indexDate The date associated with the pattern index.
   * @param percentLong Pointer to the percentage of long trades.
   * @param percentShort Pointer to the percentage of short trades.
   * @param numTrades The total number of trades for this pattern.
   * @param consecutiveLosses The maximum number of consecutive losses.
   */
  PatternDescription(const char *fileName, unsigned int patternIndex,
      unsigned long indexDate, std::shared_ptr<decimal7> percentLong, std::shared_ptr<decimal7> percentShort,
      unsigned int numTrades, unsigned int consecutiveLosses);

  /**
   * @brief Convenience overload accepting std::string for filename to simplify
   *        construction from std::string expressions (e.g. "pattern" + std::to_string(i) + ".txt").
   *
   * Delegates to the existing const char* constructor.
   */
  PatternDescription(const std::string &fileName, unsigned int patternIndex,
      unsigned long indexDate, std::shared_ptr<decimal7> percentLong, std::shared_ptr<decimal7> percentShort,
      unsigned int numTrades, unsigned int consecutiveLosses)
    : PatternDescription(fileName.c_str(), patternIndex, indexDate, percentLong, percentShort, numTrades, consecutiveLosses)
  {}
  /**
   * @brief Copy constructor.
   * @param rhs The PatternDescription object to copy.
   */
  PatternDescription (const PatternDescription& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PatternDescription object to assign.
   * @return A reference to this PatternDescription object.
   */
  PatternDescription& operator=(const PatternDescription &rhs);
  /**
   * @brief Destructor.
   */
  ~PatternDescription();

  /**
   * @brief Gets the file name.
   * @return The file name.
   */
  const std::string& getFileName() const;
  /**
   * @brief Gets the pattern index.
   * @return The pattern index.
   */
  unsigned int getpatternIndex() const;
  /**
   * @brief Gets the index date.
   * @return The index date.
   */
  unsigned int getIndexDate() const;
  /**
   * @brief Gets the percentage of long trades.
   * @return Pointer to the percentage of long trades.
   */
  decimal7* getPercentLong() const;
  /**
   * @brief Gets the percentage of short trades.
   * @return Pointer to the percentage of short trades.
   */
  decimal7* getPercentShort() const;
  
  /**
   * @brief Gets the percentage of long trades as shared_ptr.
   * @return Shared pointer to the percentage of long trades.
   */
  std::shared_ptr<decimal7> getPercentLongShared() const;
  /**
   * @brief Gets the percentage of short trades as shared_ptr.
   * @return Shared pointer to the percentage of short trades.
   */
  std::shared_ptr<decimal7> getPercentShortShared() const;
  /**
   * @brief Gets the total number of trades.
   * @return The total number of trades.
   */
  unsigned int numTrades() const;
  /**
   * @brief Gets the maximum number of consecutive losses.
   * @return The maximum number of consecutive losses.
   */
  unsigned int numConsecutiveLosses() const;

  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Calculates the hash code for this object.
   * @return The hash code.
   */
  unsigned long long hashCode();

private:
  /**
   * @brief The name of the file containing the pattern.
   */
  std::string mFileName;
  /**
   * @brief The index of the pattern in the file.
   */
  unsigned int mPatternIndex;
  /**
   * @brief The date associated with the pattern index.
   */
  unsigned long mIndexDate;
  /**
   * @brief Shared pointer to the percentage of long trades.
   */
  std::shared_ptr<decimal7> mPercentLong;
  /**
   * @brief Shared pointer to the percentage of short trades.
   */
  std::shared_ptr<decimal7> mPercentShort;
  /**
   * @brief The total number of trades for this pattern.
   */
  unsigned int mNumTrades;
  /**
   * @brief The maximum number of consecutive losses.
   */
  unsigned int mConsecutiveLosses;
  /**
   * @brief Cached hash code.
   */
  unsigned long long mComputedHash;
};

/**
 * @brief Shared pointer type for PatternDescription.
 */
typedef std::shared_ptr<PatternDescription> PatternDescriptionPtr;

//////////////////////////////////////

/**
 * @brief Utility class to evaluate the maximum number of bars back needed for a pattern expression.
 */
class PalPatternMaxBars
{
public:
    /**
     * @brief Evaluates a pattern expression to find the maximum bar offset.
     * @param expression Pointer to the PatternExpression to evaluate.
     * @return The maximum number of bars back required by the expression.
     * @throws std::domain_error if an unknown PatternExpression type is encountered.
     */
    static unsigned int evaluateExpression (PatternExpression *expression)
    {
      if (AndExpr *pAnd = dynamic_cast<AndExpr*>(expression))
	{
	  unsigned int lhsBars = PalPatternMaxBars::evaluateExpression (pAnd->getLHS());
	  unsigned int rhsBars = PalPatternMaxBars::evaluateExpression (pAnd->getRHS());

	  return std::max (lhsBars, rhsBars);
	}
      else if (GreaterThanExpr *pGreaterThan = dynamic_cast<GreaterThanExpr*>(expression))
	{
	  unsigned int lhs = pGreaterThan->getLHS()->getBarOffset () + pGreaterThan->getLHS()->extraBarsNeeded();
	  unsigned int rhs = pGreaterThan->getRHS()->getBarOffset () + pGreaterThan->getRHS()->extraBarsNeeded();;

	  return std::max (lhs, rhs);
	}
      else
	throw std::domain_error ("Unknown derived class of PatternExpression");
    }

};

/**
 * @brief Represents a complete Price Action Lab trading pattern.
 *
 * This class encapsulates all components of a trading pattern, including its description,
 * the pattern expression itself, market entry logic, profit targets, and stop losses.
 * It also includes attributes like volatility and portfolio filtering.
 */
class PriceActionLabPattern : public std::enable_shared_from_this<PriceActionLabPattern>
{
 public:
  /**
   * @brief Enumerates attributes for portfolio filtering.
   */
  enum PortfolioAttribute {
    PORTFOLIO_FILTER_LONG,  /**< Filter for long patterns. */
    PORTFOLIO_FILTER_SHORT, /**< Filter for short patterns. */
    PORTFOLIO_FILTER_NONE   /**< No portfolio filter. */
  };
  /**
   * @brief Enumerates attributes for volatility.
   */
  enum VolatilityAttribute {
    VOLATILITY_VERY_HIGH, /**< Very high volatility. */
    VOLATILITY_HIGH,      /**< High volatility. */
    VOLATILITY_LOW,       /**< Low volatility. */
    VOLATILITY_NORMAL,    /**< Normal volatility. */
    VOLATILITY_NONE       /**< No volatility attribute. */
  };

public:
  /**
   * @brief Constructs a PriceActionLabPattern.
   * @param description Pointer to the PatternDescription.
   * @param pattern Pointer to the PatternExpression.
   * @param entry Pointer to the MarketEntryExpression.
   * @param profitTarget Pointer to the ProfitTargetInPercentExpression.
   * @param stopLoss Pointer to the StopLossInPercentExpression.
   */
  PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern,
   std::shared_ptr<MarketEntryExpression> entry,
   std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
   std::shared_ptr<StopLossInPercentExpression> stopLoss);

  /**
   * @brief Constructs a PriceActionLabPattern with volatility and portfolio attributes.
   * @param description Pointer to the PatternDescription.
   * @param pattern Pointer to the PatternExpression.
   * @param entry Shared pointer to the MarketEntryExpression.
   * @param profitTarget Shared pointer to the ProfitTargetInPercentExpression.
   * @param stopLoss Shared pointer to the StopLossInPercentExpression.
   * @param volatilityAttribute The volatility attribute.
   * @param portfolioAttribute The portfolio filter attribute.
   */
  PriceActionLabPattern (PatternDescription* description, PatternExpression* pattern,
   std::shared_ptr<MarketEntryExpression> entry,
   std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
   std::shared_ptr<StopLossInPercentExpression> stopLoss,
   VolatilityAttribute volatilityAttribute,
   PortfolioAttribute portfolioAttribute);

  /**
   * @brief Constructs a PriceActionLabPattern with shared pointers for description and pattern.
   * @param description Shared pointer to the PatternDescription.
   * @param pattern Shared pointer to the PatternExpression.
   * @param entry Shared pointer to the MarketEntryExpression.
   * @param profitTarget Shared pointer to the ProfitTargetInPercentExpression.
   * @param stopLoss Shared pointer to the StopLossInPercentExpression.
   */
  PriceActionLabPattern (PatternDescriptionPtr description,
   PatternExpressionPtr pattern,
   std::shared_ptr<MarketEntryExpression> entry,
   std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
   std::shared_ptr<StopLossInPercentExpression> stopLoss);

  /**
   * @brief Convenience overload to construct a PriceActionLabPattern with shared pointers and attributes.
   * @param description Shared pointer to the PatternDescription.
   * @param pattern Shared pointer to the PatternExpression.
   * @param entry Shared pointer to the MarketEntryExpression.
   * @param profitTarget Shared pointer to the ProfitTargetInPercentExpression.
   * @param stopLoss Shared pointer to the StopLossInPercentExpression.
   * @param volatilityAttribute The volatility attribute.
   * @param portfolioAttribute The portfolio filter attribute.
   */
  PriceActionLabPattern(PatternDescriptionPtr description,
                        PatternExpressionPtr pattern,
                        std::shared_ptr<MarketEntryExpression> entry,
                        std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
                        std::shared_ptr<StopLossInPercentExpression> stopLoss,
                        VolatilityAttribute volatilityAttribute,
                        PortfolioAttribute portfolioAttribute);
  
  /**
   * @brief Copy constructor.
   * @param rhs The PriceActionLabPattern object to copy.
   */
  PriceActionLabPattern (const PriceActionLabPattern& rhs);
  /**
   * @brief Assignment operator.
   * @param rhs The PriceActionLabPattern object to assign.
   * @return A reference to this PriceActionLabPattern object.
   */
  PriceActionLabPattern& operator=(const  PriceActionLabPattern &rhs);
  /**
   * @brief Destructor.
   */
  ~PriceActionLabPattern();

  /**
   * @brief Clones the pattern with new profit target and stop loss.
   * @param profitTarget Shared pointer to the new ProfitTargetInPercentExpression.
   * @param stopLoss Shared pointer to the new StopLossInPercentExpression.
   * @return A shared pointer to the cloned PriceActionLabPattern.
   */
  shared_ptr<PriceActionLabPattern> clone (std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
  		   std::shared_ptr<StopLossInPercentExpression> stopLoss);

  /**
   * @brief Gets the file name from the pattern description.
   * @return The file name.
   */
  const std::string& getFileName() const;
  /**
   * @brief Gets the base file name (without extension) from the pattern description.
   * @return The base file name.
   */
  const std::string getBaseFileName() const;

  /**
   * @brief Gets the pattern index from the pattern description.
   * @return The pattern index.
   */
  unsigned int getpatternIndex() const;
  /**
   * @brief Gets the index date from the pattern description.
   * @return The index date.
   */
  unsigned int getIndexDate() const;

  /**
   * @brief Gets the pattern expression.
   * @return Shared pointer to the PatternExpression.
   */
  PatternExpressionPtr getPatternExpression() const;
  /**
   * @brief Gets the market entry expression.
   * @return Shared pointer to the MarketEntryExpression.
   */
  std::shared_ptr<MarketEntryExpression> getMarketEntry() const;
  /**
   * @brief Gets the profit target expression.
   * @return Shared pointer to the ProfitTargetInPercentExpression.
   */
  std::shared_ptr<ProfitTargetInPercentExpression> getProfitTarget() const;
  /**
   * @brief Gets the profit target value as a decimal.
   * @return The profit target value.
   */
  decimal7 getProfitTargetAsDecimal() const;
  /**
   * @brief Gets the stop loss expression.
   * @return Shared pointer to the StopLossInPercentExpression.
   */
  std::shared_ptr<StopLossInPercentExpression> getStopLoss() const;
  
  // Legacy raw pointer interface for backward compatibility
  MarketEntryExpression* getMarketEntryRaw() const { return mEntry.get(); }
  ProfitTargetInPercentExpression* getProfitTargetRaw() const { return mProfitTarget.get(); }
  StopLossInPercentExpression* getStopLossRaw() const { return mStopLoss.get(); }
  /**
   * @brief Gets the stop loss value as a decimal.
   * @return The stop loss value.
   */
  decimal7 getStopLossAsDecimal() const;
  /**
   * @brief Gets the pattern description.
   * @return Shared pointer to the PatternDescription.
   */
  PatternDescriptionPtr getPatternDescription() const;

  /**
   * @brief Gets the maximum number of bars back required for this pattern.
   * @return The maximum number of bars back.
   */
  unsigned int getMaxBarsBack() const
  {
    return mMaxBarsBack;
  }

  /**
   * @brief Gets the payoff ratio (profit target / stop loss).
   * @return The payoff ratio.
   */
  decimal7 getPayoffRatio() const
  {
    return mPayOffRatio;
  }

  /**
   * @brief Accepts a PalCodeGenVisitor.
   * @param v The visitor.
   */
  void accept (PalCodeGenVisitor &v);
  /**
   * @brief Checks if this is a long pattern.
   * @return True if it is a long pattern, false otherwise.
   */
  bool isLongPattern() const
  {
    if (!mEntry) {
      std::cerr << "ERROR: mEntry is null in isLongPattern() for pattern "
                << getFileName() << " index " << getpatternIndex() << std::endl;
      return false;
    }
    
    // Check if we have an abstract base class (this should never happen)
    if (typeid(*mEntry) == typeid(MarketEntryExpression)) {
      std::cerr << "FATAL ERROR: Pattern " << getFileName() << " index " << getpatternIndex()
                << " has abstract MarketEntryExpression instead of concrete implementation!" << std::endl;
      std::cerr << "  This indicates a memory corruption or object construction issue." << std::endl;
      // Return a safe default instead of calling pure virtual function
      return false;
    }
    
    return mEntry->isLongPattern();
  }
  /**
   * @brief Checks if this is a short pattern.
   * @return True if it is a short pattern, false otherwise.
   */
  bool isShortPattern() const
  {
    if (!mEntry) {
      std::cerr << "ERROR: mEntry is null in isShortPattern() for pattern "
                << getFileName() << " index " << getpatternIndex() << std::endl;
      return false;
    }
    
    // Check if we have an abstract base class (this should never happen)
    if (typeid(*mEntry) == typeid(MarketEntryExpression)) {
      std::cerr << "FATAL ERROR: Pattern " << getFileName() << " index " << getpatternIndex()
                << " has abstract MarketEntryExpression in isShortPattern()!" << std::endl;
      std::cerr << "  This indicates a memory corruption or object construction issue." << std::endl;
      // Return a safe default instead of calling pure virtual function
      return false;
    }
    
    return mEntry->isShortPattern();
  }
  /**
   * @brief Calculates the hash code for this pattern.
   * @return The hash code.
   */
  unsigned long long hashCode();
  /**
   * @brief Checks if the pattern has a volatility attribute.
   * @return True if it has a volatility attribute, false otherwise.
   */
  bool hasVolatilityAttribute() const;
  /**
   * @brief Checks if the pattern is a low volatility pattern.
   * @return True if it is a low volatility pattern, false otherwise.
   */
  bool isLowVolatilityPattern() const;
  /**
   * @brief Checks if the pattern is a normal volatility pattern.
   * @return True if it is a normal volatility pattern, false otherwise.
   */
  bool isNormalVolatilityPattern() const;
  /**
   * @brief Checks if the pattern is a high volatility pattern.
   * @return True if it is a high volatility pattern, false otherwise.
   */
  bool isHighVolatilityPattern() const;
  /**
   * @brief Checks if the pattern is a very high volatility pattern.
   * @return True if it is a very high volatility pattern, false otherwise.
   */
  bool isVeryHighVolatilityPattern() const;

  /**
   * @brief Gets the volatility attribute of the pattern.
   * @return The volatility attribute.
   */
  VolatilityAttribute getVolatilityAttribute() const;

  /**
   * @brief Checks if the pattern has a portfolio attribute.
   * @return True if it has a portfolio attribute, false otherwise.
   */
  bool hasPortfolioAttribute() const;
  /**
   * @brief Checks if the pattern is filtered for long trades.
   * @return True if it is filtered for long trades, false otherwise.
   */
  bool isFilteredLongPattern() const;
  /**
   * @brief Checks if the pattern is filtered for short trades.
   * @return True if it is filtered for short trades, false otherwise.
   */
  bool isFilteredShortPattern() const;

  /**
   * @brief Gets the portfolio filter attribute of the pattern.
   * @return The portfolio filter attribute.
   */
  PortfolioAttribute getPortfolioAttribute() const;
private:
  /**
   * @brief Calculates the hash for a given string.
   * @param key The string to hash.
   * @return The hash value.
   */
    unsigned long long getStringHash (const std::string& key);

private:
  /**
   * @brief Shared pointer to the pattern expression.
   */
  PatternExpressionPtr mPattern;
  /**
   * @brief Shared pointer to the market entry expression.
   */
  std::shared_ptr<MarketEntryExpression> mEntry;
  /**
   * @brief Shared pointer to the profit target expression.
   */
  std::shared_ptr<ProfitTargetInPercentExpression> mProfitTarget;
  /**
   * @brief Shared pointer to the stop loss expression.
   */
  std::shared_ptr<StopLossInPercentExpression> mStopLoss;
  /**
   * @brief Shared pointer to the pattern description.
   */
  PatternDescriptionPtr mPatternDescription;
  /**
   * @brief Static map for caching string hashes.
   */
  static std::map<std::string, unsigned long long> mCachedStringHashMap;
  /**
   * @brief Volatility attribute of the pattern.
   */
  VolatilityAttribute mVolatilityAttribute;
  /**
   * @brief Portfolio filter attribute of the pattern.
   */
  PortfolioAttribute  mPortfolioAttribute;
  /**
   * @brief Maximum number of bars back required for this pattern.
   */
  unsigned int mMaxBarsBack;
  /**
   * @brief Payoff ratio (profit target / stop loss).
   */
  decimal7 mPayOffRatio;
};

/**
 * @brief Shared pointer type for PriceActionLabPattern.
 */
typedef std::shared_ptr<PriceActionLabPattern> PALPatternPtr;

/**
 * @brief Compares two PriceActionLabPattern objects for equality.
 * Two patterns are equal if they have the same direction, profit target,
 * stop loss, volatility attribute, portfolio attribute, and structurally
 * identical pattern expression trees. Filename and pattern index are
 * intentionally excluded to support cross-file pattern comparison.
 */
bool operator==(const PriceActionLabPattern& lhs, const PriceActionLabPattern& rhs);

/**
 * @brief Compares two PriceActionLabPattern objects for inequality.
 */
bool operator!=(const PriceActionLabPattern& lhs, const PriceActionLabPattern& rhs);

// When we have the smae pattern with different reward to risk or
// different volatilities (1 standard deviation version two standard deviation)
// volatility we need to chose which pattern to use

/**
 * @brief Interface for tie-breaking when multiple patterns match.
 *
 * This class defines a strategy for choosing between two conflicting patterns.
 */
class PatternTieBreaker
{
public:
  /**
   * @brief Default constructor.
   */
  PatternTieBreaker()
  {}

  /**
   * @brief Virtual destructor.
   */
  virtual ~PatternTieBreaker()
  {}

  /**
   * @brief Selects one of the two patterns based on a tie-breaking rule.
   * @param pattern1 The first pattern.
   * @param pattern2 The second pattern.
   * @return The selected pattern.
   */
  virtual PALPatternPtr getTieBreakerPattern(PALPatternPtr pattern1, 
					     PALPatternPtr pattern2) const = 0;
};

/**
 * @brief Tie-breaker that prefers patterns with the smallest volatility.
 */
class SmallestVolatilityTieBreaker : public PatternTieBreaker
{
public:
  /**
   * @brief Default constructor.
   */
  SmallestVolatilityTieBreaker() : PatternTieBreaker()
  {}

  /**
   * @brief Destructor.
   */
  ~SmallestVolatilityTieBreaker()
  {}

  /**
   * @brief Selects the pattern with the smallest volatility.
   * @param pattern1 The first pattern.
   * @param pattern2 The second pattern.
   * @return The pattern with the smallest volatility. If volatilities are equal or not defined, pattern1 is returned.
   */
  PALPatternPtr getTieBreakerPattern(PALPatternPtr pattern1, 
				     PALPatternPtr pattern2) const;
};

/**
 * @brief Shared pointer type for PatternTieBreaker.
 */
typedef std::shared_ptr<PatternTieBreaker> PatternTieBreakerPtr;

/**
 * @brief Represents a system of Price Action Lab patterns.
 *
 * This class manages a collection of long and short patterns,
 * and can use a PatternTieBreaker to resolve conflicts.
 */
class PriceActionLabSystem
{
 private:
  /**
   * @brief Internal map type for storing patterns, keyed by hash code.
   */
  typedef std::multimap<unsigned long long, PALPatternPtr> MapType;

  public:
  /**
   * @brief Constant iterator for the list of all patterns.
   */
  typedef std::list<PALPatternPtr>::const_iterator ConstPatternIterator;
  /**
   * @brief Iterator for sorted patterns (long or short).
   */
  typedef MapType::iterator SortedPatternIterator;
  /**
   * @brief Constant iterator for sorted patterns (long or short).
   */
  typedef MapType::const_iterator ConstSortedPatternIterator;

  /**
   * @brief Constructs a PriceActionLabSystem with a single pattern and a tie-breaker.
   * @param pattern The initial pattern to add.
   * @param tieBreaker The tie-breaker strategy.
   * @param useTieBreaker Flag to indicate whether to use the tie-breaker.
   */
  PriceActionLabSystem (PALPatternPtr pattern, 
			PatternTieBreakerPtr tieBreaker,
			bool useTieBreaker);
  /**
   * @brief Constructs a PriceActionLabSystem with a tie-breaker.
   * @param tieBreaker The tie-breaker strategy.
   * @param useTieBreaker Flag to indicate whether to use the tie-breaker (defaults to false).
   */
  PriceActionLabSystem (PatternTieBreakerPtr tieBreaker,
			bool useTieBreaker = false);
  /**
   * @brief Constructs a PriceActionLabSystem with a list of patterns and a tie-breaker.
   * @param listOfPatterns A list of patterns to add.
   * @param tieBreaker The tie-breaker strategy.
   * @param useTieBreaker Flag to indicate whether to use the tie-breaker (defaults to false).
   */
  PriceActionLabSystem (std::list<PALPatternPtr>& listOfPatterns, 
			PatternTieBreakerPtr tieBreaker,
			bool useTieBreaker = false);
  /**
   * @brief Default constructor. Initializes with a default tie-breaker (SmallestVolatilityTieBreaker).
   */
  PriceActionLabSystem();
  /**
   * @brief Destructor.
   */
  ~PriceActionLabSystem();
  /**
   * @brief Gets a constant iterator to the beginning of the long patterns.
   * @return Constant iterator.
   */
  ConstSortedPatternIterator patternLongsBegin() const;
  /**
   * @brief Gets a constant iterator to the end of the long patterns.
   * @return Constant iterator.
   */
  ConstSortedPatternIterator patternLongsEnd() const;

  /**
   * @brief Gets an iterator to the beginning of the long patterns.
   * @return Iterator.
   */
  SortedPatternIterator patternLongsBegin();
  /**
   * @brief Gets an iterator to the end of the long patterns.
   * @return Iterator.
   */
  SortedPatternIterator patternLongsEnd();

  /**
   * @brief Gets a constant iterator to the beginning of the short patterns.
   * @return Constant iterator.
   */
  ConstSortedPatternIterator patternShortsBegin() const;
  /**
   * @brief Gets a constant iterator to the end of the short patterns.
   * @return Constant iterator.
   */
  ConstSortedPatternIterator patternShortsEnd() const;

  /**
   * @brief Gets an iterator to the beginning of the short patterns.
   * @return Iterator.
   */
  SortedPatternIterator patternShortsBegin();
  /**
   * @brief Gets an iterator to the end of the short patterns.
   * @return Iterator.
   */
  SortedPatternIterator patternShortsEnd();

  /**
   * @brief Gets a constant iterator to the beginning of all patterns.
   * @return Constant iterator.
   */
  ConstPatternIterator allPatternsBegin() const;
  /**
   * @brief Gets a constant iterator to the end of all patterns.
   * @return Constant iterator.
   */
  ConstPatternIterator allPatternsEnd() const;

  /**
   * @brief Adds a pattern to the system.
   * @param pattern The pattern to add.
   */
  void addPattern (PALPatternPtr pattern);
  /**
   * @brief Gets the total number of patterns in the system.
   * @return The total number of patterns.
   */
  unsigned long getNumPatterns() const;
  /**
   * @brief Gets the number of long patterns in the system.
   * @return The number of long patterns.
   */
  unsigned long getNumLongPatterns() const;
  /**
   * @brief Gets the number of short patterns in the system.
   * @return The number of short patterns.
   */
  unsigned long getNumShortPatterns() const;

private:
  /**
   * @brief Adds a long pattern to the system.
   * @param pattern The long pattern to add.
   */
  void addLongPattern (PALPatternPtr pattern);
  /**
   * @brief Adds a short pattern to the system.
   * @param pattern The short pattern to add.
   */
  void addShortPattern (PALPatternPtr pattern);
  /**
   * @brief Initializes and validates the provided tie-breaker.
   * @param providedTieBreaker The tie-breaker to initialize.
   */
  void initializeAndValidateTieBreaker(PatternTieBreakerPtr providedTieBreaker);
  /**
   * @brief Adds a pattern to the specified map, handling tie-breaking if necessary.
   * @param pattern The pattern to add.
   * @param patternMap The map to add the pattern to (passed by reference).
   * @param mapIdentifier A string identifier for logging purposes.
   */
  void addPatternToMap(PALPatternPtr pattern,
		       MapType& patternMap, // Pass map by reference
		       const std::string& mapIdentifier); // For logging


private:
  /**
   * @brief Map of long patterns.
   */
  MapType mLongsPatternMap;
  /**
   * @brief Map of short patterns.
   */
  MapType mShortsPatternMap;
  /**
   * @brief The pattern tie-breaker strategy.
   */
  PatternTieBreakerPtr mPatternTieBreaker;
  /**
   * @brief List of all patterns in the system.
   */
  std::list<PALPatternPtr> mAllPatterns;
  /**
   * @brief Flag indicating whether to use the tie-breaker.
   */
  bool mUseTieBreaker;
};

///////////////////////////////


/**
 * @brief Factory class for creating AST (Abstract Syntax Tree) nodes.
 *
 * This class provides methods to create various components of a Price Action Lab pattern,
 * such as price bar references, market entry expressions, and profit/stop loss targets.
 * It often reuses existing objects to save memory.
 */
class AstFactory
{
public:
  /**
   * @brief Maximum number of bar offsets for predefined price bar references.
   */
  static const int MaxNumBarOffsets = 15;
  
  /**
   * @brief Default constructor. Initializes predefined objects.
   */
  AstFactory();
  /**
   * @brief Destructor. Cleans up allocated objects.
   */
  ~AstFactory();

  AstFactory(const AstFactory&) = delete;
  AstFactory& operator=(const AstFactory&) = delete;
  
  /**
   * @brief Gets a PriceBarOpen reference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a PriceBarOpen object.
   */
  std::shared_ptr<PriceBarReference> getPriceOpen (unsigned int barOffset);
  /**
   * @brief Gets a PriceBarHigh reference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a PriceBarHigh object.
   */
  std::shared_ptr<PriceBarReference> getPriceHigh (unsigned int barOffset);
  /**
   * @brief Gets a PriceBarLow reference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a PriceBarLow object.
   */
  std::shared_ptr<PriceBarReference> getPriceLow (unsigned int barOffset);
  /**
   * @brief Gets a PriceBarClose reference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a PriceBarClose object.
   */
  std::shared_ptr<PriceBarReference> getPriceClose (unsigned int barOffset);
  /**
   * @brief Gets a VolumeBarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a VolumeBarReference object.
   */
  std::shared_ptr<PriceBarReference> getVolume (unsigned int barOffset);
  /**
   * @brief Gets a Roc1BarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a Roc1BarReference object.
   */
  std::shared_ptr<PriceBarReference> getRoc1 (unsigned int barOffset);
  /**
   * @brief Gets an IBS1BarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to an IBS1BarReference object.
   */
  std::shared_ptr<PriceBarReference> getIBS1 (unsigned int barOffset);
  /**
   * @brief Gets an IBS2BarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to an IBS2BarReference object.
   */
  std::shared_ptr<PriceBarReference> getIBS2 (unsigned int barOffset);
  /**
   * @brief Gets an IBS3BarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to an IBS3BarReference object.
   */
  std::shared_ptr<PriceBarReference> getIBS3 (unsigned int barOffset);
  /**
   * @brief Gets a MeanderBarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a MeanderBarReference object.
   */
  std::shared_ptr<PriceBarReference> getMeander (unsigned int barOffset);
  /**
   * @brief Gets a VChartLowBarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a VChartLowBarReference object.
   */
  std::shared_ptr<PriceBarReference> getVChartLow (unsigned int barOffset);
  /**
   * @brief Gets a VChartHighBarReference for the given bar offset.
   * @param barOffset The bar offset.
   * @return Shared pointer to a VChartHighBarReference object.
   */
  std::shared_ptr<PriceBarReference> getVChartHigh (unsigned int barOffset);
  /**
   * @brief Gets a LongMarketEntryOnOpen expression.
   * @return Shared pointer to a LongMarketEntryOnOpen object.
   */
  std::shared_ptr<MarketEntryExpression> getLongMarketEntryOnOpen();
  /**
   * @brief Gets a ShortMarketEntryOnOpen expression.
   * @return Shared pointer to a ShortMarketEntryOnOpen object.
   */
  std::shared_ptr<MarketEntryExpression> getShortMarketEntryOnOpen();
  /**
   * @brief Gets a decimal7 number from a string.
   * @param numString The string representation of the number.
   * @return Shared pointer to a decimal7 object.
   */
  std::shared_ptr<decimal7> getDecimalNumber (char *numString);
  /**
   * @brief Gets a decimal7 number from an integer.
   * @param num The integer value.
   * @return Shared pointer to a decimal7 object.
   */
  std::shared_ptr<decimal7> getDecimalNumber (int num);
  /**
   * @brief Gets a LongSideProfitTargetInPercent expression.
   * @param profitTarget Shared pointer to the profit target value.
   * @return Shared pointer to a LongSideProfitTargetInPercent object.
   */
  std::shared_ptr<LongSideProfitTargetInPercent> getLongProfitTarget (std::shared_ptr<decimal7> profitTarget);
  /**
   * @brief Gets a ShortSideProfitTargetInPercent expression.
   * @param profitTarget Shared pointer to the profit target value.
   * @return Shared pointer to a ShortSideProfitTargetInPercent object.
   */
  std::shared_ptr<ShortSideProfitTargetInPercent> getShortProfitTarget (std::shared_ptr<decimal7> profitTarget);
  /**
   * @brief Gets a LongSideStopLossInPercent expression.
   * @param stopLoss Shared pointer to the stop loss value.
   * @return Shared pointer to a LongSideStopLossInPercent object.
   */
  std::shared_ptr<LongSideStopLossInPercent> getLongStopLoss(std::shared_ptr<decimal7> stopLoss);
  /**
   * @brief Gets a ShortSideStopLossInPercent expression.
   * @param stopLoss Shared pointer to the stop loss value.
   * @return Shared pointer to a ShortSideStopLossInPercent object.
   */
  std::shared_ptr<ShortSideStopLossInPercent> getShortStopLoss(std::shared_ptr<decimal7> stopLoss);

  // Legacy raw pointer interface for backward compatibility during migration
  PriceBarReference* getPriceOpenRaw (unsigned int barOffset) { return getPriceOpen(barOffset).get(); }
  PriceBarReference* getPriceHighRaw (unsigned int barOffset) { return getPriceHigh(barOffset).get(); }
  PriceBarReference* getPriceLowRaw (unsigned int barOffset) { return getPriceLow(barOffset).get(); }
  PriceBarReference* getPriceCloseRaw (unsigned int barOffset) { return getPriceClose(barOffset).get(); }
  PriceBarReference* getVolumeRaw (unsigned int barOffset) { return getVolume(barOffset).get(); }
  PriceBarReference* getRoc1Raw (unsigned int barOffset) { return getRoc1(barOffset).get(); }
  PriceBarReference* getIBS1Raw (unsigned int barOffset) { return getIBS1(barOffset).get(); }
  PriceBarReference* getIBS2Raw (unsigned int barOffset) { return getIBS2(barOffset).get(); }
  PriceBarReference* getIBS3Raw (unsigned int barOffset) { return getIBS3(barOffset).get(); }
  PriceBarReference* getMeanderRaw (unsigned int barOffset) { return getMeander(barOffset).get(); }
  PriceBarReference* getVChartLowRaw (unsigned int barOffset) { return getVChartLow(barOffset).get(); }
  PriceBarReference* getVChartHighRaw (unsigned int barOffset) { return getVChartHigh(barOffset).get(); }
  MarketEntryExpression* getLongMarketEntryOnOpenRaw() { return getLongMarketEntryOnOpen().get(); }
  MarketEntryExpression* getShortMarketEntryOnOpenRaw() { return getShortMarketEntryOnOpen().get(); }
  decimal7* getDecimalNumberRaw (char *numString) { return getDecimalNumber(numString).get(); }
  decimal7* getDecimalNumberRaw (int num) { return getDecimalNumber(num).get(); }
  LongSideProfitTargetInPercent* getLongProfitTargetRaw (decimal7 *profitTarget) {
    return getLongProfitTarget(std::shared_ptr<decimal7>(profitTarget, [](decimal7*){})).get();
  }
  ShortSideProfitTargetInPercent* getShortProfitTargetRaw (decimal7 *profitTarget) {
    return getShortProfitTarget(std::shared_ptr<decimal7>(profitTarget, [](decimal7*){})).get();
  }
  LongSideStopLossInPercent* getLongStopLossRaw(decimal7 *stopLoss) {
    return getLongStopLoss(std::shared_ptr<decimal7>(stopLoss, [](decimal7*){})).get();
  }
  ShortSideStopLossInPercent* getShortStopLossRaw(decimal7 *stopLoss) {
    return getShortStopLoss(std::shared_ptr<decimal7>(stopLoss, [](decimal7*){})).get();
  }

private:
  /**
   * @brief Initializes the arrays of predefined price bar references.
   */
  void initializePriceBars();

private:

  /**
   * @brief Array of predefined PriceBarOpen references.
   */
  std::shared_ptr<PriceBarReference> mPredefinedPriceOpen[MaxNumBarOffsets];
  /**
   * @brief Array of predefined PriceBarHigh references.
   */
  std::shared_ptr<PriceBarReference> mPredefinedPriceHigh[MaxNumBarOffsets];
  /**
   * @brief Array of predefined PriceBarLow references.
   */
  std::shared_ptr<PriceBarReference> mPredefinedPriceLow[MaxNumBarOffsets];
  /**
   * @brief Array of predefined PriceBarClose references.
   */
  std::shared_ptr<PriceBarReference> mPredefinedPriceClose[MaxNumBarOffsets];
  /**
   * @brief Array of predefined VolumeBarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedVolume[MaxNumBarOffsets];
  /**
   * @brief Array of predefined Roc1BarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedRoc1[MaxNumBarOffsets];
  /**
   * @brief Array of predefined IBS1BarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedIBS1[MaxNumBarOffsets];
  /**
   * @brief Array of predefined IBS2BarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedIBS2[MaxNumBarOffsets];
  /**
   * @brief Array of predefined IBS3BarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedIBS3[MaxNumBarOffsets];
  /**
   * @brief Array of predefined MeanderBarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedMeander[MaxNumBarOffsets];
  /**
   * @brief Array of predefined VChartLowBarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedVChartLow[MaxNumBarOffsets];
  /**
   * @brief Array of predefined VChartHighBarReference objects.
   */
  std::shared_ptr<PriceBarReference> mPredefinedVChartHigh[MaxNumBarOffsets];
  /**
   * @brief Predefined LongMarketEntryOnOpen expression.
   */
  std::shared_ptr<MarketEntryExpression> mLongEntryOnOpen;
  /**
   * @brief Predefined ShortMarketEntryOnOpen expression.
   */
  std::shared_ptr<MarketEntryExpression> mShortEntryOnOpen;
  /**
   * @brief Map for caching decimal numbers created from strings.
   */
  std::map<std::string, std::shared_ptr<decimal7>> mDecimalNumMap;
  /**
   * @brief Map for caching decimal numbers created from integers.
   */
  std::map<int, std::shared_ptr<decimal7>> mDecimalNumMap2;
  /**
   * @brief Map for caching LongSideProfitTargetInPercent objects.
   */
  std::map<decimal7, std::shared_ptr<LongSideProfitTargetInPercent>> mLongsProfitTargets;
  /**
   * @brief Map for caching ShortSideProfitTargetInPercent objects.
   */
  std::map<decimal7, std::shared_ptr<ShortSideProfitTargetInPercent>> mShortsProfitTargets;
  /**
   * @brief Map for caching LongSideStopLossInPercent objects.
   */
  std::map<decimal7, std::shared_ptr<LongSideStopLossInPercent>> mLongsStopLoss;
  /**
   * @brief Map for caching ShortSideStopLossInPercent objects.
   */
  std::map<decimal7, std::shared_ptr<ShortSideStopLossInPercent>> mShortsStopLoss;
  mutable std::mutex mDecimalNumMapMutex;
  mutable std::mutex mDecimalNumMap2Mutex;
  mutable std::mutex mLongsProfitTargetsMutex;
  mutable std::mutex mShortsProfitTargetsMutex;
  mutable std::mutex mLongsStopLossMutex;
  mutable std::mutex mShortsStopLossMutex;
};



#endif
