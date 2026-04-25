import QtQuick
import LibrovaQt

Item {
    id: root

    readonly property string state: shellController.currentStateName

    component ReadyShell: Item {
        anchors.fill: parent
        readonly property bool _globalDropEnabled: !importAdapter.hasActiveJob && catalogAdapter.activeCollectionId < 0

        SidebarNav {
            id: sidebar
            anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
            width: LibrovaTheme.sidebarWidth
        }

        Rectangle {
            id: sidebarDivider
            anchors { top: parent.top; bottom: parent.bottom; left: sidebar.right }
            width: 1
            color: LibrovaTheme.sidebarBorder
        }

        SectionLoader {
            id: sections
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: sidebarDivider.right
                right: parent.right
            }
        }

        DropArea {
            anchors.fill: parent
            enabled: _globalDropEnabled
            keys: ["text/uri-list"]
            onDropped: function(drop) {
                const paths = []
                for (let i = 0; i < drop.urls.length; ++i) {
                    const p = firstRunController.toLocalPath(drop.urls[i])
                    if (p.length > 0)
                        paths.push(p)
                }

                if (paths.length > 0) {
                    shellState.lastSection = "import"
                    sections.applySourcePathsAndStart(paths)
                }
            }
        }

        Shortcut {
            sequence: "Esc"
            context: Qt.WindowShortcut
            enabled: shellState.lastSection === "library" && sections.hasSelectedBook()
            onActivated: sections.closeSelectedBook()
        }

        Shortcut {
            sequence: "Delete"
            context: Qt.WindowShortcut
            enabled: shellState.lastSection === "library"
                && sections.hasSelectedBook()
                && !sections.hasFocusedLibraryTextInput()
            onActivated: sections.moveSelectedBookToTrash()
        }
    }

    Loader {
        id: _screenLoader
        anchors.fill: parent
        sourceComponent: {
            switch (root.state) {
            case "idle":
            case "opening":
                return _openingScreen
            case "firstRun":
                return _firstRunScreen
            case "ready":
                return _readyScreen
            case "recovery":
                return _recoveryScreen
            case "damagedLibrary":
                return _damagedLibraryScreen
            case "fatalError":
                return _fatalErrorScreen
            default:
                return _openingScreen
            }
        }
    }

    Component { id: _openingScreen; OpeningView {} }
    Component { id: _firstRunScreen; FirstRunView {} }
    Component { id: _readyScreen; ReadyShell {} }
    Component { id: _recoveryScreen; StartupRecoveryView { damaged: false } }
    Component { id: _damagedLibraryScreen; StartupRecoveryView { damaged: true } }
    Component { id: _fatalErrorScreen; FatalErrorView {} }
}
