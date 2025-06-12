/**
 * @file PalAst.cpp
 * @brief Implementation of the Abstract Syntax Tree (AST) nodes for Price Action Lab.
 *
 * This file contains the definitions of constructors, destructors,
 * visitor pattern accept methods, hash code generation, and other
 * specific logic for each AST node type. It also includes the
 * implementation of the AstFactory for creating and managing AST nodes.
 */
#include <random>
#include "PalAst.h"
#include "PalCodeGenVisitor.h"
#include <stdio.h> // For GetBaseFilename (legacy, consider cstdio and string manipulation)
#include <string.h> // For strerror (legacy, consider cerrno and string)
#include <errno.h>  // For errno

/**
 * @brief Static constant defining the maximum number of bar offsets for which
 *        PriceBarReference objects are pre-cached by the AstFactory.
 */
const int AstFactory::MaxNumBarOffsets;

/**
 * @brief Computes a hash for a given C-style string.
 * This function uses a simple rolling hash algorithm.
 * @param s The input null-terminated C-style string.
 * @return The computed 64-bit hash value.
 */
unsigned long long hash_str(const char* s)
{
    unsigned long long h = 31ULL; // Initial seed
    while (*s) {
        // Mixing step: combines current hash with character value
        h = (h * 54059ULL) ^ (static_cast<unsigned long long>(s[0]) * 76963ULL);
        ++s;
    }
    return h;
}

/**
 * @brief Combines a new value into an existing hash seed.
 * This function uses a mixing algorithm inspired by FNV (Fowler-Noll-Vo)
 * and Boost's hash_combine. It's designed to distribute hash values well.
 * @param seed The current hash value (will be modified).
 * @param value The new value to incorporate into the hash.
 */
static inline void hash_combine(unsigned long long &seed, unsigned long long value)
{
    // Combines the seed with the new value using XOR and bit shifts
    // The constant 0x9e3779b97f4a7c15ULL is derived from the golden ratio,
    // often used in hash functions to improve distribution.
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

/**
 * @brief Extracts the base filename from a path (removes extension).
 * @param filename The full filename or path.
 * @return The base filename without the extension. If no extension is found,
 *         or if the '.' is the first character, the original filename is returned.
 */
std::string GetBaseFilename(const char *filename)
{
    std::string fName(filename);
    size_t pos = fName.rfind("."); // Find the last occurrence of '.'
    if(pos == std::string::npos)  // No extension found.
        return fName;

    if(pos == 0)    // '.' is at the front (e.g., ".bashrc"). Not treated as an extension.
        return fName;

    return fName.substr(0, pos); // Extract substring before the last '.'
}

// --- PriceBarReference ---

/**
 * @brief Constructs a PriceBarReference.
 * @param barOffset The offset of the price bar (e.g., 0 for current bar, 1 for previous).
 */
PriceBarReference::PriceBarReference(unsigned int barOffset) : mBarOffset(barOffset)
{}


/**
 * @brief Destructor for PriceBarReference.
 * Virtual destructor to ensure proper cleanup of derived classes.
 */
PriceBarReference::~PriceBarReference()
{}

/**
 * @brief Copy constructor for PriceBarReference.
 * @param rhs The PriceBarReference object to copy from.
 */
PriceBarReference::PriceBarReference (const PriceBarReference& rhs) 
  : mBarOffset (rhs.mBarOffset) 
{}

/**
 * @brief Assignment operator for PriceBarReference.
 * @param rhs The PriceBarReference object to assign from.
 * @return A reference to this PriceBarReference object.
 */
PriceBarReference& 
PriceBarReference::operator=(const PriceBarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  mBarOffset = rhs.mBarOffset;
  return *this;
}

/**
 * @brief Gets the bar offset for this price bar reference.
 * @return The bar offset value.
 */
unsigned int
PriceBarReference::getBarOffset() const
{
  return mBarOffset;
}


// --- PriceBarOpen ---

/**
 * @brief Constructs a PriceBarOpen object.
 * @param barOffset The offset of the price bar.
 */
PriceBarOpen :: PriceBarOpen(unsigned int barOffset) : 
  PriceBarReference(barOffset),
  mComputedHash(0) // Initialize computed hash to 0 (not yet computed)
{}

/**
 * @brief Copy constructor for PriceBarOpen.
 * @param rhs The PriceBarOpen object to copy from.
 */
PriceBarOpen::PriceBarOpen (const PriceBarOpen& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for PriceBarOpen.
 * @param rhs The PriceBarOpen object to assign from.
 * @return A reference to this PriceBarOpen object.
 */
PriceBarOpen& 
PriceBarOpen::operator=(const PriceBarOpen &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for PriceBarOpen.
 */
PriceBarOpen :: ~PriceBarOpen()
{}

/**
 * @brief Accepts a code generation visitor.
 * Implements the visitor pattern for PriceBarOpen nodes.
 * @param v The PalCodeGenVisitor instance.
 */
void PriceBarOpen::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to the visitor's visit method for PriceBarOpen
}

/**
 * @brief Computes the hash code for this PriceBarOpen node.
 * The hash is computed based on the class name ("PriceBarOpen") and the bar offset.
 * The result is cached after the first computation to improve performance.
 * @return The 64-bit hash code.
 */
unsigned long long PriceBarOpen::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("PriceBarOpen"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::OPEN for PriceBarOpen objects.
 */
PriceBarReference::ReferenceType PriceBarOpen::getReferenceType()
{
  return PriceBarReference::OPEN;
}

/**
 * @brief Gets the number of extra bars needed for this reference type.
 * For PriceBarOpen, no extra bars are needed beyond the offset.
 * @return 0.
 */
int PriceBarOpen::extraBarsNeeded() const
{
  return 0;
}

// --- PriceBarHigh ---

/**
 * @brief Constructs a PriceBarHigh object.
 * @param barOffset The offset of the price bar.
 */
PriceBarHigh::PriceBarHigh(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash(0) // Initialize computed hash
{}

/**
 * @brief Destructor for PriceBarHigh.
 */
PriceBarHigh::~PriceBarHigh()
{}

/**
 * @brief Copy constructor for PriceBarHigh.
 * @param rhs The PriceBarHigh object to copy from.
 */
PriceBarHigh::PriceBarHigh (const PriceBarHigh& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for PriceBarHigh.
 * @param rhs The PriceBarHigh object to assign from.
 * @return A reference to this PriceBarHigh object.
 */
PriceBarHigh& 
PriceBarHigh::operator=(const PriceBarHigh &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void PriceBarHigh::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for PriceBarHigh
}

/**
 * @brief Computes the hash code for this PriceBarHigh node.
 * The hash is computed based on the class name ("PriceBarHigh") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long PriceBarHigh::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("PriceBarHigh"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::HIGH for PriceBarHigh objects.
 */
PriceBarReference::ReferenceType PriceBarHigh::getReferenceType()
{
  return PriceBarReference::HIGH;
}

/**
 * @brief Gets the number of extra bars needed for this reference type.
 * For PriceBarHigh, no extra bars are needed beyond the offset.
 * @return 0.
 */
int PriceBarHigh::extraBarsNeeded() const
{
  return 0;
}


// --- PriceBarLow ---

/**
 * @brief Constructs a PriceBarLow object.
 * @param barOffset The offset of the price bar.
 */
PriceBarLow :: PriceBarLow(unsigned int barOffset) : 
  PriceBarReference(barOffset),
  mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for PriceBarLow.
 * @param rhs The PriceBarLow object to copy from.
 */
PriceBarLow::PriceBarLow (const PriceBarLow& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for PriceBarLow.
 * @param rhs The PriceBarLow object to assign from.
 * @return A reference to this PriceBarLow object.
 */
PriceBarLow& 
PriceBarLow::operator=(const PriceBarLow &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for PriceBarLow.
 */
PriceBarLow :: ~PriceBarLow()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void PriceBarLow::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for PriceBarLow
}

/**
 * @brief Computes the hash code for this PriceBarLow node.
 * The hash is computed based on the class name ("PriceBarLow") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long PriceBarLow::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("PriceBarLow"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::LOW for PriceBarLow objects.
 */
PriceBarReference::ReferenceType PriceBarLow::getReferenceType()
{
  return PriceBarReference::LOW;
}

/**
 * @brief Gets the number of extra bars needed for this reference type.
 * For PriceBarLow, no extra bars are needed beyond the offset.
 * @return 0.
 */
int PriceBarLow::extraBarsNeeded() const
{
  return 0;
}

// --- PriceBarClose ---

/**
 * @brief Constructs a PriceBarClose object.
 * @param barOffset The offset of the price bar.
 */
PriceBarClose::PriceBarClose(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for PriceBarClose.
 * @param rhs The PriceBarClose object to copy from.
 */
PriceBarClose::PriceBarClose (const PriceBarClose& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for PriceBarClose.
 * @param rhs The PriceBarClose object to assign from.
 * @return A reference to this PriceBarClose object.
 */
PriceBarClose& 
PriceBarClose::operator=(const PriceBarClose &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for PriceBarClose.
 */
PriceBarClose::~PriceBarClose()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
PriceBarClose::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for PriceBarClose
}

/**
 * @brief Computes the hash code for this PriceBarClose node.
 * The hash is computed based on the class name ("PriceBarClose") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long PriceBarClose::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("PriceBarClose"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::CLOSE for PriceBarClose objects.
 */
PriceBarReference::ReferenceType PriceBarClose::getReferenceType()
{
  return PriceBarReference::CLOSE;
}

/**
 * @brief Gets the number of extra bars needed for this reference type.
 * For PriceBarClose, no extra bars are needed beyond the offset.
 * @return 0.
 */
int PriceBarClose::extraBarsNeeded() const
{
  return 0;
}

// --- VolumeBarReference ---

/**
 * @brief Constructs a VolumeBarReference object.
 * @param barOffset The offset of the price bar.
 */
VolumeBarReference::VolumeBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for VolumeBarReference.
 * @param rhs The VolumeBarReference object to copy from.
 */
VolumeBarReference::VolumeBarReference (const VolumeBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for VolumeBarReference.
 * @param rhs The VolumeBarReference object to assign from.
 * @return A reference to this VolumeBarReference object.
 */
VolumeBarReference& 
VolumeBarReference::operator=(const VolumeBarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for VolumeBarReference.
 */
VolumeBarReference::~VolumeBarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
VolumeBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for VolumeBarReference
}

/**
 * @brief Computes the hash code for this VolumeBarReference node.
 * The hash is computed based on the class name ("VolumeBarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long VolumeBarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("VolumeBarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::VOLUME for VolumeBarReference objects.
 */
PriceBarReference::ReferenceType VolumeBarReference::getReferenceType()
{
  return PriceBarReference::VOLUME;
}

/**
 * @brief Gets the number of extra bars needed for this reference type.
 * For VolumeBarReference, no extra bars are needed beyond the offset.
 * @return 0.
 */
int VolumeBarReference::extraBarsNeeded() const
{
  return 0;
}


// --- Roc1BarReference ---

/**
 * @brief Constructs a Roc1BarReference object.
 * @param barOffset The offset of the price bar.
 */
Roc1BarReference::Roc1BarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for Roc1BarReference.
 * @param rhs The Roc1BarReference object to copy from.
 */
Roc1BarReference::Roc1BarReference (const Roc1BarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for Roc1BarReference.
 * @param rhs The Roc1BarReference object to assign from.
 * @return A reference to this Roc1BarReference object.
 */
Roc1BarReference& 
Roc1BarReference::operator=(const Roc1BarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for Roc1BarReference.
 */
Roc1BarReference::~Roc1BarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
Roc1BarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for Roc1BarReference
}

/**
 * @brief Computes the hash code for this Roc1BarReference node.
 * The hash is computed based on the class name ("Roc1BarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long Roc1BarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("Roc1BarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::ROC1 for Roc1BarReference objects.
 */
PriceBarReference::ReferenceType Roc1BarReference::getReferenceType()
{
  return PriceBarReference::ROC1;
}

/**
 * @brief Gets the number of extra bars needed for this reference type.
 * For Roc1BarReference (1-period Rate of Change), 1 extra bar is needed.
 * @return 1.
 */
int Roc1BarReference::extraBarsNeeded() const
{
  return 1;
}

// --- MeanderBarReference ---

/**
 * @brief Constructs a MeanderBarReference object.
 * @param barOffset The offset of the price bar.
 */
MeanderBarReference::MeanderBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for MeanderBarReference.
 * @param rhs The MeanderBarReference object to copy from.
 */
MeanderBarReference::MeanderBarReference (const MeanderBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for MeanderBarReference.
 * @param rhs The MeanderBarReference object to assign from.
 * @return A reference to this MeanderBarReference object.
 */
MeanderBarReference& 
MeanderBarReference::operator=(const MeanderBarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for MeanderBarReference.
 */
MeanderBarReference::~MeanderBarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
MeanderBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for MeanderBarReference
}

/**
 * @brief Computes the hash code for this MeanderBarReference node.
 * The hash is computed based on the class name ("MeanderBarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long MeanderBarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("MeanderBarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::MEANDER for MeanderBarReference objects.
 */
PriceBarReference::ReferenceType MeanderBarReference::getReferenceType()
{
  return PriceBarReference::MEANDER;
}

/**
 * @brief Gets the number of extra bars needed for the Meander indicator.
 * @return 5 (as Meander typically requires a lookback period).
 */
int MeanderBarReference::extraBarsNeeded() const
{
  return 5;
}

// --- VChartLowBarReference ---

/**
 * @brief Constructs a VChartLowBarReference object.
 * @param barOffset The offset of the price bar.
 */
VChartLowBarReference::VChartLowBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for VChartLowBarReference.
 * @param rhs The VChartLowBarReference object to copy from.
 */
VChartLowBarReference::VChartLowBarReference (const VChartLowBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for VChartLowBarReference.
 * @param rhs The VChartLowBarReference object to assign from.
 * @return A reference to this VChartLowBarReference object.
 */
VChartLowBarReference& 
VChartLowBarReference::operator=(const VChartLowBarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for VChartLowBarReference.
 */
VChartLowBarReference::~VChartLowBarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
VChartLowBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for VChartLowBarReference
}

/**
 * @brief Computes the hash code for this VChartLowBarReference node.
 * The hash is computed based on the class name ("VChartLowBarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long VChartLowBarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("VChartLowBarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::VCHARTLOW for VChartLowBarReference objects.
 */
PriceBarReference::ReferenceType VChartLowBarReference::getReferenceType()
{
  return PriceBarReference::VCHARTLOW;
}

/**
 * @brief Gets the number of extra bars needed for the VChartLow indicator.
 * @return 6 (as VChart indicators typically require a lookback period).
 */
int VChartLowBarReference::extraBarsNeeded() const
{
  return 6;
}

// --- VChartHighBarReference ---

/**
 * @brief Constructs a VChartHighBarReference object.
 * @param barOffset The offset of the price bar.
 */
VChartHighBarReference::VChartHighBarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for VChartHighBarReference.
 * @param rhs The VChartHighBarReference object to copy from.
 */
VChartHighBarReference::VChartHighBarReference (const VChartHighBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for VChartHighBarReference.
 * @param rhs The VChartHighBarReference object to assign from.
 * @return A reference to this VChartHighBarReference object.
 */
VChartHighBarReference& 
VChartHighBarReference::operator=(const VChartHighBarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for VChartHighBarReference.
 */
VChartHighBarReference::~VChartHighBarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
VChartHighBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for VChartHighBarReference
}

/**
 * @brief Computes the hash code for this VChartHighBarReference node.
 * The hash is computed based on the class name ("VChartHighBarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long VChartHighBarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("VChartHighBarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::VCHARTHIGH for VChartHighBarReference objects.
 */
PriceBarReference::ReferenceType VChartHighBarReference::getReferenceType()
{
  return PriceBarReference::VCHARTHIGH;
}

/**
 * @brief Gets the number of extra bars needed for the VChartHigh indicator.
 * @return 6 (as VChart indicators typically require a lookback period).
 */
int VChartHighBarReference::extraBarsNeeded() const
{
  return 6;
}

// --- IBS1BarReference ---

/**
 * @brief Constructs an IBS1BarReference object.
 * IBS (Internal Bar Strength) type 1.
 * @param barOffset The offset of the price bar.
 */
IBS1BarReference::IBS1BarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for IBS1BarReference.
 * @param rhs The IBS1BarReference object to copy from.
 */
IBS1BarReference::IBS1BarReference (const IBS1BarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for IBS1BarReference.
 * @param rhs The IBS1BarReference object to assign from.
 * @return A reference to this IBS1BarReference object.
 */
IBS1BarReference& 
IBS1BarReference::operator=(const IBS1BarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for IBS1BarReference.
 */
IBS1BarReference::~IBS1BarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
IBS1BarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for IBS1BarReference
}

/**
 * @brief Computes the hash code for this IBS1BarReference node.
 * The hash is computed based on the class name ("IBS1BarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long IBS1BarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("IBS1BarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::IBS1 for IBS1BarReference objects.
 */
PriceBarReference::ReferenceType IBS1BarReference::getReferenceType()
{
  return PriceBarReference::IBS1;
}

/**
 * @brief Gets the number of extra bars needed for IBS1.
 * @return 0.
 */
int IBS1BarReference::extraBarsNeeded() const
{
  return 0;
}

// --- IBS2BarReference ---

/**
 * @brief Constructs an IBS2BarReference object.
 * IBS (Internal Bar Strength) type 2.
 * @param barOffset The offset of the price bar.
 */
IBS2BarReference::IBS2BarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for IBS2BarReference.
 * @param rhs The IBS2BarReference object to copy from.
 */
IBS2BarReference::IBS2BarReference (const IBS2BarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for IBS2BarReference.
 * @param rhs The IBS2BarReference object to assign from.
 * @return A reference to this IBS2BarReference object.
 */
IBS2BarReference& 
IBS2BarReference::operator=(const IBS2BarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for IBS2BarReference.
 */
IBS2BarReference::~IBS2BarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
IBS2BarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for IBS2BarReference
}

/**
 * @brief Computes the hash code for this IBS2BarReference node.
 * The hash is computed based on the class name ("IBS2BarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long IBS2BarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("IBS2BarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::IBS2 for IBS2BarReference objects.
 */
PriceBarReference::ReferenceType IBS2BarReference::getReferenceType()
{
  return PriceBarReference::IBS2;
}

/**
 * @brief Gets the number of extra bars needed for IBS2.
 * @return 1.
 */
int IBS2BarReference::extraBarsNeeded() const
{
  return 1;
}


// --- IBS3BarReference ---

/**
 * @brief Constructs an IBS3BarReference object.
 * IBS (Internal Bar Strength) type 3.
 * @param barOffset The offset of the price bar.
 */
IBS3BarReference::IBS3BarReference(unsigned int barOffset) 
  : PriceBarReference(barOffset),
    mComputedHash (0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for IBS3BarReference.
 * @param rhs The IBS3BarReference object to copy from.
 */
IBS3BarReference::IBS3BarReference (const IBS3BarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Assignment operator for IBS3BarReference.
 * @param rhs The IBS3BarReference object to assign from.
 * @return A reference to this IBS3BarReference object.
 */
IBS3BarReference& 
IBS3BarReference::operator=(const IBS3BarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Destructor for IBS3BarReference.
 */
IBS3BarReference::~IBS3BarReference()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
IBS3BarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for IBS3BarReference
}

/**
 * @brief Computes the hash code for this IBS3BarReference node.
 * The hash is computed based on the class name ("IBS3BarReference") and the bar offset.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long IBS3BarReference::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("IBS3BarReference"); // Start with class name hash
      hash_combine(seed, static_cast<unsigned long long>(getBarOffset())); // Combine bar offset
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::IBS3 for IBS3BarReference objects.
 */
PriceBarReference::ReferenceType IBS3BarReference::getReferenceType()
{
  return PriceBarReference::IBS3;
}

/**
 * @brief Gets the number of extra bars needed for IBS3.
 * @return 2.
 */
int IBS3BarReference::extraBarsNeeded() const
{
  return 2;
}

// --- MomersionFilterBarReference (Commented Out) ---
// Note: The following block is commented out in the original source.
// Doxygen comments are added here for completeness if it were active.
/*
*//**
 * @brief Constructs a MomersionFilterBarReference object.
 * Represents a reference to a Momersion Filter indicator value.
 * @param barOffset The offset of the price bar.
 * @param period The period used for the Momersion Filter calculation.
 *//*
MomersionFilterBarReference::MomersionFilterBarReference(unsigned int barOffset,
							 unsigned int period)
  : PriceBarReference(barOffset),
    mComputedHash (0), // Initialize computed hash
    mPeriod (period)
{}

*//**
 * @brief Copy constructor for MomersionFilterBarReference.
 * @param rhs The MomersionFilterBarReference object to copy from.
 *//*
MomersionFilterBarReference::MomersionFilterBarReference (const MomersionFilterBarReference& rhs)
  : PriceBarReference (rhs),
    mComputedHash (rhs.mComputedHash), // Copy cached hash
    mPeriod (rhs.mPeriod)
{}

*//**
 * @brief Assignment operator for MomersionFilterBarReference.
 * @param rhs The MomersionFilterBarReference object to assign from.
 * @return A reference to this MomersionFilterBarReference object.
 *//*
MomersionFilterBarReference& 
MomersionFilterBarReference::operator=(const MomersionFilterBarReference &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PriceBarReference::operator=(rhs); // Call base class assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  mPeriod = rhs.mPeriod;             // Copy period
  return *this;
}

*//**
 * @brief Destructor for MomersionFilterBarReference.
 *//*
MomersionFilterBarReference::~MomersionFilterBarReference()
{}

*//**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 *//*
void 
MomersionFilterBarReference::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for MomersionFilterBarReference
}

*//**
 * @brief Computes the hash code for this MomersionFilterBarReference node.
 * The hash is computed based on a fixed seed, the bar offset, and the period.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 *//*
unsigned long long MomersionFilterBarReference::hashCode()
{
  unsigned long long result = mComputedHash;

  if (result == 0) // Check if hash is already computed
    {
      // This hash implementation seems different from others, using fixed constants.
      // Consider standardizing if this class becomes active.
      result = 73;
      result = 113 * result + getBarOffset();
      // Note: The period is not explicitly included in this hash calculation,
      // which might lead to collisions if period is a distinguishing factor.
      // A more robust hash would include `mPeriod`.
      // e.g., hash_combine(seed, static_cast<unsigned long long>(mPeriod));
      mComputedHash = result; // Cache the result
    }

  return result;
}

*//**
 * @brief Gets the specific type of this price bar reference.
 * @return PriceBarReference::MOMERSIONFILTER for MomersionFilterBarReference objects.
 *//*
PriceBarReference::ReferenceType MomersionFilterBarReference::getReferenceType()
{
  return PriceBarReference::MOMERSIONFILTER;
}

*//**
 * @brief Gets the number of extra bars needed for the Momersion Filter.
 * This is determined by the filter's period.
 * @return The period of the Momersion Filter.
 *//*
int MomersionFilterBarReference::extraBarsNeeded() const
{
  return mPeriod;
}

*//**
 * @brief Gets the period used for the Momersion Filter.
 * @return The Momersion Filter period.
 *//*
unsigned int MomersionFilterBarReference::getMomersionPeriod() const
{
  return mPeriod;
}
*/

// --- PatternExpression ---

/**
 * @brief Default constructor for PatternExpression.
 */
PatternExpression::PatternExpression()
{}

/**
 * @brief Copy constructor for PatternExpression.
 * @param rhs The PatternExpression object to copy from.
 */
PatternExpression::PatternExpression (const PatternExpression& rhs)
{}

/**
 * @brief Assignment operator for PatternExpression.
 * @param rhs The PatternExpression object to assign from.
 * @return A reference to this PatternExpression object.
 */
PatternExpression& PatternExpression::operator=(const PatternExpression &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;
  // Base class has no members to copy, but good practice for derived classes
  return *this;
}

/**
 * @brief Virtual destructor for PatternExpression.
 * Ensures proper cleanup of derived expression types.
 */
PatternExpression::~PatternExpression()
{}

// --- GreaterThanExpr ---

/**
 * @brief Constructs a GreaterThanExpr object.
 * Represents a "greater than" comparison between two price bar references.
 * @param lhs Pointer to the PriceBarReference on the left-hand side of the comparison.
 * @param rhs Pointer to the PriceBarReference on the right-hand side of the comparison.
 * @note This class assumes that the ownership of `lhs` and `rhs` is managed elsewhere (typically by AstFactory).
 */
GreaterThanExpr::GreaterThanExpr (std::shared_ptr<PriceBarReference> lhs, std::shared_ptr<PriceBarReference> rhs)
  : PatternExpression(),
    mLhs(lhs),
    mRhs(rhs)
{}

GreaterThanExpr::GreaterThanExpr (PriceBarReference *lhs, PriceBarReference *rhs)
  : PatternExpression(),
    mLhs(lhs ? std::shared_ptr<PriceBarReference>(lhs, [](PriceBarReference*){}) : nullptr),
    mRhs(rhs ? std::shared_ptr<PriceBarReference>(rhs, [](PriceBarReference*){}) : nullptr)
{}

/**
 * @brief Copy constructor for GreaterThanExpr.
 * @param rhs The GreaterThanExpr object to copy from.
 */
GreaterThanExpr::GreaterThanExpr (const GreaterThanExpr& rhs)
  : PatternExpression(rhs), // Call base class copy constructor
  mLhs(rhs.mLhs),           // Copy shared_ptr (increases ref count)
  mRhs(rhs.mRhs)            // Copy shared_ptr (increases ref count)
{}

/**
 * @brief Assignment operator for GreaterThanExpr.
 * @param rhs The GreaterThanExpr object to assign from.
 * @return A reference to this GreaterThanExpr object.
 */
GreaterThanExpr&
GreaterThanExpr::operator=(const GreaterThanExpr &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PatternExpression::operator=(rhs); // Call base class assignment
  mLhs = rhs.mLhs; // Copy shared_ptr
  mRhs = rhs.mRhs; // Copy shared_ptr
  return *this;
}

/**
 * @brief Destructor for GreaterThanExpr.
 * @note Does not delete `mLhs` or `mRhs` as their ownership is managed by `AstFactory`.
 */
GreaterThanExpr::~GreaterThanExpr()
{}

/**
 * @brief Gets the left-hand side PriceBarReference of the expression.
 * @return Pointer to the left-hand side PriceBarReference.
 */
PriceBarReference *GreaterThanExpr::getLHS() const
{
  return mLhs.get();
}

std::shared_ptr<PriceBarReference> GreaterThanExpr::getLHSShared() const
{
  return mLhs;
}

/**
 * @brief Gets the right-hand side PriceBarReference of the expression.
 * @return Pointer to the right-hand side PriceBarReference.
 */
PriceBarReference *GreaterThanExpr::getRHS() const
{
  return mRhs.get();
}

std::shared_ptr<PriceBarReference> GreaterThanExpr::getRHSShared() const
{
  return mRhs;
}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
GreaterThanExpr::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for GreaterThanExpr
}

/**
 * @brief Computes the hash code for this GreaterThanExpr node.
 * The hash is computed based on the class name ("GreaterThanExpr") and the hash codes
 * of its left-hand side (LHS) and right-hand side (RHS) PriceBarReference nodes.
 * @return The 64-bit hash code.
 */
unsigned long long GreaterThanExpr::hashCode()
{
  unsigned long long seed = hash_str("GreaterThanExpr"); // Start with class name hash
  if (mLhs) hash_combine(seed, mLhs->hashCode()); // Combine hash of LHS
  if (mRhs) hash_combine(seed, mRhs->hashCode()); // Combine hash of RHS
  return seed;
}

// --- AndExpr ---

/**
 * @brief Constructs an AndExpr object using shared pointers for its operands.
 * Represents a logical "AND" operation between two pattern expressions.
 * @param lhs Shared pointer to the PatternExpression on the left-hand side.
 * @param rhs Shared pointer to the PatternExpression on the right-hand side.
 */
AndExpr::AndExpr(PatternExpressionPtr lhs, PatternExpressionPtr rhs)
  : mLeftHandSide(std::move(lhs)), // Use std::move for efficiency with rvalues
    mRightHandSide(std::move(rhs))
{}

/**
 * @brief Constructs an AndExpr object using raw pointers for its operands.
 * This constructor attempts to obtain shared ownership if the raw pointers
 * are already managed by `std::shared_ptr` (via `enable_shared_from_this`).
 * If not, it takes ownership by creating new `std::shared_ptr`s.
 * @param lhs Raw pointer to the left-hand side PatternExpression.
 * @param rhs Raw pointer to the right-hand side PatternExpression.
 */
AndExpr::AndExpr (PatternExpression *lhs, PatternExpression *rhs)
  : mLeftHandSide (), // Initialize with empty shared_ptr
    mRightHandSide () // Initialize with empty shared_ptr
{
  try
    {
      // Attempt to obtain a shared_ptr that shares ownership if one already exists
      // for the object pointed to by `lhs`. Requires `lhs` to be derived from
      // `std::enable_shared_from_this` and already managed by a shared_ptr.
      mLeftHandSide = lhs->shared_from_this();
    }
  catch (const std::bad_weak_ptr&)
    {
      // This exception occurs if `lhs` is not (yet) managed by any std::shared_ptr,
      // or if `lhs` is not derived from std::enable_shared_from_this.
      // In this context, it usually means `lhs` was newly created (e.g., `new GreaterThanExpr(...)`)
      // and AndExpr should take ownership.
      mLeftHandSide.reset(lhs); // Creates a new shared_ptr that now owns `lhs`.
    }

  try
    {
      mRightHandSide = rhs->shared_from_this();
    }
  catch (const std::bad_weak_ptr&)
    {
      mRightHandSide.reset(rhs); // AndExpr takes ownership of `rhs`.
    }
}

/**
 * @brief Copy constructor for AndExpr.
 * Performs a shallow copy of the shared pointers, increasing their reference counts.
 * @param rhs The AndExpr object to copy from.
 */
AndExpr::AndExpr (const AndExpr& rhs)
  : PatternExpression(rhs), // Call base class copy constructor
  mLeftHandSide(rhs.mLeftHandSide),   // Copy shared_ptr (increases ref count)
  mRightHandSide(rhs.mRightHandSide) // Copy shared_ptr (increases ref count)
{}

/**
 * @brief Assignment operator for AndExpr.
 * @param rhs The AndExpr object to assign from.
 * @return A reference to this AndExpr object.
 */
AndExpr& 
AndExpr::operator=(const AndExpr &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  PatternExpression::operator=(rhs); // Call base class assignment
  mLeftHandSide = rhs.mLeftHandSide;   // Assign shared_ptr (handles ref counts)
  mRightHandSide = rhs.mRightHandSide; // Assign shared_ptr
  return *this;
}

/**
 * @brief Destructor for AndExpr.
 * The `std::shared_ptr` members (`mLeftHandSide`, `mRightHandSide`) will
 * automatically manage the lifetime of the pointed-to expressions.
 */
AndExpr::~AndExpr()
{}

/**
 * @brief Gets the left-hand side PatternExpression.
 * @return Raw pointer to the left-hand side PatternExpression.
 */
PatternExpression* 
AndExpr::getLHS() const
{
  return mLeftHandSide.get(); // Returns the raw pointer from the shared_ptr
}

/**
 * @brief Gets the right-hand side PatternExpression.
 * @return Raw pointer to the right-hand side PatternExpression.
 */
PatternExpression*
AndExpr::getRHS() const
{
  return mRightHandSide.get(); // Returns the raw pointer from the shared_ptr
}

/**
 * @brief Gets the left-hand side PatternExpression as shared_ptr.
 * @return Shared pointer to the left-hand side PatternExpression.
 */
std::shared_ptr<PatternExpression>
AndExpr::getLHSShared() const
{
  return mLeftHandSide;
}

/**
 * @brief Gets the right-hand side PatternExpression as shared_ptr.
 * @return Shared pointer to the right-hand side PatternExpression.
 */
std::shared_ptr<PatternExpression>
AndExpr::getRHSShared() const
{
  return mRightHandSide;
}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
AndExpr::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for AndExpr
}

/**
 * @brief Computes the hash code for this AndExpr node.
 * The hash is computed based on the class name ("AndExpr") and the hash codes
 * of its left-hand side (LHS) and right-hand side (RHS) PatternExpression nodes.
 * @return The 64-bit hash code.
 */
unsigned long long AndExpr::hashCode()
{
  unsigned long long seed = hash_str("AndExpr"); // Start with class name hash
  hash_combine(seed, getLHS()->hashCode()); // Combine hash of LHS
  hash_combine(seed, getRHS()->hashCode()); // Combine hash of RHS
  return seed;
}

// --- ProfitTargetInPercentExpression ---

/**
 * @brief Constructs a ProfitTargetInPercentExpression.
 * @param profitTarget Shared pointer to a decimal7 value representing the profit target in percent.
 */
ProfitTargetInPercentExpression::ProfitTargetInPercentExpression(std::shared_ptr<decimal7> profitTarget)
  : mProfitTarget (profitTarget),
    mComputedHash(0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for ProfitTargetInPercentExpression.
 * @param rhs The ProfitTargetInPercentExpression object to copy from.
 */
ProfitTargetInPercentExpression::ProfitTargetInPercentExpression (const ProfitTargetInPercentExpression& rhs) 
  : mProfitTarget (rhs.mProfitTarget),    // Copy pointer (ownership managed by factory)
    mComputedHash (rhs.mComputedHash)   // Copy cached hash
{}

/**
 * @brief Assignment operator for ProfitTargetInPercentExpression.
 * @param rhs The ProfitTargetInPercentExpression object to assign from.
 * @return A reference to this ProfitTargetInPercentExpression object.
 */
ProfitTargetInPercentExpression& 
ProfitTargetInPercentExpression::operator=(const ProfitTargetInPercentExpression &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  mProfitTarget = rhs.mProfitTarget; // Copy pointer
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Virtual destructor for ProfitTargetInPercentExpression.
 */
ProfitTargetInPercentExpression::~ProfitTargetInPercentExpression()
{}

/**
 * @brief Gets the profit target value.
 * @return Pointer to the decimal7 profit target value.
 */
decimal7 *ProfitTargetInPercentExpression::getProfitTarget() const
{
  return mProfitTarget.get();
}

/**
 * @brief Gets the profit target value as shared_ptr.
 * @return Shared pointer to the decimal7 profit target value.
 */
std::shared_ptr<decimal7> ProfitTargetInPercentExpression::getProfitTargetShared() const
{
  return mProfitTarget;
}

/**
 * @brief Computes the hash code for this ProfitTargetInPercentExpression node.
 * The hash is computed based on the class name ("ProfitTargetInPercentExpression")
 * and the string representation of the profit target value.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long
ProfitTargetInPercentExpression::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("ProfitTargetInPercentExpression"); // Start with class name hash
      if (mProfitTarget) {
        auto s = num::toString(*mProfitTarget); // Convert decimal to string
        hash_combine(seed, hash_str(s.c_str())); // Combine hash of the string value
      }
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

// --- LongSideProfitTargetInPercent ---

/**
 * @brief Constructs a LongSideProfitTargetInPercent object.
 * @param profitTarget Shared pointer to the decimal7 profit target value.
 */
LongSideProfitTargetInPercent::LongSideProfitTargetInPercent (std::shared_ptr<decimal7> profitTarget)
  : ProfitTargetInPercentExpression (profitTarget) // Call base constructor
{}

/**
 * @brief Copy constructor for LongSideProfitTargetInPercent.
 * @param rhs The LongSideProfitTargetInPercent object to copy from.
 */
LongSideProfitTargetInPercent::LongSideProfitTargetInPercent (const LongSideProfitTargetInPercent& rhs) :
  ProfitTargetInPercentExpression (rhs) // Call base copy constructor
{}

/**
 * @brief Assignment operator for LongSideProfitTargetInPercent.
 * @param rhs The LongSideProfitTargetInPercent object to assign from.
 * @return A reference to this LongSideProfitTargetInPercent object.
 */
LongSideProfitTargetInPercent& 
LongSideProfitTargetInPercent::operator=(const LongSideProfitTargetInPercent &rhs)
{
 if (this == &rhs) // Self-assignment check
    return *this;

  ProfitTargetInPercentExpression::operator=(rhs); // Call base assignment
  return *this;
}

/**
 * @brief Destructor for LongSideProfitTargetInPercent.
 */
LongSideProfitTargetInPercent::~LongSideProfitTargetInPercent()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
LongSideProfitTargetInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for LongSideProfitTargetInPercent
}

// --- ShortSideProfitTargetInPercent ---

/**
 * @brief Constructs a ShortSideProfitTargetInPercent object.
 * @param profitTarget Shared pointer to the decimal7 profit target value.
 */
ShortSideProfitTargetInPercent::ShortSideProfitTargetInPercent (std::shared_ptr<decimal7> profitTarget)
  : ProfitTargetInPercentExpression (profitTarget) // Call base constructor
{}

/**
 * @brief Copy constructor for ShortSideProfitTargetInPercent.
 * @param rhs The ShortSideProfitTargetInPercent object to copy from.
 */
ShortSideProfitTargetInPercent::ShortSideProfitTargetInPercent (const ShortSideProfitTargetInPercent& rhs) :
  ProfitTargetInPercentExpression (rhs) // Call base copy constructor
{}

/**
 * @brief Assignment operator for ShortSideProfitTargetInPercent.
 * @param rhs The ShortSideProfitTargetInPercent object to assign from.
 * @return A reference to this ShortSideProfitTargetInPercent object.
 */
ShortSideProfitTargetInPercent& 
ShortSideProfitTargetInPercent::operator=(const ShortSideProfitTargetInPercent &rhs)
{
 if (this == &rhs) // Self-assignment check
    return *this;

  ProfitTargetInPercentExpression::operator=(rhs); // Call base assignment
  return *this;
}

/**
 * @brief Destructor for ShortSideProfitTargetInPercent.
 */
ShortSideProfitTargetInPercent::~ShortSideProfitTargetInPercent()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
ShortSideProfitTargetInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for ShortSideProfitTargetInPercent
}

// --- StopLossInPercentExpression ---

/**
 * @brief Constructs a StopLossInPercentExpression.
 * @param stopLoss Shared pointer to a decimal7 value representing the stop loss in percent.
 */
StopLossInPercentExpression::StopLossInPercentExpression(std::shared_ptr<decimal7> stopLoss) :
  mStopLoss (stopLoss),
  mComputedHash(0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for StopLossInPercentExpression.
 * @param rhs The StopLossInPercentExpression object to copy from.
 */
StopLossInPercentExpression::StopLossInPercentExpression (const StopLossInPercentExpression& rhs) 
  : mStopLoss (rhs.mStopLoss),        // Copy pointer (ownership managed by factory)
    mComputedHash (rhs.mComputedHash)   // Copy cached hash
{}

/**
 * @brief Assignment operator for StopLossInPercentExpression.
 * @param rhs The StopLossInPercentExpression object to assign from.
 * @return A reference to this StopLossInPercentExpression object.
 */
StopLossInPercentExpression& 
StopLossInPercentExpression::operator=(const StopLossInPercentExpression &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  mStopLoss = rhs.mStopLoss;         // Copy pointer
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Virtual destructor for StopLossInPercentExpression.
 */
StopLossInPercentExpression::~StopLossInPercentExpression()
{}

/**
 * @brief Gets the stop loss value.
 * @return Pointer to the decimal7 stop loss value.
 */
decimal7 *StopLossInPercentExpression::getStopLoss() const
{
  return mStopLoss.get();
}

/**
 * @brief Gets the stop loss value as shared_ptr.
 * @return Shared pointer to the decimal7 stop loss value.
 */
std::shared_ptr<decimal7> StopLossInPercentExpression::getStopLossShared() const
{
  return mStopLoss;
}

/**
 * @brief Computes the hash code for this StopLossInPercentExpression node.
 * The hash is computed based on the class name ("StopLossInPercentExpression")
 * and the string representation of the stop loss value.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long
StopLossInPercentExpression::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("StopLossInPercentExpression"); // Start with class name hash
      if (mStopLoss) {
        auto s = num::toString(*mStopLoss); // Convert decimal to string
        hash_combine(seed, hash_str(s.c_str())); // Combine hash of the string value
      }
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

// --- LongSideStopLossInPercent ---

/**
 * @brief Constructs a LongSideStopLossInPercent object.
 * @param stopLoss Shared pointer to the decimal7 stop loss value.
 */
LongSideStopLossInPercent::LongSideStopLossInPercent(std::shared_ptr<decimal7> stopLoss)
  : StopLossInPercentExpression (stopLoss) // Call base constructor
{}

/**
 * @brief Copy constructor for LongSideStopLossInPercent.
 * @param rhs The LongSideStopLossInPercent object to copy from.
 */
LongSideStopLossInPercent::LongSideStopLossInPercent (const LongSideStopLossInPercent& rhs) 
  : StopLossInPercentExpression (rhs) // Call base copy constructor
{}

/**
 * @brief Assignment operator for LongSideStopLossInPercent.
 * @param rhs The LongSideStopLossInPercent object to assign from.
 * @return A reference to this LongSideStopLossInPercent object.
 */
LongSideStopLossInPercent& 
LongSideStopLossInPercent::operator=(const LongSideStopLossInPercent &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  StopLossInPercentExpression::operator=(rhs); // Call base assignment
  return *this;
}

/**
 * @brief Destructor for LongSideStopLossInPercent.
 */
LongSideStopLossInPercent::~LongSideStopLossInPercent()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
LongSideStopLossInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for LongSideStopLossInPercent
}

// --- ShortSideStopLossInPercent ---

/**
 * @brief Constructs a ShortSideStopLossInPercent object.
 * @param stopLoss Shared pointer to the decimal7 stop loss value.
 */
ShortSideStopLossInPercent::ShortSideStopLossInPercent(std::shared_ptr<decimal7> stopLoss)
  : StopLossInPercentExpression (stopLoss) // Call base constructor
{}

/**
 * @brief Copy constructor for ShortSideStopLossInPercent.
 * @param rhs The ShortSideStopLossInPercent object to copy from.
 */
ShortSideStopLossInPercent::ShortSideStopLossInPercent (const ShortSideStopLossInPercent& rhs) 
  : StopLossInPercentExpression (rhs) // Call base copy constructor
{}

/**
 * @brief Assignment operator for ShortSideStopLossInPercent.
 * @param rhs The ShortSideStopLossInPercent object to assign from.
 * @return A reference to this ShortSideStopLossInPercent object.
 */
ShortSideStopLossInPercent& 
ShortSideStopLossInPercent::operator=(const ShortSideStopLossInPercent &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  StopLossInPercentExpression::operator=(rhs); // Call base assignment
  return *this;
}

/**
 * @brief Destructor for ShortSideStopLossInPercent.
 */
ShortSideStopLossInPercent::~ShortSideStopLossInPercent()
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
ShortSideStopLossInPercent::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for ShortSideStopLossInPercent
}

// --- MarketEntryExpression ---

/**
 * @brief Default constructor for MarketEntryExpression.
 * This is the base class for different types of market entry logic.
 */
MarketEntryExpression::MarketEntryExpression()
{}

/**
 * @brief Virtual destructor for MarketEntryExpression.
 */
MarketEntryExpression::~MarketEntryExpression()
{}

/**
 * @brief Assignment operator for MarketEntryExpression.
 * @param rhs The MarketEntryExpression object to assign from.
 * @return A reference to this MarketEntryExpression object.
 */
MarketEntryExpression& 
MarketEntryExpression::operator=(const MarketEntryExpression &rhs)
{
  // No members in base, but include self-assignment check for good practice
  if (this == &rhs)
    return *this;
  return *this;
}

/**
 * @brief Copy constructor for MarketEntryExpression.
 * @param rhs The MarketEntryExpression object to copy from.
 */
MarketEntryExpression::MarketEntryExpression (const MarketEntryExpression& rhs)
{}

// --- MarketEntryOnOpen ---

/**
 * @brief Default constructor for MarketEntryOnOpen.
 * Represents a market entry that occurs at the open of the next bar.
 */
MarketEntryOnOpen::MarketEntryOnOpen() 
  : MarketEntryExpression() // Call base constructor
{}

/**
 * @brief Virtual destructor for MarketEntryOnOpen.
 */
MarketEntryOnOpen::~MarketEntryOnOpen()
{}

/**
 * @brief Assignment operator for MarketEntryOnOpen.
 * @param rhs The MarketEntryOnOpen object to assign from.
 * @return A reference to this MarketEntryOnOpen object.
 */
MarketEntryOnOpen& 
MarketEntryOnOpen::operator=(const MarketEntryOnOpen &rhs)
{
 if (this == &rhs) // Self-assignment check
    return *this;

  MarketEntryExpression::operator=(rhs); // Call base assignment
  return *this;
}

/**
 * @brief Copy constructor for MarketEntryOnOpen.
 * @param rhs The MarketEntryOnOpen object to copy from.
 */
MarketEntryOnOpen::MarketEntryOnOpen (const MarketEntryOnOpen& rhs)
  : MarketEntryExpression (rhs) // Call base copy constructor
{}

// --- LongMarketEntryOnOpen ---

/**
 * @brief Default constructor for LongMarketEntryOnOpen.
 * Represents a long market entry at the open of the next bar.
 */
LongMarketEntryOnOpen::LongMarketEntryOnOpen() 
  : MarketEntryOnOpen(), // Call base constructor
    mComputedHash(0)   // Initialize computed hash
{}

/**
 * @brief Destructor for LongMarketEntryOnOpen.
 */
LongMarketEntryOnOpen::~LongMarketEntryOnOpen()
{}

/**
 * @brief Assignment operator for LongMarketEntryOnOpen.
 * @param rhs The LongMarketEntryOnOpen object to assign from.
 * @return A reference to this LongMarketEntryOnOpen object.
 */
LongMarketEntryOnOpen& 
LongMarketEntryOnOpen::operator=(const LongMarketEntryOnOpen &rhs)
{
 if (this == &rhs) // Self-assignment check
    return *this;

  MarketEntryOnOpen::operator=(rhs); // Call base assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Copy constructor for LongMarketEntryOnOpen.
 * @param rhs The LongMarketEntryOnOpen object to copy from.
 */
LongMarketEntryOnOpen::LongMarketEntryOnOpen (const LongMarketEntryOnOpen& rhs)
  : MarketEntryOnOpen (rhs),        // Call base copy constructor
    mComputedHash(rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
LongMarketEntryOnOpen::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for LongMarketEntryOnOpen
}

/**
 * @brief Computes the hash code for this LongMarketEntryOnOpen node.
 * The hash is computed based on the class name ("LongMarketEntryOnOpen")
 * and a one-time random 64-bit number. This implies that different instances
 * will have different hash codes, even if they represent the same logical entry.
 * Consider if this is the desired behavior or if a more deterministic hash is needed.
 * @return The 64-bit hash code.
 */
unsigned long long 
LongMarketEntryOnOpen::hashCode()
{
 if (mComputedHash == 0) { // Check if hash is already computed
    // Base on the class name
    unsigned long long seed = hash_str("LongMarketEntryOnOpen");
    // Generate a one‑time random 64‑bit number for uniqueness
    std::random_device rd;
    unsigned long long rand_val = (static_cast<unsigned long long>(rd()) << 32) | rd();

    // Mix it in
    hash_combine(seed, rand_val);
    mComputedHash = seed; // Cache the result
  }
  return mComputedHash;  
}

// --- ShortMarketEntryOnOpen ---

/**
 * @brief Default constructor for ShortMarketEntryOnOpen.
 * Represents a short market entry at the open of the next bar.
 */
ShortMarketEntryOnOpen::ShortMarketEntryOnOpen() 
  : MarketEntryOnOpen(), // Call base constructor
    mComputedHash(0)   // Initialize computed hash
{}

/**
 * @brief Destructor for ShortMarketEntryOnOpen.
 */
ShortMarketEntryOnOpen::~ShortMarketEntryOnOpen()
{}

/**
 * @brief Assignment operator for ShortMarketEntryOnOpen.
 * @param rhs The ShortMarketEntryOnOpen object to assign from.
 * @return A reference to this ShortMarketEntryOnOpen object.
 */
ShortMarketEntryOnOpen& 
ShortMarketEntryOnOpen::operator=(const ShortMarketEntryOnOpen &rhs)
{
 if (this == &rhs) // Self-assignment check
    return *this;

  MarketEntryOnOpen::operator=(rhs); // Call base assignment
  mComputedHash = rhs.mComputedHash; // Copy cached hash
  return *this;
}

/**
 * @brief Copy constructor for ShortMarketEntryOnOpen.
 * @param rhs The ShortMarketEntryOnOpen object to copy from.
 */
ShortMarketEntryOnOpen::ShortMarketEntryOnOpen (const ShortMarketEntryOnOpen& rhs)
  : MarketEntryOnOpen (rhs),        // Call base copy constructor
    mComputedHash(rhs.mComputedHash) // Copy cached hash
{}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
ShortMarketEntryOnOpen::accept (PalCodeGenVisitor &v)
{
  v.visit(this); // Delegates to visitor's visit method for ShortMarketEntryOnOpen
}

/**
 * @brief Computes the hash code for this ShortMarketEntryOnOpen node.
 * The hash is computed based on the class name ("ShortMarketEntryOnOpen")
 * and a one-time random 64-bit number. Similar to LongMarketEntryOnOpen,
 * this ensures instance uniqueness rather than logical equality for hashing.
 * @return The 64-bit hash code.
 */
unsigned long long 
ShortMarketEntryOnOpen::hashCode()
{
  if (mComputedHash == 0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("ShortMarketEntryOnOpen"); // Start with class name hash
      // Generate a one‑time random 64‑bit number for uniqueness
      std::random_device rd;
      unsigned long long rand_val = (static_cast<unsigned long long>(rd()) << 32) | rd();
      hash_combine(seed, rand_val); // Mix it in
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

// --- PatternDescription ---

/**
 * @brief Constructs a PatternDescription object.
 * Stores metadata associated with a trading pattern.
 * @param fileName The name of the file from which the pattern was loaded.
 * @param patternIndex The index of the pattern within its source file.
 * @param indexDate The date associated with the pattern's discovery or indexing.
 * @param percentLong Pointer to a decimal7 value representing the historical percentage of profitable long trades.
 * @param percentShort Pointer to a decimal7 value representing the historical percentage of profitable short trades.
 * @param numTrades The total number of historical trades for this pattern.
 * @param consecutiveLosses The maximum number of consecutive historical losses for this pattern.
 * @note Ownership of `percentLong` and `percentShort` pointers is managed by `AstFactory`.
 */
PatternDescription::PatternDescription(const char *fileName, unsigned int patternIndex,
		     unsigned long indexDate, std::shared_ptr<decimal7> percentLong, std::shared_ptr<decimal7> percentShort,
		     unsigned int numTrades, unsigned int consecutiveLosses)
  : mFileName (fileName),
    mPatternIndex (patternIndex),
    mIndexDate (indexDate),
    mPercentLong (percentLong),
    mPercentShort (percentShort),
    mNumTrades (numTrades),
    mConsecutiveLosses (consecutiveLosses),
    mComputedHash(0) // Initialize computed hash
{}

/**
 * @brief Copy constructor for PatternDescription.
 * @param rhs The PatternDescription object to copy from.
 */
PatternDescription::PatternDescription (const PatternDescription& rhs)
  : mFileName (rhs.mFileName),
    mPatternIndex (rhs.mPatternIndex),
    mIndexDate (rhs.mIndexDate),
    mPercentLong (rhs.mPercentLong),       // Copy pointer
    mPercentShort (rhs.mPercentShort),     // Copy pointer
    mNumTrades (rhs.mNumTrades),
    mConsecutiveLosses (rhs.mConsecutiveLosses),
    mComputedHash(rhs.mComputedHash)       // Copy cached hash
{}

/**
 * @brief Assignment operator for PatternDescription.
 * @param rhs The PatternDescription object to assign from.
 * @return A reference to this PatternDescription object.
 */
PatternDescription& 
PatternDescription::operator=(const PatternDescription &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  mFileName = rhs.mFileName;
  mPatternIndex = rhs.mPatternIndex;
  mIndexDate = rhs.mIndexDate;
  mPercentLong = rhs.mPercentLong;
  mPercentShort = rhs.mPercentShort;
  mNumTrades = rhs.mNumTrades;
  mConsecutiveLosses = rhs.mConsecutiveLosses;
  mComputedHash = rhs.mComputedHash;

  return *this;
}

/**
 * @brief Destructor for PatternDescription.
 */
PatternDescription::~PatternDescription()
{}

/**
 * @brief Gets the filename associated with the pattern.
 * @return Constant reference to the filename string.
 */
const std::string& 
PatternDescription::getFileName() const
{
  return mFileName;
}

/**
 * @brief Gets the pattern index from its source file.
 * @return The pattern index.
 */
unsigned int 
PatternDescription::getpatternIndex() const
{
  return mPatternIndex;
}

/**
 * @brief Gets the index date of the pattern.
 * @return The index date.
 */
unsigned int 
PatternDescription::getIndexDate() const
{
  return mIndexDate;
}

/**
 * @brief Gets the historical percentage of profitable long trades.
 * @return Pointer to the decimal7 value for percent long.
 */
decimal7*
PatternDescription::getPercentLong() const
{
  return mPercentLong.get();
}

/**
 * @brief Gets the historical percentage of profitable short trades.
 * @return Pointer to the decimal7 value for percent short.
 */
decimal7*
PatternDescription::getPercentShort() const
{
  return mPercentShort.get();
}

/**
 * @brief Gets the historical percentage of profitable long trades as shared_ptr.
 * @return Shared pointer to the decimal7 value for percent long.
 */
std::shared_ptr<decimal7>
PatternDescription::getPercentLongShared() const
{
  return mPercentLong;
}

/**
 * @brief Gets the historical percentage of profitable short trades as shared_ptr.
 * @return Shared pointer to the decimal7 value for percent short.
 */
std::shared_ptr<decimal7>
PatternDescription::getPercentShortShared() const
{
  return mPercentShort;
}

/**
 * @brief Gets the total number of historical trades for the pattern.
 * @return The number of trades.
 */
unsigned int 
PatternDescription::numTrades() const
{
  return mNumTrades;
}

/**
 * @brief Gets the maximum number of consecutive historical losses for the pattern.
 * @return The maximum number of consecutive losses.
 */
unsigned int 
PatternDescription::numConsecutiveLosses() const
{
  return mConsecutiveLosses;
}

/**
 * @brief Computes the hash code for this PatternDescription node.
 * The hash is computed based on the class name ("PatternDescription") and all its
 * member variables, including the string representations of decimal values.
 * It is cached after the first computation.
 * @return The 64-bit hash code.
 */
unsigned long long
PatternDescription::hashCode()
{
  if (mComputedHash==0) // Check if hash is already computed
    {
      unsigned long long seed = hash_str("PatternDescription"); // Start with class name hash
      hash_combine(seed, hash_str(mFileName.c_str()));         // Combine filename
      hash_combine(seed, static_cast<unsigned long long>(mPatternIndex)); // Combine pattern index
      hash_combine(seed, static_cast<unsigned long long>(mIndexDate));    // Combine index date
      if (mPercentLong) {
        hash_combine(seed, hash_str(num::toString(*mPercentLong).c_str()));  // Combine percent long (as string)
      }
      if (mPercentShort) {
        hash_combine(seed, hash_str(num::toString(*mPercentShort).c_str())); // Combine percent short (as string)
      }
      hash_combine(seed, static_cast<unsigned long long>(mNumTrades));          // Combine number of trades
      hash_combine(seed, static_cast<unsigned long long>(mConsecutiveLosses)); // Combine consecutive losses
      mComputedHash = seed; // Cache the result
    }

  return mComputedHash;
}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void PatternDescription::accept (PalCodeGenVisitor &v)
{
  v.visit (this); // Delegates to visitor's visit method for PatternDescription
}

// --- PriceActionLabPattern ---

/**
 * @brief Static map to cache hash values of strings, used by `getStringHash`.
 * This helps to avoid recomputing hashes for frequently used strings (e.g., filenames).
 */
std::map<std::string, unsigned long long> PriceActionLabPattern:: mCachedStringHashMap;

/**
 * @brief Constructs a PriceActionLabPattern with default volatility and portfolio attributes.
 * @param description Pointer to the PatternDescription.
 * @param pattern Pointer to the PatternExpression.
 * @param entry Pointer to the MarketEntryExpression.
 * @param profitTarget Pointer to the ProfitTargetInPercentExpression.
 * @param stopLoss Pointer to the StopLossInPercentExpression.
 * @note Ownership of raw pointers is typically managed by AstFactory or through conversion to shared_ptr in other constructors.
 */
PriceActionLabPattern::PriceActionLabPattern (PatternDescription* description,
					      PatternExpression* pattern,
					      std::shared_ptr<MarketEntryExpression> entry,
					      std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
					      std::shared_ptr<StopLossInPercentExpression> stopLoss)
  : PriceActionLabPattern (description, pattern, entry,
			   profitTarget, stopLoss,
			   VOLATILITY_NONE, PORTFOLIO_FILTER_NONE) // Delegate to the more specific constructor
{}

/**
 * @brief Clones the current pattern with potentially new profit target and stop loss values.
 * This is useful for creating variations of a pattern.
 * @param profitTarget Pointer to the new ProfitTargetInPercentExpression.
 * @param stopLoss Pointer to the new StopLossInPercentExpression.
 * @return A shared_ptr to the newly created PriceActionLabPattern.
 */
shared_ptr<PriceActionLabPattern>
PriceActionLabPattern::clone (std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
			      std::shared_ptr<StopLossInPercentExpression> stopLoss)
{
  // Creates a new pattern instance, copying existing description, pattern expression, and market entry,
  // but using the provided profit target and stop loss.
  // Attributes like volatility and portfolio filter are taken from the original pattern.
  return std::make_shared<PriceActionLabPattern>(getPatternDescription(), // shared_ptr
						  getPatternExpression(),  // shared_ptr
						  getMarketEntry(),        // shared_ptr
						  profitTarget,            // shared_ptr
						  stopLoss,                // shared_ptr
                                                  mVolatilityAttribute,    // existing attribute
                                                  mPortfolioAttribute);    // existing attribute
}

/**
 * @brief Constructs a PriceActionLabPattern using shared_ptr for description and pattern.
 * @param description Shared_ptr to the PatternDescription.
 * @param pattern Shared_ptr to the PatternExpression.
 * @param entry Pointer to the MarketEntryExpression.
 * @param profitTarget Pointer to the ProfitTargetInPercentExpression.
 * @param stopLoss Pointer to the StopLossInPercentExpression.
 * Initializes with default VOLATILITY_NONE and PORTFOLIO_FILTER_NONE.
 * Calculates `mMaxBarsBack` and `mPayOffRatio` upon construction.
 */
PriceActionLabPattern::PriceActionLabPattern(PatternDescriptionPtr description,
					      PatternExpressionPtr pattern,
					      std::shared_ptr<MarketEntryExpression> entry,
					      std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
					      std::shared_ptr<StopLossInPercentExpression> stopLoss)
  : mPattern (pattern),
    mEntry (entry),
    mProfitTarget (profitTarget),
    mStopLoss (stopLoss),
    mPatternDescription (description),
    mVolatilityAttribute(VOLATILITY_NONE),
    mPortfolioAttribute (PORTFOLIO_FILTER_NONE),
    mMaxBarsBack(0),
    mPayOffRatio()
{
  mMaxBarsBack = PalPatternMaxBars::evaluateExpression (mPattern.get());
  mPayOffRatio = getProfitTargetAsDecimal() / getStopLossAsDecimal();
}

/**
 * @brief Constructs a PriceActionLabPattern with explicit volatility and portfolio attributes,
 *        taking raw pointers for description and pattern, and attempting to promote them to shared_ptr.
 * @param description Raw pointer to the PatternDescription.
 * @param pattern Raw pointer to the PatternExpression.
 * @param entry Pointer to the MarketEntryExpression.
 * @param profitTarget Pointer to the ProfitTargetInPercentExpression.
 * @param stopLoss Pointer to the StopLossInPercentExpression.
 * @param volatilityAttribute The volatility attribute for this pattern.
 * @param portfolioAttribute The portfolio filter attribute for this pattern.
 * Calculates `mMaxBarsBack` and `mPayOffRatio` upon construction.
 * This constructor handles cases where `description` or `pattern` might not yet be managed by a shared_ptr.
 */
PriceActionLabPattern::PriceActionLabPattern (PatternDescription* description,
					      PatternExpression* pattern,
					      std::shared_ptr<MarketEntryExpression> entry,
					      std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
					      std::shared_ptr<StopLossInPercentExpression> stopLoss,
					      VolatilityAttribute volatilityAttribute,
					      PortfolioAttribute portfolioAttribute)
  : mEntry (entry),
    mProfitTarget (profitTarget),
    mStopLoss (stopLoss),
    mVolatilityAttribute (volatilityAttribute),
    mPortfolioAttribute (portfolioAttribute),
    mMaxBarsBack(0),
    mPayOffRatio()
{
  try
    {
      // Attempt to share ownership if `pattern` is already managed by a shared_ptr
      this->mPattern = pattern->shared_from_this();
    }
  catch (const std::bad_weak_ptr&) {
        // If not, take ownership by creating a new shared_ptr
        this->mPattern.reset(pattern);
    }

    try
    {
      // Attempt to share ownership if `description` is already managed by a shared_ptr
      this->mPatternDescription = description->shared_from_this();
    }
    catch (const std::bad_weak_ptr&) {
        // If not, take ownership
        this->mPatternDescription.reset(description);
    }

  mMaxBarsBack = PalPatternMaxBars::evaluateExpression (mPattern.get());
  mPayOffRatio = getProfitTargetAsDecimal() / getStopLossAsDecimal();
}

/**
 * @brief Constructs a PriceActionLabPattern with explicit attributes and shared_ptrs for description and pattern.
 * @param description Shared_ptr to the PatternDescription.
 * @param pattern Shared_ptr to the PatternExpression.
 * @param entry Pointer to the MarketEntryExpression.
 * @param profitTarget Pointer to the ProfitTargetInPercentExpression.
 * @param stopLoss Pointer to the StopLossInPercentExpression.
 * @param volatilityAttribute The volatility attribute.
 * @param portfolioAttribute The portfolio filter attribute.
 * Calculates `mMaxBarsBack` and `mPayOffRatio` upon construction.
 */
PriceActionLabPattern::PriceActionLabPattern(PatternDescriptionPtr description,
					     PatternExpressionPtr pattern,
					     std::shared_ptr<MarketEntryExpression> entry,
					     std::shared_ptr<ProfitTargetInPercentExpression> profitTarget,
					     std::shared_ptr<StopLossInPercentExpression> stopLoss,
					     VolatilityAttribute volatilityAttribute,
					     PortfolioAttribute portfolioAttribute)
  : mPattern(std::move(pattern)), // Move shared_ptr
    mEntry(entry),
    mProfitTarget(profitTarget),
    mStopLoss(stopLoss),
    mPatternDescription(std::move(description)), // Move shared_ptr
    mVolatilityAttribute(volatilityAttribute),
    mPortfolioAttribute(portfolioAttribute),
    mMaxBarsBack(0),
    mPayOffRatio()
{
  mMaxBarsBack = PalPatternMaxBars::evaluateExpression (mPattern.get());
  mPayOffRatio = getProfitTargetAsDecimal() / getStopLossAsDecimal();
}

/**
 * @brief Copy constructor for PriceActionLabPattern.
 * Performs a shallow copy of members, including shared_ptrs (increasing their reference count).
 * @param rhs The PriceActionLabPattern object to copy from.
 */
PriceActionLabPattern::PriceActionLabPattern (const PriceActionLabPattern& rhs)
  : mPattern (rhs.mPattern),
    mEntry (rhs.mEntry),
    mProfitTarget (rhs.mProfitTarget),
    mStopLoss (rhs.mStopLoss),
    mPatternDescription (rhs.mPatternDescription),
    mVolatilityAttribute (rhs.mVolatilityAttribute),
    mPortfolioAttribute (rhs.mPortfolioAttribute),
    mMaxBarsBack(rhs.mMaxBarsBack),
    mPayOffRatio(rhs.mPayOffRatio)
{}

/**
 * @brief Assignment operator for PriceActionLabPattern.
 * @param rhs The PriceActionLabPattern object to assign from.
 * @return A reference to this PriceActionLabPattern object.
 */
PriceActionLabPattern& 
PriceActionLabPattern::operator=(const  PriceActionLabPattern &rhs)
{
  if (this == &rhs) // Self-assignment check
    return *this;

  mPattern = rhs.mPattern;
  mEntry = rhs.mEntry;
  mProfitTarget = rhs.mProfitTarget;
  mStopLoss = rhs.mStopLoss;
  mPatternDescription = rhs.mPatternDescription;
  mVolatilityAttribute = rhs.mVolatilityAttribute;
  mPortfolioAttribute = rhs.mPortfolioAttribute;
  mMaxBarsBack = rhs.mMaxBarsBack;
  mPayOffRatio = rhs.mPayOffRatio;

  return *this;
}

/**
 * @brief Destructor for PriceActionLabPattern.
 * @note Does not delete `mEntry`, `mProfitTarget`, or `mStopLoss` raw pointers
 * as their ownership is typically managed by `AstFactory` or through shared_ptrs
 * in specific construction paths. Shared_ptrs `mPattern` and `mPatternDescription`
 * handle their own lifecycle.
 */
PriceActionLabPattern::~PriceActionLabPattern()
{
}

/**
 * @brief Gets the pattern expression component of this trading pattern.
 * @return A shared_ptr to the PatternExpression.
 */
PatternExpressionPtr 
PriceActionLabPattern::getPatternExpression() const
{
  return mPattern;
}

/**
 * @brief Gets the filename from the pattern's description.
 * @return A constant reference to the filename string.
 */
const std::string& 
PriceActionLabPattern::getFileName() const
{
  return mPatternDescription->getFileName();
}

/**
 * @brief Gets the base filename (without extension) from the pattern's description.
 * @return The base filename string.
 */
const std::string PriceActionLabPattern::getBaseFileName() const
{
  return  GetBaseFilename (mPatternDescription->getFileName().c_str());
}

/**
 * @brief Gets the pattern index from the pattern's description.
 * @return The pattern index.
 */
unsigned int 
PriceActionLabPattern::getpatternIndex() const
{
  return mPatternDescription->getpatternIndex();
}

/**
 * @brief Gets the index date from the pattern's description.
 * @return The index date.
 */
unsigned int 
PriceActionLabPattern::getIndexDate() const
{
  return mPatternDescription->getIndexDate();
}

/**
 * @brief Gets the market entry expression component.
 * @return Pointer to the MarketEntryExpression.
 */
std::shared_ptr<MarketEntryExpression>
PriceActionLabPattern::getMarketEntry() const
{
  return mEntry;
}


/**
 * @brief Gets the profit target expression component.
 * @return Pointer to the ProfitTargetInPercentExpression.
 */
std::shared_ptr<ProfitTargetInPercentExpression>
PriceActionLabPattern::getProfitTarget() const
{
  return mProfitTarget;
}

/**
 * @brief Gets the profit target value as a decimal7 type.
 * @return The profit target value.
 */
decimal7
PriceActionLabPattern::getProfitTargetAsDecimal() const
{
  // Assumes getProfitTarget() and its internal getProfitTarget() always return valid pointers.
  decimal7 targetValue = *(getProfitTarget()->getProfitTarget());
  return targetValue;
}

/**
 * @brief Gets the stop loss expression component.
 * @return Pointer to the StopLossInPercentExpression.
 */
std::shared_ptr<StopLossInPercentExpression>
PriceActionLabPattern::getStopLoss() const
{
  return mStopLoss;
}

/**
 * @brief Gets the stop loss value as a decimal7 type.
 * @return The stop loss value.
 */
decimal7 
PriceActionLabPattern::getStopLossAsDecimal() const
{
  // Assumes getStopLoss() and its internal getStopLoss() always return valid pointers.
  decimal7 stopValue = *(getStopLoss()->getStopLoss());
  return stopValue;
}

/**
 * @brief Gets the pattern description component.
 * @return A shared_ptr to the PatternDescription.
 */
PatternDescriptionPtr 
PriceActionLabPattern::getPatternDescription() const
{
  return mPatternDescription;
}

/**
 * @brief Checks if the pattern has any volatility attribute set (Low, High, or VeryHigh).
 * @return True if a volatility attribute is set, false otherwise.
 */
bool 
PriceActionLabPattern::hasVolatilityAttribute() const
{
  return isLowVolatilityPattern() || isHighVolatilityPattern() || isVeryHighVolatilityPattern();
}

/**
 * @brief Checks if the pattern is marked as a low volatility pattern.
 * @return True if VOLATILITY_LOW is set, false otherwise.
 */
bool 
PriceActionLabPattern::isLowVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_LOW);
}

/**
 * @brief Checks if the pattern is marked as a normal volatility pattern.
 * @return True if VOLATILITY_NORMAL is set, false otherwise.
 */
bool 
PriceActionLabPattern::isNormalVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_NORMAL);
}

/**
 * @brief Checks if the pattern is marked as a high volatility pattern.
 * @return True if VOLATILITY_HIGH is set, false otherwise.
 */
bool 
PriceActionLabPattern::isHighVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_HIGH);
}

/**
 * @brief Checks if the pattern is marked as a very high volatility pattern.
 * @return True if VOLATILITY_VERY_HIGH is set, false otherwise.
 */
bool 
PriceActionLabPattern::isVeryHighVolatilityPattern() const
{
  return (mVolatilityAttribute == PriceActionLabPattern::VOLATILITY_VERY_HIGH);
}

/**
 * @brief Checks if the pattern has any portfolio filter attribute set (Long or Short).
 * @return True if a portfolio filter attribute is set, false otherwise.
 */
bool 
PriceActionLabPattern::hasPortfolioAttribute() const
{
  return (isFilteredLongPattern() || isFilteredShortPattern());
}

/**
 * @brief Checks if the pattern is filtered for long trades.
 * @return True if PORTFOLIO_FILTER_LONG is set, false otherwise.
 */
bool 
PriceActionLabPattern::isFilteredLongPattern() const
{
  return (mPortfolioAttribute == PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
}

/**
 * @brief Checks if the pattern is filtered for short trades.
 * @return True if PORTFOLIO_FILTER_SHORT is set, false otherwise.
 */
bool 
PriceActionLabPattern::isFilteredShortPattern() const
{
  return (mPortfolioAttribute == PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);
}

/**
 * @brief Accepts a code generation visitor.
 * @param v The PalCodeGenVisitor instance.
 */
void 
PriceActionLabPattern::accept (PalCodeGenVisitor &v)
{
  v.visit (this); // Delegates to visitor's visit method for PriceActionLabPattern
}


/**
 * @brief Gets a cached hash for a given string, or computes and caches it if not found.
 * This is used internally by `hashCode()` to optimize hashing of filenames.
 * @param key The string for which to get the hash.
 * @return The 64-bit hash value for the string.
 */
unsigned long long
PriceActionLabPattern::getStringHash (const std::string& key)
{
  std::map<std::string, unsigned long long>::iterator pos;

  pos = mCachedStringHashMap.find (key); // Check cache
  if (pos != mCachedStringHashMap.end())
    return (pos->second); // Return cached hash
  else
    {
      unsigned long long hashVal = hash_str (key.c_str()); // Compute hash
      mCachedStringHashMap.insert (std::make_pair(key, hashVal)); // Cache it
      return hashVal;
    }
}

/**
 * @brief Computes the hash code for this PriceActionLabPattern node.
 * The hash is computed based on the class name ("PriceActionLabPattern"), the base filename,
 * the hash codes of its constituent parts (pattern expression, description, entry, target, stop),
 * and its volatility and portfolio attributes.
 * @return The 64-bit hash code.
 */
unsigned long long
PriceActionLabPattern::hashCode()
{
  // Start FNV offset - a common practice for initializing hash seeds.
  constexpr unsigned long long FNV_offset = 0xcbf29ce484222325ULL;
  unsigned long long seed = FNV_offset;

  // Combine type identifier for PriceActionLabPattern itself.
  hash_combine(seed, hash_str("PriceActionLabPattern"));

  // Combine hash of the base filename (uses cached string hashing).
  auto key = getBaseFileName();
  hash_combine(seed, getStringHash(key));

  // Combine hash codes of all sub-components.
  hash_combine(seed, getPatternExpression()->hashCode());
  hash_combine(seed, getPatternDescription()->hashCode());
  hash_combine(seed, getMarketEntry()->hashCode());
  hash_combine(seed, getProfitTarget()->hashCode());
  hash_combine(seed, getStopLoss()->hashCode());

  // Combine volatility and portfolio attributes.
  hash_combine(seed, static_cast<unsigned long long>(mVolatilityAttribute));
  hash_combine(seed, static_cast<unsigned long long>(mPortfolioAttribute));

  return seed;
}

// --- AstFactory ---

/**
 * @brief Constructs an AstFactory.
 * Initializes various caches and pre-creates commonly used AST nodes
 * (like PriceBarOpen[0], PriceBarClose[1], etc.) up to `MaxNumBarOffsets`
 * to optimize memory usage and performance by reusing these objects.
 * Also initializes specific market entry objects.
 */
AstFactory::AstFactory() :
  mLongEntryOnOpen (std::make_shared<LongMarketEntryOnOpen>()),    // Pre-create long entry on open
  mShortEntryOnOpen (std::make_shared<ShortMarketEntryOnOpen>()),   // Pre-create short entry on open
  mDecimalNumMap(),      // Initialize map for string-to-decimal caching
  mDecimalNumMap2(),     // Initialize map for int-to-decimal caching
  mLongsProfitTargets(), // Initialize map for long profit target caching
  mShortsProfitTargets(),// Initialize map for short profit target caching
  mLongsStopLoss(),      // Initialize map for long stop loss caching
  mShortsStopLoss()      // Initialize map for short stop loss caching
{
  initializePriceBars(); // Pre-populate price bar reference objects
}

/**
 * @brief Destructor for AstFactory.
 * Cleans up all dynamically allocated AST nodes that were created and cached
 * by the factory, including predefined price bar references and market entry objects.
 * Shared_ptr managed objects (like cached decimal numbers, profit targets, stop losses)
 * are automatically deallocated when their reference counts drop to zero.
 */
AstFactory::~AstFactory()
{
  // All objects are now managed by std::shared_ptr and will be deallocated automatically
  // when their reference counts drop to zero.
}

/**
 * @brief Gets or creates a LongSideProfitTargetInPercent object.
 * If an object with the same profit target value already exists in the cache,
 * it's returned; otherwise, a new one is created, cached, and returned.
 * @param profitTarget Pointer to the decimal7 profit target value.
 * @return Pointer to the cached or newly created LongSideProfitTargetInPercent object.
 */
std::shared_ptr<LongSideProfitTargetInPercent> AstFactory::getLongProfitTarget (std::shared_ptr<decimal7> profitTarget)
{
  std::lock_guard<std::mutex> lock(mLongsProfitTargetsMutex);
  std::map<decimal7, std::shared_ptr<LongSideProfitTargetInPercent>>::const_iterator pos;

  pos = mLongsProfitTargets.find (*profitTarget); // Check cache
  if (pos != mLongsProfitTargets.end())
    return pos->second; // Return cached shared_ptr
  else
    {
      // Create new, store in shared_ptr, cache it, then return shared_ptr
      auto p = std::make_shared<LongSideProfitTargetInPercent>(profitTarget);
      mLongsProfitTargets.insert (std::make_pair(*profitTarget, p));
      return p;
    }
}

std::shared_ptr<ShortSideProfitTargetInPercent> AstFactory::getShortProfitTarget (std::shared_ptr<decimal7> profitTarget)
{
  std::lock_guard<std::mutex> lock(mShortsProfitTargetsMutex);
  std::map<decimal7, std::shared_ptr<ShortSideProfitTargetInPercent>>::const_iterator pos;

  pos = mShortsProfitTargets.find (*profitTarget); // Check cache
  if (pos != mShortsProfitTargets.end())
    return pos->second; // Return cached shared_ptr
  else
    {
      auto p = std::make_shared<ShortSideProfitTargetInPercent>(profitTarget);
      mShortsProfitTargets.insert (std::make_pair(*profitTarget, p));
      return p;
    }
}

std::shared_ptr<LongSideStopLossInPercent> AstFactory::getLongStopLoss(std::shared_ptr<decimal7> stopLoss)
{
  std::lock_guard<std::mutex> lock(mLongsStopLossMutex);
  std::map<decimal7, std::shared_ptr<LongSideStopLossInPercent>>::const_iterator pos;

  pos = mLongsStopLoss.find (*stopLoss); // Check cache
  if (pos != mLongsStopLoss.end())
    return pos->second; // Return cached shared_ptr
  else
    {
      auto p = std::make_shared<LongSideStopLossInPercent>(stopLoss);
      mLongsStopLoss.insert (std::make_pair(*stopLoss, p));
      return p;
    }
}

std::shared_ptr<ShortSideStopLossInPercent> AstFactory::getShortStopLoss(std::shared_ptr<decimal7> stopLoss)
{
  std::lock_guard<std::mutex> lock(mShortsStopLossMutex);
  std::map<decimal7, std::shared_ptr<ShortSideStopLossInPercent>>::const_iterator pos;

  pos = mShortsStopLoss.find (*stopLoss); // Check cache
  if (pos != mShortsStopLoss.end())
    return pos->second; // Return cached shared_ptr
  else
    {
      auto p = std::make_shared<ShortSideStopLossInPercent>(stopLoss);
      mShortsStopLoss.insert (std::make_pair(*stopLoss, p));
      return p;
    }
}

std::shared_ptr<MarketEntryExpression> AstFactory::getLongMarketEntryOnOpen()
{
  return mLongEntryOnOpen;
}

std::shared_ptr<MarketEntryExpression> AstFactory::getShortMarketEntryOnOpen()
{
    return mShortEntryOnOpen;
}

/**
 * @brief Initializes arrays of predefined PriceBarReference objects.
 * This method is called by the constructor to pre-populate common price bar references
 * for bar offsets from 0 up to `MaxNumBarOffsets - 1`.
 */
void AstFactory::initializePriceBars()
{
  for (unsigned int i = 0; i < AstFactory::MaxNumBarOffsets; i++)
    {
      mPredefinedPriceOpen[i] = std::make_shared<PriceBarOpen>(i);
      mPredefinedPriceHigh[i] = std::make_shared<PriceBarHigh>(i);
      mPredefinedPriceLow[i] = std::make_shared<PriceBarLow>(i);
      mPredefinedPriceClose[i] = std::make_shared<PriceBarClose>(i);
      mPredefinedVolume[i] = std::make_shared<VolumeBarReference>(i);
      mPredefinedRoc1[i] = std::make_shared<Roc1BarReference>(i);

      mPredefinedIBS1[i] = std::make_shared<IBS1BarReference>(i);
      mPredefinedIBS2[i] = std::make_shared<IBS2BarReference>(i);
      mPredefinedIBS3[i] = std::make_shared<IBS3BarReference>(i);
      
      mPredefinedMeander[i] = std::make_shared<MeanderBarReference>(i);
      mPredefinedVChartLow[i] = std::make_shared<VChartLowBarReference>(i);
      mPredefinedVChartHigh[i] = std::make_shared<VChartHighBarReference>(i);
    }
}

/**
 * @brief Gets a PriceBarOpen reference for the given bar offset.
 * Returns a pre-cached object if `barOffset` is within `MaxNumBarOffsets`,
 * otherwise creates a new one (which the caller might need to manage or the factory if it were designed to cache all).
 * @param barOffset The bar offset.
 * @return Pointer to a PriceBarOpen object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getPriceOpen (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceOpen[barOffset];
  else
    return std::make_shared<PriceBarOpen>(barOffset);
}

/**
 * @brief Gets a PriceBarHigh reference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Pointer to a PriceBarHigh object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getPriceHigh (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceHigh[barOffset];
  else
    return std::make_shared<PriceBarHigh>(barOffset);
}

/**
 * @brief Gets a PriceBarLow reference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Pointer to a PriceBarLow object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getPriceLow (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceLow[barOffset];
  else
    return std::make_shared<PriceBarLow>(barOffset);
}

/**
 * @brief Gets a PriceBarClose reference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Pointer to a PriceBarClose object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getPriceClose (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedPriceClose[barOffset];
  else
    return std::make_shared<PriceBarClose>(barOffset);
}

std::shared_ptr<PriceBarReference> AstFactory::getVolume (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedVolume[barOffset];
  else
    return std::make_shared<VolumeBarReference>(barOffset);
}

std::shared_ptr<PriceBarReference> AstFactory::getRoc1 (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedRoc1[barOffset];
  else
    return std::make_shared<Roc1BarReference>(barOffset);
}

/**
 * @brief Gets an IBS1BarReference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Pointer to an IBS1BarReference object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getIBS1 (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedIBS1[barOffset];
  else
    return std::make_shared<IBS1BarReference>(barOffset);
}


/**
 * @brief Gets an IBS2BarReference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Shared pointer to an IBS2BarReference object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getIBS2 (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedIBS2[barOffset];
  else
    return std::make_shared<IBS2BarReference>(barOffset);
}


/**
 * @brief Gets an IBS3BarReference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Shared pointer to an IBS3BarReference object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getIBS3 (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedIBS3[barOffset];
  else
    return std::make_shared<IBS3BarReference>(barOffset);
}



/**
 * @brief Gets a MeanderBarReference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Shared pointer to a MeanderBarReference object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getMeander (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedMeander[barOffset];
  else
    return std::make_shared<MeanderBarReference>(barOffset);
}



/**
 * @brief Gets a VChartLowBarReference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Shared pointer to a VChartLowBarReference object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getVChartLow (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedVChartLow[barOffset];
  else
    return std::make_shared<VChartLowBarReference>(barOffset);
}



/**
 * @brief Gets a VChartHighBarReference for the given bar offset.
 * Returns a pre-cached object or creates a new one.
 * @param barOffset The bar offset.
 * @return Shared pointer to a VChartHighBarReference object.
 */
std::shared_ptr<PriceBarReference> AstFactory::getVChartHigh (unsigned int barOffset)
{
  if (barOffset < AstFactory::MaxNumBarOffsets)
    return mPredefinedVChartHigh[barOffset];
  else
    return std::make_shared<VChartHighBarReference>(barOffset);
}



std::shared_ptr<decimal7> AstFactory::getDecimalNumber (char *numString)
{
  std::lock_guard<std::mutex> lock(mDecimalNumMapMutex);
  std::string key(numString);
  std::map<std::string, std::shared_ptr<decimal7>>::iterator pos;

  pos = mDecimalNumMap.find (key); // Check cache
  if (pos != mDecimalNumMap.end())
    return pos->second; // Return cached shared_ptr
  else
    {
      // Create new, store in shared_ptr, cache it, then return shared_ptr
      decimal7 num = num::fromString<decimal7 >(key);
      auto p = std::make_shared<decimal7>(num);
      mDecimalNumMap.insert (std::make_pair(key, p));
      return p;
    }
}

std::shared_ptr<decimal7> AstFactory::getDecimalNumber (int num)
{
  std::lock_guard<std::mutex> lock(mDecimalNumMap2Mutex);
  int key = num;
  std::map<int, std::shared_ptr<decimal7>>::iterator pos;

  pos = mDecimalNumMap2.find (key); // Check cache using integer key
  if (pos != mDecimalNumMap2.end())
    return pos->second; // Return cached shared_ptr
  else
    {
      // Create new, store in shared_ptr, cache it, then return shared_ptr
      auto p = std::make_shared<decimal7>(num);
      mDecimalNumMap2.insert (std::make_pair(key, p));
      return p;
    }
}

