#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "Domain/ServiceContracts.hpp"

namespace Librova::Application {

class CScopedStructuredProgressSink final : public Librova::Domain::IProgressSink, public Librova::Domain::IStructuredImportProgressSink
{
public:
    CScopedStructuredProgressSink(
        Librova::Domain::IProgressSink& innerSink,
        std::size_t totalEntries,
        std::size_t processedEntries,
        std::size_t contributionEntries);

    void ReportValue(int percent, std::string_view message) override;
    [[nodiscard]] bool IsCancellationRequested() const override;
    void ReportStructuredProgress(
        std::size_t totalEntries,
        std::size_t processedEntries,
        std::size_t importedEntries,
        std::size_t failedEntries,
        std::size_t skippedEntries,
        int percent,
        std::string_view message) override;

private:
    [[nodiscard]] int MapPercent(int localPercent) const noexcept;

    Librova::Domain::IProgressSink& m_innerSink;
    std::size_t m_totalEntries = 0;
    std::size_t m_processedEntries = 0;
    std::size_t m_contributionEntries = 1;
};

void ReportStructuredProgressIfSupported(
    Librova::Domain::IProgressSink& progressSink,
    std::size_t totalEntries,
    std::size_t processedEntries,
    std::size_t importedEntries,
    std::size_t failedEntries,
    std::size_t skippedEntries,
    std::string_view message);

} // namespace Librova::Application
