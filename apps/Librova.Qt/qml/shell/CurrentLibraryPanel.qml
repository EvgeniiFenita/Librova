import QtQuick
import LibrovaQt

Rectangle {
    id: root

    property var openAction: null
    property var newAction: null

    width: parent ? parent.width : 216
    implicitHeight: 142
    radius: LibrovaTheme.radiusMedium
    color: LibrovaTheme.surfaceMuted
    border.color: LibrovaTheme.border
    border.width: 1
    // The panel is muted/disabled while an import is running.
    enabled: !importAdapter.hasActiveJob
    opacity: enabled ? 1.0 : 0.4

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            text: "LIBRARY"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            font.weight: LibrovaTypography.weightSemiBold
            font.letterSpacing: LibrovaTypography.spacingEyebrow
            color: LibrovaTheme.accent
        }

        Text {
            width: parent.width
            text: shellController.currentLibraryRoot || preferences.libraryRoot || "No library selected"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            color: LibrovaTheme.textMuted
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideMiddle
        }

        Text {
            width: parent.width
            text: catalogAdapter.libraryStatisticsText
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            font.weight: LibrovaTypography.weightSemiBold
            color: LibrovaTheme.textSecondary
            wrapMode: Text.WordWrap
        }

        Row {
            width: parent.width
            spacing: 6

            LButton {
                width: (parent.width - 6) / 2
                text: "Open"
                iconPath: LibrovaIcons.folderOpen
                variant: "secondary"
                onClicked: if (root.openAction) root.openAction()
            }

            LButton {
                width: (parent.width - 6) / 2
                text: "New"
                iconPath: LibrovaIcons.addFolder
                variant: "secondary"
                onClicked: if (root.newAction) root.newAction()
            }
        }
    }
}
