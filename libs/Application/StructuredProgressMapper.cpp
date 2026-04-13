#include "Application/StructuredProgressMapper.hpp"

namespace Librova::Application {

CScopedStructuredProgressSink::CScopedStructuredProgressSink(
    Librova::Domain::IProgressSink& innerSink,
    const std::size_t totalEntries,
    const std::size_t processedEntries,
    const std::size_t contributionEntries,
    const std::size_t priorImportedEntries,
    const std::size_t priorFailedEntries,
    const std::size_t priorSkippedEntries)
    : m_innerSink(innerSink)
    , m_totalEntries(totalEntries)
    , m_processedEntries(processedEntries)
    , m_contributionEntries(std::max<std::size_t>(contributionEntries, 1))
    , m_priorImportedEntries(priorImportedEntries)
    , m_priorFailedEntries(priorFailedEntries)
    , m_priorSkippedEntries(priorSkippedEntries)
{
}

void CScopedStructuredProgressSink::ReportValue(const int percent, const std::string_view message)
{
    m_innerSink.ReportValue(MapPercent(percent), message);
}

bool CScopedStructuredProgressSink::IsCancellationRequested() const
{
    return m_innerSink.IsCancellationRequested();
}

void CScopedStructuredProgressSink::ReportStructuredProgress(
    const std::size_t totalEntries,
    const std::size_t processedEntries,
    const std::size_t importedEntries,
    const std::size_t failedEntries,
    const std::size_t skippedEntries,
    const int percent,
    const std::string_view message)
{
    if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&m_innerSink); structuredSink != nullptr)
    {
        structuredSink->ReportStructuredProgress(
            m_totalEntries,
            m_processedEntries + processedEntries,
            m_priorImportedEntries + importedEntries,
            m_priorFailedEntries + failedEntries,
            m_priorSkippedEntries + skippedEntries,
            MapPercent(percent),
            message);
        return;
    }

    m_innerSink.ReportValue(MapPercent(percent), message);
}

int CScopedStructuredProgressSink::MapPercent(const int localPercent) const noexcept
{
    const auto clampedLocalPercent = std::clamp(localPercent, 0, 100);
    const auto scaledProgress = static_cast<long long>(m_processedEntries * 100)
        + (static_cast<long long>(m_contributionEntries) * clampedLocalPercent);
    const auto denominator = static_cast<long long>(std::max<std::size_t>(m_totalEntries, 1)) * 100LL;
    return static_cast<int>(std::clamp((scaledProgress * 100LL) / denominator, 0LL, 100LL));
}

void ReportStructuredProgressIfSupported(
    Librova::Domain::IProgressSink& progressSink,
    const std::size_t totalEntries,
    const std::size_t processedEntries,
    const std::size_t importedEntries,
    const std::size_t failedEntries,
    const std::size_t skippedEntries,
    const std::string_view message)
{
    if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
    {
        const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(totalEntries, 1));
        structuredSink->ReportStructuredProgress(
            totalEntries,
            processedEntries,
            importedEntries,
            failedEntries,
            skippedEntries,
            percent,
            message);
    }
}

} // namespace Librova::Application
