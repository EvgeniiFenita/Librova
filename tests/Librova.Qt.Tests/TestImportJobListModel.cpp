#include <QtTest>

#include "App/LibraryImportFacade.hpp"
#include "Domain/DomainError.hpp"
#include "Models/ImportJobListModel.hpp"

using namespace LibrovaQt;
using namespace Librova::ApplicationJobs;

class TestImportJobListModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void update_job_exposes_progress_summary()
    {
        ImportJobListModel model;
        model.addJob(7);

        SImportJobSnapshot snapshot;
        snapshot.JobId = 7;
        snapshot.Status = EImportJobStatus::Running;
        snapshot.TotalEntries = 10;
        snapshot.ProcessedEntries = 4;
        snapshot.ImportedEntries = 3;
        snapshot.FailedEntries = 1;
        snapshot.SkippedEntries = 0;

        model.updateJob(snapshot);

        const QModelIndex index = model.index(0);
        QCOMPARE(
            model.data(index, ImportJobListModel::ProgressSummaryTextRole).toString(),
            QStringLiteral("Processed 4 of 10 files (0%). Imported 3, failed 1, skipped 0."));
    }

    void update_job_exposes_warning_lines()
    {
        ImportJobListModel model;
        model.addJob(9);

        SImportJobSnapshot snapshot;
        snapshot.JobId = 9;
        snapshot.Status = EImportJobStatus::Failed;
        snapshot.Warnings = {"Bad ZIP entry", "Skipped duplicate"};

        model.updateJob(snapshot);

        const QModelIndex index = model.index(0);
        QCOMPARE(
            model.data(index, ImportJobListModel::WarningsTextRole).toString(),
            QStringLiteral("Bad ZIP entry\nSkipped duplicate"));
    }

    void set_terminal_result_exposes_summary_and_error()
    {
        ImportJobListModel model;
        model.addJob(11);

        SImportJobResult result;
        result.Snapshot.JobId = 11;
        result.Snapshot.Status = EImportJobStatus::Failed;
        result.ImportResult = Librova::Application::SImportResult{};
        result.ImportResult->Summary.TotalEntries = 3;
        result.ImportResult->Summary.ImportedEntries = 1;
        result.ImportResult->Summary.FailedEntries = 2;
        result.ImportResult->Summary.SkippedEntries = 0;
        result.ImportResult->Summary.Warnings = {"Converter fallback was not available"};
        result.Error = Librova::Domain::SDomainError{
            .Code = Librova::Domain::EDomainErrorCode::ConverterFailed,
            .Message = "fb2cng exited with code 1"};

        model.setTerminalResult(result);

        const QModelIndex index = model.index(0);
        QCOMPARE(
            model.data(index, ImportJobListModel::ResultSummaryTextRole).toString(),
            QStringLiteral("Processed 3 files. Imported 1, failed 2, skipped 0."));
        QCOMPARE(
            model.data(index, ImportJobListModel::WarningsTextRole).toString(),
            QStringLiteral("Converter fallback was not available"));
        QCOMPARE(
            model.data(index, ImportJobListModel::ErrorTextRole).toString(),
            QStringLiteral("ConverterFailed: fb2cng exited with code 1"));
    }
};

QTEST_MAIN(TestImportJobListModel)
#include "TestImportJobListModel.moc"
