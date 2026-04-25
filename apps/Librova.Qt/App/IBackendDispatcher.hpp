#pragma once

#include <functional>

#include <QString>

#include "App/ILibraryApplication.hpp"

namespace LibrovaQt {

// Thin dispatch interface that decouples adapters from the concrete
// QtApplicationBackend, allowing synchronous stubs in tests.
class IBackendDispatcher
{
public:
    virtual ~IBackendDispatcher() = default;

    // Schedule an operation on the backend worker thread.
    // onComplete/onError (optional) are invoked on the GUI thread after the operation.
    virtual void dispatch(
        std::function<void(Librova::Application::ILibraryApplication&)> operation,
        std::function<void()> onComplete = {},
        std::function<void(const QString&)> onError = {}) = 0;

    [[nodiscard]] virtual bool isOpen() const = 0;
};

} // namespace LibrovaQt
