import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

Popup {
    id: root

    property var targetBookId: null
    property string selectedIconKey: "books"

    function openForLibrary() {
        targetBookId = null
        open()
    }

    function openForBook(bookId) {
        targetBookId = bookId
        open()
    }

    modal: true
    anchors.centerIn: Overlay.overlay
    padding: 22
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: LibrovaTheme.surfaceElevated
        radius: LibrovaTheme.radiusLarge
        border.color: LibrovaTheme.accentBorder
        border.width: 1
    }

    Column {
        width: 476
        spacing: 16

        Text {
            text: "COLLECTIONS"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            font.weight: LibrovaTypography.weightSemiBold
            font.letterSpacing: LibrovaTypography.spacingEyebrow
            color: LibrovaTheme.accent
        }

        Text {
            text: "Create Collection"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeMd
            font.weight: LibrovaTypography.weightSemiBold
            color: LibrovaTheme.textPrimary
        }

        Text {
            text: root.targetBookId === null
                  ? "Choose a name and icon for the new collection."
                  : "Choose a name and icon. The selected book will be added to it."
            width: parent.width
            wrapMode: Text.WordWrap
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeBase
            color: LibrovaTheme.textSecondary
        }

        LTextInput {
            id: _collectionName
            width: parent.width
            placeholderText: "Collection name"
        }

        GridView {
            id: _iconGrid
            width: parent.width
            height: 192
            cellWidth: 56
            cellHeight: 64
            model: LibrovaIcons.collectionOptions
            clip: true

            delegate: Rectangle {
                required property var modelData

                width: 50
                height: 58
                radius: LibrovaTheme.radiusSmall
                color: root.selectedIconKey === modelData.key
                    ? LibrovaTheme.accentSurface
                    : (_iconHover.containsMouse ? LibrovaTheme.surfaceHover : LibrovaTheme.surfaceAlt)
                border.color: root.selectedIconKey === modelData.key
                    ? LibrovaTheme.accent
                    : LibrovaTheme.border
                border.width: root.selectedIconKey === modelData.key ? 2 : 1

                Text {
                    anchors.centerIn: parent
                    text: modelData.glyph
                    font.pixelSize: 20
                    lineHeight: 28
                }

                HoverHandler {
                    id: _iconHover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: root.selectedIconKey = parent.modelData.key
                }
            }
        }

        Row {
            spacing: 10
            anchors.right: parent.right

            LButton {
                text: "Cancel"
                variant: "secondary"
                onClicked: root.close()
            }

            LButton {
                text: "Create"
                variant: "primary"
                enabled: _collectionName.text.trim().length > 0
                onClicked: {
                    const name = _collectionName.text.trim()
                    if (root.targetBookId === null)
                        catalogAdapter.createCollection(name, root.selectedIconKey)
                    else
                        catalogAdapter.createCollectionForBook(root.targetBookId, name, root.selectedIconKey)
                    root.close()
                }
            }
        }
    }

    onOpened: {
        _collectionName.text = ""
        root.selectedIconKey = "books"
        _collectionName.forceActiveFocus()
    }
}
