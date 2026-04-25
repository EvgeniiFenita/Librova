#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#include <QObject>
#include <QThread>

#include "App/IBackendDispatcher.hpp"
#include "App/ILibraryApplication.hpp"

namespace LibrovaQt {

// Owns ILibraryApplication on a dedicated worker thread.
//
// All ILibraryApplication calls are serialized on the worker thread.
// Results are delivered back to the caller's thread via Qt queued connections.
//
// Generation IDs guard against stale openAsync completions arriving after
// a close/re-open cycle.
class QtApplicationBackend : public QObject, public IBackendDispatcher
{
    Q_OBJECT

public:
    explicit QtApplicationBackend(QObject* parent = nullptr);
    ~QtApplicationBackend() override;

    // Begin opening the library asynchronously on the worker thread.
    // Emits opened() or openFailed(error) on the GUI thread when done.
    void openAsync(const Librova::Application::SLibraryApplicationConfig& config);

    // Dispatch an operation to the worker thread.
    // onComplete (optional) is invoked on the GUI thread after the operation finishes.
    void dispatch(
        std::function<void(Librova::Application::ILibraryApplication&)> operation,
        std::function<void()> onComplete = {},
        std::function<void(const QString&)> onError = {}) override;

    // Blocking teardown. Destroys ILibraryApplication on worker thread, then stops it.
    // Must be called before destruction if openAsync was ever called.
    void close();

    [[nodiscard]] bool isOpen() const override;

Q_SIGNALS:
    void opened();
    void openFailed(const QString& error);

private:
    class CWorker;

    QThread m_workerThread;
    CWorker* m_worker = nullptr; // lifetime managed by m_workerThread
    std::atomic<std::uint64_t> m_openGeneration{0};
    std::atomic_bool m_isOpen{false};
};

} // namespace LibrovaQt
