import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Right-side details panel for a selected book.
//
// book — JS object (null → panel has width: 0 in the parent; content is clipped)
//
// Signals:
//   closeRequested()         — close button clicked
//   exportRequested(bookId)  — parent should open FileDialog and call exportAdapter
//   exportAsEpubRequested(bookId) — parent should open FileDialog and force EPUB export
//   trashRequested(bookId)   — parent should call trashAdapter

Rectangle {
    id: root

    property var book: null
    property var details: null
    property bool loading: false

    signal closeRequested()
    signal exportRequested(var bookId)
    signal exportAsEpubRequested(var bookId)
    signal trashRequested(var bookId)
    signal copyTitleRequested(string title)

    color: LibrovaTheme.surface

    // ── Close button ───────────────────────────────────────────────────────────

    Rectangle {
        id: _closeBtn
        anchors {
            top:        parent.top
            right:      parent.right
            topMargin:  LibrovaTheme.sp3
            rightMargin: LibrovaTheme.sp3
        }
        z:      1
        width:  32; height: 32; radius: 16
        color:  _closeHover.containsMouse ? LibrovaTheme.surfaceHover : "transparent"
        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }

        Text {
            anchors.centerIn: parent
            text:           "\u00D7"
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeMd
            color:          _closeHover.containsMouse ? LibrovaTheme.textPrimary : LibrovaTheme.textSecondary
        }

        HoverHandler { id: _closeHover; cursorShape: Qt.PointingHandCursor }
        TapHandler   { onTapped: root.closeRequested() }
    }

    // ── Scrollable content ────────────────────────────────────────────────────

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip:         true

        Column {
            width:   parent.width
            padding: LibrovaTheme.paddingCard
            spacing: LibrovaTheme.sp4

            // Spacer so content clears the close button
            Item { width: 1; height: 20 }

            // ── Cover ─────────────────────────────────────────────────────────

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width:  160
                height: 216
                radius: LibrovaTheme.radiusSmall
                color:  LibrovaTheme.matte
                clip:   true

                Image {
                    anchors.fill: parent
                source: {
                    const cover = root._coverPath()
                    if (!cover || cover.length === 0)
                        return ""
                    const p = cover
                        if (p.startsWith("/") || /^[A-Za-z]:[\\/]/.test(p))
                            return "file:///" + p
                        const root2 = preferences.libraryRoot.replace(/\\/g, "/").replace(/\/$/, "")
                        return "file:///" + root2 + "/" + p
                    }
                    fillMode:     Image.PreserveAspectFit
                    visible:      status === Image.Ready
                    smooth:       true
                    asynchronous: true
                }

                Item {
                    anchors.fill: parent
                    visible:      !(root.book && root.book.coverPath && root.book.coverPath.length > 0)

                    Rectangle { anchors.fill: parent; color: LibrovaTheme.surfaceMuted }

                    Text {
                        anchors.centerIn: parent
                        text:           root._title().length > 0
                                        ? root._title().charAt(0).toUpperCase() : "?"
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: 64
                        font.weight:    LibrovaTypography.weightLight
                        color:          LibrovaTheme.accentDim
                        opacity:        0.6
                    }
                }
            }

            // ── Title ─────────────────────────────────────────────────────────

            Text {
                width:          parent.width - 2 * LibrovaTheme.paddingCard
                text:           root._title()
                font.family:    LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeMd
                font.weight:    LibrovaTypography.weightSemiBold
                color:          LibrovaTheme.textPrimary
                wrapMode:       Text.WordWrap
                lineHeight:     1.3
            }

            // ── Authors ───────────────────────────────────────────────────────

            Text {
                width:          parent.width - 2 * LibrovaTheme.paddingCard
                text:           root._authors()
                font.family:    LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeBase
                color:          LibrovaTheme.textSecondary
                wrapMode:       Text.WordWrap
                visible:        text.length > 0
            }

            // ── Metadata grid ─────────────────────────────────────────────────

            Text {
                width: parent.width - 2 * LibrovaTheme.paddingCard
                text: "Loading details..."
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: LibrovaTypography.sizeXs
                color: LibrovaTheme.textMuted
                visible: root.loading
            }

            Grid {
                width:         parent.width - 2 * LibrovaTheme.paddingCard
                columns:       2
                rowSpacing:    LibrovaTheme.sp3
                columnSpacing: LibrovaTheme.sp4

                Repeater {
                    model: root._metaRows()

                    delegate: Column {
                        spacing: 2

                        Text {
                            text:          modelData.label
                            font.family:   LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeXs
                            color:         LibrovaTheme.textMuted
                            font.letterSpacing: LibrovaTypography.spacingEyebrow
                        }

                        Text {
                            text:           modelData.value
                            font.family:    LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeSm
                            color:          LibrovaTheme.textPrimary
                        }
                    }
                }
            }

            // ── Series ────────────────────────────────────────────────────────

            Column {
                width:   parent.width - 2 * LibrovaTheme.paddingCard
                spacing: 2
                visible: root._seriesName().length > 0

                Text {
                    text:          "SERIES"
                    font.family:   LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeXs
                    color:         LibrovaTheme.textMuted
                    font.letterSpacing: LibrovaTypography.spacingEyebrow
                }

                Text {
                    text:           root.book
                                    ? (root._seriesName()
                                       + (root._seriesIndex() > 0
                                          ? " \u2023 #" + root._seriesIndex() : ""))
                                    : ""
                    font.family:    LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeSm
                    color:          LibrovaTheme.textPrimary
                }
            }

            // ── Genres ────────────────────────────────────────────────────────

            Column {
                width:   parent.width - 2 * LibrovaTheme.paddingCard
                spacing: LibrovaTheme.sp2
                visible: root._genres().length > 0

                Text {
                    text:          "GENRES"
                    font.family:   LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeXs
                    color:         LibrovaTheme.textMuted
                    font.letterSpacing: LibrovaTypography.spacingEyebrow
                }

                Flow {
                    width:   parent.width
                    spacing: LibrovaTheme.sp2

                    Repeater {
                        model: root._genres()

                        delegate: LGenreChip {
                            text:      modelData
                            checked:   true
                            checkable: false
                        }
                    }
                }
            }

            Column {
                width: parent.width - 2 * LibrovaTheme.paddingCard
                spacing: LibrovaTheme.sp2
                visible: root._description().length > 0

                Text {
                    text: "ANNOTATION"
                    font.family: LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeXs
                    color: LibrovaTheme.textMuted
                    font.letterSpacing: LibrovaTypography.spacingEyebrow
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: _descriptionText.implicitHeight + 24
                    radius: LibrovaTheme.radiusMedium
                    color: LibrovaTheme.surfaceMuted
                    border.color: LibrovaTheme.border
                    border.width: 1

                    Text {
                        id: _descriptionText
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 12
                        }
                        text: root._description()
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        font.italic: true
                        color: LibrovaTheme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ── Actions ───────────────────────────────────────────────────────

            Rectangle {
                width:  parent.width - 2 * LibrovaTheme.paddingCard
                height: 1
                color:  LibrovaTheme.border
            }

            Column {
                width:   parent.width - 2 * LibrovaTheme.paddingCard
                spacing: LibrovaTheme.sp3

                Rectangle {
                    width: parent.width
                    implicitHeight: _exportBusyText.implicitHeight + 20
                    radius: LibrovaTheme.radiusMedium
                    color: LibrovaTheme.accentSurface
                    border.color: LibrovaTheme.accentBorder
                    border.width: 1
                    visible: exportAdapter.isBusy

                    Text {
                        id: _exportBusyText
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 10
                        }
                        text: "Export in progress..."
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        font.weight: LibrovaTypography.weightSemiBold
                        color: LibrovaTheme.textPrimary
                        wrapMode: Text.WordWrap
                    }
                }

                LButton {
                    width:    parent.width
                    text:     "Export as EPUB"
                    variant:  "accent"
                    iconPath: LibrovaIcons.export_
                    visible:  root._canExportAsEpub()
                    enabled:  !exportAdapter.isBusy
                    onClicked: root.book && root.exportAsEpubRequested(root.book.bookId)
                }

                Rectangle {
                    width: parent.width
                    implicitHeight: _epubHint.implicitHeight + 20
                    radius: LibrovaTheme.radiusMedium
                    color: LibrovaTheme.surfaceMuted
                    border.color: LibrovaTheme.border
                    border.width: 1
                    visible: root._showExportAsEpubHint()

                    Text {
                        id: _epubHint
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: 10
                        }
                        text: "Configure a converter in Settings to enable EPUB export."
                        font.family: LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeXs
                        color: LibrovaTheme.textMuted
                        wrapMode: Text.WordWrap
                    }
                }

                LButton {
                    width:    parent.width
                    text:     "Export"
                    variant:  "secondary"
                    iconPath: LibrovaIcons.export_
                    enabled:  !exportAdapter.isBusy
                    onClicked: root.book && root.exportRequested(root.book.bookId)
                }

                LButton {
                    width:    parent.width
                    text:     "Move to Recycle Bin"
                    variant:  "destructive"
                    iconPath: LibrovaIcons.trash
                    enabled:  !exportAdapter.isBusy
                    onClicked: root.book && root.trashRequested(root.book.bookId)
                }
            }

            Item { width: 1; height: LibrovaTheme.sp4 }
        }
    }

    // ── Private helpers ────────────────────────────────────────────────────────

    function _metaRows() {
        if (!book) return []
        const rows = []
        if (_year() > 0)                               rows.push({ label: "YEAR",       value: String(_year())          })
        if (_language().length > 0)                    rows.push({ label: "LANGUAGE",   value: _language().toUpperCase() })
        if (_format().length > 0)                      rows.push({ label: "FORMAT",     value: _format().toUpperCase()   })
        if (_publisher().length > 0)                   rows.push({ label: "PUBLISHER",  value: _publisher()              })
        if (_isbn().length > 0)                        rows.push({ label: "ISBN",       value: _isbn()                   })
        if (_identifier().length > 0)                  rows.push({ label: "IDENTIFIER", value: _identifier()             })
        if (_sizeBytes() > 0)                          rows.push({ label: "SIZE",       value: _fmtSize(_sizeBytes())    })
        return rows
    }

    function _fmtSize(bytes) {
        if (bytes < 1024)         return bytes + " B"
        if (bytes < 1048576)      return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1048576).toFixed(1) + " MB"
    }

    function _isFb2Book() {
        return root._format().toLowerCase() === "fb2"
    }

    function _canExportAsEpub() {
        return _isFb2Book() && shellController.hasConverter
    }

    function _showExportAsEpubHint() {
        return _isFb2Book() && !shellController.hasConverter
    }

    function _hasDetails() {
        return root.details && root.details.hasBook && root.book && root.details.bookId === root.book.bookId
    }

    function _title() {
        return _hasDetails() ? root.details.title : (root.book ? root.book.title : "")
    }

    function _authors() {
        return _hasDetails() ? root.details.authors : (root.book ? root.book.authors : "")
    }

    function _language() {
        return _hasDetails() ? root.details.language : (root.book ? root.book.language : "")
    }

    function _seriesName() {
        return _hasDetails() ? root.details.seriesName : (root.book ? (root.book.seriesName || "") : "")
    }

    function _seriesIndex() {
        return _hasDetails() ? root.details.seriesIndex : (root.book ? (root.book.seriesIndex || 0) : 0)
    }

    function _year() {
        return _hasDetails() ? root.details.year : (root.book ? (root.book.year || 0) : 0)
    }

    function _genres() {
        return _hasDetails() ? root.details.genres : (root.book ? (root.book.genres || []) : [])
    }

    function _format() {
        return _hasDetails() ? root.details.format : (root.book ? (root.book.format || "") : "")
    }

    function _sizeBytes() {
        return _hasDetails() ? root.details.sizeBytes : (root.book ? (root.book.sizeBytes || 0) : 0)
    }

    function _coverPath() {
        return _hasDetails() ? root.details.coverPath : (root.book ? (root.book.coverPath || "") : "")
    }

    function _publisher() {
        return _hasDetails() ? root.details.publisher : ""
    }

    function _isbn() {
        return _hasDetails() ? root.details.isbn : ""
    }

    function _identifier() {
        return _hasDetails() ? root.details.identifier : ""
    }

    function _description() {
        return _hasDetails() ? root.details.description : ""
    }
}
