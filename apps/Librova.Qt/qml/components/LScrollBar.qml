import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// Styled vertical/horizontal scrollbar matching the warm-sepia design system.
// Usage: ScrollBar.vertical: LScrollBar {}
ScrollBar {
    id: root
    hoverEnabled: true

    // Show when position changes (mouse wheel scroll), hide after idle
    property bool _scrolling: false
    onPositionChanged: { _scrolling = true; _hideTimer.restart() }
    Timer { id: _hideTimer; interval: 800; onTriggered: root._scrolling = false }

    contentItem: Rectangle {
        implicitWidth:  root.orientation === Qt.Vertical   ? 5 : 0
        implicitHeight: root.orientation === Qt.Horizontal ? 5 : 0
        radius: 3

        HoverHandler { id: _h }

        color: root.pressed ? LibrovaTheme.accent
             : _h.hovered   ? LibrovaTheme.textSecondary
             :                 LibrovaTheme.textMuted

        opacity: root.size < 1.0 && (root.pressed || _h.hovered || root._scrolling) ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: LibrovaTheme.animFast } }
    }

    background: Item {}
}
