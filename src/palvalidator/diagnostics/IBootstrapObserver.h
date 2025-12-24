#pragma once
#include "BootstrapDiagnosticRecord.h"

namespace palvalidator::diagnostics {

class IBootstrapObserver {
public:
    virtual ~IBootstrapObserver() = default;
    virtual void onBootstrapResult(const BootstrapDiagnosticRecord& record) = 0;
};

} // namespace palvalidator::diagnostics

