import QtQuick
import QtQuick.Controls
import LibrovaQt

// Fixed-size book card: matte cover area, letter placeholder, title and author labels.
//
// book     — JS object {bookId, title, authors, coverPath, ...}
// selected — amber border ring + amber title colour
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

    implicitWidth:  172
    implicitHeight: 300

    // ── Cover ──────────────────────────────────────────────────────────────────

    Rectangle {
        id: coverBox
        anchors.horizontalCenter: parent.horizontalCenter
        width:  152
        height: 228
        radius: LibrovaTheme.radiusMedium
        color:  LibrovaTheme.matte
        clip:   true

        border.color: root.selected ? LibrovaTheme.accent : "transparent"
        border.width: 2

        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }

        Image {
            anchors.fill: parent
            source:       root._coverSrc()
            fillMode:     Image.PreserveAspectFit
            visible:      status === Image.Ready
            smooth:       true
            asynchronous: true
            cache:        true
        }

        // Letter placeholder — visible when no cover or image not yet loaded
        Item {
            anchors.fill: parent
            visible:      !root._hasCover()

            Rectangle { anchors.fill: parent; color: LibrovaTheme.surfaceMuted }

            Text {
                anchors.centerIn: parent
                text:             root._placeholderLetter()
                font.family:      LibrovaTypography.fontFamily
                font.pixelSize:   48
                font.weight:      LibrovaTypography.weightSemiBold
                color:            LibrovaTheme.coverPlaceholder
            }
        }

        // Hover tint overlay
        Rectangle {
            anchors.fill: parent
            color:        "#FFFFF8"
            opacity:      _area.containsMouse && !root.selected ? 0.05 : 0.0
            Behavior on opacity { NumberAnimation { duration: LibrovaTheme.animFast } }
        }
    }

    // ── Labels ─────────────────────────────────────────────────────────────────

    Column {
        anchors {
            top:       coverBox.bottom
            topMargin: 10
            left:      parent.left
            leftMargin: 10
            right:     parent.right
            rightMargin: 10
        }
        spacing: 3

        Text {
            width:          parent.width
            text:           root.book ? root.book.title : ""
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeBase
            font.weight:    LibrovaTypography.weightSemiBold
            color:          root.selected ? LibrovaTheme.accent : LibrovaTheme.textPrimary
            wrapMode:       Text.WordWrap
            maximumLineCount: 2
            elide:          Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
        }

        Text {
            width:          parent.width
            text:           root.book ? root.book.authors : ""
            font.family:    LibrovaTypography.fontFamily
            font.pixelSize: LibrovaTypography.sizeSm
            color:          LibrovaTheme.textMuted
            elide:          Text.ElideRight
        }
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
        const root = preferences.libraryRoot.replace(/\\/g, "/").replace(/\/$/, "")
        return "file:///" + root + "/" + p
    }

    function _placeholderLetter() {
        return (book && book.title && book.title.length > 0)
            ? book.title.charAt(0).toUpperCase()
            : "?"
    }
}
