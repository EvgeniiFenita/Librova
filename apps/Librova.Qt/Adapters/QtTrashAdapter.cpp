#include "Adapters/QtTrashAdapter.hpp"

#include <memory>

#include "App/LibraryTrashFacade.hpp"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTrash, "librova.trash")

namespace LibrovaQt {

QtTrashAdapter::QtTrashAdapter(IBackendDispatcher* backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
{
}

void QtTrashAdapter::moveToTrash(qint64 bookId)
{
    if (!m_backend->isOpen())
    {
        emit trashFailed(bookId, QStringLiteral("Backend not open"));
        return;
    }

    const Librova::Domain::SBookId bid{static_cast<std::int64_t>(bookId)};
    auto result = std::make_shared<std::optional<Librova::Application::STrashedBookResult>>();
    const qint64 qbid = bookId;

    m_backend->dispatch(
        [result, bid](auto& app) { *result = app.MoveBookToTrash(bid); },
        [this, result, qbid]() {
            if (result->has_value())
            {
                const auto& r = **result;
                const QString dest =
                    r.Destination == Librova::Application::ETrashDestination::RecycleBin
                        ? QStringLiteral("recycle_bin")
                        : QStringLiteral("managed_trash");
                qCInfo(lcTrash) << "MoveToTrash: bookId=" << qbid
                                << "dest=" << dest
                                << "orphans=" << r.HasOrphanedFiles;
                emit trashed(qbid, dest, r.HasOrphanedFiles);
            }
            else
            {
                qCWarning(lcTrash) << "MoveToTrash failed: bookId=" << qbid;
                emit trashFailed(qbid, QStringLiteral("Trash operation failed"));
            }
        },
        [this, qbid](const QString& error) {
            qCWarning(lcTrash) << "MoveToTrash failed:" << error << "bookId=" << qbid;
            emit trashFailed(qbid, error);
        });
}

} // namespace LibrovaQt
