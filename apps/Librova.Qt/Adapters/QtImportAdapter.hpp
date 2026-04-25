#pragma once

#include <filesystem>
#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "App/IBackendDispatcher.hpp"
#include "Models/ImportJobListModel.hpp"

namespace LibrovaQt {

// Manages import job lifecycle: validate → start → poll → complete/fail.
//
// Progress is polled every 500 ms on the GUI thread via QTimer.
// The adapter supports one active job at a time; multiple sequential jobs
// are tracked in ImportJobListModel until explicitly removed.
class QtImportAdapter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(LibrovaQt::ImportJobListModel* importJobListModel READ importJobListModel CONSTANT)
    Q_PROPERTY(bool hasActiveJob READ hasActiveJob NOTIFY hasActiveJobChanged)

public:
    explicit QtImportAdapter(
        IBackendDispatcher* backend,
        std::function<std::filesystem::path()> stagingRootResolver = {},
        QObject* parent = nullptr);

    [[nodiscard]] ImportJobListModel* importJobListModel() const { return m_importJobListModel; }
    [[nodiscard]] bool hasActiveJob() const;

    Q_INVOKABLE void validateSources(const QStringList& paths);

    // Start import. The working directory is resolved by the adapter from the
    // runtime workspace path managed by the backend (injected at construction).
    Q_INVOKABLE void startImport(
        const QStringList& paths,
        bool forceEpub,
        bool importCovers,
        bool allowDuplicates);

    Q_INVOKABLE void cancelImport(quint64 jobId);
    Q_INVOKABLE void removeJob(quint64 jobId);

Q_SIGNALS:
    void sourcesValidated(bool ok, const QString& blockingMessage);
    void importStarted(quint64 jobId);
    void jobUpdated(quint64 jobId);
    void importCompleted(quint64 jobId, bool success, const QString& message);
    void hasActiveJobChanged();

private:
    void startPolling(quint64 jobId);
    void stopPolling();
    void pollSnapshot();

    IBackendDispatcher*  m_backend;
    ImportJobListModel*  m_importJobListModel;
    QTimer*              m_pollTimer;
    quint64              m_activeJobId = 0;
    bool                 m_polling     = false;

    // Staging area passed to StartImport as the working directory.
    std::function<std::filesystem::path()> m_stagingRootResolver;
};

} // namespace LibrovaQt
