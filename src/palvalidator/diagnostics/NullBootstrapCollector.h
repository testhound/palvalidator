#pragma once
#include "IBootstrapObserver.h"

namespace palvalidator::diagnostics {

class NullBootstrapCollector : public IBootstrapObserver {
public:
    NullBootstrapCollector() = default;
    ~NullBootstrapCollector() override = default;

    // No-op implementation: intentionally does nothing
    void onBootstrapResult(const BootstrapDiagnosticRecord& /*record*/) override {}
};

} // namespace palvalidator::diagnostics

