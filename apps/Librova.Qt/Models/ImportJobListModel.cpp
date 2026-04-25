#include "Models/ImportJobListModel.hpp"

#include <algorithm>

#include "Adapters/QtStringConversions.hpp"

namespace LibrovaQt {

namespace {

QString formatProgressSummary(const Librova::ApplicationJobs::SImportJobSnapshot& snapshot)
{
    using E = Librova::ApplicationJobs::EImportJobStatus;

    if (snapshot.TotalEntries == 0)
    {
        switch (snapshot.Status)
        {
            case E::Completed:
            case E::Failed:
                return QStringLiteral("No supported files were found in the selected import sources.");
            case E::Cancelled:
                return QStringLiteral("Import was cancelled before Librova finished processing the selected files.");
            default:
                return QStringLiteral("Preparing import workload...");
        }
    }

    const std::size_t processedEntries = std::min(snapshot.ProcessedEntries, snapshot.TotalEntries);
    const int percent = std::clamp(snapshot.Percent, 0, 100);
    const QString fileNoun = snapshot.TotalEntries == 1 ? QStringLiteral("file") : QStringLiteral("files");
    return QStringLiteral("Processed %1 of %2 %3 (%4%). Imported %5, failed %6, skipped %7.")
        .arg(processedEntries)
        .arg(snapshot.TotalEntries)
        .arg(fileNoun)
        .arg(percent)
        .arg(snapshot.ImportedEntries)
        .arg(snapshot.FailedEntries)
        .arg(snapshot.SkippedEntries);
}

QString formatTerminalSummary(const std::optional<Librova::ApplicationJobs::SImportJobResult>& terminalResult)
{
    if (!terminalResult.has_value() || !terminalResult->ImportResult.has_value())
    {
        return QStringLiteral("No import summary available.");
    }

    const auto& summary = terminalResult->ImportResult->Summary;
    if (summary.TotalEntries == 0)
    {
        return QStringLiteral("No supported files were found in the selected import sources.");
    }

    const QString fileNoun = summary.TotalEntries == 1 ? QStringLiteral("file") : QStringLiteral("files");
    return QStringLiteral("Processed %1 %2. Imported %3, failed %4, skipped %5.")
        .arg(summary.TotalEntries)
        .arg(fileNoun)
        .arg(summary.ImportedEntries)
        .arg(summary.FailedEntries)
        .arg(summary.SkippedEntries);
}

QString formatWarningsText(
    const Librova::ApplicationJobs::SImportJobSnapshot& snapshot,
    const std::optional<Librova::ApplicationJobs::SImportJobResult>& terminalResult)
{
    const auto* warnings = &snapshot.Warnings;
    if (terminalResult.has_value()
        && terminalResult->ImportResult.has_value()
        && !terminalResult->ImportResult->Summary.Warnings.empty())
    {
        warnings = &terminalResult->ImportResult->Summary.Warnings;
    }

    if (warnings->empty())
    {
        return QStringLiteral("No warnings.");
    }

    QStringList lines;
    lines.reserve(static_cast<qsizetype>(warnings->size()));
    for (const auto& warning : *warnings)
    {
        lines.append(qtFromUtf8(warning));
    }

    return lines.join(QLatin1Char('\n'));
}

QString domainErrorCodeToString(Librova::Domain::EDomainErrorCode code)
{
    using E = Librova::Domain::EDomainErrorCode;

    switch (code)
    {
        case E::Validation:           return QStringLiteral("Validation");
        case E::UnsupportedFormat:    return QStringLiteral("UnsupportedFormat");
        case E::DuplicateRejected:    return QStringLiteral("DuplicateRejected");
        case E::ParserFailure:        return QStringLiteral("ParserFailure");
        case E::ConverterUnavailable: return QStringLiteral("ConverterUnavailable");
        case E::ConverterFailed:      return QStringLiteral("ConverterFailed");
        case E::StorageFailure:       return QStringLiteral("StorageFailure");
        case E::DatabaseFailure:      return QStringLiteral("DatabaseFailure");
        case E::Cancellation:         return QStringLiteral("Cancellation");
        case E::IntegrityIssue:       return QStringLiteral("IntegrityIssue");
        case E::NotFound:             return QStringLiteral("NotFound");
    }

    return QStringLiteral("Unknown");
}

QString formatErrorText(const std::optional<Librova::ApplicationJobs::SImportJobResult>& terminalResult)
{
    if (!terminalResult.has_value() || !terminalResult->Error.has_value())
    {
        return QStringLiteral("No error.");
    }

    return QStringLiteral("%1: %2")
        .arg(domainErrorCodeToString(terminalResult->Error->Code))
        .arg(qtFromUtf8(terminalResult->Error->Message));
}

} // namespace

ImportJobListModel::ImportJobListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void ImportJobListModel::addJob(Librova::ApplicationJobs::TImportJobId jobId)
{
    SJobEntry entry;
    entry.Snapshot.JobId = jobId;
    entry.Snapshot.Status = Librova::ApplicationJobs::EImportJobStatus::Pending;

    const int row = static_cast<int>(m_items.size());
    beginInsertRows({}, row, row);
    m_items.push_back(std::move(entry));
    endInsertRows();
    emit countChanged();
}

void ImportJobListModel::updateJob(const Librova::ApplicationJobs::SImportJobSnapshot& snapshot)
{
    const int row = indexOfJob(snapshot.JobId);
    if (row < 0)
        return;
    m_items[static_cast<std::size_t>(row)].Snapshot = snapshot;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void ImportJobListModel::setTerminalResult(const Librova::ApplicationJobs::SImportJobResult& result)
{
    const int row = indexOfJob(result.Snapshot.JobId);
    if (row < 0)
        return;

    auto& entry = m_items[static_cast<std::size_t>(row)];
    entry.Snapshot = result.Snapshot;
    entry.TerminalResult = result;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void ImportJobListModel::removeJob(Librova::ApplicationJobs::TImportJobId jobId)
{
    const int row = indexOfJob(jobId);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_items.erase(m_items.begin() + row);
    endRemoveRows();
    emit countChanged();
}

int ImportJobListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_items.size());
}

QVariant ImportJobListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        return {};

    const auto& entry = m_items[static_cast<std::size_t>(index.row())];
    const auto& snap = entry.Snapshot;

    switch (role)
    {
        case JobIdRole:           return static_cast<qulonglong>(snap.JobId);
        case StatusStrRole:       return statusToString(snap.Status);
        case PercentRole:         return snap.Percent;
        case MessageRole:         return qtFromUtf8(snap.Message);
        case TotalEntriesRole:    return static_cast<int>(snap.TotalEntries);
        case ProcessedEntriesRole:return static_cast<int>(snap.ProcessedEntries);
        case ImportedEntriesRole: return static_cast<int>(snap.ImportedEntries);
        case FailedEntriesRole:   return static_cast<int>(snap.FailedEntries);
        case SkippedEntriesRole:  return static_cast<int>(snap.SkippedEntries);
        case IsTerminalRole:      return snap.IsTerminal();
        case ProgressSummaryTextRole: return formatProgressSummary(snap);
        case ResultSummaryTextRole: return formatTerminalSummary(entry.TerminalResult);
        case WarningsTextRole:    return formatWarningsText(snap, entry.TerminalResult);
        case ErrorTextRole:       return formatErrorText(entry.TerminalResult);
        default:                  return {};
    }
}

QHash<int, QByteArray> ImportJobListModel::roleNames() const
{
    return {
        {JobIdRole,            "jobId"},
        {StatusStrRole,        "statusStr"},
        {PercentRole,          "percent"},
        {MessageRole,          "message"},
        {TotalEntriesRole,     "totalEntries"},
        {ProcessedEntriesRole, "processedEntries"},
        {ImportedEntriesRole,  "importedEntries"},
        {FailedEntriesRole,    "failedEntries"},
        {SkippedEntriesRole,   "skippedEntries"},
        {IsTerminalRole,       "isTerminal"},
        {ProgressSummaryTextRole, "progressSummaryText"},
        {ResultSummaryTextRole, "resultSummaryText"},
        {WarningsTextRole,     "warningsText"},
        {ErrorTextRole,        "errorText"},
    };
}

QString ImportJobListModel::statusToString(Librova::ApplicationJobs::EImportJobStatus status)
{
    using E = Librova::ApplicationJobs::EImportJobStatus;
    switch (status)
    {
        case E::Pending:     return QStringLiteral("pending");
        case E::Running:     return QStringLiteral("running");
        case E::Completed:   return QStringLiteral("completed");
        case E::Failed:      return QStringLiteral("failed");
        case E::Cancelled:   return QStringLiteral("cancelled");
        case E::Cancelling:  return QStringLiteral("cancelling");
        case E::RollingBack: return QStringLiteral("rolling_back");
        case E::Compacting:  return QStringLiteral("compacting");
        default:             return QStringLiteral("unknown");
    }
}

int ImportJobListModel::indexOfJob(Librova::ApplicationJobs::TImportJobId jobId) const
{
    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].Snapshot.JobId == jobId)
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace LibrovaQt
