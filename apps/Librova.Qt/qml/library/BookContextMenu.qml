import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Right-click context menu for a book.
// Position is set externally by the host (LibrarySection) before opening.
//
// Property:
//   bookId — must be set before open() is called
//
// Signals:
//   exportRequested(bookId)
//   exportAsEpubRequested(bookId)
//   trashRequested(bookId)

Popup {
    id: root

    property var bookId: null
    property string bookTitle: ""
    property string bookFormat: ""
    property var collectionIds: []

    signal exportRequested(var bookId)
    signal exportAsEpubRequested(var bookId)
    signal trashRequested(var bookId)
    signal copyTitleRequested(string title)
    signal addToCollectionRequested(var bookId, var collectionId)
    signal removeFromCollectionRequested(var bookId, var collectionId)
    signal createCollectionForBookRequested(var bookId)

    padding:     LibrovaTheme.sp1
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color:        LibrovaTheme.surfaceElevated
        radius:       LibrovaTheme.radiusMedium
        border.color: LibrovaTheme.accentBorder
        border.width: 1
        layer.enabled: true
        layer.effect: null   // shadow placeholder; replace with MultiEffect if available
    }

    Column {
        width: 180

        ContextMenuItem {
            text:     "Export"
            iconPath: LibrovaIcons.export_
            onActivated: {
                root.close()
                root.exportRequested(root.bookId)
            }
        }

        ContextMenuItem {
            visible: root._canExportAsEpub()
            text:     "Export as EPUB"
            iconPath: LibrovaIcons.export_
            onActivated: {
                root.close()
                root.exportAsEpubRequested(root.bookId)
            }
        }

        // Divider
        Rectangle {
            width:  parent.width
            height: 1
            color:  LibrovaTheme.border
        }

        Text {
            width: parent.width
            leftPadding: 12
            topPadding: 7
            bottomPadding: 5
            text: "Add to"
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            font.weight: LibrovaTypography.weightSemiBold
            color: LibrovaTheme.textMuted
            visible: catalogAdapter.collectionListModel.count > 0
        }

        Repeater {
            model: catalogAdapter.collectionListModel
            delegate: ContextMenuItem {
                visible: model.isDeletable
                text: root._isInCollection(model.collectionId) ? "\u2713 " + model.name : model.name
                iconText: ""
                onActivated: {
                    root.close()
                    if (root._isInCollection(model.collectionId))
                        root.removeFromCollectionRequested(root.bookId, model.collectionId)
                    else
                        root.addToCollectionRequested(root.bookId, model.collectionId)
                }
            }
        }

        ContextMenuItem {
            text: "Create new..."
            iconText: "+"
            onActivated: {
                root.close()
                root.createCollectionForBookRequested(root.bookId)
            }
        }

        Rectangle {
            width:  parent.width
            height: 1
            color:  LibrovaTheme.border
            visible: catalogAdapter.activeCollectionId > 0
        }

        ContextMenuItem {
            visible: catalogAdapter.activeCollectionId > 0
            text: "Remove from collection"
            iconText: "\u2715"
            onActivated: {
                root.close()
                root.removeFromCollectionRequested(root.bookId, catalogAdapter.activeCollectionId)
            }
        }

        ContextMenuItem {
            text:     "Copy Title"
            iconPath: LibrovaIcons.copy
            onActivated: {
                root.close()
                root.copyTitleRequested(root.bookTitle)
            }
        }

        Rectangle {
            width:  parent.width
            height: 1
            color:  LibrovaTheme.border
        }

        ContextMenuItem {
            text:      "Move to Trash"
            iconPath:  LibrovaIcons.trash
            isDestruct: true
            onActivated: {
                root.close()
                root.trashRequested(root.bookId)
            }
        }
    }

    function _canExportAsEpub() {
        return root.bookFormat && root.bookFormat.toLowerCase() === "fb2" && shellController.hasConverter
    }

    function _isInCollection(collectionId) {
        if (!root.collectionIds)
            return false
        for (let i = 0; i < root.collectionIds.length; ++i) {
            if (Number(root.collectionIds[i]) === Number(collectionId))
                return true
        }
        return false
    }

    component ContextMenuItem: Rectangle {
        id: _mi

        property string text:       ""
        property string iconPath:   ""
        property string iconText:   ""
        property bool   isDestruct: false

        signal activated()

        width:  parent ? parent.width : 0
        height: 34
        radius: LibrovaTheme.radiusSmall
        color:  _miHover.hovered
                ? (_mi.isDestruct ? LibrovaTheme.dangerHoverBg : LibrovaTheme.surfaceHover)
                : "transparent"

        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }

        Row {
            anchors {
                left:           parent.left
                leftMargin:     LibrovaTheme.sp3
                verticalCenter: parent.verticalCenter
            }
            spacing: LibrovaTheme.sp3

            // SVG icon slot
            LIcon {
                visible:               _mi.iconPath !== ""
                width:                 visible ? size : 0
                height:                visible ? size : 0
                anchors.verticalCenter: parent.verticalCenter
                iconPath:              _mi.iconPath
                iconColor:             _mi.isDestruct ? LibrovaTheme.danger : LibrovaTheme.textSecondary
                size:                  16
            }

            // Text icon slot (for "+" etc.)
            Text {
                visible:               _mi.iconPath === "" && _mi.iconText !== ""
                width:                 visible ? implicitWidth : 0
                anchors.verticalCenter: parent.verticalCenter
                text:                  _mi.iconText
                font.family:           LibrovaTypography.fontFamily
                font.pixelSize:        LibrovaTypography.sizeBase
                color:                 _mi.isDestruct ? LibrovaTheme.danger : LibrovaTheme.textSecondary
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text:           _mi.text
                font.family:    LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeBase
                color:          _mi.isDestruct ? LibrovaTheme.danger : LibrovaTheme.textPrimary
            }
        }

        HoverHandler { id: _miHover; cursorShape: Qt.PointingHandCursor }
        TapHandler   { onTapped: _mi.activated() }
    }
}
