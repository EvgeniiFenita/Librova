#pragma once

#include <QObject>
#include <QString>

#include "App/IBackendDispatcher.hpp"

namespace LibrovaQt {

// Dispatches book-to-trash operations.
// Reports destination (RecycleBin | ManagedTrash) and orphan flag via signal.
class QtTrashAdapter : public QObject
{
    Q_OBJECT

public:
    explicit QtTrashAdapter(IBackendDispatcher* backend, QObject* parent = nullptr);

    Q_INVOKABLE void moveToTrash(qint64 bookId);

Q_SIGNALS:
    // destination: "recycle_bin" or "managed_trash"
    void trashed(qint64 bookId, const QString& destination, bool hadOrphanedFiles);
    void trashFailed(qint64 bookId, const QString& error);

private:
    IBackendDispatcher* m_backend;
};

} // namespace LibrovaQt
