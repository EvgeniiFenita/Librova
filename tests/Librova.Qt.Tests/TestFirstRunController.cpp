#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "Controllers/FirstRunController.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;

namespace {

void CreateManagedLibraryLayout(const QString& root)
{
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("Database"))));
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("Objects"))));
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("Logs"))));
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("Trash"))));

    QFile db(QDir(root).filePath(QStringLiteral("Database/librova.db")));
    QVERIFY(db.open(QIODevice::WriteOnly));
    db.close();
}

} // namespace

class TestFirstRunController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ── validateNewPath ──────────────────────────────────────────────────────

    void validateNewPath_empty_returns_error()
    {
        FirstRunController c;
        QVERIFY(!c.validateNewPath(QStringLiteral("")).isEmpty());
    }

    void validateNewPath_relative_path_returns_error()
    {
        FirstRunController c;
        QVERIFY(!c.validateNewPath(QStringLiteral("relative/path")).isEmpty());
    }

    void validateNewPath_existing_file_returns_error()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-file")));
        QVERIFY(tmp.isValid());

        const QString filePath = tmp.filePath(QStringLiteral("testfile.txt"));
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        FirstRunController c;
        QVERIFY(!c.validateNewPath(filePath).isEmpty());
    }

    void validateNewPath_non_empty_dir_returns_error()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-nonempty")));
        QVERIFY(tmp.isValid());

        QFile f(tmp.filePath(QStringLiteral("something.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        FirstRunController c;
        QVERIFY(!c.validateNewPath(tmp.path()).isEmpty());
    }

    void validateNewPath_parent_does_not_exist_returns_error()
    {
        FirstRunController c;
        const QString ghost = QStringLiteral("C:/nonexistent_librova_parent_xyz/sublibrary");
        QVERIFY(!c.validateNewPath(ghost).isEmpty());
    }

    void validateNewPath_new_path_with_existing_parent_returns_ok()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-newpath")));
        QVERIFY(tmp.isValid());

        // A non-existing subdirectory whose parent (tmp.path()) exists.
        const QString newPath = tmp.filePath(QStringLiteral("NewLibrary"));

        FirstRunController c;
        QVERIFY(c.validateNewPath(newPath).isEmpty());
    }

    void validateNewPath_existing_empty_dir_returns_ok()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-emptydir")));
        QVERIFY(tmp.isValid());

        FirstRunController c;
        QVERIFY(c.validateNewPath(tmp.path()).isEmpty());
    }

    // ── validateExistingPath ─────────────────────────────────────────────────

    void validateExistingPath_empty_returns_error()
    {
        FirstRunController c;
        QVERIFY(!c.validateExistingPath(QStringLiteral("")).isEmpty());
    }

    void validateExistingPath_relative_path_returns_error()
    {
        FirstRunController c;
        QVERIFY(!c.validateExistingPath(QStringLiteral("relative/path")).isEmpty());
    }

    void validateExistingPath_nonexistent_dir_returns_error()
    {
        FirstRunController c;
        const QString ghost = QStringLiteral("C:/nonexistent_librova_xyz_dir");
        QVERIFY(!c.validateExistingPath(ghost).isEmpty());
    }

    void validateExistingPath_existing_plain_dir_returns_error()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-existing")));
        QVERIFY(tmp.isValid());

        FirstRunController c;
        QVERIFY(!c.validateExistingPath(tmp.path()).isEmpty());
    }

    void validateExistingPath_missing_required_directory_returns_error()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-incomplete")));
        QVERIFY(tmp.isValid());
        QVERIFY(QDir().mkpath(QDir(tmp.path()).filePath(QStringLiteral("Database"))));

        QFile db(QDir(tmp.path()).filePath(QStringLiteral("Database/librova.db")));
        QVERIFY(db.open(QIODevice::WriteOnly));
        db.close();

        FirstRunController c;
        QVERIFY(!c.validateExistingPath(tmp.path()).isEmpty());
    }

    void validateExistingPath_missing_database_returns_error()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-nodb")));
        QVERIFY(tmp.isValid());
        QVERIFY(QDir().mkpath(QDir(tmp.path()).filePath(QStringLiteral("Database"))));
        QVERIFY(QDir().mkpath(QDir(tmp.path()).filePath(QStringLiteral("Objects"))));
        QVERIFY(QDir().mkpath(QDir(tmp.path()).filePath(QStringLiteral("Logs"))));
        QVERIFY(QDir().mkpath(QDir(tmp.path()).filePath(QStringLiteral("Trash"))));

        FirstRunController c;
        QVERIFY(!c.validateExistingPath(tmp.path()).isEmpty());
    }

    void validateExistingPath_complete_managed_library_returns_ok()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-library")));
        QVERIFY(tmp.isValid());
        CreateManagedLibraryLayout(tmp.path());

        FirstRunController c;
        QVERIFY(c.validateExistingPath(tmp.path()).isEmpty());
    }

    void validateExistingPathForCurrentLibrary_rejects_nested_library_root()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-nested-open")));
        QVERIFY(tmp.isValid());
        CreateManagedLibraryLayout(tmp.path());

        const QString nested = QDir(tmp.path()).filePath(QStringLiteral("Objects/Nested"));
        CreateManagedLibraryLayout(nested);

        FirstRunController c;
        QVERIFY(!c.validateExistingPathForCurrentLibrary(nested, tmp.path()).isEmpty());
    }

    void validateNewPathForCurrentLibrary_rejects_nested_existing_empty_dir()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("frc-nested-create")));
        QVERIFY(tmp.isValid());
        CreateManagedLibraryLayout(tmp.path());

        const QString nested = QDir(tmp.path()).filePath(QStringLiteral("Objects/NewLibrary"));
        QVERIFY(QDir().mkpath(nested));

        FirstRunController c;
        QVERIFY(!c.validateNewPathForCurrentLibrary(nested, tmp.path()).isEmpty());
    }

    // ── toLocalPath ──────────────────────────────────────────────────────────

    void toLocalPath_file_url_returns_local_path()
    {
        const QString path = QStringLiteral("C:/some/path/to/folder");
        const QUrl url = QUrl::fromLocalFile(path);
        QCOMPARE(FirstRunController::toLocalPath(url), path);
    }

    void toLocalPath_cyrillic_path_round_trips()
    {
        const QString path = QStringLiteral("C:/Книги/Моя библиотека");
        const QUrl url = QUrl::fromLocalFile(path);
        QCOMPARE(FirstRunController::toLocalPath(url), path);
    }

    // ── fromLocalPath ────────────────────────────────────────────────────────

    void fromLocalPath_produces_valid_file_url()
    {
        const QString path = QStringLiteral("C:/some/path");
        const QUrl url = FirstRunController::fromLocalPath(path);
        QVERIFY(url.isLocalFile());
        QCOMPARE(url.toLocalFile(), path);
    }

    void fromLocalPath_cyrillic_path_is_valid_url()
    {
        const QString path = QStringLiteral("C:/Книги");
        const QUrl url = FirstRunController::fromLocalPath(path);
        QVERIFY(url.isValid());
        QVERIFY(url.isLocalFile());
        QCOMPARE(url.toLocalFile(), path);
    }

    // ── suggestExportPath ────────────────────────────────────────────────────

    void suggestExportPath_returns_valid_file_url()
    {
        const QUrl url = FirstRunController::suggestExportPath(QStringLiteral("mybook.epub"));
        QVERIFY(url.isValid());
        QVERIFY(url.isLocalFile());
        QVERIFY(url.toLocalFile().endsWith(QStringLiteral("mybook.epub")));
    }

    void suggestExportPath_parent_directory_exists()
    {
        const QUrl url = FirstRunController::suggestExportPath(QStringLiteral("test.epub"));
        const QString localPath = url.toLocalFile();
        const QString parentDir = QFileInfo(localPath).absolutePath();
        QVERIFY(QDir(parentDir).exists());
    }

    void suggestExportPath_cyrillic_filename_is_valid_url()
    {
        const QUrl url =
            FirstRunController::suggestExportPath(QStringLiteral("Тестовая книга.epub"));
        QVERIFY(url.isValid());
        QVERIFY(url.isLocalFile());
        QVERIFY(url.toLocalFile().endsWith(QStringLiteral("Тестовая книга.epub")));
    }
};

QTEST_APPLESS_MAIN(TestFirstRunController)
#include "TestFirstRunController.moc"
