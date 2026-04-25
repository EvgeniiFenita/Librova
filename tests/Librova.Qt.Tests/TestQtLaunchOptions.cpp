#include <QTest>

#include "App/QtLaunchOptions.hpp"

using LibrovaQt::SQtLaunchOptions;

class TestQtLaunchOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noArgs_allDefaults()
    {
        const auto opts = SQtLaunchOptions::Parse({QStringLiteral("app")});
        QCOMPARE(opts.firstRun, false);
        QCOMPARE(opts.startupErrorRecovery, false);
        QCOMPARE(opts.secondRun, false);
        QCOMPARE(opts.createLibraryRoot, false);
        QVERIFY(!opts.libraryRoot.has_value());
        QVERIFY(!opts.logFile.has_value());
        QVERIFY(!opts.preferencesFile.has_value());
        QVERIFY(!opts.shellStateFile.has_value());
    }

    void firstRunFlag_setsFirstRun()
    {
        const auto opts = SQtLaunchOptions::Parse({QStringLiteral("app"), QStringLiteral("--first-run")});
        QCOMPARE(opts.firstRun, true);
        QCOMPARE(opts.startupErrorRecovery, false);
        QCOMPARE(opts.secondRun, false);
    }

    void recoveryFlag_setsRecovery()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"), QStringLiteral("--startup-error-recovery")});
        QCOMPARE(opts.startupErrorRecovery, true);
        QCOMPARE(opts.firstRun, false);
    }

    void secondRunFlag_setsSecondRun()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"), QStringLiteral("--second-run")});
        QCOMPARE(opts.secondRun, true);
    }

    void libraryRootOption_isCapture()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"), QStringLiteral("--library-root"), QStringLiteral("C:/Books")});
        QVERIFY(opts.libraryRoot.has_value());
        QCOMPARE(*opts.libraryRoot, QStringLiteral("C:/Books"));
    }

    void createLibraryRootFlag_setsCreateMode()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"),
             QStringLiteral("--library-root"), QStringLiteral("C:/Books"),
             QStringLiteral("--create-library-root")});
        QVERIFY(opts.libraryRoot.has_value());
        QCOMPARE(*opts.libraryRoot, QStringLiteral("C:/Books"));
        QCOMPARE(opts.createLibraryRoot, true);
    }

    void logFileOption_isCaptured()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"), QStringLiteral("--log-file"), QStringLiteral("C:/log.txt")});
        QVERIFY(opts.logFile.has_value());
        QCOMPARE(*opts.logFile, QStringLiteral("C:/log.txt"));
    }

    void prefsFileOption_isCaptured()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"),
             QStringLiteral("--preferences-file"),
             QStringLiteral("C:/prefs.json")});
        QVERIFY(opts.preferencesFile.has_value());
        QCOMPARE(*opts.preferencesFile, QStringLiteral("C:/prefs.json"));
    }

    void shellStateFileOption_isCaptured()
    {
        const auto opts = SQtLaunchOptions::Parse(
            {QStringLiteral("app"),
             QStringLiteral("--shell-state-file"),
             QStringLiteral("C:/state.json")});
        QVERIFY(opts.shellStateFile.has_value());
        QCOMPARE(*opts.shellStateFile, QStringLiteral("C:/state.json"));
    }

    void allFlags_parsedTogether()
    {
        const auto opts = SQtLaunchOptions::Parse({
            QStringLiteral("app"),
            QStringLiteral("--first-run"),
            QStringLiteral("--log-file"), QStringLiteral("my.log"),
            QStringLiteral("--preferences-file"), QStringLiteral("prefs.json"),
            QStringLiteral("--shell-state-file"), QStringLiteral("state.json"),
        });
        QCOMPARE(opts.firstRun, true);
        QVERIFY(opts.logFile.has_value());
        QVERIFY(opts.preferencesFile.has_value());
        QVERIFY(opts.shellStateFile.has_value());
    }
};

QTEST_APPLESS_MAIN(TestQtLaunchOptions)
#include "TestQtLaunchOptions.moc"
