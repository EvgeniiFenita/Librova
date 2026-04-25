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

            Rectangle {
                anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
                width: 1
                color: LibrovaTheme.accentBorder
            }

            Column {
                anchors.centerIn: parent
                spacing: 0

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "qrc:/assets/librova_hero.png"
                    width:  140
                    height: 140
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Item { width: 1; height: 20 }

                Column {
                    spacing: 10
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        text: "LIBROVA"
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: 15
                        font.weight: LibrovaTypography.weightBold
                        font.letterSpacing: LibrovaTypography.spacingBrand
                        color: LibrovaTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: "YOUR BOOKS. ORGANIZED."
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeXs
                        font.letterSpacing: 1.2
                        color: LibrovaTheme.textMuted
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
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
                                    implicitHeight: _createColumn.implicitHeight + 32
                                    radius: LibrovaTheme.radiusMedium
                                    activeFocusOnTab: true

                                    property bool _hov: false

                                    color: root.isCreateModeSelected
                                        ? LibrovaTheme.accentSurface
                                        : (_hov ? LibrovaTheme.surfaceHover : LibrovaTheme.surface)
                                    border.color: root.isCreateModeSelected
                                        ? LibrovaTheme.accent
                                        : (activeFocus || _hov ? LibrovaTheme.borderStrong : LibrovaTheme.border)
                                    border.width: root.isCreateModeSelected ? 2 : 1

                                    Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
                                    Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }

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
                                        cursorShape: Qt.PointingHandCursor
                                        onHoveredChanged: parent._hov = hovered
                                    }
                                    TapHandler {
                                        onTapped: root.selectedMode = "create"
                                    }
                                    Keys.onSpacePressed:  root.selectedMode = "create"
                                    Keys.onReturnPressed: root.selectedMode = "create"
                                }

                                Rectangle {
                                    width: (parent.width - 10) / 2
                                    implicitHeight: _openColumn.implicitHeight + 32
                                    radius: LibrovaTheme.radiusMedium
                                    activeFocusOnTab: true

                                    property bool _hov: false

                                    color: root.isOpenModeSelected
                                        ? LibrovaTheme.accentSurface
                                        : (_hov ? LibrovaTheme.surfaceHover : LibrovaTheme.surface)
                                    border.color: root.isOpenModeSelected
                                        ? LibrovaTheme.accent
                                        : (activeFocus || _hov ? LibrovaTheme.borderStrong : LibrovaTheme.border)
                                    border.width: root.isOpenModeSelected ? 2 : 1

                                    Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
                                    Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }

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
                                        cursorShape: Qt.PointingHandCursor
                                        onHoveredChanged: parent._hov = hovered
                                    }
                                    TapHandler {
                                        onTapped: root.selectedMode = "open"
                                    }
                                    Keys.onSpacePressed:  root.selectedMode = "open"
                                    Keys.onReturnPressed: root.selectedMode = "open"
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

                            Row {
                                width: parent.width
                                spacing: 8

                                LTextInput {
                                    id: _pathField
                                    width: parent.width - 8 - 120
                                    placeholderText: "C:\\Libraries\\Librova"
                                }

                                LButton {
                                    width: 120
                                    text: "Browse…"
                                    variant: "secondary"
                                    onClicked: _folderDialog.open()
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
