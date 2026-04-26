import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import LibrovaQt

Rectangle {
    id: root

    color: LibrovaTheme.sidebar

    FolderDialog {
        id: _openLibraryDialog
        title: "Open existing Librova library"
        onAccepted: {
            const p = firstRunController.toLocalPath(selectedFolder)
            if (p.length > 0) {
                const validation = firstRunController.validateExistingPathForCurrentLibrary(p, shellController.currentLibraryRoot)
                if (validation.length > 0) {
                    _toast.show(validation, "error")
                    return
                }
                shellController.handleLibrarySwitch(p)
            }
        }
    }

    FolderDialog {
        id: _newLibraryDialog
        title: "Create new Librova library"
        onAccepted: {
            const p = firstRunController.toLocalPath(selectedFolder)
            if (p.length > 0) {
                const validation = firstRunController.validateNewPathForCurrentLibrary(p, shellController.currentLibraryRoot)
                if (validation.length > 0) {
                    _toast.show(validation, "error")
                    return
                }
                shellController.handleFirstRunComplete(p, true)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0A0806" }
            GradientStop { position: 1.0; color: "#1A1409" }
        }
    }

    Column {
        id: _bottom
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 14
        }
        spacing: 5

        CurrentLibraryPanel {
            width: parent.width
            openAction: function() { _openLibraryDialog.open() }
            newAction: function() { _newLibraryDialog.open() }
            enabled: !importAdapter.hasActiveJob
        }

        LNavItem {
            width: parent.width
            text: "Settings"
            iconPath: LibrovaIcons.settings
            active: shellState.lastSection === "settings"
            enabled: !importAdapter.hasActiveJob
            onClicked: shellState.lastSection = "settings"
        }

        Text {
            width: parent.width
            text: "Version " + Qt.application.version
            horizontalAlignment: Text.AlignHCenter
            font.family: LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            color: LibrovaTheme.textMuted
            topPadding: 6
        }
    }

    Column {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: _bottom.top
            margins: 14
            topMargin: 10
        }
        spacing: 4

        // Brand logo block — icon badge + wordmark + divider
        Item {
            width: parent.width
            height: 68

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                leftPadding: 2

                // Brand medallion badge
                Image {
                    width: 56; height: 56
                    source: "qrc:/assets/brand_badge.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3

                    Text {
                        text: "Librova"
                        font.family:        LibrovaTypography.fontFamily
                        font.pixelSize:     LibrovaTypography.sizeMd
                        font.weight:        LibrovaTypography.weightSemiBold
                        font.letterSpacing: LibrovaTypography.spacingMeta
                        color: LibrovaTheme.textPrimary
                    }

                    Text {
                        text: "Your shelf, your story"
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeXs
                        color: LibrovaTheme.textMuted
                    }
                }
            }

            // Amber divider at the bottom
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: LibrovaTheme.accentBorder
            }
        }

        Column {
            width: parent.width
            spacing: 5
            topPadding: 18

            LNavItem {
                width: parent.width
                text: "Library"
                iconPath: LibrovaIcons.library
                active: shellState.lastSection === "library" && catalogAdapter.activeCollectionId < 0
                enabled: !importAdapter.hasActiveJob
                onClicked: {
                    shellState.lastSection = "library"
                    catalogAdapter.clearCollectionFilter()
                }
            }

            LNavItem {
                width: parent.width
                text: "Import"
                iconPath: LibrovaIcons.import_
                active: shellState.lastSection === "import"
                enabled: !importAdapter.hasActiveJob
                onClicked: shellState.lastSection = "import"
            }
        }

        Rectangle {
            width: parent.width
            height: Math.min(330, Math.max(80, parent.height - 260))
            radius: LibrovaTheme.radiusMedium
            color: LibrovaTheme.surfaceMuted
            border.color: LibrovaTheme.border
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text {
                    text: "COLLECTIONS"
                    font.family: LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeXs
                    font.weight: LibrovaTypography.weightSemiBold
                    font.letterSpacing: LibrovaTypography.spacingEyebrow
                    color: LibrovaTheme.accent
                }

                Column {
                    width: parent.width
                    spacing: 6
                    visible: catalogAdapter.collectionListModel.count === 0

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "qrc:/assets/no_collections.png"
                        width: 100; height: 65
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }

                    Text {
                        width: parent.width
                        text: "No collections yet"
                        horizontalAlignment: Text.AlignHCenter
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeXs
                        color: LibrovaTheme.textMuted
                    }

                    LButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Create one"
                        variant: "secondary"
                        implicitWidth: 120
                        onClicked: _createDlg.openForLibrary()
                    }
                }

                ListView {
                    width: parent.width
                    height: Math.max(0, parent.height - 84)
                    clip: true
                    spacing: 4
                    visible: catalogAdapter.collectionListModel.count > 0
                    model: catalogAdapter.collectionListModel

                    delegate: LNavItem {
                        width:     ListView.view ? ListView.view.width : 0
                        glyphText: LibrovaIcons.collectionGlyph(model.iconKey)
                        text:      model.name
                        active:    catalogAdapter.activeCollectionId === model.collectionId
                        onClicked: {
                            shellState.lastSection = "library"
                            catalogAdapter.setCollectionFilter(model.collectionId)
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton && model.isDeletable) {
                                    _deleteDlg.collectionId = model.collectionId
                                    _deleteDlg.collectionName = model.name
                                    _deleteDlg.open()
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: LibrovaTheme.border
                    visible: catalogAdapter.collectionListModel.count > 0
                }

                LButton {
                    width: parent.width
                    text: "＋  New collection"
                    variant: "accent"
                    visible: catalogAdapter.collectionListModel.count > 0
                    onClicked: _createDlg.openForLibrary()
                }
            }
        }
    }

    CollectionCreateDialog {
        id: _createDlg
    }

    LToast {
        id: _toast
        parent: root
    }

    Popup {
        id: _deleteDlg
        property var collectionId: -1
        property string collectionName: ""
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: LibrovaTheme.dangerSurface
            radius: LibrovaTheme.radiusLarge
            border.color: LibrovaTheme.dangerBorder
            border.width: 1
        }
        Column {
            width: 456
            spacing: 16
            Text { text: "COLLECTIONS"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
            Text { text: "Delete Collection"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
            Text { width: parent.width; text: "Delete '" + _deleteDlg.collectionName + "'? Books stay in the library; only collection membership is removed."; wrapMode: Text.WordWrap; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary }
            Row {
                spacing: 10
                anchors.right: parent.right
                LButton { text: "Cancel"; variant: "secondary"; onClicked: _deleteDlg.close() }
                LButton {
                    text: "Delete"
                    variant: "destructive"
                    onClicked: {
                        catalogAdapter.deleteCollection(_deleteDlg.collectionId)
                        _deleteDlg.close()
                    }
                }
            }
        }
    }

}
