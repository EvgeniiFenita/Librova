import QtQuick
import LibrovaQt

// Import section — delegates to ImportView.
Rectangle {
    id: root
    color: LibrovaTheme.background

    ImportView {
        id: _importView
        anchors.fill: parent
    }

    function applySourcePathsAndStart(paths) {
        _importView.applySourcePathsAndStart(paths)
    }
}
