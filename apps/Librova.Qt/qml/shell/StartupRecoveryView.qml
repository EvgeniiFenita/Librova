import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import LibrovaQt

Rectangle {
    id: root

    property bool damaged: false

    color: LibrovaTheme.dangerSurface

    FolderDialog {
        id: _folderDialog
        title: "Choose library folder"
        onAccepted: {
            const p = firstRunController.toLocalPath(selectedFolder)
            if (p.length > 0) _pathField.text = p
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        Column {
            width: Math.min(parent.width - 80, 820)
            anchors.left: parent.left
            anchors.leftMargin: 40
            anchors.top: parent.top
            anchors.topMargin: 32
            spacing: 18

            Text {
                text: "RECOVERY"
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeXs
                font.weight: LibrovaTypography.weightBold
                font.letterSpacing: LibrovaTypography.spacingEyebrow
                color: LibrovaTheme.accent
            }

            Text {
                text: "Startup failed"
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeXl
                font.weight: LibrovaTypography.weightBold
                color: LibrovaTheme.dangerText
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
                text: root.damaged
                    ? "The configured library exists but could not be opened."
                    : "The configured library root is unavailable or invalid."
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeBase
                color: LibrovaTheme.textPrimary
                wrapMode: Text.WordWrap
            }

            Rectangle {
                width: parent.width
                implicitHeight: _whatNext.implicitHeight + 32
                radius: LibrovaTheme.radiusMedium
                color: "#281C12"
                border.color: LibrovaTheme.dangerBorder
                border.width: 1

                Column {
                    id: _whatNext
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 16
                    }
                    spacing: 10

                    Text { text: "What to do next"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                    Text { width: parent.width; text: shellController.startupGuidanceText; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }

                    Grid {
                        width: parent.width
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 16
                        Text { text: "UI LOG"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textMuted }
                        Text { width: _whatNext.width - 120; text: shellController.uiLogFilePath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "UI STATE"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textMuted }
                        Text { width: _whatNext.width - 120; text: shellController.uiStateFilePath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "PREFERENCES"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textMuted }
                        Text { width: _whatNext.width - 120; text: shellController.uiPreferencesFilePath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                    }
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

                    Text { text: "Choose Another Library Root"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                    Text { width: parent.width; text: "If startup failed because the configured library root is unavailable or invalid, pick a different managed library root and retry startup from here."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }

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
                        id: _openError
                        width: parent.width
                        visible: false
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        color: LibrovaTheme.warning
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        id: _newError
                        width: parent.width
                        visible: text.length > 0
                        text: _pathField.text.length > 0 ? firstRunController.validateNewPath(_pathField.text) : ""
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        color: LibrovaTheme.warning
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 10

                        LButton {
                            text: "Retry With This Library"
                            variant: "primary"
                            enabled: _pathField.text.length > 0
                            onClicked: {
                                const err = firstRunController.validateExistingPath(_pathField.text)
                                if (err.length > 0) {
                                    _openError.text = err
                                    _openError.visible = true
                                    return
                                }
                                _openError.text = ""
                                _openError.visible = false
                                shellController.handleRecoveryComplete(_pathField.text, false)
                            }
                        }

                        LButton {
                            text: "Create New"
                            variant: "secondary"
                            enabled: _pathField.text.length > 0 && _newError.text.length === 0
                            onClicked: {
                                _openError.text = ""
                                _openError.visible = false
                                shellController.handleRecoveryComplete(_pathField.text, true)
                            }
                        }
                    }
                }
            }
        }
    }
}
