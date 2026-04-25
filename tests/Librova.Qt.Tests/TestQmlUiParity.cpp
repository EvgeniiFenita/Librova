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
        QVERIFY(qml.contains(QStringLiteral("Select Files...")));
        QVERIFY(qml.contains(QStringLiteral("Select Folder...")));
        QVERIFY(qml.contains(QStringLiteral("Import In Progress")));
        QVERIFY(qml.contains(QStringLiteral("Last Import Result")));
        QVERIFY(qml.contains(QStringLiteral("The import pipeline is running. Navigation and other actions stay locked until the import finishes or is cancelled.")));
        QVERIFY(qml.contains(QStringLiteral("Drop local .fb2, .epub, or .zip files and folders here, or use Select Files... / Select Folder...")));

        QVERIFY(!qml.contains(QStringLiteral("Import History")));
        QVERIFY(!qml.contains(QStringLiteral("Add files and click Start Import")));
        QVERIFY(!qml.contains(QStringLiteral("Start Import")));
        QVERIFY(!qml.contains(QStringLiteral("Browse files")));
        QVERIFY(!qml.contains(QStringLiteral("Browse folder")));
    }

    void settingsView_keepsSingleScrollWorkflow()
    {
        const QString qml = readFile(QStringLiteral("settings/SettingsView.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("PREFERENCES")));
        QVERIFY(qml.contains(QStringLiteral("FB2 CONVERTER")));
        QVERIFY(qml.contains(QStringLiteral("Converter Executable")));
        QVERIFY(qml.contains(QStringLiteral("ABOUT")));
        QVERIFY(qml.contains(QStringLiteral("DIAGNOSTICS")));
        QVERIFY(qml.contains(QStringLiteral("property bool _settingsInitialized: false")));
        QVERIFY(qml.contains(QStringLiteral("root._settingsInitialized = true")));

        QVERIFY(!qml.contains(QStringLiteral("LibraryPathSettingsSection")));
        QVERIFY(!qml.contains(QStringLiteral("ConverterSettingsSection")));
        QVERIFY(!qml.contains(QStringLiteral("property string _section")));
        QVERIFY(!qml.contains(QStringLiteral("LNavItem")));
        QVERIFY(!qml.contains(QStringLiteral("Configuration file")));
        QVERIFY(!qml.contains(QStringLiteral("converterConfigPath")));
        QVERIFY(!qml.contains(QStringLiteral("text: \"Save\"")));
        QVERIFY(!qml.contains(QStringLiteral("Component.onCompleted: {\n        if (root.exePath.length > 0)")));
    }

    void firstRunView_keepsHeroLayout()
    {
        const QString qml = readFile(QStringLiteral("shell/FirstRunView.qml"));
        const QString shell = readFile(QStringLiteral("shell/AppShell.qml"));

        QVERIFY(!qml.isEmpty());
        QVERIFY(!shell.isEmpty());
        QVERIFY(qml.contains(QStringLiteral("id: _heroPanel")));
        QVERIFY(qml.contains(QStringLiteral("width: 260")));
        QVERIFY(qml.contains(QStringLiteral("anchors.fill: parent")));
        QVERIFY(qml.contains(QStringLiteral("anchors.top: parent.top")));
        QVERIFY(qml.contains(QStringLiteral("anchors.bottom: parent.bottom")));
        QVERIFY(qml.contains(QStringLiteral("width: parent.width - _heroPanel.width")));
        QVERIFY(qml.contains(QStringLiteral("width: Math.min(parent.width - 80, 480)")));
        QVERIFY(qml.contains(QStringLiteral("color: LibrovaTheme.accentSurface")));
        QVERIFY(qml.contains(QStringLiteral("text: \"Set up your library\"")));
        QVERIFY(qml.contains(QStringLiteral("property string selectedMode: \"create\"")));
        QVERIFY(qml.contains(QStringLiteral("TapHandler")));
        QVERIFY(qml.contains(QStringLiteral("continueButtonText")));
        QVERIFY(qml.contains(QStringLiteral("shellController.handleFirstRunComplete(_pathField.text, root.isCreateModeSelected)")));
        QVERIFY(!qml.contains(QStringLiteral("shellController.handleFirstRunComplete(_pathField.text, true)")));
        QVERIFY(!qml.contains(QStringLiteral("shellController.handleFirstRunComplete(_pathField.text, false)")));
        QVERIFY(shell.contains(QStringLiteral("Loader {")));
        QVERIFY(shell.contains(QStringLiteral("sourceComponent:")));
        QVERIFY(!shell.contains(QStringLiteral("visible: root.state === \"firstRun\"")));
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
        QVERIFY(shell.contains(QStringLiteral("catalogAdapter.activeCollectionId < 0")));
        QVERIFY(shell.contains(QStringLiteral("shellState.lastSection = \"import\"")));
        QVERIFY(shell.contains(QStringLiteral("sections.applySourcePathsAndStart(paths)")));
        QVERIFY(sectionLoader.contains(QStringLiteral("function applySourcePathsAndStart(paths)")));
        QVERIFY(sectionLoader.contains(QStringLiteral("function moveSelectedBookToTrash()")));
        QVERIFY(importSection.contains(QStringLiteral("_importView.applySourcePathsAndStart(paths)")));
        QVERIFY(importView.contains(QStringLiteral("function applySourcePathsAndStart(paths)")));
        QVERIFY(importView.contains(QStringLiteral("readonly property bool _dropImportEnabled: !importAdapter.hasActiveJob && catalogAdapter.activeCollectionId < 0")));
    }

    void libraryView_keepsCollectionsInMainSidebar()
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
        QVERIFY(!librarySection.contains(QStringLiteral("CollectionSidebar")));

        QVERIFY(sidebar.contains(QStringLiteral("COLLECTIONS")));
        QVERIFY(sidebar.contains(QStringLiteral("New collection")));
        QVERIFY(sidebar.contains(QStringLiteral("CurrentLibraryPanel")));
        QVERIFY(sidebar.contains(QStringLiteral("enabled: !importAdapter.hasActiveJob")));
        QVERIFY(sidebar.contains(QStringLiteral("validateExistingPathForCurrentLibrary")));
        QVERIFY(sidebar.contains(QStringLiteral("validateNewPathForCurrentLibrary")));
        QVERIFY(!sidebar.contains(QStringLiteral("Settings stays accessible during an active import")));
        QVERIFY(contextMenu.contains(QStringLiteral("Remove from collection")));
        QVERIFY(contextMenu.contains(QStringLiteral("Move to Trash")));
        QVERIFY(contextMenu.contains(QStringLiteral("Create new...")));
        QVERIFY(librarySection.contains(QStringLiteral("LibraryToolbar")));
        QVERIFY(librarySection.contains(QStringLiteral("id: _contentArea")));
        QVERIFY(librarySection.contains(QStringLiteral("topMargin: _toolbar.height")));
        QVERIFY(librarySection.contains(QStringLiteral("selectedBookIndex")));
        QVERIFY(librarySection.contains(QStringLiteral("function closeSelectedBook()")));
        QVERIFY(librarySection.contains(QStringLiteral("function moveSelectedBookToTrash()")));
        const QString bookGrid = readFile(QStringLiteral("library/BookGrid.qml"));
        QVERIFY(bookGrid.contains(QStringLiteral("positionViewAtIndex")));
        QVERIFY(bookGrid.contains(QStringLiteral("GridView.Center")));
        QVERIFY(bookGrid.contains(QStringLiteral("signal bookContextMenu(var book, int bookIndex")));
        QVERIFY(librarySection.contains(QStringLiteral("_selectedBook = book")));
        QVERIFY(librarySection.contains(QStringLiteral("catalogAdapter.loadBookDetails(book.bookId)")));
        QVERIFY(!bookGrid.contains(QStringLiteral("signal bookContextMenu(var bookId")));
        QVERIFY(bookGrid.contains(QStringLiteral("id: _emptyCard")));
        QVERIFY(bookGrid.contains(QStringLiteral("height: _emptyState.implicitHeight + 36")));
        QVERIFY(bookGrid.contains(QStringLiteral("x: (root.width - width) / 2")));
        QVERIFY(bookGrid.contains(QStringLiteral("y: (root.height - height) / 2")));
        QVERIFY(readFile(QStringLiteral("library/BookDetailsPanel.qml")).contains(QStringLiteral("Export in progress...")));
    }

    void libraryToolbar_keepsDebounceIntervals()
    {
        const QString toolbar = readFile(QStringLiteral("library/LibraryToolbar.qml"));

        QVERIFY(!toolbar.isEmpty());
        QVERIFY(toolbar.contains(QStringLiteral("height: implicitHeight")));
        QVERIFY(toolbar.contains(QStringLiteral("interval: 250")));
        QVERIFY(!toolbar.contains(QStringLiteral("interval: 300")));
        QVERIFY(!toolbar.contains(QStringLiteral("interval: 400")));
    }

    void libraryToolbar_usesFilterLabelAndPersistentSort()
    {
        const QString toolbar = readFile(QStringLiteral("library/LibraryToolbar.qml"));
        const QString librarySection = readFile(QStringLiteral("sections/LibrarySection.qml"));

        QVERIFY(!toolbar.isEmpty());
        QVERIFY(!librarySection.isEmpty());
        QVERIFY(toolbar.contains(QStringLiteral("text: _filterLabel()")));
        QVERIFY(toolbar.contains(QStringLiteral("return n === 0 ? \"Filters\" : \"Filters")));
        QVERIFY(!toolbar.contains(QStringLiteral("width: 54")));
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
