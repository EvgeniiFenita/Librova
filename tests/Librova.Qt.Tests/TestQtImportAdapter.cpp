#include <QtTest>

#include "Adapters/QtImportAdapter.hpp"
#include "App/LibraryImportFacade.hpp"
#include "Domain/DomainError.hpp"
#include "TestHelpers/MockLibraryApplication.hpp"
#include "TestHelpers/SyncTestDispatcher.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;
using namespace Librova::ApplicationJobs;

class TestQtImportAdapter : public QObject
{
    Q_OBJECT

private:
    MockLibraryApplication m_app;
    SyncTestDispatcher     m_dispatcher{&m_app};

private Q_SLOTS:
    void validate_sources_ok_emits_signal()
    {
        m_app.validateSourcesResult.BlockingMessage = std::nullopt;

        QtImportAdapter adapter(&m_dispatcher);
        QSignalSpy spy(&adapter, &QtImportAdapter::sourcesValidated);

        adapter.validateSources({QStringLiteral("C:/books/test.fb2")});

        QCOMPARE(spy.count(), 1);
        const bool ok = spy.at(0).at(0).toBool();
        QVERIFY(ok);
    }

    void validate_sources_blocked_emits_false()
    {
        m_app.validateSourcesResult.BlockingMessage = "Unsupported format";

        QtImportAdapter adapter(&m_dispatcher);
        QSignalSpy spy(&adapter, &QtImportAdapter::sourcesValidated);

        adapter.validateSources({QStringLiteral("C:/books/test.xyz")});

        QCOMPARE(spy.count(), 1);
        const bool ok = spy.at(0).at(0).toBool();
        QVERIFY(!ok);
        QVERIFY(!spy.at(0).at(1).toString().isEmpty());
    }

    void validate_sources_backend_exception_emits_false_with_error()
    {
        m_app.throwOnValidateImportSources = true;

        QtImportAdapter adapter(&m_dispatcher);
        QSignalSpy spy(&adapter, &QtImportAdapter::sourcesValidated);

        adapter.validateSources({QStringLiteral("C:/books/test.fb2")});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
        QVERIFY(spy.at(0).at(1).toString().contains(QStringLiteral("validation failed")));
    }

    void start_import_emits_started_signal()
    {
        m_app.startImportResult = 7;

        // Provide a terminal snapshot so polling stops immediately.
        SImportJobSnapshot snap;
        snap.JobId = 7;
        snap.Status = EImportJobStatus::Completed;
        snap.Message = "Done";
        m_app.jobSnapshot = snap;

        const QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("import")));
        QVERIFY(tmp.isValid());
        const auto stagingRoot = std::filesystem::path(tmp.path().toStdWString());
        QtImportAdapter adapter(&m_dispatcher, [stagingRoot]() { return stagingRoot; });
        QSignalSpy startedSpy(&adapter, &QtImportAdapter::importStarted);

        adapter.startImport({QStringLiteral("C:/books/book.fb2")}, false, true, false);

        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(startedSpy.at(0).at(0).toULongLong(), 7ULL);
        QVERIFY(m_app.lastStartImportRequest.has_value());
        QCOMPARE(m_app.lastStartImportRequest->WorkingDirectory, stagingRoot);
    }

    void start_import_sets_has_active_job()
    {
        m_app.startImportResult = 5;

        // Non-terminal snapshot to keep polling active.
        SImportJobSnapshot snap;
        snap.JobId = 5;
        snap.Status = EImportJobStatus::Running;
        m_app.jobSnapshot = snap;

        const QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("import")));
        QVERIFY(tmp.isValid());
        const auto stagingRoot = std::filesystem::path(tmp.path().toStdWString());
        QtImportAdapter adapter(&m_dispatcher, [stagingRoot]() { return stagingRoot; });
        adapter.startImport({QStringLiteral("C:/books/book.fb2")}, false, true, false);

        QVERIFY(adapter.hasActiveJob());
    }

    void terminal_poll_uses_import_result_details()
    {
        m_app.startImportResult = 12;

        SImportJobSnapshot snapshot;
        snapshot.JobId = 12;
        snapshot.Status = EImportJobStatus::Completed;
        snapshot.Message = "Snapshot message";
        snapshot.TotalEntries = 2;
        snapshot.ProcessedEntries = 2;
        snapshot.ImportedEntries = 2;
        m_app.jobSnapshot = snapshot;

        SImportJobResult result;
        result.Snapshot = snapshot;
        result.Snapshot.Message = "Terminal message";
        result.ImportResult = Librova::Application::SImportResult{};
        result.ImportResult->Summary.TotalEntries = 2;
        result.ImportResult->Summary.ImportedEntries = 2;
        result.ImportResult->Summary.Warnings = {"Duplicate metadata normalized"};
        m_app.jobResult = result;

        const QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("import")));
        QVERIFY(tmp.isValid());
        const auto stagingRoot = std::filesystem::path(tmp.path().toStdWString());
        QtImportAdapter adapter(&m_dispatcher, [stagingRoot]() { return stagingRoot; });
        QSignalSpy completedSpy(&adapter, &QtImportAdapter::importCompleted);

        adapter.startImport({QStringLiteral("C:/books/book.fb2")}, false, true, false);

        QTRY_COMPARE(completedSpy.count(), 1);
        const QModelIndex index = adapter.importJobListModel()->index(0);
        QCOMPARE(
            adapter.importJobListModel()->data(index, ImportJobListModel::ResultSummaryTextRole).toString(),
            QStringLiteral("Processed 2 files. Imported 2, failed 0, skipped 0."));
        QCOMPARE(
            adapter.importJobListModel()->data(index, ImportJobListModel::WarningsTextRole).toString(),
            QStringLiteral("Duplicate metadata normalized"));
        QCOMPARE(
            adapter.importJobListModel()->data(index, ImportJobListModel::MessageRole).toString(),
            QStringLiteral("Terminal message"));
    }

    void completed_import_with_failed_entries_is_not_reported_as_successful_workflow()
    {
        m_app.startImportResult = 13;

        SImportJobSnapshot snapshot;
        snapshot.JobId = 13;
        snapshot.Status = EImportJobStatus::Completed;
        snapshot.Message = "Completed with failures";
        snapshot.TotalEntries = 2;
        snapshot.ProcessedEntries = 2;
        snapshot.ImportedEntries = 1;
        snapshot.FailedEntries = 1;
        m_app.jobSnapshot = snapshot;

        SImportJobResult result;
        result.Snapshot = snapshot;
        result.ImportResult = Librova::Application::SImportResult{};
        result.ImportResult->Summary.TotalEntries = 2;
        result.ImportResult->Summary.ImportedEntries = 1;
        result.ImportResult->Summary.FailedEntries = 1;
        m_app.jobResult = result;

        const QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("import")));
        QVERIFY(tmp.isValid());
        const auto stagingRoot = std::filesystem::path(tmp.path().toStdWString());
        QtImportAdapter adapter(&m_dispatcher, [stagingRoot]() { return stagingRoot; });
        QSignalSpy completedSpy(&adapter, &QtImportAdapter::importCompleted);

        adapter.startImport({QStringLiteral("C:/books/book.fb2")}, false, true, false);

        QTRY_COMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.first().at(1).toBool(), false);
    }

    void closed_dispatcher_skips_start()
    {
        SyncTestDispatcher closed(nullptr);
        QtImportAdapter adapter(&closed);
        QSignalSpy spy(&adapter, &QtImportAdapter::importStarted);

        adapter.startImport({}, false, false, false);

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestQtImportAdapter)
#include "TestQtImportAdapter.moc"
