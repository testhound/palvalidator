#pragma once
#include "IBootstrapObserver.h"
#include <fstream>
#include <mutex>
#include <filesystem>

namespace palvalidator::diagnostics {

class CsvBootstrapCollector : public IBootstrapObserver {
public:
    explicit CsvBootstrapCollector(const std::string& filepath);
    ~CsvBootstrapCollector();

    void onBootstrapResult(const BootstrapDiagnosticRecord& record) override;

private:
    void writeHeaderIfNeeded();

    std::string m_filepath;
    std::ofstream m_ofs;
    std::mutex m_mutex;
    bool m_headerWritten = false;
};

} // namespace palvalidator::diagnostics

