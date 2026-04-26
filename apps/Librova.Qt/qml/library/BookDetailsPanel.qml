import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LibrovaQt

// Right-side details panel for a selected book.
//
// book — JS object (null → panel has width: 0 in the parent; content is clipped)
//
// Signals:
//   closeRequested()              — close button clicked
//   exportRequested(bookId)       — parent should open FileDialog and call exportAdapter
//   exportAsEpubRequested(bookId) — parent should open FileDialog and force EPUB export
//   trashRequested(bookId)        — parent should call trashAdapter

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

    // Deterministic placeholder palettes — mirrors BookCard
    readonly property var _placeholderPalettes: [
        ["#6E2E1F", "#A54F24", "#E0A347"],
        ["#533047", "#8B4550", "#D48245"],
        ["#433719", "#7A6130", "#D0A34D"],
        ["#4D2B21", "#92512C", "#D99454"],
        ["#343C2B", "#68713F", "#C99645"],
        ["#5F3027", "#A24335", "#D97950"],
        ["#3B3247", "#685175", "#C28D57"]
    ]

    // ── Close button ───────────────────────────────────────────────────────────

    Rectangle {
        id: _closeBtn
        anchors {
            top:         parent.top
            right:       parent.right
            topMargin:   LibrovaTheme.sp3
            rightMargin: LibrovaTheme.sp3
        }
        z:            2
        width:        28
        height:       28
        radius:       6
        color:        _closeHover.hovered ? LibrovaTheme.surfaceHover : LibrovaTheme.surfaceMuted
        border.color: _closeHover.hovered ? LibrovaTheme.borderStrong : LibrovaTheme.border
        border.width: 1

        Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }

        LIcon {
            anchors.centerIn: parent
            iconPath:  LibrovaIcons.close
            iconColor: _closeHover.hovered ? LibrovaTheme.textPrimary : LibrovaTheme.textSecondary
            size:      14
        }

        HoverHandler { id: _closeHover; cursorShape: Qt.PointingHandCursor }
        TapHandler   { onTapped: root.closeRequested() }
    }

    // ── Main layout ────────────────────────────────────────────────────────────

    ColumnLayout {
        anchors {
            fill:         parent
            topMargin:    LibrovaTheme.sp3 + 28 + LibrovaTheme.sp3
            leftMargin:   LibrovaTheme.paddingCard
            rightMargin:  LibrovaTheme.paddingCard
            bottomMargin: LibrovaTheme.paddingCard
        }
        spacing: LibrovaTheme.sp3

        // ── Title (full width) ─────────────────────────────────────────────────

        Text {
            Layout.fillWidth: true
            text:           root._title()
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeMd
            font.weight:    LibrovaTypography.weightSemiBold
            color:          LibrovaTheme.textPrimary
            wrapMode:       Text.WordWrap
            lineHeight:     1.3
        }

        Text {
            Layout.fillWidth: true
            text:           "Loading details..."
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeXs
            color:          LibrovaTheme.textMuted
            visible:        root.loading
        }

        // ── Cover (left) + meta (right) ────────────────────────────────────────

        RowLayout {
            Layout.fillWidth: true
            spacing:          LibrovaTheme.sp4

            // Cover
            Item {
                Layout.preferredWidth:  150
                Layout.preferredHeight: 210
                Layout.alignment:       Qt.AlignTop

                // Placeholder gradient (visible until real cover loads)
                Rectangle {
                    anchors.fill:   parent
                    radius:         LibrovaTheme.radiusSmall
                    visible:        root._showCoverPlaceholder()
                    border.color:   Qt.rgba(1, 1, 1, 0.08)
                    border.width:   1
                    gradient: Gradient {
                        GradientStop { position: 0.0;  color: root._placeholderColor(0) }
                        GradientStop { position: 0.58; color: root._placeholderColor(1) }
                        GradientStop { position: 1.0;  color: root._placeholderColor(2) }
                    }

                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 10 }
                        width:  4
                        radius: 2
                        color:  Qt.rgba(0, 0, 0, 0.16)
                    }

                    Text {
                        anchors.centerIn: parent
                        text:           root._placeholderLetter()
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: 56
                        font.weight:    LibrovaTypography.weightSemiBold
                        color:          LibrovaTheme.coverPlaceholder
                        opacity:        0.9
                    }
                }

                // Real cover image
                Image {
                    id:           _coverImage
                    anchors.fill: parent
                    source: {
                        const cover = root._coverPath()
                        if (!cover || cover.length === 0) return ""
                        const p = cover
                        if (p.startsWith("/") || /^[A-Za-z]:[\\/]/.test(p))
                            return "file:///" + p
                        const root2 = preferences.libraryRoot.replace(/\\/g, "/").replace(/\/$/, "")
                        return "file:///" + root2 + "/" + p
                    }
                    fillMode:     Image.PreserveAspectFit
                    smooth:       true
                    mipmap:       true
                    asynchronous: true
                    cache:        true
                    clip:         true
                    opacity:      status === Image.Ready ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.InOutQuad } }
                }
            }

            // Authors + meta pairs (single column beside the cover)
            Column {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing:          LibrovaTheme.sp3

                // Authors — 2-line max with ellipsis; tooltip shows full list on truncation
                Item {
                    width:          parent.width
                    implicitHeight: _authorsText.implicitHeight
                    visible:        root._authors().length > 0

                    Text {
                        id:               _authorsText
                        width:            parent.width
                        text:             root._authors()
                        font.family:      LibrovaTypography.fontFamily
                        font.pixelSize:   LibrovaTypography.sizeSm
                        color:            LibrovaTheme.textSecondary
                        wrapMode:         Text.WordWrap
                        maximumLineCount: 2
                        elide:            Text.ElideRight
                    }

                    HoverHandler { id: _authorsHover }

                    ToolTip {
                        visible:  _authorsHover.hovered && _authorsText.truncated
                        delay:    500
                        timeout:  8000
                        width:    280
                        padding:  10

                        contentItem: Text {
                            text:           root._authors().split(", ").join("\n")
                            font.family:    LibrovaTypography.fontFamily
                            font.pixelSize: LibrovaTypography.sizeSm
                            color:          LibrovaTheme.textPrimary
                            wrapMode:       Text.WordWrap
                            lineHeight:     1.4
                        }

                        background: Rectangle {
                            color:        LibrovaTheme.surfaceAlt
                            border.color: LibrovaTheme.borderStrong
                            border.width: 1
                            radius:       LibrovaTheme.radiusSmall
                        }
                    }
                }

                Column {
                    width:   parent.width
                    spacing: LibrovaTheme.sp2

                    Repeater {
                        model: root._coverColMeta()

                        delegate: Column {
                            width:   parent.width
                            spacing: 1

                            Text {
                                text:               modelData.label
                                font.family:        LibrovaTypography.fontFamily
                                font.pixelSize:     LibrovaTypography.sizeXs
                                color:              LibrovaTheme.textMuted
                                font.letterSpacing: LibrovaTypography.spacingEyebrow
                            }

                            Text {
                                width:          parent.width
                                text:           modelData.value
                                font.family:    LibrovaTypography.fontFamily
                                font.pixelSize: LibrovaTypography.sizeSm
                                color:          LibrovaTheme.textSecondary
                                wrapMode:       Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

        // ── ISBN + Series (full width, below cover row) ────────────────────────

        // ── ISBN + Series (2-column) + Publisher (full width) ─────────────────

        Column {
            Layout.fillWidth: true
            spacing:          LibrovaTheme.sp3
            visible:          root._isbn().length > 0 || root._seriesName().length > 0 || root._publisher().length > 0

            // ISBN and Series side by side
            Row {
                width:   parent.width
                spacing: LibrovaTheme.sp4
                visible: root._isbn().length > 0 || root._seriesName().length > 0

                Column {
                    width:   (root._isbn().length > 0 && root._seriesName().length > 0)
                             ? (parent.width - LibrovaTheme.sp4) / 2
                             : parent.width
                    spacing: 1
                    visible: root._isbn().length > 0

                    Text {
                        text:               "ISBN"
                        font.family:        LibrovaTypography.fontFamily
                        font.pixelSize:     LibrovaTypography.sizeXs
                        color:              LibrovaTheme.textMuted
                        font.letterSpacing: LibrovaTypography.spacingEyebrow
                    }

                    Text {
                        width:          parent.width
                        text:           root._isbn()
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        color:          LibrovaTheme.textSecondary
                        wrapMode:       Text.WordWrap
                    }
                }

                Column {
                    width:   (root._isbn().length > 0 && root._seriesName().length > 0)
                             ? (parent.width - LibrovaTheme.sp4) / 2
                             : parent.width
                    spacing: 1
                    visible: root._seriesName().length > 0

                    Text {
                        text:               "SERIES"
                        font.family:        LibrovaTypography.fontFamily
                        font.pixelSize:     LibrovaTypography.sizeXs
                        color:              LibrovaTheme.textMuted
                        font.letterSpacing: LibrovaTypography.spacingEyebrow
                    }

                    Text {
                        width:          parent.width
                        text:           root.book
                                        ? (root._seriesName()
                                           + (root._seriesIndex() > 0 ? " \u2023 #" + root._seriesIndex() : ""))
                                        : ""
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        color:          LibrovaTheme.textSecondary
                        wrapMode:       Text.WordWrap
                    }
                }
            }

            // Publisher — full width (can be long)
            Column {
                width:   parent.width
                spacing: 1
                visible: root._publisher().length > 0

                Text {
                    text:               "PUBLISHER"
                    font.family:        LibrovaTypography.fontFamily
                    font.pixelSize:     LibrovaTypography.sizeXs
                    color:              LibrovaTheme.textMuted
                    font.letterSpacing: LibrovaTypography.spacingEyebrow
                }

                Text {
                    width:          parent.width
                    text:           root._publisher()
                    font.family:    LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeSm
                    color:          LibrovaTheme.textSecondary
                    wrapMode:       Text.WordWrap
                }
            }
        }

        // ── Genres ────────────────────────────────────────────────────────────

        Column {
            Layout.fillWidth: true
            spacing:          LibrovaTheme.sp2
            visible:          root._genres().length > 0

            Text {
                text:               "GENRES"
                font.family:        LibrovaTypography.fontFamily
                font.pixelSize:     LibrovaTypography.sizeXs
                color:              LibrovaTheme.textMuted
                font.letterSpacing: LibrovaTypography.spacingEyebrow
            }

            Flow {
                width:   parent.width
                spacing: LibrovaTheme.sp2

                Repeater {
                    model: root._genres()

                    delegate: Rectangle {
                        implicitHeight: 26
                        implicitWidth:  _genreLabel.implicitWidth + 2 * LibrovaTheme.sp3
                        radius:         LibrovaTheme.radiusLarge
                        color:          LibrovaTheme.surfaceAlt
                        border.color:   LibrovaTheme.border
                        border.width:   1

                        Text {
                            id:               _genreLabel
                            anchors.centerIn: parent
                            text:             modelData
                            font.family:      LibrovaTypography.fontFamily
                            font.pixelSize:   LibrovaTypography.sizeSm
                            color:            LibrovaTheme.textSecondary
                        }
                    }
                }
            }
        }

        // ── Annotation — expands to fill available space; box is capped at content height ──

        // Fills all remaining space when description is present
        Item {
            id:               _annotSection
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible:          root._description().length > 0

            Text {
                id: _annotLabel
                anchors { top: parent.top; left: parent.left; right: parent.right }
                text:               "ANNOTATION"
                font.family:        LibrovaTypography.fontFamily
                font.pixelSize:     LibrovaTypography.sizeXs
                color:              LibrovaTheme.textMuted
                font.letterSpacing: LibrovaTypography.spacingEyebrow
            }

            Rectangle {
                anchors {
                    top:       _annotLabel.bottom
                    topMargin: LibrovaTheme.sp2
                    left:      parent.left
                    right:     parent.right
                }
                // Grow to fill available area, but never taller than the actual content
                height: Math.min(
                    parent.height - _annotLabel.implicitHeight - LibrovaTheme.sp2,
                    Math.max(60, _descText.implicitHeight + 24)
                )
                radius:       LibrovaTheme.radiusMedium
                color:        LibrovaTheme.surfaceMuted
                border.color: LibrovaTheme.border
                border.width: 1
                clip:         true

                Flickable {
                    anchors {
                        fill:        parent
                        margins:     12
                        rightMargin: 18
                    }
                    contentWidth:  width
                    contentHeight: _descText.implicitHeight
                    clip:          true
                    interactive:   _descText.implicitHeight > height
                    ScrollBar.vertical: LScrollBar {
                        visible: _descText.implicitHeight > parent.height
                    }

                    Text {
                        id:             _descText
                        width:          parent.width
                        text:           root._description()
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeSm
                        font.italic:    true
                        color:          LibrovaTheme.textSecondary
                        wrapMode:       Text.WordWrap
                        lineHeight:     1.5
                    }
                }
            }
        }

        // Spacer — used only when there is no annotation; keeps buttons at the bottom
        Item {
            Layout.fillHeight: true
            visible:           root._description().length === 0
        }

        // ── Divider ───────────────────────────────────────────────────────────

        Rectangle {
            Layout.fillWidth: true
            height:           1
            color:            LibrovaTheme.border
        }

        // ── Action buttons (3 equal, stacked) ─────────────────────────────────

        Column {
            Layout.fillWidth: true
            spacing:          LibrovaTheme.sp3

            Rectangle {
                width:          parent.width
                implicitHeight: _exportBusyText.implicitHeight + 20
                radius:         LibrovaTheme.radiusMedium
                color:          LibrovaTheme.accentSurface
                border.color:   LibrovaTheme.accentBorder
                border.width:   1
                visible:        exportAdapter.isBusy

                Text {
                    id: _exportBusyText
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                    text:           "Export in progress..."
                    font.family:    LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeSm
                    font.weight:    LibrovaTypography.weightSemiBold
                    color:          LibrovaTheme.textPrimary
                    wrapMode:       Text.WordWrap
                }
            }

            // Export as EPUB
            Rectangle {
                width:        parent.width
                height:       46
                radius:       LibrovaTheme.radiusMedium
                color:        _epubHover.hovered && !exportAdapter.isBusy
                              ? LibrovaTheme.accentSurfaceHover : LibrovaTheme.accentSurface
                border.color: _epubHover.hovered && !exportAdapter.isBusy
                              ? LibrovaTheme.accent : LibrovaTheme.accentBorder
                border.width: 1
                visible:      root._canExportAsEpub()
                opacity:      exportAdapter.isBusy ? 0.4 : 1.0

                Behavior on color        { ColorAnimation { duration: LibrovaTheme.animFast } }
                Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }

                Row {
                    anchors.centerIn: parent
                    spacing:          LibrovaTheme.sp2

                    LIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconPath:  LibrovaIcons.export_
                        iconColor: _epubHover.hovered && !exportAdapter.isBusy
                                   ? LibrovaTheme.accentBright : LibrovaTheme.accent
                        size:      22
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text:           "Export as EPUB"
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeMd
                        font.weight:    LibrovaTypography.weightSemiBold
                        color:          _epubHover.hovered && !exportAdapter.isBusy
                                        ? LibrovaTheme.accentBright : LibrovaTheme.accent
                        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
                    }
                }

                HoverHandler { id: _epubHover; cursorShape: Qt.PointingHandCursor }
                TapHandler   {
                    enabled:  !exportAdapter.isBusy
                    onTapped: root.book && root.exportAsEpubRequested(root.book.bookId)
                }
            }

            // EPUB hint
            Rectangle {
                width:          parent.width
                implicitHeight: _epubHint.implicitHeight + 20
                radius:         LibrovaTheme.radiusMedium
                color:          LibrovaTheme.surfaceMuted
                border.color:   LibrovaTheme.border
                border.width:   1
                visible:        root._showExportAsEpubHint()

                Text {
                    id: _epubHint
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                    text:           "Configure a converter in Settings to enable EPUB export."
                    font.family:    LibrovaTypography.fontFamily
                    font.pixelSize: LibrovaTypography.sizeXs
                    color:          LibrovaTheme.textMuted
                    wrapMode:       Text.WordWrap
                }
            }

            // Export
            Rectangle {
                width:        parent.width
                height:       46
                radius:       LibrovaTheme.radiusMedium
                color:        _exportHover.hovered && !exportAdapter.isBusy
                              ? LibrovaTheme.surfaceHover : LibrovaTheme.surfaceAlt
                border.color: LibrovaTheme.border
                border.width: 1
                opacity:      exportAdapter.isBusy ? 0.4 : 1.0

                Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }

                Row {
                    anchors.centerIn: parent
                    spacing:          LibrovaTheme.sp2

                    LIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconPath:  LibrovaIcons.export_
                        iconColor: LibrovaTheme.textSecondary
                        size:      22
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text:           "Export"
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeMd
                        color:          LibrovaTheme.textSecondary
                    }
                }

                HoverHandler { id: _exportHover; cursorShape: Qt.PointingHandCursor }
                TapHandler   {
                    enabled:  !exportAdapter.isBusy
                    onTapped: root.book && root.exportRequested(root.book.bookId)
                }
            }

            // Move to Recycle Bin
            Rectangle {
                width:        parent.width
                height:       46
                radius:       LibrovaTheme.radiusMedium
                color:        _trashHover.hovered && !exportAdapter.isBusy
                              ? LibrovaTheme.dangerHoverBg : LibrovaTheme.dangerSurface
                border.color: LibrovaTheme.dangerBorder
                border.width: 1
                opacity:      exportAdapter.isBusy ? 0.4 : 1.0

                Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }

                Row {
                    anchors.centerIn: parent
                    spacing:          LibrovaTheme.sp2

                    LIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconPath:  LibrovaIcons.trash
                        iconColor: _trashHover.hovered && !exportAdapter.isBusy ? LibrovaTheme.dangerText : LibrovaTheme.danger
                        size:      22
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text:           "Move to Recycle Bin"
                        font.family:    LibrovaTypography.fontFamily
                        font.pixelSize: LibrovaTypography.sizeMd
                        font.weight:    LibrovaTypography.weightSemiBold
                        color:          _trashHover.hovered && !exportAdapter.isBusy ? LibrovaTheme.dangerText : LibrovaTheme.danger
                        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
                    }
                }

                HoverHandler { id: _trashHover; cursorShape: Qt.PointingHandCursor }
                TapHandler   {
                    enabled:  !exportAdapter.isBusy
                    onTapped: root.book && root.trashRequested(root.book.bookId)
                }
            }
        }
    }

    // ── Private helpers ────────────────────────────────────────────────────────

    // Meta shown beside the cover: year, language, format, size (publisher moved to full-width section)
    function _coverColMeta() {
        if (!book) return []
        const rows = []
        if (_year() > 0)             rows.push({ label: "YEAR",      value: String(_year())           })
        if (_language().length > 0)  rows.push({ label: "LANGUAGE",  value: _language().toUpperCase()  })
        if (_format().length > 0)    rows.push({ label: "FORMAT",    value: _format().toUpperCase()    })
        if (_sizeBytes() > 0)        rows.push({ label: "SIZE",      value: _fmtSize(_sizeBytes())     })
        return rows
    }

    function _fmtSize(bytes) {
        if (bytes < 1024)    return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1048576).toFixed(1) + " MB"
    }

    function _isFb2Book()           { return root._format().toLowerCase() === "fb2" }
    function _canExportAsEpub()     { return _isFb2Book() && shellController.hasConverter }
    function _showExportAsEpubHint(){ return _isFb2Book() && !shellController.hasConverter }

    function _hasDetails() {
        return root.details && root.details.hasBook && root.book && root.details.bookId === root.book.bookId
    }

    function _title()      { return _hasDetails() ? root.details.title       : (root.book ? root.book.title               : "") }
    function _authors()    { return _hasDetails() ? root.details.authors     : (root.book ? root.book.authors             : "") }
    function _language()   { return _hasDetails() ? root.details.language    : (root.book ? root.book.language            : "") }
    function _seriesName() { return _hasDetails() ? root.details.seriesName  : (root.book ? (root.book.seriesName  || "") : "") }
    function _seriesIndex(){ return _hasDetails() ? root.details.seriesIndex : (root.book ? (root.book.seriesIndex || 0)  : 0)  }
    function _year()       { return _hasDetails() ? root.details.year        : (root.book ? (root.book.year        || 0)  : 0)  }
    function _genres()     { return _hasDetails() ? root.details.genres      : (root.book ? (root.book.genres      || []) : []) }
    function _format()     { return _hasDetails() ? root.details.format      : (root.book ? (root.book.format      || "") : "") }
    function _sizeBytes()  { return _hasDetails() ? root.details.sizeBytes   : (root.book ? (root.book.sizeBytes   || 0)  : 0)  }
    function _coverPath()  { return _hasDetails() ? root.details.coverPath   : (root.book ? (root.book.coverPath   || "") : "") }
    function _publisher()  { return _hasDetails() ? root.details.publisher   : "" }
    function _isbn()       { return _hasDetails() ? root.details.isbn        : "" }
    function _description(){ return _hasDetails() ? root.details.description : "" }

    // ── Cover placeholder helpers (mirrors BookCard) ───────────────────────────

    function _showCoverPlaceholder() {
        return !(root.book && root._coverPath().length > 0) || _coverImage.status !== Image.Ready
    }

    function _placeholderColor(offset) {
        const palette = root._placeholderPalettes[root._placeholderPaletteIndex()]
        return palette[offset % palette.length]
    }

    function _placeholderPaletteIndex() {
        const seed = root._placeholderSeedText()
        let hash = 17
        for (let i = 0; i < seed.length; ++i)
            hash = ((hash * 31) + seed.charCodeAt(i)) | 0
        return Math.abs(hash) % root._placeholderPalettes.length
    }

    function _placeholderSeedText() {
        const title   = root.book && root.book.title   ? root.book.title   : "Librova"
        const authors = root.book && root.book.authors ? root.book.authors : ""
        return title + "|" + authors
    }

    function _placeholderLetter() {
        const t = root._title()
        return t.length > 0 ? t.charAt(0).toUpperCase() : "?"
    }
}
