#pragma once

#include <exception>

#include "App/IBackendDispatcher.hpp"
#include "App/ILibraryApplication.hpp"

namespace LibrovaQt::Tests {

// Synchronous IBackendDispatcher stub for unit tests.
// Executes the operation immediately on the calling thread and calls onComplete inline.
// This is safe only in tests; production code must not call WaitImportJob through a
// real serialized executor (see ILibraryApplication comment).
class SyncTestDispatcher : public IBackendDispatcher
{
public:
    explicit SyncTestDispatcher(Librova::Application::ILibraryApplication* app)
        : m_app(app)
    {
    }

    void dispatch(
        std::function<void(Librova::Application::ILibraryApplication&)> operation,
        std::function<void()> onComplete = {},
        std::function<void(const QString&)> onError = {}) override
    {
        if (!m_app)
        {
            if (onError)
                onError(QStringLiteral("Backend not open"));
            return;
        }

        try
        {
            if (operation)
                operation(*m_app);
        }
        catch (const std::exception& ex)
        {
            if (onError)
                onError(QString::fromUtf8(ex.what()));
            return;
        }

        if (onComplete)
            onComplete();
    }

    [[nodiscard]] bool isOpen() const override { return m_app != nullptr; }

private:
    Librova::Application::ILibraryApplication* m_app;
};

} // namespace LibrovaQt::Tests
