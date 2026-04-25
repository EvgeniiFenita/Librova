#include "Controllers/FirstRunController.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace LibrovaQt {
namespace {

bool isDirectory(const QDir& root, const QString& relativePath)
{
    return QFileInfo(root.filePath(relativePath)).isDir();
}

bool isFile(const QDir& root, const QString& relativePath)
{
    return QFileInfo(root.filePath(relativePath)).isFile();
}

bool isNestedWithinCurrentLibraryRoot(const QString& selectedPath, const QString& currentLibraryRoot)
{
    if (selectedPath.trimmed().isEmpty() || currentLibraryRoot.trimmed().isEmpty())
        return false;

    const QFileInfo currentInfo(QDir::cleanPath(currentLibraryRoot));
    if (!currentInfo.exists() || !currentInfo.isDir())
        return false;

    const QString selectedAbsolute = QDir::cleanPath(QFileInfo(selectedPath).absoluteFilePath());
    const QString currentAbsolute = QDir::cleanPath(currentInfo.absoluteFilePath());
    if (selectedAbsolute.compare(currentAbsolute, Qt::CaseInsensitive) == 0)
        return false;

    const QString relative = QDir(currentAbsolute).relativeFilePath(selectedAbsolute);
    return !relative.startsWith(QLatin1String(".."))
        && !QDir::isAbsolutePath(relative);
}

} // namespace

FirstRunController::FirstRunController(QObject* parent)
    : QObject(parent)
{
}

QString FirstRunController::validateNewPath(const QString& path) const
{
    const QString clean = QDir::cleanPath(path.trimmed());
    if (clean.isEmpty())
        return QStringLiteral("Please enter a folder path.");

    if (!QFileInfo(clean).isAbsolute())
        return QStringLiteral("Please enter an absolute path.");

    const QFileInfo info(clean);
    if (info.exists()) {
        if (!info.isDir())
            return QStringLiteral("That path points to a file, not a folder.");
        if (!QDir(clean).isEmpty())
            return QStringLiteral(
                "That folder is not empty. Choose an empty folder or a new location.");
    } else {
        const QDir parent = info.dir();
        if (!parent.exists())
            return QStringLiteral("The parent folder does not exist.");
    }

    return {};
}

QString FirstRunController::validateNewPathForCurrentLibrary(
    const QString& path,
    const QString& currentLibraryRoot) const
{
    const QString validation = validateNewPath(path);
    if (!validation.isEmpty())
        return validation;

    if (isNestedWithinCurrentLibraryRoot(path, currentLibraryRoot))
        return QStringLiteral(
            "Choose a folder outside the current library. Librova cannot create a new library inside an existing managed library.");

    return {};
}

QString FirstRunController::validateExistingPath(const QString& path) const
{
    const QString clean = QDir::cleanPath(path.trimmed());
    if (clean.isEmpty())
        return QStringLiteral("Please enter a folder path.");

    if (!QFileInfo(clean).isAbsolute())
        return QStringLiteral("Please enter an absolute path.");

    if (!QDir(clean).exists())
        return QStringLiteral("That folder does not exist.");

    const QDir root(clean);
    if (!isDirectory(root, QStringLiteral("Database")))
        return QStringLiteral("That folder is not a complete Librova library. The Database folder is missing.");
    if (!isDirectory(root, QStringLiteral("Objects")))
        return QStringLiteral("That folder is not a complete Librova library. The Objects folder is missing.");
    if (!isDirectory(root, QStringLiteral("Logs")))
        return QStringLiteral("That folder is not a complete Librova library. The Logs folder is missing.");
    if (!isDirectory(root, QStringLiteral("Trash")))
        return QStringLiteral("That folder is not a complete Librova library. The Trash folder is missing.");
    if (!isFile(root, QStringLiteral("Database/librova.db")))
        return QStringLiteral("That folder is not a complete Librova library. The managed database is missing.");

    return {};
}

QString FirstRunController::validateExistingPathForCurrentLibrary(
    const QString& path,
    const QString& currentLibraryRoot) const
{
    const QString validation = validateExistingPath(path);
    if (!validation.isEmpty())
        return validation;

    if (isNestedWithinCurrentLibraryRoot(path, currentLibraryRoot))
        return QStringLiteral(
            "Choose a folder outside the current library. Librova cannot open a nested subfolder as a separate library.");

    return {};
}

QString FirstRunController::toLocalPath(const QUrl& url)
{
    return url.toLocalFile();
}

QUrl FirstRunController::fromLocalPath(const QString& path)
{
    return QUrl::fromLocalFile(path);
}

QUrl FirstRunController::suggestExportPath(const QString& filename)
{
    // Use Documents as the default folder so FileDialog can resolve the parent directory.
    // QUrl::fromLocalFile() percent-encodes Cyrillic and other non-ASCII chars correctly.
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QUrl::fromLocalFile(docs + "/" + filename);
}

} // namespace LibrovaQt
