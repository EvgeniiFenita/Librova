import QtQuick
import LibrovaQt

Rectangle {
    id: root

    color: LibrovaTheme.surface

    Column {
        anchors {
            left: parent.left
            top: parent.top
            margins: 40
        }
        width: Math.min(parent.width - 80, 680)
        spacing: 14

        Rectangle {
            width: 132
            height: 40
            radius: LibrovaTheme.radiusSmall
            color: LibrovaTheme.accentSurface
            border.color: LibrovaTheme.accentBorder
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 10

                Text {
                    text: "▰"
                    font.family: LibrovaTypography.fontFamily
                    font.pixelSize: 22
                    color: LibrovaTheme.accent
                }

                Text {
                    text: "Librova"
                    font.family: LibrovaTypography.fontFamily
                    font.pixelSize: 20
                    font.weight: LibrovaTypography.weightBold
                    color: LibrovaTheme.textPrimary
                }
            }
        }

        Text {
            text: "LAUNCHING"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            font.weight: LibrovaTypography.weightBold
            font.letterSpacing: LibrovaTypography.spacingEyebrow
            color: LibrovaTheme.accent
        }

        Text {
            text: "Librova is starting"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXl
            font.weight: LibrovaTypography.weightBold
            color: LibrovaTheme.textPrimary
        }

        Text {
            text: shellController.statusText
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: 15
            font.weight: LibrovaTypography.weightSemiBold
            color: LibrovaTheme.textPrimary
        }

        Text {
            width: parent.width
            text: shellController.startupGuidanceText
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeBase
            color: LibrovaTheme.textSecondary
            wrapMode: Text.WordWrap
        }
    }
}
