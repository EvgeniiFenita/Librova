#pragma once

#include <QObject>
#include <QString>

#include "App/IBackendDispatcher.hpp"

namespace LibrovaQt {

// Dispatches single-book export operations.
class QtExportAdapter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)

public:
    explicit QtExportAdapter(IBackendDispatcher* backend, QObject* parent = nullptr);
    [[nodiscard]] bool isBusy() const { return m_isBusy; }

    // Opens a native Save-file dialog (via QFileDialog::getSaveFileName) and, if the user
    // confirms, dispatches the export. suggestedName is the pre-filled filename (no directory).
    // This avoids the QML FileDialog SaveFile-mode crash with non-ASCII filenames.
    Q_INVOKABLE void showExportDialog(
        qint64 bookId,
        const QString& suggestedName,
        const QString& forcedFormat = {});

    // Export book to destinationPath.
    // format: "epub" | "fb2" | "" (keep original).
    Q_INVOKABLE void exportBook(
        qint64         bookId,
        const QString& destinationPath,
        const QString& format = {});

Q_SIGNALS:
    void exported(qint64 bookId, const QString& destinationPath);
    void exportFailed(qint64 bookId, const QString& error);
    void isBusyChanged();

private:
    void setBusy(bool value);

    IBackendDispatcher* m_backend;
    bool m_isBusy = false;
};

} // namespace LibrovaQt
