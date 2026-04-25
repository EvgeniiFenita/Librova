import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Sidebar navigation button.
//
// Set `active: true` on the currently selected section.
// Disabled state (e.g. import in progress): opacity 0.45 dims the whole item.
// The amber left-edge bar is only visible in the active state.

Button {
    id: root

    property bool   active:    false
    property string iconPath:  ""
    property string glyphText: ""

    implicitWidth:  parent ? parent.width : 200
    implicitHeight: LibrovaTheme.controlHeight

    leftPadding:  LibrovaTheme.sp3
    rightPadding: LibrovaTheme.sp3
    topPadding:   0
    bottomPadding: 0

    opacity: enabled ? 1.0 : 0.45

    background: Rectangle {
        radius:       LibrovaTheme.radiusSmall
        color:        root._bg()
        border.color: root.active  ? LibrovaTheme.accentBorder
                    : root.hovered ? LibrovaTheme.borderStrong
                    : "transparent"
        border.width: 1

        Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
        Rectangle {
            visible:       root.active
            anchors {
                left:        parent.left
                leftMargin:  1
                top:         parent.top
                topMargin:   5
                bottom:      parent.bottom
                bottomMargin: 5
            }
            width:  3
            radius: 1.5
            color:  LibrovaTheme.accent
        }
    }

    contentItem: Item {
        id: _content

        readonly property real  _leftPad:    root.active ? LibrovaTheme.sp3 : LibrovaTheme.sp2
        readonly property bool  _hasIcon:    root.iconPath !== ""
        readonly property bool  _hasGlyph:   root.glyphText !== ""
        readonly property real  _iconSlotW:  16 + LibrovaTheme.sp2
        readonly property real  _glyphSlotW: 20
        readonly property color _textColor:  root.active ? LibrovaTheme.textPrimary : LibrovaTheme.textMuted

        // SVG icon for Library / Import / Settings items.
        LIcon {
            id: _icon
            visible:                _content._hasIcon
            x:                      _content._leftPad
            anchors.verticalCenter: parent.verticalCenter
            iconPath:               root.iconPath
            iconColor:              _content._textColor
            size:                   16
        }

        // Emoji glyph for collection items — rendered with system emoji font.
        Text {
            id: _glyph
            visible:                _content._hasGlyph
            x:                      _content._leftPad
            width:                  _content._glyphSlotW
            anchors.verticalCenter: parent.verticalCenter
            text:                   root.glyphText
            font.family:            "Segoe UI Emoji"
            font.pixelSize:         13
            horizontalAlignment:    Text.AlignHCenter
            verticalAlignment:      Text.AlignVCenter
        }

        Text {
            x:                   _content._leftPad
                                 + (_content._hasIcon  ? _content._iconSlotW  : 0)
                                 + (_content._hasGlyph ? _content._glyphSlotW : 0)
            width:               parent.width - x
            text:                root.text
            font.family:         LibrovaTypography.fontFamily
            font.pixelSize:      LibrovaTypography.sizeBase
            font.weight:         LibrovaTypography.weightRegular
            color:               _content._textColor
            verticalAlignment:   Text.AlignVCenter
            elide:               Text.ElideRight

            Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
        }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    function _bg(): color {
        if (root.active) {
            return root.hovered ? LibrovaTheme.navActiveHover : LibrovaTheme.navActive
        }
        if (root.pressed) return LibrovaTheme.surfaceMuted
        if (root.hovered) return LibrovaTheme.surfaceHover
        return LibrovaTheme.surfaceMuted
    }
}
