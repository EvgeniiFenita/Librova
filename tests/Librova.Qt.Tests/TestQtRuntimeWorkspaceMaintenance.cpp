#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>

#include "App/QtRuntimeWorkspaceMaintenance.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;

class TestQtRuntimeWorkspaceMaintenance : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void prepareForSession_empty_path_is_noop()
    {
        // Must not throw or crash when given an empty path.
        QtRuntimeWorkspaceMaintenance::PrepareForSession({});
    }

    void prepareForSession_nonexistent_root_is_noop()
    {
        const std::filesystem::path ghost =
            std::filesystem::temp_directory_path() / L"librova_nonexistent_rwm_xyz";
        QtRuntimeWorkspaceMaintenance::PrepareForSession(ghost);
    }

    void prepareForSession_removes_generated_import_workspace()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("rwm-import")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());
        const auto targetDir = root / "ImportWorkspaces" / "GeneratedUiImportWorkspace";
        std::filesystem::create_directories(targetDir);

        QFile f(QString::fromStdWString((targetDir / "file.tmp").wstring()));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QtRuntimeWorkspaceMaintenance::PrepareForSession(root);

        QVERIFY(!QDir(QString::fromStdWString(targetDir.wstring())).exists());
    }

    void prepareForSession_removes_converter_workspace()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("rwm-converter")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());
        const auto targetDir = root / "ConverterWorkspace";
        std::filesystem::create_directories(targetDir);

        QFile f(QString::fromStdWString((targetDir / "output.epub").wstring()));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QtRuntimeWorkspaceMaintenance::PrepareForSession(root);

        QVERIFY(!QDir(QString::fromStdWString(targetDir.wstring())).exists());
    }

    void prepareForSession_removes_managed_storage_staging()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("rwm-staging")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());
        const auto targetDir = root / "ManagedStorageStaging";
        std::filesystem::create_directories(targetDir);

        QFile f(QString::fromStdWString((targetDir / "pending.tmp").wstring()));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QtRuntimeWorkspaceMaintenance::PrepareForSession(root);

        QVERIFY(!QDir(QString::fromStdWString(targetDir.wstring())).exists());
    }

    void prepareForSession_removes_empty_import_workspaces_parent()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("rwm-importparent")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());
        // Only the target subdirectory; ImportWorkspaces will be empty after cleanup.
        std::filesystem::create_directories(
            root / "ImportWorkspaces" / "GeneratedUiImportWorkspace");

        QtRuntimeWorkspaceMaintenance::PrepareForSession(root);

        QVERIFY(!QDir(QString::fromStdWString((root / "ImportWorkspaces").wstring())).exists());
    }

    void prepareForSession_keeps_non_empty_import_workspaces_parent()
    {
        QTemporaryDir tmp(TestWorkspaceTemplate(QStringLiteral("rwm-importparentkept")));
        QVERIFY(tmp.isValid());

        const std::filesystem::path root(tmp.path().toStdWString());
        std::filesystem::create_directories(
            root / "ImportWorkspaces" / "GeneratedUiImportWorkspace");
        // Another child keeps ImportWorkspaces non-empty after the target dir is removed.
        std::filesystem::create_directories(root / "ImportWorkspaces" / "OtherWorkspace");

        QtRuntimeWorkspaceMaintenance::PrepareForSession(root);

        QVERIFY(QDir(QString::fromStdWString((root / "ImportWorkspaces").wstring())).exists());
    }
};

QTEST_APPLESS_MAIN(TestQtRuntimeWorkspaceMaintenance)
#include "TestQtRuntimeWorkspaceMaintenance.moc"
