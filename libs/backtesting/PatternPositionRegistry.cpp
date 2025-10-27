// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential

#include "PatternPositionRegistry.h"
#include <iostream>
#include <iomanip>

namespace mkc_timeseries
{

void PatternPositionRegistry::registerOrderPattern(uint32_t orderID, 
                                                   std::shared_ptr<PriceActionLabPattern> pattern) {
    if (!pattern) {
        return; // Don't register null patterns
    }
    
    std::lock_guard<std::mutex> lock(mMutex);
    mOrderPatterns[orderID] = pattern;
    mTotalOrdersRegistered++;
}

void PatternPositionRegistry::transferOrderToPosition(uint32_t orderID, uint32_t positionID) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    auto it = mOrderPatterns.find(orderID);
    if (it != mOrderPatterns.end()) {
        auto pattern = it->second;
        
        // Register position pattern mapping
        mPositionPatterns[positionID] = pattern;
        
        // Update reverse mapping
        mPatternPositions[pattern].push_back(positionID);
        
        // Keep order mapping for potential debugging/tracing
        // Note: We don't remove the order mapping here to maintain audit trail
        
        mTotalPositionsRegistered++;
    }
}

std::shared_ptr<PriceActionLabPattern> 
PatternPositionRegistry::getPatternForPosition(uint32_t positionID) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPositionPatterns.find(positionID);
    return (it != mPositionPatterns.end()) ? it->second : nullptr;
}

std::shared_ptr<PriceActionLabPattern> 
PatternPositionRegistry::getPatternForOrder(uint32_t orderID) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mOrderPatterns.find(orderID);
    return (it != mOrderPatterns.end()) ? it->second : nullptr;
}

std::vector<uint32_t> PatternPositionRegistry::getPositionsForPattern(
    std::shared_ptr<PriceActionLabPattern> pattern) const {
    if (!pattern) {
        return std::vector<uint32_t>();
    }
    
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPatternPositions.find(pattern);
    return (it != mPatternPositions.end()) ? it->second : std::vector<uint32_t>();
}

std::vector<std::shared_ptr<PriceActionLabPattern>> 
PatternPositionRegistry::getAllPatterns() const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::shared_ptr<PriceActionLabPattern>> patterns;
    
    for (const auto& entry : mPatternPositions) {
        patterns.push_back(entry.first);
    }
    
    return patterns;
}

void PatternPositionRegistry::removeOrder(uint32_t orderID) {
    std::lock_guard<std::mutex> lock(mMutex);
    mOrderPatterns.erase(orderID);
}

void PatternPositionRegistry::removePosition(uint32_t positionID) {
    std::lock_guard<std::mutex> lock(mMutex);
    
    auto it = mPositionPatterns.find(positionID);
    if (it != mPositionPatterns.end()) {
        auto pattern = it->second;
        mPositionPatterns.erase(it);
        
        // Remove from reverse mapping
        auto patIt = mPatternPositions.find(pattern);
        if (patIt != mPatternPositions.end()) {
            auto& positions = patIt->second;
            positions.erase(std::remove(positions.begin(), positions.end(), positionID), 
                           positions.end());
            
            // Remove pattern entry if no positions remain
            if (positions.empty()) {
                mPatternPositions.erase(patIt);
            }
        }
    }
}

bool PatternPositionRegistry::hasPatternForOrder(uint32_t orderID) const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mOrderPatterns.find(orderID) != mOrderPatterns.end();
}

bool PatternPositionRegistry::hasPatternForPosition(uint32_t positionID) const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mPositionPatterns.find(positionID) != mPositionPatterns.end();
}

size_t PatternPositionRegistry::getOrderCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mOrderPatterns.size();
}

size_t PatternPositionRegistry::getPositionCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mPositionPatterns.size();
}

size_t PatternPositionRegistry::getPatternCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mPatternPositions.size();
}

size_t PatternPositionRegistry::getTotalOrdersRegistered() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mTotalOrdersRegistered;
}

size_t PatternPositionRegistry::getTotalPositionsRegistered() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mTotalPositionsRegistered;
}

void PatternPositionRegistry::clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mOrderPatterns.clear();
    mPositionPatterns.clear();
    mPatternPositions.clear();
    mTotalOrdersRegistered = 0;
    mTotalPositionsRegistered = 0;
}

void PatternPositionRegistry::generateDebugReport(std::ostream& output) const {
    std::lock_guard<std::mutex> lock(mMutex);
    
    output << "=== PatternPositionRegistry Debug Report ===" << std::endl;
    output << "Orders tracked: " << mOrderPatterns.size() << " (Total registered: " << mTotalOrdersRegistered << ")" << std::endl;
    output << "Positions tracked: " << mPositionPatterns.size() << " (Total registered: " << mTotalPositionsRegistered << ")" << std::endl;
    output << "Patterns tracked: " << mPatternPositions.size() << std::endl;
    output << std::endl;
    
    if (!mPatternPositions.empty()) {
        output << "Pattern -> Position mappings:" << std::endl;
        for (const auto& entry : mPatternPositions) {
            const auto& pattern = entry.first;
            const auto& positions = entry.second;
            
            output << "  Pattern [" << pattern.get() << "]: " << positions.size() << " positions (";
            for (size_t i = 0; i < positions.size(); ++i) {
                if (i > 0) output << ", ";
                output << positions[i];
            }
            output << ")" << std::endl;
        }
    }
}

} // namespace mkc_timeseries