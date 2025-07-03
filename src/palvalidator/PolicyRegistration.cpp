#include "PolicyRegistration.h"

namespace statistics {

void initializePolicyRegistry() {
    // Register all available computation policies with comprehensive metadata
    
    // Basic Policies
    registerPolicyWithMetadata<mkc_timeseries::AllHighResLogPFPolicy>(
        "AllHighResLogPFPolicy",
        "High-Resolution Log Profit Factor",
        "High-resolution logarithmic profit factor analysis for detailed performance measurement",
        "basic",
        false,
        "1.0.0",
        "MKC Associates",
        {"profit-factor", "logarithmic", "high-resolution"},
        {"Sufficient trade history"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::RobustProfitFactorPolicy>(
        "RobustProfitFactorPolicy",
        "Robust Profit Factor",
        "Robust profit factor calculation with outlier handling for stable performance metrics",
        "basic",
        false,
        "1.0.0",
        "MKC Associates",
        {"profit-factor", "robust", "outlier-resistant"},
        {"Minimum 10 trades"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::NonGranularProfitFactorPolicy>(
        "NonGranularProfitFactorPolicy",
        "Non-Granular Profit Factor",
        "Simplified profit factor calculation without granular trade analysis",
        "basic",
        false,
        "1.0.0",
        "MKC Associates",
        {"profit-factor", "simple"},
        {"Basic trade data"}
    );
    
    // Return-based Policies
    registerPolicyWithMetadata<mkc_timeseries::CumulativeReturnPolicy>(
        "CumulativeReturnPolicy",
        "Cumulative Return Analysis",
        "Cumulative return calculation for long-term performance assessment",
        "returns",
        false,
        "1.0.0",
        "MKC Associates",
        {"returns", "cumulative", "long-term"},
        {"Price history"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::NormalizedReturnPolicy>(
        "NormalizedReturnPolicy",
        "Normalized Return Analysis",
        "Risk-adjusted normalized return calculations for fair performance comparison",
        "returns",
        false,
        "1.0.0",
        "MKC Associates",
        {"returns", "normalized", "risk-adjusted"},
        {"Volatility data"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::PessimisticReturnRatioPolicy>(
        "PessimisticReturnRatioPolicy",
        "Pessimistic Return Ratio",
        "Conservative return ratio analysis emphasizing downside risk",
        "returns",
        false,
        "1.0.0",
        "MKC Associates",
        {"returns", "conservative", "downside-risk"},
        {"Drawdown data"}
    );
    
    // PAL-specific Policies
    registerPolicyWithMetadata<mkc_timeseries::PalProfitabilityPolicy>(
        "PalProfitabilityPolicy",
        "PAL Profitability Analysis",
        "Pattern Analysis and Logic specific profitability metrics",
        "pal-specific",
        false,
        "1.0.0",
        "MKC Associates",
        {"pal", "profitability", "pattern-analysis"},
        {"PAL pattern data"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::GatedPerformanceScaledPalPolicy>(
        "GatedPerformanceScaledPalPolicy",
        "Gated Performance Scaled PAL",
        "Advanced PAL analysis with performance gating and scaling for robust pattern validation",
        "advanced",
        false,
        "1.2.0",
        "MKC Associates",
        {"pal", "gated", "scaled", "advanced", "recommended"},
        {"PAL patterns", "Performance thresholds"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::ConfidenceAdjustedPalPolicy>(
        "ConfidenceAdjustedPalPolicy",
        "Confidence Adjusted PAL",
        "PAL analysis with confidence interval adjustments for statistical reliability",
        "advanced",
        false,
        "1.1.0",
        "MKC Associates",
        {"pal", "confidence", "statistical", "advanced"},
        {"Statistical significance data"}
    );
    
    // Enhanced and Hybrid Policies
    registerPolicyWithMetadata<mkc_timeseries::EnhancedBarScorePolicy>(
        "EnhancedBarScorePolicy",
        "Enhanced Bar Score Analysis",
        "Advanced bar-by-bar scoring methodology for detailed market timing analysis",
        "advanced",
        false,
        "1.1.0",
        "MKC Associates",
        {"enhanced", "bar-score", "timing", "advanced"},
        {"High-frequency bar data"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::HybridEnhancedTradeAwarePolicy>(
        "HybridEnhancedTradeAwarePolicy",
        "Hybrid Enhanced Trade-Aware",
        "Sophisticated hybrid approach combining multiple methodologies with trade-aware enhancements",
        "advanced",
        false,
        "1.2.0",
        "MKC Associates",
        {"hybrid", "enhanced", "trade-aware", "sophisticated"},
        {"Multiple data sources", "Trade execution data"}
    );
    
    // Swing Trading Policies
    registerPolicyWithMetadata<mkc_timeseries::AccumulationSwingIndexPolicy>(
        "AccumulationSwingIndexPolicy",
        "Accumulation Swing Index",
        "Swing trading analysis based on accumulation patterns and market momentum",
        "swing-trading",
        false,
        "1.0.0",
        "MKC Associates",
        {"swing", "accumulation", "momentum"},
        {"Volume data", "Price swings"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::HybridSwingTradePolicy>(
        "HybridSwingTradePolicy",
        "Hybrid Swing Trade Analysis",
        "Advanced swing trading policy combining multiple swing detection methodologies",
        "swing-trading",
        false,
        "1.1.0",
        "MKC Associates",
        {"swing", "hybrid", "multi-method"},
        {"Swing detection algorithms"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::ProfitFactorGatedSwingPolicy>(
        "ProfitFactorGatedSwingPolicy",
        "Profit Factor Gated Swing",
        "Swing trading analysis with profit factor gating for quality control",
        "swing-trading",
        false,
        "1.0.0",
        "MKC Associates",
        {"swing", "profit-factor", "gated", "quality-control"},
        {"Profit factor thresholds"}
    );
    
    // Bootstrap-based Policies (Experimental)
    registerPolicyWithMetadata<mkc_timeseries::BootStrappedProfitFactorPolicy>(
        "BootStrappedProfitFactorPolicy",
        "Bootstrap Profit Factor",
        "Bootstrap-based profit factor analysis for statistical robustness testing",
        "experimental",
        true,
        "0.9.0",
        "MKC Associates",
        {"bootstrap", "profit-factor", "statistical", "experimental"},
        {"Sufficient sample size", "Bootstrap libraries"}
    );
    
    registerPolicyWithMetadata<mkc_timeseries::BootStrappedProfitabilityPFPolicy>(
        "BootStrappedProfitabilityPFPolicy",
        "Bootstrap Profitability with PF",
        "Advanced bootstrap profitability analysis combined with profit factor metrics",
        "experimental",
        true,
        "0.8.0",
        "MKC Associates",
        {"bootstrap", "profitability", "profit-factor", "experimental"},
        {"Large dataset", "Statistical computing resources"}
    );
}

} // namespace statistics