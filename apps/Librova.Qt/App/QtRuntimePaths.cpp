#include "App/QtRuntimePaths.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDevice>
#include <QIODevice>
#include <QCryptographicHash>
#include <QStandardPaths>

#include "Foundation/UnicodeConversion.hpp"

namespace LibrovaQt {

bool QtRuntimePaths::IsPortableMode(const QString& exeDir)
{
    return QFileInfo::exists(
        QDir(exeDir).filePath(QString::fromUtf8(PortableMarkerFileName)));
}

QString QtRuntimePaths::DataRootDir(const QString& exeDir)
{
    if (IsPortableMode(exeDir))
    {
        return QDir(exeDir).filePath(QString::fromUtf8(PortableDataDirName));
    }
    return LocalAppDataDir();
}

QString QtRuntimePaths::LocalAppDataDir()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    // Qt appends org/app name; we want just Librova under AppData/Local.
    // AppLocalDataLocation already includes the app name on Windows.
    return base;
}

QString QtRuntimePaths::DefaultPreferencesFilePath(const QString& exeDir)
{
    return QDir(DataRootDir(exeDir)).filePath(QStringLiteral("preferences.json"));
}

QString QtRuntimePaths::DefaultShellStateFilePath(const QString& exeDir)
{
    return QDir(DataRootDir(exeDir)).filePath(QStringLiteral("shell-state.json"));
}

QString QtRuntimePaths::DefaultLogFilePath(const QString& exeDir)
{
    return QDir(DataRootDir(exeDir)).filePath(QStringLiteral("Logs/ui.log"));
}

std::filesystem::path QtRuntimePaths::UiLogFilePathForLibrary(
    const std::filesystem::path& libraryRoot)
{
    return libraryRoot / "Logs" / "ui.log";
}

std::filesystem::path QtRuntimePaths::UiRuntimeLogFilePathForLibrary(
    const std::filesystem::path& libraryRoot,
    const QString& exeDir)
{
    return RuntimeWorkspaceRoot(libraryRoot, exeDir) / "RuntimeLogs" / "ui.log";
}

std::filesystem::path QtRuntimePaths::RuntimeWorkspaceRoot(
    const std::filesystem::path& libraryRoot,
    const QString& exeDir)
{
    const std::filesystem::path runtimeBase = [&]() {
        if (IsPortableMode(exeDir))
        {
            const auto portableData = exeDir.toStdString() + "/" + PortableDataDirName;
            return Librova::Unicode::PathFromUtf8(portableData + "/Runtime");
        }
        const auto appData = LocalAppDataDir().toUtf8().toStdString();
        return Librova::Unicode::PathFromUtf8(appData + "/Runtime");
    }();

    const auto normalizedRoot = Librova::Unicode::PathToUtf8(libraryRoot.lexically_normal());
    const QByteArray rootKey = QString::fromUtf8(normalizedRoot.c_str()).toUpper().toUtf8();
    const QByteArray hash = QCryptographicHash::hash(rootKey, QCryptographicHash::Sha256);
    return runtimeBase / hash.left(8).toHex().toUpper().toStdString();
}

std::filesystem::path QtRuntimePaths::ConverterWorkingDirectory(
    const std::filesystem::path& libraryRoot,
    const QString& exeDir)
{
    return RuntimeWorkspaceRoot(libraryRoot, exeDir) / "ConverterWorkspace";
}

std::filesystem::path QtRuntimePaths::ManagedStorageStagingRoot(
    const std::filesystem::path& libraryRoot,
    const QString& exeDir)
{
    return RuntimeWorkspaceRoot(libraryRoot, exeDir) / "ManagedStorageStaging";
}

QString QtRuntimePaths::ResolvePreferredLibraryRoot(
    const QString& preferredLibraryRoot,
    const QString& portablePreferredLibraryRoot,
    const QString& exeDir)
{
    if (!IsPortableMode(exeDir))
    {
        return preferredLibraryRoot;
    }

    const QString normalizedExeDir = QDir::fromNativeSeparators(QDir(exeDir).absolutePath());

    if (!portablePreferredLibraryRoot.trimmed().isEmpty())
    {
        const QFileInfo portableInfo(
            QDir(normalizedExeDir).filePath(portablePreferredLibraryRoot));
        const QString portableCandidate = QDir::fromNativeSeparators(portableInfo.absoluteFilePath());
        if (QDir(portableCandidate).exists()
            && EnsureTrailingSeparator(portableCandidate).startsWith(
                EnsureTrailingSeparator(normalizedExeDir),
                Qt::CaseInsensitive))
        {
            return portableCandidate;
        }
    }

    if (preferredLibraryRoot.trimmed().isEmpty())
    {
        return {};
    }

    if (QDir::isAbsolutePath(preferredLibraryRoot))
    {
        return QDir::fromNativeSeparators(QDir(preferredLibraryRoot).absolutePath());
    }

    const QFileInfo preferredInfo(QDir(normalizedExeDir).filePath(preferredLibraryRoot));
    const QString preferredCandidate = QDir::fromNativeSeparators(preferredInfo.absoluteFilePath());
    if (EnsureTrailingSeparator(preferredCandidate).startsWith(
            EnsureTrailingSeparator(normalizedExeDir),
            Qt::CaseInsensitive))
    {
        return preferredCandidate;
    }

    return {};
}

QString QtRuntimePaths::BuildPortableLibraryRootPreference(
    const QString& libraryRoot,
    const QString& exeDir)
{
    if (libraryRoot.trimmed().isEmpty() || !QDir::isAbsolutePath(libraryRoot) || !IsPortableMode(exeDir))
    {
        return {};
    }

    const QString normalizedExeDir = QDir::fromNativeSeparators(QDir(exeDir).absolutePath());
    const QString normalizedLibraryRoot = QDir::fromNativeSeparators(QDir(libraryRoot).absolutePath());
    if (!EnsureTrailingSeparator(normalizedLibraryRoot).startsWith(
            EnsureTrailingSeparator(normalizedExeDir),
            Qt::CaseInsensitive))
    {
        return {};
    }

    const QString relativePath =
        QDir::fromNativeSeparators(QDir(normalizedExeDir).relativeFilePath(normalizedLibraryRoot));
    return QDir::isAbsolutePath(relativePath) ? QString{} : relativePath;
}

void QtRuntimePaths::SyncRuntimeLogFile(
    const std::filesystem::path& runtimeLogPath,
    const std::filesystem::path& retainedLogPath)
{
    if (runtimeLogPath.empty() || retainedLogPath.empty())
    {
        return;
    }

    const QString runtimeLog = QString::fromUtf8(runtimeLogPath.u8string().c_str());
    const QString retainedLog = QString::fromUtf8(retainedLogPath.u8string().c_str());
    if (runtimeLog.trimmed().isEmpty() || retainedLog.trimmed().isEmpty() || !QFileInfo::exists(runtimeLog))
    {
        return;
    }

    QDir().mkpath(QFileInfo(retainedLog).absolutePath());

    QFile source(runtimeLog);
    if (!source.open(QIODevice::ReadOnly))
    {
        return;
    }

    if (QFileInfo::exists(retainedLog))
    {
        QFile destination(retainedLog);
        if (!destination.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            return;
        }

        if (destination.size() > 0)
        {
            destination.write("\n", 1);
        }

        destination.write(source.readAll());
        destination.close();
    }
    else
    {
        source.close();
        if (!QFile::copy(runtimeLog, retainedLog))
        {
            return;
        }
    }

    QFile::remove(runtimeLog);
}

QString QtRuntimePaths::EnsureTrailingSeparator(const QString& path)
{
    const QString normalizedPath = QDir::fromNativeSeparators(path);
    if (normalizedPath.endsWith(QLatin1Char('/')))
    {
        return normalizedPath;
    }

    return normalizedPath + QLatin1Char('/');
}

} // namespace LibrovaQt
