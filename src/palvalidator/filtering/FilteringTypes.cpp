#include "FilteringTypes.h"

namespace palvalidator
{
  namespace filtering
  {
    
    FilteringSummary::FilteringSummary()
      : mInsufficientCount(0),
	mFlaggedCount(0),
	mFlagPassCount(0),
	mFailLBoundCount(0),
	mFailLVarCount(0),
	mFailSplitCount(0),
	mFailTailCount(0),
	mFailRegimeMixCount(0) 
    {}
  } // namespace filtering
} // namespace palvalidator
