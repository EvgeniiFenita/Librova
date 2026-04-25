import QtQuick
import LibrovaQt

// Shown when the backend fails with an unrecoverable error.
Item {
    id: root

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - LibrovaTheme.sp8 * 2, 480)
        height: content.implicitHeight + LibrovaTheme.paddingCard * 2
        radius: LibrovaTheme.radiusMedium
        color: LibrovaTheme.dangerSurface
        border.color: LibrovaTheme.dangerBorder
        border.width: 1

        Column {
            id: content
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: LibrovaTheme.paddingCard
            }
            spacing: LibrovaTheme.sp3

            Text {
                text: "Unable to start"
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeLg
                font.weight: LibrovaTypography.weightSemiBold
                color: LibrovaTheme.dangerText
            }

            Text {
                width: parent.width
                text: shellController.fatalErrorMessage || "An unexpected error occurred."
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeBase
                color: LibrovaTheme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
    }
}
