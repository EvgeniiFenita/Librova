#include <QFile>
#include <QTest>

namespace {

QString readFile(const QString& relativePath)
{
    QFile file(QStringLiteral(LIBROVA_SOURCE_DIR) + QStringLiteral("/apps/Librova.Qt/qml/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestQmlUiParity : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void importView_keepsImmediateImportWorkflow()
    {
        const QString qml = readFile(QStringLiteral("import/ImportView.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("Drop files or folders here")));
        QVERIFY(qml.contains(QStringLiteral("Dropping or selecting starts the import immediately.")));
        QVERIFY(qml.contains(QStringLiteral("Import In Progress")));

        QVERIFY(!qml.contains(QStringLiteral("Import History")));
        QVERIFY(!qml.contains(QStringLiteral("Add files and click Start Import")));
        QVERIFY(!qml.contains(QStringLiteral("Start Import")));
    }

    void shellSupportsWindowLevelDropImport()
    {
        const QString shell = readFile(QStringLiteral("shell/AppShell.qml"));
        const QString sectionLoader = readFile(QStringLiteral("shell/SectionLoader.qml"));
        const QString importSection = readFile(QStringLiteral("sections/ImportSection.qml"));
        const QString importView = readFile(QStringLiteral("import/ImportView.qml"));

        QVERIFY(!shell.isEmpty());
        QVERIFY(!sectionLoader.isEmpty());
        QVERIFY(!importSection.isEmpty());
        QVERIFY(!importView.isEmpty());
        QVERIFY(shell.contains(QStringLiteral("DropArea")));
        QVERIFY(shell.contains(QStringLiteral("keys: [\"text/uri-list\"]")));
        QVERIFY(shell.contains(QStringLiteral("sections.applySourcePathsAndStart(paths)")));
        QVERIFY(sectionLoader.contains(QStringLiteral("function applySourcePathsAndStart(paths)")));
        QVERIFY(importSection.contains(QStringLiteral("_importView.applySourcePathsAndStart(paths)")));
        QVERIFY(importView.contains(QStringLiteral("function applySourcePathsAndStart(paths)")));
    }

    void librarySection_keepsCoreCompositionAndCommands()
    {
        const QString librarySection = readFile(QStringLiteral("sections/LibrarySection.qml"));
        const QString sidebar = readFile(QStringLiteral("shell/SidebarNav.qml"));
        const QString contextMenu = readFile(QStringLiteral("library/BookContextMenu.qml"));

        QVERIFY(!librarySection.isEmpty());
        QVERIFY(!sidebar.isEmpty());
        QVERIFY(!contextMenu.isEmpty());
        QVERIFY(librarySection.contains(QStringLiteral("LibraryToolbar")));
        QVERIFY(librarySection.contains(QStringLiteral("BookGrid")));
        QVERIFY(librarySection.contains(QStringLiteral("BookDetailsPanel")));
        QVERIFY(librarySection.contains(QStringLiteral("catalogAdapter.loadBookDetails(book.bookId)")));
        QVERIFY(librarySection.contains(QStringLiteral("function closeSelectedBook()")));
        QVERIFY(librarySection.contains(QStringLiteral("function moveSelectedBookToTrash()")));

        QVERIFY(sidebar.contains(QStringLiteral("COLLECTIONS")));
        QVERIFY(sidebar.contains(QStringLiteral("New collection")));
        QVERIFY(sidebar.contains(QStringLiteral("CurrentLibraryPanel")));

        QVERIFY(contextMenu.contains(QStringLiteral("Remove from collection")));
        QVERIFY(contextMenu.contains(QStringLiteral("Move to Trash")));
        QVERIFY(contextMenu.contains(QStringLiteral("Create new...")));
    }

    void settingsView_keepsSingleScrollWorkflow()
    {
        const QString qml = readFile(QStringLiteral("settings/SettingsView.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("PREFERENCES")));
        QVERIFY(qml.contains(QStringLiteral("FB2 CONVERTER")));
        QVERIFY(qml.contains(QStringLiteral("ABOUT")));
        QVERIFY(qml.contains(QStringLiteral("DIAGNOSTICS")));
        QVERIFY(qml.contains(QStringLiteral("property bool _settingsInitialized: false")));

        QVERIFY(!qml.contains(QStringLiteral("LibraryPathSettingsSection")));
        QVERIFY(!qml.contains(QStringLiteral("ConverterSettingsSection")));
        QVERIFY(!qml.contains(QStringLiteral("property string _section")));
    }

    void firstRunView_keepsModeSelectionWorkflow()
    {
        const QString qml = readFile(QStringLiteral("shell/FirstRunView.qml"));
        const QString shell = readFile(QStringLiteral("shell/AppShell.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(!shell.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("property string selectedMode: \"create\"")));
        QVERIFY(qml.contains(QStringLiteral("TapHandler")));
        QVERIFY(qml.contains(QStringLiteral("continueButtonText")));
        QVERIFY(qml.contains(QStringLiteral("shellController.handleFirstRunComplete(_pathField.text, root.isCreateModeSelected)")));
        QVERIFY(!qml.contains(QStringLiteral("shellController.handleFirstRunComplete(_pathField.text, true)")));
        QVERIFY(!qml.contains(QStringLiteral("shellController.handleFirstRunComplete(_pathField.text, false)")));
        QVERIFY(shell.contains(QStringLiteral("sourceComponent:")));
    }

    void libraryToolbar_keepsPersistentSort()
    {
        const QString toolbar = readFile(QStringLiteral("library/LibraryToolbar.qml"));
        const QString librarySection = readFile(QStringLiteral("sections/LibrarySection.qml"));

        QVERIFY(!toolbar.isEmpty());
        QVERIFY(!librarySection.isEmpty());
        QVERIFY(toolbar.contains(QStringLiteral("preferences.preferredSortKey = root.sortBy")));
        QVERIFY(toolbar.contains(QStringLiteral("preferences.preferredSortDescending = root.sortDir === \"desc\"")));
        QVERIFY(librarySection.contains(QStringLiteral("preferences.preferredSortKey")));
        QVERIFY(!librarySection.contains(QStringLiteral("catalogAdapter.setSortBy(\"title\", \"asc\")")));
    }

    void importView_restoresStateTransitions()
    {
        const QString qml = readFile(QStringLiteral("import/ImportView.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("shellState.sourcePaths = paths")));
        QVERIFY(qml.contains(QStringLiteral("shellState.lastSection = \"library\"")));
        QVERIFY(qml.contains(QStringLiteral("_describeSources(shellState.sourcePaths)")));
    }

    void sectionLoader_preservesSectionLifetimes()
    {
        const QString qml = readFile(QStringLiteral("shell/SectionLoader.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("LibrarySection")));
        QVERIFY(qml.contains(QStringLiteral("ImportSection")));
        QVERIFY(qml.contains(QStringLiteral("SettingsSection")));
        QVERIFY(qml.contains(QStringLiteral("function closeSelectedBook()")));
        QVERIFY(qml.contains(QStringLiteral("function hasFocusedLibraryTextInput()")));
        QVERIFY(qml.contains(QStringLiteral("visible: shellState.lastSection === \"import\"")));
        QVERIFY(!qml.contains(QStringLiteral("source:")));
        QVERIFY(!qml.contains(QStringLiteral("Qt.resolvedUrl")));
    }

    void readyShell_restoresLibraryShortcuts()
    {
        const QString shell = readFile(QStringLiteral("shell/AppShell.qml"));

        QVERIFY(!shell.isEmpty());
        QVERIFY(shell.contains(QStringLiteral("Shortcut {")));
        QVERIFY(shell.contains(QStringLiteral("sequence: \"Esc\"")));
        QVERIFY(shell.contains(QStringLiteral("sequence: \"Delete\"")));
        QVERIFY(shell.contains(QStringLiteral("sections.closeSelectedBook()")));
        QVERIFY(shell.contains(QStringLiteral("!sections.hasFocusedLibraryTextInput()")));
    }

    void runTestsScriptRunsQtOnlyChecks()
    {
        QFile file(QStringLiteral(LIBROVA_SOURCE_DIR) + QStringLiteral("/scripts/Run-Tests.ps1"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString script = QString::fromUtf8(file.readAll());

        QVERIFY(script.contains(QStringLiteral("cmake --build --preset")));
        QVERIFY(script.contains(QStringLiteral("ctest --test-dir")));
        QVERIFY(script.contains(QStringLiteral("ValidateBoundary.ps1")));
    }
};

QTEST_APPLESS_MAIN(TestQmlUiParity)
#include "TestQmlUiParity.moc"
