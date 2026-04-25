#include "Adapters/QtImportAdapter.hpp"

#include <memory>
#include <string>

#include "Adapters/QtStringConversions.hpp"
#include "Foundation/UnicodeConversion.hpp"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcImport, "librova.import")

namespace LibrovaQt {

namespace {
constexpr int kPollIntervalMs = 500;

std::filesystem::path DefaultImportWorkspace()
{
    return std::filesystem::current_path() / "out" / "runtime" / "qt" / "ImportWorkspace";
}
} // namespace

QtImportAdapter::QtImportAdapter(
    IBackendDispatcher* backend,
    std::function<std::filesystem::path()> stagingRootResolver,
    QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    , m_importJobListModel(new ImportJobListModel(this))
    , m_pollTimer(new QTimer(this))
    , m_stagingRootResolver(std::move(stagingRootResolver))
{
    if (!m_stagingRootResolver)
    {
        m_stagingRootResolver = []() { return DefaultImportWorkspace(); };
    }

    m_pollTimer->setInterval(kPollIntervalMs);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &QtImportAdapter::pollSnapshot);
}

bool QtImportAdapter::hasActiveJob() const
{
    return m_polling;
}

void QtImportAdapter::validateSources(const QStringList& paths)
{
    if (!m_backend->isOpen())
    {
        emit sourcesValidated(false, QStringLiteral("Backend not open"));
        return;
    }

    std::vector<std::filesystem::path> fsPaths;
    fsPaths.reserve(static_cast<std::size_t>(paths.size()));
    for (const auto& p : paths)
        fsPaths.push_back(qtToPath(p));

    auto result = std::make_shared<Librova::Application::SImportSourceValidation>();
    m_backend->dispatch(
        [result, fsPaths](auto& app) { *result = app.ValidateImportSources(fsPaths); },
        [this, result]() {
            const bool ok = !result->HasBlockingError();
            const QString msg = result->BlockingMessage.has_value()
                                    ? qtFromUtf8(*result->BlockingMessage)
                                    : QString{};
            qCInfo(lcImport) << "ValidateSources: ok=" << ok;
            emit sourcesValidated(ok, msg);
        },
        [this](const QString& error) {
            qCWarning(lcImport) << "ValidateSources failed:" << error;
            emit sourcesValidated(false, error);
        });
}

void QtImportAdapter::startImport(
    const QStringList& paths,
    bool               forceEpub,
    bool               importCovers,
    bool               allowDuplicates)
{
    if (!m_backend->isOpen())
    {
        qCWarning(lcImport) << "startImport: backend not open";
        return;
    }
    if (m_polling)
    {
        qCWarning(lcImport) << "startImport: another import is already running";
        return;
    }

    Librova::Application::SImportRequest req;
    req.ForceEpubConversion  = forceEpub;
    req.ImportCovers         = importCovers;
    req.AllowProbableDuplicates = allowDuplicates;
    for (const auto& p : paths)
        req.SourcePaths.push_back(qtToPath(p));

    const std::filesystem::path stagingRoot = m_stagingRootResolver();
    auto jobId = std::make_shared<Librova::ApplicationJobs::TImportJobId>(0);
    m_backend->dispatch(
        [jobId, req, stagingRoot](auto& app) mutable {
            auto mutableReq = req;
            mutableReq.WorkingDirectory = stagingRoot;
            mutableReq.JobId = 0; // assigned by StartImport
            *jobId = app.StartImport(mutableReq);
        },
        [this, jobId]() {
            const quint64 jid = static_cast<quint64>(*jobId);
            m_importJobListModel->addJob(*jobId);
            qCInfo(lcImport) << "Import started: jobId=" << jid;
            startPolling(jid);
            emit importStarted(jid);
            emit hasActiveJobChanged();
        },
        [this](const QString& error) {
            qCWarning(lcImport) << "StartImport failed:" << error;
            emit importCompleted(0, false, error);
        });
}

void QtImportAdapter::cancelImport(quint64 jobId)
{
    if (!m_backend->isOpen())
        return;
    const auto jid = static_cast<Librova::ApplicationJobs::TImportJobId>(jobId);
    auto ok = std::make_shared<bool>(false);
    m_backend->dispatch(
        [ok, jid](auto& app) { *ok = app.CancelImportJob(jid); },
        [this, ok, jobId]() {
            qCInfo(lcImport) << "CancelImport: ok=" << *ok << "jobId=" << jobId;
        },
        [jobId](const QString& error) {
            qCWarning(lcImport) << "CancelImport failed:" << error << "jobId=" << jobId;
        });
}

void QtImportAdapter::removeJob(quint64 jobId)
{
    if (!m_backend->isOpen())
        return;
    const auto jid = static_cast<Librova::ApplicationJobs::TImportJobId>(jobId);
    m_backend->dispatch(
        [jid](auto& app) { app.RemoveImportJob(jid); },
        [this, jobId]() {
            m_importJobListModel->removeJob(
                static_cast<Librova::ApplicationJobs::TImportJobId>(jobId));
            qCInfo(lcImport) << "RemoveJob: jobId=" << jobId;
        },
        [jobId](const QString& error) {
            qCWarning(lcImport) << "RemoveJob failed:" << error << "jobId=" << jobId;
        });
}

// ── Private ───────────────────────────────────────────────────────────────────

void QtImportAdapter::startPolling(quint64 jobId)
{
    m_activeJobId = jobId;
    m_polling     = true;
    m_pollTimer->start();
}

void QtImportAdapter::stopPolling()
{
    m_pollTimer->stop();
    m_polling     = false;
    m_activeJobId = 0;
    emit hasActiveJobChanged();
}

void QtImportAdapter::pollSnapshot()
{
    if (!m_backend->isOpen())
    {
        stopPolling();
        return;
    }

    const auto jid = static_cast<Librova::ApplicationJobs::TImportJobId>(m_activeJobId);
    auto snap = std::make_shared<std::optional<Librova::ApplicationJobs::SImportJobSnapshot>>();

    m_backend->dispatch(
        [snap, jid](auto& app) { *snap = app.GetImportJobSnapshot(jid); },
        [this, snap]() {
            if (!snap->has_value())
            {
                // Job vanished unexpectedly.
                qCWarning(lcImport) << "pollSnapshot: no snapshot for jobId=" << m_activeJobId;
                stopPolling();
                return;
            }

            const auto& s = **snap;
            m_importJobListModel->updateJob(s);
            const quint64 jid = static_cast<quint64>(s.JobId);
            emit jobUpdated(jid);

            if (s.IsTerminal())
            {
                auto result = std::make_shared<std::optional<Librova::ApplicationJobs::SImportJobResult>>();
                m_backend->dispatch(
                    [result, jid](auto& app) { *result = app.GetImportJobResult(jid); },
                    [this, jid, s, result]() {
                        if (result->has_value())
                        {
                            m_importJobListModel->setTerminalResult(**result);
                        }
                        else
                        {
                            m_importJobListModel->updateJob(s);
                        }

                        const auto& terminalSnapshot = result->has_value() ? result->value().Snapshot : s;
                        const bool success =
                            terminalSnapshot.Status == Librova::ApplicationJobs::EImportJobStatus::Completed
                            && terminalSnapshot.FailedEntries == 0;
                        const QString msg = qtFromUtf8(terminalSnapshot.Message);
                        qCInfo(lcImport) << "Import terminal: jobId=" << jid
                                         << "status=" << static_cast<int>(terminalSnapshot.Status)
                                         << "imported=" << terminalSnapshot.ImportedEntries;
                        stopPolling();
                        emit importCompleted(jid, success, msg);
                    },
                    [this, jid](const QString& error) {
                        qCWarning(lcImport) << "GetImportJobResult failed:" << error
                                            << "jobId=" << jid;
                        stopPolling();
                        emit importCompleted(jid, false, error);
                    });
            }
        },
        [this](const QString& error) {
            const quint64 jid = m_activeJobId;
            qCWarning(lcImport) << "GetImportJobSnapshot failed:" << error
                                << "jobId=" << jid;
            stopPolling();
            emit importCompleted(jid, false, error);
        });
}

} // namespace LibrovaQt
