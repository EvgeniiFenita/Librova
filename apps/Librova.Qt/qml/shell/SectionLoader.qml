import QtQuick
import LibrovaQt

// Keeps all primary sections alive and switches visibility, matching the
// Sections remain alive so view state is preserved across navigation.
Item {
    id: root

    LibrarySection {
        anchors.fill: parent
        visible: shellState.lastSection !== "import" && shellState.lastSection !== "settings"
    }

    ImportSection {
        id: _importSection
        anchors.fill: parent
        visible: shellState.lastSection === "import"
    }

    SettingsSection {
        anchors.fill: parent
        visible: shellState.lastSection === "settings"
    }

    function applySourcePathsAndStart(paths) {
        _importSection.applySourcePathsAndStart(paths)
    }

    function closeSelectedBook() {
        return _librarySection.closeSelectedBook()
    }

    function moveSelectedBookToTrash() {
        return _librarySection.moveSelectedBookToTrash()
    }

    function hasSelectedBook() {
        return _librarySection.hasSelectedBook()
    }

    function hasFocusedLibraryTextInput() {
        return _librarySection.hasFocusedTextInput()
    }
}
