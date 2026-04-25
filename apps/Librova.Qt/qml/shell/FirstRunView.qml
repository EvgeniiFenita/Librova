import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import LibrovaQt

Rectangle {
    id: root

    property string selectedMode: "create"
    readonly property bool isCreateModeSelected: selectedMode === "create"
    readonly property bool isOpenModeSelected: selectedMode === "open"
    readonly property string validationMessage: {
        if (_pathField.text.length === 0)
            return ""
        return isCreateModeSelected
            ? firstRunController.validateNewPath(_pathField.text)
            : firstRunController.validateExistingPath(_pathField.text)
    }
    readonly property string helperText: isOpenModeSelected
        ? "Choose an absolute directory path to an existing Librova managed library."
        : "Choose an absolute directory path on an available drive. The folder may be new or empty; Librova will create its managed structure after a successful first launch."
    readonly property string continueButtonText: isOpenModeSelected ? "Open Library" : "Continue"

    color: LibrovaTheme.surface

    FolderDialog {
        id: _folderDialog
        title: "Choose library folder"
        onAccepted: {
            const p = firstRunController.toLocalPath(selectedFolder)
            if (p.length > 0)
                _pathField.text = p
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: _heroPanel
            width: 260
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: LibrovaTheme.accentSurface
            border.color: LibrovaTheme.accentBorder
            border.width: 1

            Rectangle {
                anchors {
                    top: parent.top
                    right: parent.right
                    bottom: parent.bottom
                }
                width: 1
                color: LibrovaTheme.accentBorder
            }

            Column {
                anchors.centerIn: parent
                spacing: 20

                LIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconPath:  LibrovaIcons.library
                    iconColor: LibrovaTheme.accent
                    size:      100
                }

                Column {
                    spacing: 6
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        text: "Librova"
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: 22
                        font.weight: LibrovaTypography.weightBold
                        color: LibrovaTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: "Your shelf, your story"
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeXs
                        color: LibrovaTheme.textMuted
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        ScrollView {
            id: _setupScroll
            width: parent.width - _heroPanel.width
            height: parent.height
            clip: true
            contentWidth: availableWidth

            Item {
                width: Math.max(_setupScroll.availableWidth, 0)
                implicitHeight: Math.max(_formColumn.implicitHeight + 80, _setupScroll.height)

                Column {
                    id: _formColumn
                    width: Math.min(parent.width - 80, 480)
                    anchors.top: parent.top
                    anchors.topMargin: 40
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 24

                    Column {
                        spacing: 8

                        Text {
                            text: "WELCOME"
                            font.family: LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeXs
                            font.weight: LibrovaTypography.weightBold
                            font.letterSpacing: LibrovaTypography.spacingEyebrow
                            color: LibrovaTheme.accent
                        }

                        Text {
                            text: "Set up your library"
                            font.family: LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeXl
                            font.weight: LibrovaTypography.weightBold
                            color: LibrovaTheme.textPrimary
                        }

                        Text {
                            text: shellController.statusText
                            font.family: LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeBase
                            color: LibrovaTheme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }

                    Rectangle {
                        width: parent.width
                        implicitHeight: _content.implicitHeight + 32
                        radius: LibrovaTheme.radiusLarge
                        color: LibrovaTheme.accentSurface
                        border.color: LibrovaTheme.accentBorder
                        border.width: 1

                        Column {
                            id: _content
                            anchors {
                                left: parent.left
                                right: parent.right
                                top: parent.top
                                margins: 16
                            }
                            spacing: 14

                            Text {
                                text: "Library Root Folder"
                                font.family: LibrovaTypography.fontFamily
                                font.pixelSize: LibrovaTypography.sizeMd
                                font.weight: LibrovaTypography.weightSemiBold
                                color: LibrovaTheme.textPrimary
                            }

                            Row {
                                width: parent.width
                                spacing: 10

                                Rectangle {
                                    width: (parent.width - 10) / 2
                                    implicitHeight: _createColumn.implicitHeight + 20
                                    radius: LibrovaTheme.radiusMedium
                                    color: root.isCreateModeSelected
                                        ? LibrovaTheme.accentSurface
                                        : (_createHover.containsMouse ? LibrovaTheme.surfaceHover : LibrovaTheme.surface)
                                    border.color: root.isCreateModeSelected ? LibrovaTheme.accentBorder : LibrovaTheme.border
                                    border.width: 1

                                    Column {
                                        id: _createColumn
                                        anchors {
                                            left: parent.left
                                            right: parent.right
                                            top: parent.top
                                            margins: 16
                                        }
                                        spacing: 10

                                        LIcon {
                                            iconPath:  LibrovaIcons.addFolder
                                            iconColor: root.isCreateModeSelected ? LibrovaTheme.accent : LibrovaTheme.textMuted
                                            size:      22
                                        }
                                        Text {
                                            text: "Create New"
                                            font.family: LibrovaTypography.fontFamily
                                            font.pixelSize: LibrovaTypography.sizeBase
                                            font.weight: LibrovaTypography.weightSemiBold
                                            color: LibrovaTheme.textPrimary
                                        }
                                        Text {
                                            width: parent.width
                                            text: "Start a fresh library in a new or empty folder."
                                            font.family: LibrovaTypography.fontFamily
                                            font.pixelSize: LibrovaTypography.sizeXs
                                            color: LibrovaTheme.textMuted
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    HoverHandler {
                                        id: _createHover
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                    TapHandler {
                                        onTapped: root.selectedMode = "create"
                                    }
                                }

                                Rectangle {
                                    width: (parent.width - 10) / 2
                                    implicitHeight: _openColumn.implicitHeight + 20
                                    radius: LibrovaTheme.radiusMedium
                                    color: root.isOpenModeSelected
                                        ? LibrovaTheme.accentSurface
                                        : (_openHover.containsMouse ? LibrovaTheme.surfaceHover : LibrovaTheme.surface)
                                    border.color: root.isOpenModeSelected ? LibrovaTheme.accentBorder : LibrovaTheme.border
                                    border.width: 1

                                    Column {
                                        id: _openColumn
                                        anchors {
                                            left: parent.left
                                            right: parent.right
                                            top: parent.top
                                            margins: 16
                                        }
                                        spacing: 10

                                        LIcon {
                                            iconPath:  LibrovaIcons.folderOpen
                                            iconColor: root.isOpenModeSelected ? LibrovaTheme.accent : LibrovaTheme.textMuted
                                            size:      22
                                        }
                                        Text {
                                            text: "Open Existing"
                                            font.family: LibrovaTypography.fontFamily
                                            font.pixelSize: LibrovaTypography.sizeBase
                                            font.weight: LibrovaTypography.weightSemiBold
                                            color: LibrovaTheme.textPrimary
                                        }
                                        Text {
                                            width: parent.width
                                            text: "Point to a folder that already contains a Librova library."
                                            font.family: LibrovaTypography.fontFamily
                                            font.pixelSize: LibrovaTypography.sizeXs
                                            color: LibrovaTheme.textMuted
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    HoverHandler {
                                        id: _openHover
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                    TapHandler {
                                        onTapped: root.selectedMode = "open"
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                text: "Librova will store the database, managed books, covers, logs, temporary import files, and trash under this root."
                                font.family: LibrovaTypography.fontFamily
                                font.pixelSize: LibrovaTypography.sizeBase
                                color: LibrovaTheme.textSecondary
                                wrapMode: Text.WordWrap
                            }

                            Rectangle {
                                width: parent.width
                                height: LibrovaTheme.controlHeight
                                radius: LibrovaTheme.radiusMedium
                                color: LibrovaTheme.surfaceAlt
                                border.color: _pathField.activeFocus ? LibrovaTheme.accent : LibrovaTheme.border
                                border.width: _pathField.activeFocus ? 2 : 1
                                clip: true
                                Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
                                Behavior on border.width { NumberAnimation { duration: LibrovaTheme.animFast } }

                                Row {
                                    anchors.fill: parent
                                    TextField {
                                        id: _pathField
                                        height: parent.height
                                        width: parent.width - 1 - 110
                                        leftPadding:  LibrovaTheme.sp3
                                        rightPadding: LibrovaTheme.sp2
                                        topPadding: 0; bottomPadding: 0
                                        background: null
                                        placeholderText: "C:\\Libraries\\Librova"
                                        font.family: LibrovaTypography.fontFamily
                                        font.pixelSize: LibrovaTypography.sizeBase
                                        color: LibrovaTheme.textPrimary
                                        placeholderTextColor: LibrovaTheme.textMuted
                                        selectedTextColor: LibrovaTheme.textOnAccent
                                        selectionColor: Qt.rgba(LibrovaTheme.accent.r, LibrovaTheme.accent.g, LibrovaTheme.accent.b, 0.35)
                                    }
                                    Rectangle { width: 1; height: parent.height; color: LibrovaTheme.border }
                                    Rectangle {
                                        width: 110; height: parent.height
                                        color: _browseHov.containsMouse ? LibrovaTheme.surfaceHover : "transparent"
                                        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
                                        Text { anchors.centerIn: parent; text: "Browse…"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textPrimary }
                                        HoverHandler { id: _browseHov; cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: _folderDialog.open() }
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                visible: root.validationMessage.length > 0
                                text: root.validationMessage
                                font.family: LibrovaTypography.fontFamily
                                font.pixelSize: LibrovaTypography.sizeSm
                                color: LibrovaTheme.warning
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                width: parent.width
                                visible: root.validationMessage.length === 0
                                text: root.helperText
                                font.family: LibrovaTypography.fontFamily
                                font.pixelSize: LibrovaTypography.sizeXs
                                color: LibrovaTheme.textMuted
                                wrapMode: Text.WordWrap
                            }

                            LButton {
                                text: root.continueButtonText
                                variant: "primary"
                                implicitWidth: 180
                                enabled: _pathField.text.length > 0 && root.validationMessage.length === 0
                                onClicked: {
                                    shellController.handleFirstRunComplete(_pathField.text, root.isCreateModeSelected)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
