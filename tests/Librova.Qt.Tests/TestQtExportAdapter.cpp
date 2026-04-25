#include <QtTest>
#include <filesystem>

#include "Adapters/QtExportAdapter.hpp"
#include "TestHelpers/MockLibraryApplication.hpp"
#include "TestHelpers/SyncTestDispatcher.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;

class TestQtExportAdapter : public QObject
{
    Q_OBJECT

private:
    MockLibraryApplication m_app;
    SyncTestDispatcher     m_dispatcher{&m_app};

private Q_SLOTS:
    void export_success_emits_exported()
    {
        m_app.exportResult = std::filesystem::path(L"C:\\exports\\book.epub");

        QtExportAdapter adapter(&m_dispatcher);
        QSignalSpy successSpy(&adapter, &QtExportAdapter::exported);
        QSignalSpy failSpy(&adapter, &QtExportAdapter::exportFailed);
        QSignalSpy busySpy(&adapter, &QtExportAdapter::isBusyChanged);

        adapter.exportBook(1, QStringLiteral("C:/exports/book.epub"), QStringLiteral("epub"));

        QCOMPARE(failSpy.count(), 0);
        QCOMPARE(successSpy.count(), 1);
        QCOMPARE(busySpy.count(), 2);
        QCOMPARE(adapter.isBusy(), false);
        QCOMPARE(successSpy.at(0).at(0).toLongLong(), 1LL);
        QVERIFY(m_app.lastExportBookRequest.has_value());
        QVERIFY(m_app.lastExportBookRequest->ExportFormat.has_value());
    }

    void export_withoutFormat_keepsOriginalFormat()
    {
        m_app.exportResult = std::filesystem::path(L"C:\\exports\\book.fb2");

        QtExportAdapter adapter(&m_dispatcher);
        QSignalSpy successSpy(&adapter, &QtExportAdapter::exported);

        adapter.exportBook(4, QStringLiteral("C:/exports/book.fb2"), {});

        QCOMPARE(successSpy.count(), 1);
        QVERIFY(m_app.lastExportBookRequest.has_value());
        QVERIFY(!m_app.lastExportBookRequest->ExportFormat.has_value());
    }

    void export_failure_emits_failed()
    {
        m_app.exportResult = std::nullopt;

        QtExportAdapter adapter(&m_dispatcher);
        QSignalSpy successSpy(&adapter, &QtExportAdapter::exported);
        QSignalSpy failSpy(&adapter, &QtExportAdapter::exportFailed);

        adapter.exportBook(2, QStringLiteral("C:/exports/book.epub"), {});

        QCOMPARE(successSpy.count(), 0);
        QCOMPARE(failSpy.count(), 1);
        QCOMPARE(failSpy.at(0).at(0).toLongLong(), 2LL);
    }

    void closed_dispatcher_emits_failed()
    {
        SyncTestDispatcher closed(nullptr);
        QtExportAdapter adapter(&closed);
        QSignalSpy failSpy(&adapter, &QtExportAdapter::exportFailed);

        adapter.exportBook(3, QStringLiteral("C:/foo"), {});

        QCOMPARE(failSpy.count(), 1);
        QCOMPARE(failSpy.at(0).at(0).toLongLong(), 3LL);
    }
};

QTEST_MAIN(TestQtExportAdapter)
#include "TestQtExportAdapter.moc"
