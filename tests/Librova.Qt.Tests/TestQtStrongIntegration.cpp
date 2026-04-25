#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>

#include "Adapters/QtCatalogAdapter.hpp"
#include "Adapters/QtImportAdapter.hpp"
#include "App/QtApplicationBackend.hpp"
#include "Models/BookListModel.hpp"
#include "TestHelpers/TestWorkspace.hpp"

#include "App/CLibraryApplication.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;
using namespace Librova::Application;

namespace {

constexpr int kBackendTimeoutMs = 15000;
constexpr int kImportTimeoutMs  = 30000;

void WriteFb2(const std::filesystem::path& path, std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

// UTF-8 FB2 with Cyrillic title/author (same fixture as TestCLibraryApplication.cpp).
constexpr std::string_view kCyrillicFb2 =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<FictionBook xmlns:l=\"http://www.w3.org/1999/xlink\">\n"
    "  <description>\n"
    "    <title-info>\n"
    "      <book-title>\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82\xd0\xbe\xd0\xb2\xd0\xb0\xd1\x8f"
    " \xd0\xba\xd0\xbd\xd0\xb8\xd0\xb3\xd0\xb0</book-title>\n"  // "Тестовая книга"
    "      <author>\n"
    "        <first-name>\xd0\x98\xd0\xb2\xd0\xb0\xd0\xbd</first-name>\n"  // "Иван"
    "        <last-name>\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82\xd0\xbe\xd0\xb2</last-name>\n"  // "Тестов"
    "      </author>\n"
    "      <lang>ru</lang>\n"
    "    </title-info>\n"
    "  </description>\n"
    "  <body>\n"
    "    <section><p>\xd0\xa1\xd0\xbe\xd0\xb4\xd0\xb5\xd1\x80\xd0\xb6\xd0\xb0\xd0\xbd"
    "\xd0\xb8\xd0\xb5</p></section>\n"  // "Содержание"
    "  </body>\n"
    "</FictionBook>\n";

} // namespace

class TestQtStrongIntegration : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void backend_opens_and_reports_empty_catalog()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("qt-strong-open")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());

        QtApplicationBackend backend;
        QSignalSpy openedSpy(&backend, &QtApplicationBackend::opened);
        QSignalSpy failedSpy(&backend, &QtApplicationBackend::openFailed);

        backend.openAsync({
            .LibraryRoot          = root / "Library",
            .RuntimeWorkspaceRoot = root / "Runtime",
            .LogFilePath          = root / "test.log",
            .LibraryOpenMode      = ELibraryOpenMode::CreateNew,
        });
        QCOMPARE(backend.isOpen(), false);

        QVERIFY2(openedSpy.wait(kBackendTimeoutMs), "Backend did not open within timeout");
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(backend.isOpen(), true);

        QtCatalogAdapter catalog(&backend);
        QSignalSpy refreshedSpy(&catalog, &QtCatalogAdapter::refreshed);
        catalog.refresh();
        QVERIFY2(refreshedSpy.wait(kBackendTimeoutMs), "Catalog refresh timed out");

        QCOMPARE(catalog.bookListModel()->rowCount(), 0);
        QCOMPARE(catalog.totalCount(), static_cast<qint64>(0));

        backend.close();
        QCOMPARE(backend.isOpen(), false);
    }

    void backend_reports_closed_after_open_failure()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("qt-strong-open-fail")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());

        QtApplicationBackend backend;
        QSignalSpy openedSpy(&backend, &QtApplicationBackend::opened);
        QSignalSpy failedSpy(&backend, &QtApplicationBackend::openFailed);

        backend.openAsync({
            .LibraryRoot          = root / "MissingLibrary",
            .RuntimeWorkspaceRoot = root / "Runtime",
            .LogFilePath          = root / "test.log",
            .LibraryOpenMode      = ELibraryOpenMode::Open,
        });

        QVERIFY2(failedSpy.wait(kBackendTimeoutMs), "Backend did not report open failure within timeout");
        QCOMPARE(openedSpy.count(), 0);
        QCOMPARE(backend.isOpen(), false);

        backend.close();
    }

    void import_cyrillic_fb2_and_search_by_title()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("qt-strong-import")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());
        const auto fb2Path = root / "source" / "book.fb2";
        WriteFb2(fb2Path, kCyrillicFb2);

        // The import pipeline requires the working directory to exist before StartImport.
        const auto stagingRoot = root / "Runtime" / "ImportWorkspaces" / "GeneratedUiImportWorkspace";
        std::filesystem::create_directories(stagingRoot);

        QtApplicationBackend backend;
        QSignalSpy openedSpy(&backend, &QtApplicationBackend::opened);
        backend.openAsync({
            .LibraryRoot          = root / "Library",
            .RuntimeWorkspaceRoot = root / "Runtime",
            .LogFilePath          = root / "test.log",
            .LibraryOpenMode      = ELibraryOpenMode::CreateNew,
        });
        QVERIFY2(openedSpy.wait(kBackendTimeoutMs), "Backend did not open within timeout");

        // Build adapters after the backend is open.
        QtImportAdapter importAdapter(&backend, [stagingRoot]() { return stagingRoot; });
        QtCatalogAdapter catalog(&backend);

        QSignalSpy importStartedSpy(&importAdapter, &QtImportAdapter::importStarted);
        QSignalSpy importCompletedSpy(&importAdapter, &QtImportAdapter::importCompleted);

        importAdapter.startImport(
            { QString::fromStdWString(fb2Path.wstring()) },
            /*forceEpub=*/false,
            /*importCovers=*/false,
            /*allowDuplicates=*/false);

        QVERIFY2(importStartedSpy.wait(kBackendTimeoutMs), "importStarted signal not received");

        QVERIFY2(importCompletedSpy.wait(kImportTimeoutMs), "importCompleted signal not received");
        const bool success = importCompletedSpy.first().at(1).toBool();
        const QString msg  = importCompletedSpy.first().at(2).toString();
        QVERIFY2(success, qPrintable(QStringLiteral("Import failed: ") + msg));

        // Catalog must reflect the imported book.
        QSignalSpy refreshedSpy(&catalog, &QtCatalogAdapter::refreshed);
        catalog.refresh();
        QVERIFY2(refreshedSpy.wait(kBackendTimeoutMs), "Post-import catalog refresh timed out");

        const int count = catalog.bookListModel()->rowCount();
        QVERIFY2(count >= 1, "Expected at least one book after import");

        // Cyrillic full-text search must find the book.
        QSignalSpy searchSpy(&catalog, &QtCatalogAdapter::refreshed);
        catalog.setSearchText(QStringLiteral("\u0422\u0435\u0441\u0442\u043e\u0432\u0430\u044f")); // "Тестовая"
        catalog.refresh();
        QVERIFY2(searchSpy.wait(kBackendTimeoutMs), "Search refresh timed out");

        const int searchCount = catalog.bookListModel()->rowCount();
        QVERIFY2(searchCount >= 1, "Cyrillic search returned no results");

        backend.close();
    }
};

QTEST_GUILESS_MAIN(TestQtStrongIntegration)
#include "TestQtStrongIntegration.moc"
