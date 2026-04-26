import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import LibrovaQt

    Rectangle {
        id: root

        color: LibrovaTheme.surface

        property string exePath: preferences.converterExePath
        property bool _settingsInitialized: false

    Timer {
        id: _validateTimer
        interval: 500
        repeat: false
        onTriggered: {
            if (root.exePath.length > 0)
                converterValidator.validate(root.exePath)
            else
                converterValidator.clear()
        }
    }

    FileDialog {
        id: _exeDialog
        title: "Choose converter executable"
        nameFilters: ["Executable files (*.exe)", "All files (*)"]
        onAccepted: {
            const p = firstRunController.toLocalPath(selectedFile)
            if (p.length > 0) {
                root.exePath = p
                _exeField.text = p
                converterValidator.validate(p)
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width
            padding: 20
            spacing: 16

            Column {
                width: parent.width - 40
                spacing: 4
                Text { text: "PREFERENCES"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                Text { text: "Settings"; font.family: LibrovaTypography.fontFamily; font.pixelSize: 22; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
            }

            Rectangle {
                width: parent.width - 40
                implicitHeight: _converter.implicitHeight + 28
                radius: LibrovaTheme.radiusLarge
                color: LibrovaTheme.accentSurface
                border.color: LibrovaTheme.accentBorder
                border.width: 1

                Column {
                    id: _converter
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                    spacing: 14
                    Text { text: "FB2 CONVERTER"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                    Text { text: "Converter Executable"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                    Text { width: parent.width; text: "Settings are applied automatically after validation. Use × to remove the configured converter and disable FB2 conversion."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }

                    Row {
                        width: parent.width
                        spacing: 8

                        Rectangle {
                            width: parent.width - 8 - 116
                            height: LibrovaTheme.controlHeight
                            radius: LibrovaTheme.radiusMedium
                            color: LibrovaTheme.surfaceAlt
                            border.color: _exeField.activeFocus ? LibrovaTheme.accent : LibrovaTheme.border
                            border.width: _exeField.activeFocus ? 2 : 1
                            clip: true
                            Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
                            Behavior on border.width { NumberAnimation { duration: LibrovaTheme.animFast } }

                            Row {
                                anchors.fill: parent

                                TextField {
                                    id: _exeField
                                    height: parent.height
                                    width: parent.width - (root.exePath.length > 0 ? 43 : 0)
                                    leftPadding:  LibrovaTheme.sp3
                                    rightPadding: LibrovaTheme.sp2
                                    topPadding: 0; bottomPadding: 0
                                    background: null
                                    text: root.exePath
                                    placeholderText: "C:\\Tools\\fbc.exe"
                                    font.family: LibrovaTypography.fontFamily
                                    font.pixelSize: LibrovaTypography.sizeBase
                                    color: LibrovaTheme.textPrimary
                                    placeholderTextColor: LibrovaTheme.textMuted
                                    selectedTextColor: LibrovaTheme.textOnAccent
                                    selectionColor: Qt.rgba(LibrovaTheme.accent.r, LibrovaTheme.accent.g, LibrovaTheme.accent.b, 0.35)
                                    onTextChanged: {
                                        root.exePath = text
                                        if (root._settingsInitialized)
                                            _validateTimer.restart()
                                    }
                                }

                                Rectangle { width: 1; height: parent.height; color: LibrovaTheme.border; visible: root.exePath.length > 0 }
                                Rectangle {
                                    width: 42; height: parent.height
                                    visible: root.exePath.length > 0
                                    topRightRadius:    LibrovaTheme.radiusMedium
                                    bottomRightRadius: LibrovaTheme.radiusMedium
                                    color: _clearHov.hovered ? LibrovaTheme.surfaceHover : "transparent"
                                    Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
                                    Text { anchors.centerIn: parent; text: "×"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; color: LibrovaTheme.textSecondary }
                                    HoverHandler { id: _clearHov; cursorShape: Qt.PointingHandCursor }
                                    TapHandler {
                                        onTapped: {
                                            root.exePath = ""
                                            _exeField.text = ""
                                            preferences.converterExePath = ""
                                            preferences.forceEpubConversionOnImport = false
                                            converterValidator.clear()
                                        }
                                    }
                                }
                            }
                        }

                        LButton {
                            width: 116
                            height: LibrovaTheme.controlHeight
                            text: "Browse…"
                            iconPath: LibrovaIcons.folderOpen
                            variant: "secondary"
                            onClicked: _exeDialog.open()
                        }
                    }

                    Text {
                        width: parent.width
                        visible: converterValidator.status === "invalid"
                        text: "Converter not found or failed to run"
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        color: LibrovaTheme.warning
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 8
                        visible: converterValidator.status === "ok"
                        Text { text: "✓"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.success }
                        Text { text: "Converter configured successfully"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeSm; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.success }
                    }

                    Text {
                        width: parent.width
                        visible: converterValidator.status === "checking"
                        text: "Checking converter..."
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeXs
                        color: LibrovaTheme.textMuted
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                width: parent.width - 40
                implicitHeight: _about.implicitHeight + 32
                radius: LibrovaTheme.radiusLarge
                color: LibrovaTheme.surface
                border.color: LibrovaTheme.border
                border.width: 1

                Column {
                    id: _about
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 16
                    Text { text: "ABOUT"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                    Row {
                        spacing: 10
                        Image { source: "qrc:/assets/brand_badge.png"; width: 48; height: 48; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "Librova"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Grid {
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 20
                        Text { text: "VERSION"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.textMuted }
                        Text { text: Qt.application.version; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary }
                        Text { text: "AUTHOR"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.textMuted }
                        Text { text: "Evgenii Volokhovich"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary }
                        Text { text: "CONTACT"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.textMuted }
                        Text { text: "evgenii.github@gmail.com"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary }
                    }
                }
            }

            Rectangle {
                width: parent.width - 40
                implicitHeight: _diag.implicitHeight + 28
                radius: LibrovaTheme.radiusMedium
                color: LibrovaTheme.surfaceMuted
                border.color: LibrovaTheme.border
                border.width: 1

                Column {
                    id: _diag
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                    spacing: 14
                    Text { text: "DIAGNOSTICS"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                    Text { text: "Runtime Paths"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                    Text { width: parent.width; text: "If startup or import flow looks suspicious, inspect the UI log first, then the runtime workspace paths."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                    Grid {
                        width: parent.width
                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 16
                        Text { text: "UI LOG"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingMeta; color: LibrovaTheme.textMuted }
                        Text { width: _diag.width - 120; text: shellController.uiLogFilePath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "UI STATE"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingMeta; color: LibrovaTheme.textMuted }
                        Text { width: _diag.width - 120; text: shellController.uiStateFilePath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "PREFERENCES"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingMeta; color: LibrovaTheme.textMuted }
                        Text { width: _diag.width - 120; text: shellController.uiPreferencesFilePath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "RUNTIME"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingMeta; color: LibrovaTheme.textMuted }
                        Text { width: _diag.width - 120; text: shellController.runtimeWorkspaceRootPath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "CONVERTER"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingMeta; color: LibrovaTheme.textMuted }
                        Text { width: _diag.width - 120; text: shellController.converterWorkingDirectoryPath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { text: "STAGING"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingMeta; color: LibrovaTheme.textMuted }
                        Text { width: _diag.width - 120; text: shellController.managedStorageStagingRootPath; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        root._settingsInitialized = true
    }

    Connections {
        target: converterValidator
        function onStatusChanged() {
            if (converterValidator.status === "ok") {
                preferences.converterExePath = root.exePath
            } else if (converterValidator.status === "none") {
                preferences.converterExePath = ""
                preferences.forceEpubConversionOnImport = false
            }
        }
    }

    Connections {
        target: preferences
        function onConverterExePathChanged() {
            if (root.exePath !== preferences.converterExePath)
                root.exePath = preferences.converterExePath
        }
    }
}
