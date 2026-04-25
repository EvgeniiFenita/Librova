import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import LibrovaQt

Rectangle {
    id: root

    color: LibrovaTheme.surface
    readonly property bool _dropImportEnabled: !importAdapter.hasActiveJob && catalogAdapter.activeCollectionId < 0

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        enabled: root._dropImportEnabled
        onEntered: _overlay.active = true
        onExited: _overlay.active = false
        onDropped: function(drop) {
            _overlay.active = false
            const paths = []
            for (let i = 0; i < drop.urls.length; ++i) {
                const p = firstRunController.toLocalPath(drop.urls[i])
                if (p.length > 0)
                    paths.push(p)
            }
            if (paths.length > 0)
                _validateAndStart(paths)
        }
    }

    DropZoneOverlay { id: _overlay; anchors.fill: parent; z: 100 }

    FileDialog {
        id: _fileDlg
        title: "Select files to import"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Book files (*.fb2 *.epub *.zip)", "All files (*)"]
        onAccepted: {
            const paths = []
            for (let i = 0; i < selectedFiles.length; ++i) {
                const p = firstRunController.toLocalPath(selectedFiles[i])
                if (p.length > 0)
                    paths.push(p)
            }
            if (paths.length > 0)
                _validateAndStart(paths)
        }
    }

    FolderDialog {
        id: _folderDlg
        title: "Select folder to import"
        onAccepted: {
            const p = firstRunController.toLocalPath(selectedFolder)
            if (p.length > 0)
                _validateAndStart([p])
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
                opacity: importAdapter.hasActiveJob ? 0.45 : 1

                Text { text: "INGEST"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                Text { text: "Import Books"; font.family: LibrovaTypography.fontFamily; font.pixelSize: 22; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                Text {
                    width: parent.width
                    text: "Drop local .fb2, .epub, or .zip files and folders onto the window, or use the buttons below."
                    font.family: LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeBase
                    color: LibrovaTheme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                width: parent.width - 40
                height: 250
                radius: LibrovaTheme.radiusLarge
                color: LibrovaTheme.accentSurface
                border.color: LibrovaTheme.border
                border.width: 1
                opacity: importAdapter.hasActiveJob ? 0.45 : 1

                Column {
                    anchors.centerIn: parent
                    spacing: 20

                    LIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        iconPath:  LibrovaIcons.uploadCloud
                        iconColor: LibrovaTheme.accent
                        size:      44
                    }

                    Column {
                        spacing: 6
                        anchors.horizontalCenter: parent.horizontalCenter
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Drop files or folders here"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                        Text {
                            width: 480
                            text: "Supports .fb2, .epub, .zip, multiple files, and recursive folder imports. Dropping or selecting starts the import immediately."
                            horizontalAlignment: Text.AlignHCenter
                            font.family: LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeBase
                            color: LibrovaTheme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 12
                        LButton { text: "Select Files..."; variant: "primary"; enabled: !importAdapter.hasActiveJob; implicitWidth: 180; onClicked: _fileDlg.open() }
                        LButton { text: "Select Folder..."; variant: "secondary"; enabled: !importAdapter.hasActiveJob; implicitWidth: 180; onClicked: _folderDlg.open() }
                    }
                }
            }

            Text {
                width: parent.width - 40
                visible: _validationMsg.length > 0
                text: _validationMsg
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeSm
                color: LibrovaTheme.warning
                wrapMode: Text.WordWrap
            }

            Rectangle {
                width: parent.width - 40
                implicitHeight: _options.implicitHeight + 32
                radius: LibrovaTheme.radiusMedium
                color: LibrovaTheme.surfaceMuted
                border.color: LibrovaTheme.border
                border.width: 1
                opacity: importAdapter.hasActiveJob ? 0.45 : 1

                Column {
                    id: _options
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 12
                    Text { text: "OPTIONS"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                    LCheckBox {
                        id: _allowDuplicates
                        enabled: !importAdapter.hasActiveJob
                        text: "Allow probable duplicates"
                        checked: shellState.allowProbableDuplicates
                        onToggled: shellState.allowProbableDuplicates = checked
                    }
                    Text { width: parent.width; leftPadding: 28; text: "When enabled, Librova can continue after a probable duplicate warning and store the new book as a separate managed record."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textMuted; wrapMode: Text.WordWrap }
                    LCheckBox {
                        id: _forceEpub
                        enabled: !importAdapter.hasActiveJob
                        visible: shellController.hasConverter
                        text: "Force conversion to EPUB during import"
                        checked: shellController.hasConverter && preferences.forceEpubConversionOnImport
                        onToggled: preferences.forceEpubConversionOnImport = shellController.hasConverter && checked
                    }
                    Text { width: parent.width; leftPadding: 28; visible: shellController.hasConverter; text: "When enabled, FB2 imports use the configured converter and store the managed book as EPUB. When disabled, FB2 files stay FB2 even if a converter is configured."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textMuted; wrapMode: Text.WordWrap }
                    LCheckBox { id: _importCovers; enabled: !importAdapter.hasActiveJob; text: "Import covers"; checked: true }
                    Text { width: parent.width; leftPadding: 28; text: "When enabled, cover images are extracted from books and stored in the library. Uncheck to skip cover extraction and reduce import time and disk usage."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; color: LibrovaTheme.textMuted; wrapMode: Text.WordWrap }
                }
            }

            Repeater {
                model: importAdapter.importJobListModel
                delegate: Rectangle {
                    width: parent.width - 40
                    implicitHeight: _jobContent.implicitHeight + 28
                    radius: LibrovaTheme.radiusLarge
                    color: LibrovaTheme.surfaceMuted
                    border.color: model.isTerminal ? LibrovaTheme.border : LibrovaTheme.borderStrong
                    border.width: 1

                    Column {
                        id: _jobContent
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                        spacing: 14
                        Text { text: model.isTerminal ? "RESULT" : "RUNNING"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeXs; font.weight: LibrovaTypography.weightSemiBold; font.letterSpacing: LibrovaTypography.spacingEyebrow; color: LibrovaTheme.accent }
                        Text { text: model.isTerminal ? "Last Import Result" : "Import In Progress"; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeMd; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary }
                        Rectangle {
                            width: parent.width
                            height: 4
                            radius: 2
                            color: LibrovaTheme.border
                            visible: !model.isTerminal
                            Rectangle {
                                width: parent.width * Math.max(0, Math.min(1, model.percent / 100.0))
                                height: parent.height
                                radius: parent.radius
                                color: LibrovaTheme.accent
                                Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                            }
                        }
                        Text { width: parent.width; text: model.message; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary; wrapMode: Text.WordWrap }
                        Text { width: parent.width; text: model.isTerminal ? model.resultSummaryText : model.progressSummaryText; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; font.weight: LibrovaTypography.weightSemiBold; color: LibrovaTheme.textPrimary; wrapMode: Text.WordWrap }
                        Text { width: parent.width; text: _lastSourceSummary; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { width: parent.width; text: model.isTerminal ? model.warningsText : "The import pipeline is running. Navigation and other actions stay locked until the import finishes or is cancelled."; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; wrapMode: Text.WordWrap }
                        Text { width: parent.width; visible: model.isTerminal && model.errorText !== "No error."; text: model.errorText; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: "#EAA2A2"; wrapMode: Text.WordWrap }
                        Row {
                            spacing: 14
                            visible: !model.isTerminal
                            LButton { text: "Cancel"; variant: "secondary"; implicitWidth: 140; onClicked: importAdapter.cancelImport(model.jobId) }
                            Text { text: model.statusStr === "cancelling" || model.statusStr === "rolling_back" ? "Cancelling..." : ""; font.family: LibrovaTypography.fontFamily; font.pixelSize: LibrovaTypography.sizeBase; color: LibrovaTheme.textSecondary; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
            }
        }
    }

    property string _validationMsg: ""
    property var _pendingPaths: []
    property string _lastSourceSummary: _describeSources(shellState.sourcePaths)

    Connections {
        target: importAdapter
        function onSourcesValidated(ok, blockingMessage) {
            if (!ok) {
                _validationMsg = blockingMessage
                _pendingPaths = []
                return
            }
            _validationMsg = ""
            importAdapter.startImport(_pendingPaths, _forceEpub.checked, _importCovers.checked, _allowDuplicates.checked)
            _pendingPaths = []
        }
        function onImportCompleted(jobId, success, message) {
            if (success) {
                catalogAdapter.refresh()
                shellState.lastSection = "library"
            }
        }
    }

    function _validateAndStart(paths) {
        _validationMsg = ""
        _pendingPaths = paths
        shellState.sourcePaths = paths
        _lastSourceSummary = _describeSources(paths)
        importAdapter.validateSources(paths)
    }

    function applySourcePathsAndStart(paths) {
        if (!paths || paths.length === 0 || !root._dropImportEnabled)
            return

        _validateAndStart(paths)
    }

    function _describeSources(paths) {
        if (!paths || paths.length === 0)
            return "Drop local .fb2, .epub, or .zip files and folders here, or use Select Files... / Select Folder..."
        if (paths.length === 1)
            return paths[0]
        if (paths.length === 2)
            return paths[0] + "\n" + paths[1]
        return paths.length + " selected sources"
    }
}
