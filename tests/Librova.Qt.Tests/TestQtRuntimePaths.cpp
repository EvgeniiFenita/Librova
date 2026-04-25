#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "App/QtRuntimePaths.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using LibrovaQt::QtRuntimePaths;

class TestQtRuntimePaths : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noMarkerFile_notPortableMode()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());
        QCOMPARE(QtRuntimePaths::IsPortableMode(tmp.path()), false);
    }

    void withMarkerFile_isPortableMode()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());
        // Create the portable marker file.
        QFile marker(QDir(tmp.path()).filePath(
            QString::fromUtf8(QtRuntimePaths::PortableMarkerFileName)));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();

        QCOMPARE(QtRuntimePaths::IsPortableMode(tmp.path()), true);
    }

    void defaultPaths_nonPortable_doNotContainPortableDataDir()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());
        // No marker — non-portable mode.
        const QString prefs = QtRuntimePaths::DefaultPreferencesFilePath(tmp.path());
        QVERIFY(!prefs.contains(
            QString::fromUtf8(QtRuntimePaths::PortableDataDirName)));
        QVERIFY(prefs.endsWith(QStringLiteral("preferences.json")));
    }

    void defaultPaths_portable_containPortableDataDir()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());
        QFile marker(QDir(tmp.path()).filePath(
            QString::fromUtf8(QtRuntimePaths::PortableMarkerFileName)));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();

        const QString prefs = QtRuntimePaths::DefaultPreferencesFilePath(tmp.path());
        QVERIFY(prefs.contains(
            QString::fromUtf8(QtRuntimePaths::PortableDataDirName)));
    }

    void defaultLogFilePath_usesUiLogName()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());

        const QString logPath = QtRuntimePaths::DefaultLogFilePath(tmp.path());

        QVERIFY(logPath.endsWith(QStringLiteral("Logs/ui.log")));
    }

    void runtimeWorkspaceRoot_differentLibraries_differentPaths()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());

        const auto root1 = QtRuntimePaths::RuntimeWorkspaceRoot(
            std::filesystem::path("C:/Library1"), tmp.path());
        const auto root2 = QtRuntimePaths::RuntimeWorkspaceRoot(
            std::filesystem::path("C:/Library2"), tmp.path());

        QVERIFY(root1 != root2);
    }

    void runtimeWorkspaceRoot_sameLibrary_samePath()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());

        const auto root1 = QtRuntimePaths::RuntimeWorkspaceRoot(
            std::filesystem::path("C:/Library"), tmp.path());
        const auto root2 = QtRuntimePaths::RuntimeWorkspaceRoot(
            std::filesystem::path("C:/Library"), tmp.path());

        QVERIFY(root1 == root2);
    }

    void runtimeWorkspaceRoot_sameLibraryDifferentCase_samePath()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());

        const auto root1 = QtRuntimePaths::RuntimeWorkspaceRoot(
            std::filesystem::path("C:/Library"), tmp.path());
        const auto root2 = QtRuntimePaths::RuntimeWorkspaceRoot(
            std::filesystem::path("c:/library"), tmp.path());

        QVERIFY(root1 == root2);
    }

    void buildPortableLibraryRootPreference_returnsRelativePathInsidePortableBundle()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());
        QFile marker(QDir(tmp.path()).filePath(
            QString::fromUtf8(QtRuntimePaths::PortableMarkerFileName)));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();

        const QString libraryRoot = QDir(tmp.path()).filePath(QStringLiteral("PortableData/Library"));
        QDir().mkpath(libraryRoot);

        QCOMPARE(
            QtRuntimePaths::BuildPortableLibraryRootPreference(libraryRoot, tmp.path()),
            QStringLiteral("PortableData/Library"));
    }

    void resolvePreferredLibraryRoot_usesPortableRelativeLibraryWhenAbsolutePathMoved()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("runtime")));
        QVERIFY(tmp.isValid());
        QFile marker(QDir(tmp.path()).filePath(
            QString::fromUtf8(QtRuntimePaths::PortableMarkerFileName)));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();

        const QString libraryRoot = QDir(tmp.path()).filePath(QStringLiteral("PortableData/Library"));
        QDir().mkpath(libraryRoot);

        QCOMPARE(
            QtRuntimePaths::ResolvePreferredLibraryRoot(
                QStringLiteral("D:/OldLocation/Library"),
                QStringLiteral("PortableData/Library"),
                tmp.path()),
            QDir(libraryRoot).absolutePath());
    }
};

QTEST_APPLESS_MAIN(TestQtRuntimePaths)
#include "TestQtRuntimePaths.moc"
