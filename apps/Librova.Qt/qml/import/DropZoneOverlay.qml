import QtQuick
import LibrovaQt

// Full-window drop-zone overlay; shown when dragging files/folders into the window.
// Parent must supply a DropArea (or this can be used directly as a DropArea).
//
// active — true while a drag is hovering above (control from outside via DragHandler
//           or DropArea.containsDrag)

Rectangle {
    id: root

    property bool active: false

    visible: active
    color:   Qt.rgba(
        LibrovaTheme.accent.r,
        LibrovaTheme.accent.g,
        LibrovaTheme.accent.b,
        0.08)

    border.color: LibrovaTheme.accent
    border.width: 3
    radius:       LibrovaTheme.radiusLarge

    Behavior on opacity { NumberAnimation { duration: LibrovaTheme.animFast } }
    opacity: active ? 1.0 : 0.0

    Column {
        anchors.centerIn: parent
        spacing: LibrovaTheme.sp4

        // Drop icon
        LIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            iconPath:  LibrovaIcons.uploadCloud
            iconColor: LibrovaTheme.accent
            size:      56
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text:           "Drop files or folders to import"
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeMd
            font.weight:    LibrovaTypography.weightSemiBold
            color:          LibrovaTheme.accent
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text:           "FB2, EPUB, ZIP"
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeBase
            color:          LibrovaTheme.accentDim
        }
    }
}
