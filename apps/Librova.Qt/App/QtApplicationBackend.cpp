#include "App/QtApplicationBackend.hpp"

#include <QMetaObject>
#include <stdexcept>

#include "App/CLibraryApplication.hpp"
#include "Foundation/Logging.hpp"

namespace LibrovaQt {

// Lives on the worker thread; directly owns ILibraryApplication.
class QtApplicationBackend::CWorker : public QObject
{
    Q_OBJECT

public:
    explicit CWorker(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    [[nodiscard]] bool isOpen() const { return m_app != nullptr; }

    [[nodiscard]] Librova::Application::ILibraryApplication* app() const { return m_app.get(); }

    void open(
        const Librova::Application::SLibraryApplicationConfig& config,
        std::uint64_t generation,
        QtApplicationBackend* backend)
    {
        // Destroy any previously open instance first.
        m_app.reset();

        try
        {
            m_app = std::make_unique<Librova::Application::CLibraryApplication>(config);

            QMetaObject::invokeMethod(
                backend,
                [backend, generation]() {
                    // Ignore stale completions if the generation changed.
                    if (generation == backend->m_openGeneration.load())
                    {
                        backend->m_isOpen.store(true);
                        Q_EMIT backend->opened();
                    }
                },
                Qt::QueuedConnection);
        }
        catch (const std::exception& ex)
        {
            const QString error = QString::fromUtf8(ex.what());
            QMetaObject::invokeMethod(
                backend,
                [backend, generation, error]() {
                    if (generation == backend->m_openGeneration.load())
                    {
                        backend->m_isOpen.store(false);
                        Q_EMIT backend->openFailed(error);
                    }
                },
                Qt::QueuedConnection);
        }
    }

    void destroyApp() { m_app.reset(); }

private:
    std::unique_ptr<Librova::Application::ILibraryApplication> m_app;
};

// ---------------------------------------------------------------------------

QtApplicationBackend::QtApplicationBackend(QObject* parent)
    : QObject(parent)
{
    m_worker = new CWorker();
    m_worker->moveToThread(&m_workerThread);
    QObject::connect(
        &m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread.start();
}

QtApplicationBackend::~QtApplicationBackend()
{
    if (m_workerThread.isRunning())
    {
        close();
    }
}

void QtApplicationBackend::openAsync(
    const Librova::Application::SLibraryApplicationConfig& config)
{
    const std::uint64_t gen = ++m_openGeneration;
    m_isOpen.store(false);
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, config, gen, this]() { worker->open(config, gen, this); },
        Qt::QueuedConnection);
}

void QtApplicationBackend::dispatch(
    std::function<void(Librova::Application::ILibraryApplication&)> operation,
    std::function<void()> onComplete,
    std::function<void(const QString&)> onError)
{
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker,
         op = std::move(operation),
         cb = std::move(onComplete),
         err = std::move(onError),
         this]() {
            QString error;
            if (!worker->isOpen())
            {
                error = QStringLiteral("Backend not open");
            }
            else
            {
                try
                {
                    op(*worker->app());
                }
                catch (const std::exception& ex)
                {
                    Librova::Logging::Error("Backend dispatch: unhandled exception: {}", ex.what());
                    error = QString::fromUtf8(ex.what());
                }
                catch (...)
                {
                    Librova::Logging::Error("Backend dispatch: unknown exception");
                    error = QStringLiteral("Unknown backend error");
                }
            }
            if (error.isEmpty())
            {
                if (cb)
                {
                    QMetaObject::invokeMethod(this, std::move(cb), Qt::QueuedConnection);
                }
            }
            else if (err)
            {
                QMetaObject::invokeMethod(
                    this,
                    [err = std::move(err), error]() { err(error); },
                    Qt::QueuedConnection);
            }
        },
        Qt::QueuedConnection);
}

void QtApplicationBackend::close()
{
    ++m_openGeneration;
    m_isOpen.store(false);

    if (!m_workerThread.isRunning())
    {
        return;
    }

    // Destroy ILibraryApplication on the worker thread before stopping it.
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker]() { worker->destroyApp(); },
        Qt::BlockingQueuedConnection);

    m_workerThread.quit();
    m_workerThread.wait();
}

bool QtApplicationBackend::isOpen() const
{
    return m_isOpen.load();
}

} // namespace LibrovaQt

#include "QtApplicationBackend.moc"
