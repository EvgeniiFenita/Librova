#pragma once

#include <vector>

#include <QAbstractListModel>

#include "App/ImportJobService.hpp"

namespace LibrovaQt {

// Model tracking all active and recently completed import jobs.
class ImportJobListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role
    {
        JobIdRole = Qt::UserRole + 1,
        StatusStrRole,
        PercentRole,
        MessageRole,
        TotalEntriesRole,
        ProcessedEntriesRole,
        ImportedEntriesRole,
        FailedEntriesRole,
        SkippedEntriesRole,
        IsTerminalRole,
        ProgressSummaryTextRole,
        ResultSummaryTextRole,
        WarningsTextRole,
        ErrorTextRole,
    };

    explicit ImportJobListModel(QObject* parent = nullptr);

    // Insert a new job slot (Pending).
    void addJob(Librova::ApplicationJobs::TImportJobId jobId);

    // Apply a snapshot update; no-op if jobId not present.
    void updateJob(const Librova::ApplicationJobs::SImportJobSnapshot& snapshot);

    void setTerminalResult(const Librova::ApplicationJobs::SImportJobResult& result);

    // Remove a completed/cancelled job from the list.
    void removeJob(Librova::ApplicationJobs::TImportJobId jobId);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged();

private:
    struct SJobEntry
    {
        Librova::ApplicationJobs::SImportJobSnapshot Snapshot;
        std::optional<Librova::ApplicationJobs::SImportJobResult> TerminalResult;
    };

    [[nodiscard]] static QString statusToString(Librova::ApplicationJobs::EImportJobStatus status);
    [[nodiscard]] int indexOfJob(Librova::ApplicationJobs::TImportJobId jobId) const;

    std::vector<SJobEntry> m_items;
};

} // namespace LibrovaQt
