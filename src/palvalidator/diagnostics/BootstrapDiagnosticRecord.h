#pragma once

#include <string>

namespace palvalidator::diagnostics {

enum class MetricType { GeoMean, ProfitFactor };

class BootstrapDiagnosticRecord {
public:
    BootstrapDiagnosticRecord(
        std::string strategyName,
        std::string symbol,
        MetricType metricType,
        std::string chosenMethod,
        double chosenLowerBound,
        double chosenUpperBound,
        double score,
        size_t sampleSize,
        size_t numResamples,
        double standardError,
        double skewness,
        bool bcaAvailable,
        double bcaZ0,
        double bcaAccel,
        double bcaStabilityPenalty,
        double bcaLengthPenalty,
        double bcaRawLength)
        : m_strategyName(std::move(strategyName)),
          m_symbol(std::move(symbol)),
          m_metricType(metricType),
          m_chosenMethod(std::move(chosenMethod)),
          m_chosenLowerBound(chosenLowerBound),
          m_chosenUpperBound(chosenUpperBound),
          m_score(score),
          m_sampleSize(sampleSize),
          m_numResamples(numResamples),
          m_standardError(standardError),
          m_skewness(skewness),
          m_bcaAvailable(bcaAvailable),
          m_bcaZ0(bcaZ0),
          m_bcaAccel(bcaAccel),
          m_bcaStabilityPenalty(bcaStabilityPenalty),
          m_bcaLengthPenalty(bcaLengthPenalty),
          m_bcaRawLength(bcaRawLength)
    {}

    BootstrapDiagnosticRecord() = delete;

    const std::string& getStrategyName() const { return m_strategyName; }
    const std::string& getSymbol() const { return m_symbol; }
    MetricType getMetricType() const { return m_metricType; }
    const std::string& getChosenMethod() const { return m_chosenMethod; }
    double getChosenLowerBound() const { return m_chosenLowerBound; }
    double getChosenUpperBound() const { return m_chosenUpperBound; }
    double getScore() const { return m_score; }
    size_t getSampleSize() const { return m_sampleSize; }
    size_t getNumResamples() const { return m_numResamples; }
    double getStandardError() const { return m_standardError; }
    double getSkewness() const { return m_skewness; }
    bool isBcaAvailable() const { return m_bcaAvailable; }
    double getBcaZ0() const { return m_bcaZ0; }
    double getBcaAccel() const { return m_bcaAccel; }
    double getBcaStabilityPenalty() const { return m_bcaStabilityPenalty; }
    double getBcaLengthPenalty() const { return m_bcaLengthPenalty; }
    double getBcaRawLength() const { return m_bcaRawLength; }

private:
    const std::string m_strategyName;
    const std::string m_symbol;
    const MetricType m_metricType;
    const std::string m_chosenMethod;
    const double m_chosenLowerBound;
    const double m_chosenUpperBound;
    const double m_score;
    const size_t m_sampleSize;
    const size_t m_numResamples;
    const double m_standardError;
    const double m_skewness;
    const bool m_bcaAvailable;
    const double m_bcaZ0;
    const double m_bcaAccel;
    const double m_bcaStabilityPenalty;
    const double m_bcaLengthPenalty;
    const double m_bcaRawLength;
};

} // namespace palvalidator::diagnostics

