#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "App/QtApplicationBackend.hpp"
#include "App/QtLaunchOptions.hpp"
#include "Controllers/PreferencesStore.hpp"
#include "Controllers/ShellController.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using LibrovaQt::EShellState;
using LibrovaQt::PreferencesStore;
using LibrovaQt::QtApplicationBackend;
using LibrovaQt::ShellController;
using LibrovaQt::SQtLaunchOptions;

// Helper: create a valid managed library sandbox
static QString MakeSandboxLibrary(QTemporaryDir& tmp, bool create = true)
{
    const QString libPath = tmp.filePath(QStringLiteral("library"));
    if (create)
    {
        QDir().mkpath(libPath);
    }
    return libPath;
}

class TestShellController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noLibraryConfigured_startsInFirstRun()
    {
        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        shell.start(opts);

        QCOMPARE(shell.currentState(), EShellState::FirstRun);
    }

    void firstRunFlag_startsInFirstRun()
    {
        PreferencesStore prefs;
        prefs.setLibraryRoot(QStringLiteral("C:/some/library"));
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        opts.firstRun = true;
        shell.start(opts);

        QCOMPARE(shell.currentState(), EShellState::FirstRun);
    }

    void recoveryFlag_startsInRecovery()
    {
        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        opts.startupErrorRecovery = true;
        shell.start(opts);

        QCOMPARE(shell.currentState(), EShellState::Recovery);
    }

    void missingLibraryRoot_startsInRecovery()
    {
        PreferencesStore prefs;
        prefs.setLibraryRoot(QStringLiteral("C:/definitely/does/not/exist/12345"));
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        shell.start(opts);

        QCOMPARE(shell.currentState(), EShellState::Recovery);
    }

    void existingLibraryRoot_startsOpening()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("shell")));
        QVERIFY(tmp.isValid());
        const QString libPath = MakeSandboxLibrary(tmp);

        PreferencesStore prefs;
        prefs.setLibraryRoot(libPath);
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        shell.start(opts);

        QCOMPARE(shell.currentState(), EShellState::Opening);
    }

    void cliLibraryRootOverride_takesPreference()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("shell")));
        QVERIFY(tmp.isValid());
        const QString libPath = MakeSandboxLibrary(tmp);

        PreferencesStore prefs; // no library root in preferences
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        opts.libraryRoot = libPath;
        shell.start(opts);

        QCOMPARE(shell.currentState(), EShellState::Opening);
    }

    void currentStateChanged_emittedOnTransition()
    {
        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        QSignalSpy spy(&shell, &ShellController::currentStateChanged);
        SQtLaunchOptions opts;
        shell.start(opts);

        QVERIFY(spy.count() >= 1);
    }

    void handleFirstRunComplete_doesNotPersistLibraryRootBeforeSuccessfulOpen()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("shell")));
        QVERIFY(tmp.isValid());
        const QString libPath = MakeSandboxLibrary(tmp);

        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        opts.firstRun = true;
        shell.start(opts);
        QCOMPARE(shell.currentState(), EShellState::FirstRun);

        shell.handleFirstRunComplete(libPath, /*createNew=*/true);

        QCOMPARE(prefs.libraryRoot(), QString{});
        QCOMPARE(shell.currentState(), EShellState::Opening);
    }

    void handleFirstRunComplete_createNewLibraryReachesReadyAndPersistsRoot()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("shell")));
        QVERIFY(tmp.isValid());
        const QString libPath = MakeSandboxLibrary(tmp);

        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        opts.firstRun = true;
        shell.start(opts);
        QCOMPARE(shell.currentState(), EShellState::FirstRun);

        shell.handleFirstRunComplete(libPath, /*createNew=*/true);

        QTRY_COMPARE(shell.currentState(), EShellState::Ready);
        QCOMPARE(prefs.libraryRoot(), libPath);
        QVERIFY(QFileInfo::exists(QDir(libPath).filePath(QStringLiteral("Database/librova.db"))));
    }

    void hasConverter_falseWhenNoConverterExePath()
    {
        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        QCOMPARE(shell.hasConverter(), false);
    }

    void hasConverter_trueWhenConverterExePathSet()
    {
        PreferencesStore prefs;
        prefs.setConverterExePath(QStringLiteral("C:/tools/fb2cng.exe"));
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        QCOMPARE(shell.hasConverter(), true);
    }

    void converterPreferenceChange_emitsHasConverterChanged()
    {
        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);
        QSignalSpy spy(&shell, &ShellController::hasConverterChanged);

        prefs.setConverterExePath(QStringLiteral("C:/tools/fb2cng.exe"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(shell.hasConverter(), true);
    }

    void firstRunComplete_persistsConverterExecutableOnly()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("shell-converter")));
        QVERIFY(tmp.isValid());
        const QString libPath = MakeSandboxLibrary(tmp);

        PreferencesStore prefs;
        QtApplicationBackend backend;
        ShellController shell(&prefs, &backend);

        SQtLaunchOptions opts;
        opts.firstRun = true;
        shell.start(opts);

        shell.handleFirstRunComplete(
            libPath,
            /*createNew=*/true,
            QStringLiteral("C:/tools/fb2cng.exe"));

        QCOMPARE(prefs.converterExePath(), QStringLiteral("C:/tools/fb2cng.exe"));
    }
};

QTEST_GUILESS_MAIN(TestShellController)
#include "TestShellController.moc"
