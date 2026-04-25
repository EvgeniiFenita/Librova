#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace LibrovaQt {

// Path validation helpers for first-run setup and startup recovery screens.
// Also provides URL-to-local-path conversion for FolderDialog / FileDialog results.
class FirstRunController : public QObject
{
    Q_OBJECT

public:
    explicit FirstRunController(QObject* parent = nullptr);

    // Returns an empty string if the path is valid for a new library root,
    // or a user-facing error message otherwise.
    // Accepts: empty/non-existing path whose immediate parent exists (dir will be created),
    //          or an existing empty directory.
    Q_INVOKABLE QString validateNewPath(const QString& path) const;

    // Same as validateNewPath(), additionally rejects a target inside the
    // currently open managed library root.
    Q_INVOKABLE QString validateNewPathForCurrentLibrary(
        const QString& path,
        const QString& currentLibraryRoot) const;

    // Returns an empty string if the path points to an existing managed Librova library,
    // or a user-facing error message otherwise.
    Q_INVOKABLE QString validateExistingPath(const QString& path) const;

    // Same as validateExistingPath(), additionally rejects a nested subfolder
    // of the currently open managed library root.
    Q_INVOKABLE QString validateExistingPathForCurrentLibrary(
        const QString& path,
        const QString& currentLibraryRoot) const;

    // Converts a file:// URL (from FolderDialog or FileDialog) to a local filesystem path.
    // Handles Windows paths and non-ASCII characters correctly via QUrl::toLocalFile().
    Q_INVOKABLE static QString toLocalPath(const QUrl& url);

    // Converts a local filesystem path to a file:// URL suitable for FileDialog.currentFile.
    // Uses QUrl::fromLocalFile() which correctly percent-encodes non-ASCII characters (Cyrillic etc.).
    Q_INVOKABLE static QUrl fromLocalPath(const QString& path);

    // Builds a suggested export save path: Documents/<sanitizedTitle>.epub as a file:// URL.
    // Uses QStandardPaths to get a real existing directory so FileDialog doesn't crash
    // when resolving the parent folder of currentFile.
    Q_INVOKABLE static QUrl suggestExportPath(const QString& filename);
};

} // namespace LibrovaQt
