import QtQuick
import QtQuick.Controls.Basic
import LibrovaQt

// General-purpose button supporting four semantic variants.
//
// Variants:
//   "primary"     — amber-filled CTA (e.g. Continue, Save)
//   "secondary"   — SurfaceAlt bg with border (e.g. Browse, Cancel)
//   "accent"      — AccentSurface bg with amber border (e.g. Export as EPUB)
//   "destructive" — DangerSurface bg with danger border (e.g. Move to Trash)
//
// Disabled state: opacity 0.4 over the entire button (background + label).
// Hover/pressed: 120 ms ColorAnimation on background and label colour.

Button {
    id: root

    property string variant:  "secondary"
    property string iconPath: ""

    implicitWidth:  Math.max(120, implicitContentWidth + 2 * LibrovaTheme.sp4)
    implicitHeight: LibrovaTheme.controlHeight

    opacity: enabled ? 1.0 : 0.4

    background: Rectangle {
        radius:       LibrovaTheme.radiusMedium
        color:        root._bg()
        border.color: root._borderColor()
        border.width: root.variant === "primary" ? 0 : 1

        Behavior on color       { ColorAnimation { duration: LibrovaTheme.animFast } }
        Behavior on border.color { ColorAnimation { duration: LibrovaTheme.animFast } }
    }

    contentItem: Item {
        implicitWidth:  _row.implicitWidth
        implicitHeight: _row.implicitHeight

        Row {
            id: _row
            anchors.centerIn: parent
            spacing: root.iconPath.length > 0 ? 6 : 0

            // Icon slot: width collapses to 0 when no icon so text stays centred.
            Item {
                implicitWidth:  root.iconPath.length > 0 ? 14 : 0
                implicitHeight: 14
                width:          root.iconPath.length > 0 ? 14 : 0
                height:         14
                clip:           true

                LIcon {
                    anchors.centerIn: parent
                    iconPath:  root.iconPath
                    iconColor: root._fg()
                    size:      14
                }
            }

            Text {
                text:              root.text
                font.family:       LibrovaTypography.fontFamily
                font.pixelSize:    LibrovaTypography.sizeBase
                font.weight:       (root.variant === "primary" || root.variant === "destructive")
                                   ? LibrovaTypography.weightSemiBold
                                   : LibrovaTypography.weightRegular
                color:             root._fg()
                elide:             Text.ElideRight
                verticalAlignment: Text.AlignVCenter

                Behavior on color { ColorAnimation { duration: LibrovaTheme.animFast } }
            }
        }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    // ── Background resolution ─────────────────────────────────────────────────

    function _bg(): color {
        if (!root.enabled) return _bgNormal()
        if (root.pressed) return _bgPressed()
        if (root.hovered) return _bgHover()
        return _bgNormal()
    }

    function _bgNormal(): color {
        switch (root.variant) {
            case "primary":     return LibrovaTheme.accent
            case "accent":      return LibrovaTheme.accentSurface
            case "destructive": return LibrovaTheme.dangerSurface
            default:            return LibrovaTheme.surfaceAlt
        }
    }

    function _bgHover(): color {
        switch (root.variant) {
            case "primary":     return LibrovaTheme.accentBright
            case "accent":      return LibrovaTheme.accentSurfaceHover
            case "destructive": return LibrovaTheme.dangerHoverBg
            default:            return LibrovaTheme.surfaceHover
        }
    }

    function _bgPressed(): color {
        switch (root.variant) {
            case "primary":     return LibrovaTheme.accentDim
            case "accent":      return LibrovaTheme.accentMuted
            case "destructive": return LibrovaTheme.dangerSurface
            default:            return LibrovaTheme.surfaceMuted
        }
    }

    // ── Border resolution ─────────────────────────────────────────────────────

    function _borderColor(): color {
        switch (root.variant) {
            case "primary":     return "transparent"
            case "accent":      return (root.hovered && root.enabled) ? LibrovaTheme.accentBright : LibrovaTheme.accentBorder
            case "destructive": return LibrovaTheme.dangerBorder
            default:            return LibrovaTheme.border
        }
    }

    // ── Foreground resolution ─────────────────────────────────────────────────

    function _fg(): color {
        switch (root.variant) {
            case "primary":
                return LibrovaTheme.textOnAccent
            case "accent":
                return (root.hovered && root.enabled) ? LibrovaTheme.accentBright : LibrovaTheme.accent
            case "destructive":
                return (root.hovered && root.enabled) ? LibrovaTheme.dangerText : LibrovaTheme.danger
            default:
                return LibrovaTheme.textPrimary
        }
    }
}
