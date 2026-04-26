import QtQuick
import LibrovaQt

// Fixed-size book card: cover slot, deterministic placeholder, title and author footer.
//
// book     — JS object {bookId, title, authors, coverPath, ...}
// selected — amber ring around the full card + accent title colour
//
// Signals:
//   clicked()             — left-click selects the card
//   rightClicked(sx, sy)  — right-click; sx/sy are scene-space coordinates

Item {
    id: root

    property var  book:     null
    property bool selected: false

    signal clicked()
    signal rightClicked(real sceneX, real sceneY)

    implicitWidth:  188
    implicitHeight: 330

    readonly property int _coverW: 160
    readonly property int _coverH: 240
    readonly property var _placeholderPalettes: [
        ["#6E2E1F", "#A54F24", "#E0A347"],
        ["#533047", "#8B4550", "#D48245"],
        ["#433719", "#7A6130", "#D0A34D"],
        ["#4D2B21", "#92512C", "#D99454"],
        ["#343C2B", "#68713F", "#C99645"],
        ["#5F3027", "#A24335", "#D97950"],
        ["#3B3247", "#685175", "#C28D57"]
    ]

    Rectangle {
        id: _cardSurface
        anchors.fill: parent
        radius: LibrovaTheme.radiusSmall
        color: root.selected ? LibrovaTheme.accentMuted
              : _area.containsMouse ? LibrovaTheme.surfaceMuted : "transparent"
        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
    }

    // ── Cover slot ─────────────────────────────────────────────────────────────

    Item {
        id: _coverSlot
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        width:  root._coverW
        height: root._coverH

        Image {
            id: _coverImage
            anchors.fill: parent
            source: root._coverSrc()
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            asynchronous: true
            cache: true
            opacity: _coverImage.status === Image.Ready ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.InOutQuad } }
        }

        Rectangle {
            id: _placeholder
            anchors.fill: parent
            z: 2
            radius: LibrovaTheme.radiusSmall
            opacity: root._showPlaceholder() ? 1.0 : 0.0
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            gradient: Gradient {
                GradientStop { position: 0.0; color: root._placeholderColor(0) }
                GradientStop { position: 0.58; color: root._placeholderColor(1) }
                GradientStop { position: 1.0; color: root._placeholderColor(2) }
            }
            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.InOutQuad } }

            Rectangle {
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                    margins: 14
                }
                width: 4
                radius: 2
                color: Qt.rgba(0, 0, 0, 0.16)
            }

            Text {
                anchors.centerIn: parent
                text: root._placeholderLetter()
                font.family: LibrovaTypography.fontFamily
                font.pixelSize: 62
                font.weight: LibrovaTypography.weightSemiBold
                color: LibrovaTheme.coverPlaceholder
                opacity: 0.9
            }
        }
    }

    // ── Metadata footer ────────────────────────────────────────────────────────

    Column {
        id: _metadata
        anchors {
            top:       _coverSlot.bottom
            topMargin: 10
            left:      parent.left
            leftMargin: 12
            right:     parent.right
            rightMargin: 12
        }
        spacing: 4

        Text {
            width:          parent.width
            text:           root.book ? root.book.title : ""
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeBase
            font.weight:    LibrovaTypography.weightSemiBold
            color:          root.selected ? LibrovaTheme.accentBright : LibrovaTheme.textPrimary
            wrapMode:       Text.WordWrap
            maximumLineCount: 2
            elide:          Text.ElideRight
            lineHeightMode: Text.FixedHeight
            lineHeight:     18
            Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
        }

        Text {
            width:          parent.width
            text:           root.book ? root.book.authors : ""
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeSm
            color:          LibrovaTheme.textSecondary
            maximumLineCount: 1
            elide:          Text.ElideRight
            lineHeightMode: Text.FixedHeight
            lineHeight:     17
        }
    }

    Rectangle {
        id: _selectionRing
        anchors.fill: parent
        z: 10
        radius: LibrovaTheme.radiusSmall
        color: "transparent"
        border.color: root.selected ? LibrovaTheme.accent
                    : _area.containsMouse ? LibrovaTheme.borderStrong : "transparent"
        border.width: root.selected ? 2 : (_area.containsMouse ? 1 : 0)
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.width { NumberAnimation { duration: LibrovaTheme.animFast } }
    }

    // ── Input ──────────────────────────────────────────────────────────────────

    MouseArea {
        id: _area
        anchors.fill:    parent
        hoverEnabled:    true
        cursorShape:     Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                root.clicked()
            } else {
                const pt = root.mapToItem(null, mouse.x, mouse.y)
                root.rightClicked(pt.x, pt.y)
            }
        }
    }

    // ── Private helpers ────────────────────────────────────────────────────────

    function _hasCover() {
        return book && book.coverPath && book.coverPath.length > 0
    }

    function _coverSrc() {
        if (!_hasCover()) return ""
        const p = book.coverPath
        // coverPath from DB is relative to the library root; build an absolute file URL.
        if (p.startsWith("/") || /^[A-Za-z]:[\\/]/.test(p))
            return "file:///" + p
        const libRoot = preferences.libraryRoot.replace(/\\/g, "/").replace(/\/$/, "")
        return "file:///" + libRoot + "/" + p
    }

    function _showPlaceholder() {
        return !root._hasCover() || _coverImage.status !== Image.Ready
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
        const title = book && book.title ? book.title : "Librova"
        const authors = book && book.authors ? book.authors : ""
        return title + "|" + authors
    }

    function _placeholderLetter() {
        return (book && book.title && book.title.length > 0)
            ? book.title.charAt(0).toUpperCase()
            : "?"
    }
}
