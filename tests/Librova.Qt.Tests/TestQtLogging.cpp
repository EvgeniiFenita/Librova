#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "App/QtLogging.hpp"
#include "Foundation/Logging.hpp"
#include "Foundation/UnicodeConversion.hpp"
#include "TestHelpers/TestWorkspace.hpp"

class TestQtLogging : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initializeAndShutdown_doesNotThrow()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("logging")));
        QVERIFY(tmp.isValid());
        const auto logPath =
            Librova::Unicode::PathFromUtf8(
                QDir(tmp.path()).filePath(QStringLiteral("test.log")).toUtf8().toStdString());

        LibrovaQt::QtLogging::Initialize(logPath);
        QVERIFY(Librova::Logging::CLogging::IsInitialized());

        LibrovaQt::QtLogging::Shutdown();
        QVERIFY(!Librova::Logging::CLogging::IsInitialized());
    }

    void initializeTwice_secondShutdownIsIdempotent()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("logging")));
        QVERIFY(tmp.isValid());
        const auto logPath =
            Librova::Unicode::PathFromUtf8(
                QDir(tmp.path()).filePath(QStringLiteral("test.log")).toUtf8().toStdString());

        LibrovaQt::QtLogging::Initialize(logPath);
        LibrovaQt::QtLogging::Shutdown();
        // Shutdown a second time should not throw or crash.
        LibrovaQt::QtLogging::Shutdown();
    }

    void qtMessageHandler_isInstalledAfterInit()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("logging")));
        QVERIFY(tmp.isValid());
        const auto logPath =
            Librova::Unicode::PathFromUtf8(
                QDir(tmp.path()).filePath(QStringLiteral("handler.log")).toUtf8().toStdString());

        LibrovaQt::QtLogging::Initialize(logPath);
        // Posting a qWarning should not crash (message is routed to spdlog).
        qWarning("test Qt message handler");
        LibrovaQt::QtLogging::Shutdown();
    }
};

QTEST_GUILESS_MAIN(TestQtLogging)
#include "TestQtLogging.moc"
